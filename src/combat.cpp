#include "combat.h"
#include "settings.h"

DWORD WINAPI AntiRecoilWorker(LPVOID lpParam) {
    static float smoothRemainderX = 0.0f;
    static float smoothRemainderY = 0.0f;

    while (g_Running.load()) {
        Sleep(8);

        if (g_Settings.menuOpen || !g_hGameWnd) continue;
        if (GetForegroundWindow() != g_hGameWnd) continue;
        if (!g_Settings.localPlayerAlive) continue;

        int totalMoveX = 0;
        int totalMoveY = 0;

        if (g_Settings.aimAssistEnabled && g_HasAimTarget.load()) {
            bool aimKeyActive = (GetAsyncKeyState(g_Settings.aimAssistKey) & 0x8000) != 0;
            if (aimKeyActive) {
                float dx = g_AimTargetDeltaX.load();
                float dy = g_AimTargetDeltaY.load();

                float smooth = (g_Settings.aimAssistSmooth >= 1.0f) ? g_Settings.aimAssistSmooth : 1.0f;
                float stepX = (dx / smooth) + smoothRemainderX;
                float stepY = (dy / smooth) + smoothRemainderY;

                int moveX = (int)stepX;
                int moveY = (int)stepY;

                smoothRemainderX = stepX - (float)moveX;
                smoothRemainderY = stepY - (float)moveY;

                totalMoveX += moveX;
                totalMoveY += moveY;
            } else {
                smoothRemainderX = 0.0f;
                smoothRemainderY = 0.0f;
            }
        }

        if (g_Settings.antiRecoilEnabled) {
            bool isShooting = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            bool isAiming   = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

            if (isShooting && (!g_Settings.antiRecoilOnlyWhenScoped || isAiming)) {
                static DWORD lastRecoilTime = 0;
                DWORD now = GetTickCount();
                DWORD delay = (DWORD)(g_Settings.antiRecoilDelay >= 5.0f ? g_Settings.antiRecoilDelay : 15.0f);

                if (now - lastRecoilTime >= delay) {
                    lastRecoilTime = now;
                    totalMoveX += (int)g_Settings.antiRecoilHorizontal;
                    totalMoveY += (int)g_Settings.antiRecoilStrength;
                }
            }
        }

        if (totalMoveX != 0 || totalMoveY != 0) {
            mouse_event(MOUSEEVENTF_MOVE, (DWORD)totalMoveX, (DWORD)totalMoveY, 0, 0);
        }
    }
    return 0;
}
