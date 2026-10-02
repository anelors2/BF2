#pragma once
#include <windows.h>
#include <string>
#include "settings.h"
#include "memory.h"

class GameMemory {
public:
    static bool WorldToScreen(const Vector3& world, Vector2& screen, int screenWidth, int screenHeight);
    static void UpdateCamera(const Memory& mem);
    static void ReadPlayers(const Memory& mem);
    static void ReadWorldObjects(const Memory& mem);
    static void ProcessPlayerNode(const Memory& mem, uintptr_t playerPtr);
};

DWORD WINAPI MemoryReaderWorker(LPVOID lpParam);
