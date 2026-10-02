#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <cstdint>

class Memory {
public:
    HANDLE processHandle = nullptr;
    DWORD processId = 0;
    uintptr_t baseAddress = 0;
    uintptr_t rendDX9Base = 0;

    ~Memory() {
        Detach();
    }

    static bool EnableDebugPrivilege() {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;
        LUID luid;
        if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
            CloseHandle(hToken);
            return false;
        }
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
        bool success = (GetLastError() == ERROR_SUCCESS);
        CloseHandle(hToken);
        return success;
    }

    bool Attach(const std::wstring& processName) {
        Detach();
        EnableDebugPrivilege();

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W entry = {};
        entry.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(processName.c_str(), entry.szExeFile) == 0) {
                    processId = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);

        if (processId == 0) return false;

        DWORD accessLevels[] = {
            PROCESS_ALL_ACCESS,
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
            PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
            PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION
        };

        for (DWORD access : accessLevels) {
            processHandle = OpenProcess(access, FALSE, processId);
            if (processHandle) break;
        }

        if (!processHandle) return false;

        baseAddress = GetModuleBase(processName);
        rendDX9Base = GetModuleBase(L"RendDX9.dll");
        return true;
    }

    HWND FindMainWindow() const {
        if (!processId) return nullptr;
        struct Ctx { DWORD pid; HWND found; } ctx{ processId, nullptr };
        EnumWindows(
            [](HWND wnd, LPARAM lp) -> BOOL {
                auto* c = reinterpret_cast<Ctx*>(lp);
                DWORD owner = 0;
                GetWindowThreadProcessId(wnd, &owner);
                if (owner == c->pid && IsWindowVisible(wnd) && !GetWindow(wnd, GW_OWNER)) {
                    c->found = wnd;
                    return FALSE;
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&ctx));
        return ctx.found;
    }

    void Detach() {
        if (processHandle) {
            CloseHandle(processHandle);
            processHandle = nullptr;
        }
        processId = 0;
        baseAddress = 0;
        rendDX9Base = 0;
    }

    bool IsAttached() const {
        if (!processHandle) return false;
        DWORD exitCode = 0;
        if (GetExitCodeProcess(processHandle, &exitCode)) {
            return exitCode == STILL_ACTIVE;
        }
        return false;
    }

    uintptr_t GetModuleBase(const std::wstring& moduleName) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
        if (snapshot == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32W entry = {};
        entry.dwSize = sizeof(MODULEENTRY32W);
        uintptr_t base = 0;

        if (Module32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(moduleName.c_str(), entry.szModule) == 0) {
                    base = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                    break;
                }
            } while (Module32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return base;
    }

    template <typename T>
    T Read(uintptr_t address) const {
        T buffer{};
        if (processHandle && address) {
            ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(address), &buffer, sizeof(T), nullptr);
        }
        return buffer;
    }

    bool ReadRaw(uintptr_t address, void* buffer, SIZE_T size) const {
        if (!processHandle || !address || !buffer || size == 0) return false;
        SIZE_T read = 0;
        return ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(address), buffer, size, &read) && (read == size);
    }

    std::string ReadMsvcString(uintptr_t addr) const {
        if (!processHandle || !addr) return "";
        uint32_t cap = Read<uint32_t>(addr + 24);
        uint32_t len = Read<uint32_t>(addr + 20);
        if (len == 0 || len > 64) return "";

        uintptr_t charPtr = (cap >= 16) ? Read<uintptr_t>(addr + 4) : (addr + 4);
        if (!charPtr) return "";

        std::string result(len, '\0');
        if (ReadRaw(charPtr, &result[0], len)) {
            return result;
        }
        return "";
    }

    template <typename T>
    bool Write(uintptr_t address, const T& value) const {
        if (!processHandle || !address) return false;
        SIZE_T written = 0;
        return WriteProcessMemory(processHandle, reinterpret_cast<LPVOID>(address), &value, sizeof(T), &written) && written == sizeof(T);
    }
};
