#include "settings.h"

Settings g_Settings;
std::vector<PlayerData> g_Players;
std::vector<WorldObjectData> g_WorldObjects;
std::mutex g_DataMutex;

std::atomic<bool> g_Running(true);
std::atomic<bool> g_HasAimTarget(false);
std::atomic<float> g_AimTargetDeltaX(0.0f);
std::atomic<float> g_AimTargetDeltaY(0.0f);
Vector2 g_AimTarget2D = { 0.0f, 0.0f };

float g_ViewProj[16] = {};
bool g_CameraValid = false;
bool g_DebugMatrixOnScreen = false;
float g_CamFov = 0.0f;
float g_CamAspect = 0.0f;
float g_CamNear = 0.0f;
float g_CamFar = 0.0f;
Vector3 g_CamEye = { 0.0f, 0.0f, 0.0f };
Vector3 g_CamRight = { 0.0f, 0.0f, 0.0f };
Vector3 g_CamForward = { 0.0f, 0.0f, 0.0f };

HWND g_hGameWnd = nullptr;

void Settings::SaveConfig(const char* filename) {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    std::string fullPath = std::string(path) + filename;

    auto writeBool = [&](const char* sec, const char* key, bool val) {
        WritePrivateProfileStringA(sec, key, val ? "1" : "0", fullPath.c_str());
    };
    auto writeFloat = [&](const char* sec, const char* key, float val) {
        char buf[64];
        sprintf_s(buf, "%.3f", val);
        WritePrivateProfileStringA(sec, key, buf, fullPath.c_str());
    };
    auto writeInt = [&](const char* sec, const char* key, int val) {
        char buf[64];
        sprintf_s(buf, "%d", val);
        WritePrivateProfileStringA(sec, key, buf, fullPath.c_str());
    };
    auto writeColor = [&](const char* sec, const char* key, const float c[4]) {
        char buf[128];
        sprintf_s(buf, "%.3f,%.3f,%.3f,%.3f", c[0], c[1], c[2], c[3]);
        WritePrivateProfileStringA(sec, key, buf, fullPath.c_str());
    };

    writeBool("ESP", "Enabled", espEnabled);
    writeBool("ESP", "Box2D", espBox);
    writeBool("ESP", "Box3D", esp3DBox);
    writeBool("ESP", "Skeleton", espSkeleton);
    writeFloat("ESP", "SkeletonThickness", espSkeletonThickness);
    writeBool("ESP", "Snaplines", espSnaplines);
    writeBool("ESP", "Names", espNames);
    writeBool("ESP", "Distance", espDistance);
    writeBool("ESP", "HealthBar", espHealthBar);
    writeBool("ESP", "HeadPoint", espHeadPoint);
    writeFloat("ESP", "HeadRadius", espHeadRadius);
    writeBool("ESP", "OffscreenArrows", espOffscreenArrows);
    writeFloat("ESP", "OffscreenRadius", offscreenRadius);
    writeFloat("ESP", "MaxDistance", espMaxDistance);
    writeBool("ESP", "FilterDowned", espFilterDowned);
    writeBool("ESP", "ShowDownedTag", espShowDownedState);
    writeBool("ESP", "ShowKitTag", espShowKitTag);

    writeBool("World", "Vehicles", espWorldVehicles);
    writeBool("World", "Supplies", espWorldSupplies);
    writeBool("World", "ControlPoints", espWorldControlPoints);
    writeBool("World", "Hazards", espHazards);
    writeFloat("World", "MaxDist", espWorldMaxDist);

    writeBool("AimWarning", "Enabled", aimWarningEnabled);
    writeFloat("AimWarning", "MaxDist", aimWarningMaxDist);
    writeFloat("AimWarning", "AngleDeg", aimWarningAngleDeg);

    writeBool("AimAssist", "Enabled", aimAssistEnabled);
    writeFloat("AimAssist", "Smooth", aimAssistSmooth);
    writeFloat("AimAssist", "FOV", espAimbotFov);
    writeInt("AimAssist", "Key", aimAssistKey);
    writeInt("AimAssist", "TargetBone", aimAssistTargetBone);
    writeBool("AimAssist", "Snapline", aimAssistSnapline);
    writeBool("AimAssist", "FilterDowned", aimAssistFilterDowned);

    writeBool("Combat", "AntiRecoil", antiRecoilEnabled);
    writeFloat("Combat", "RecoilStrength", antiRecoilStrength);
    writeFloat("Combat", "RecoilHorizontal", antiRecoilHorizontal);
    writeFloat("Combat", "RecoilDelay", antiRecoilDelay);
    writeBool("Combat", "RecoilOnlyScoped", antiRecoilOnlyWhenScoped);

    writeBool("Radar", "Enabled", radarEnabled);
    writeFloat("Radar", "Range", radarRange);
    writeFloat("Radar", "Size", radarSize);
    writeBool("Radar", "ViewCones", radarViewCones);
    writeBool("Radar", "ShowStances", radarShowStances);
    writeBool("Radar", "ShowVehicles", radarShowVehicles);
    writeBool("Radar", "ShowSupplies", radarShowSupplies);

    writeColor("Colors", "Enemy", colEnemy);
    writeColor("Colors", "Ally", colAlly);
    writeColor("Colors", "Downed", colDowned);
    writeColor("Colors", "Skeleton", colSkel);
    writeColor("Colors", "Hazard", colHazard);
    writeColor("Colors", "Warning", colWarn);
}

void Settings::LoadConfig(const char* filename) {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    std::string fullPath = std::string(path) + filename;

    DWORD attr = GetFileAttributesA(fullPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return;

    auto readBool = [&](const char* sec, const char* key, bool def) -> bool {
        return GetPrivateProfileIntA(sec, key, def ? 1 : 0, fullPath.c_str()) != 0;
    };
    auto readFloat = [&](const char* sec, const char* key, float def) -> float {
        char buf[64];
        char defBuf[64];
        sprintf_s(defBuf, "%.3f", def);
        GetPrivateProfileStringA(sec, key, defBuf, buf, sizeof(buf), fullPath.c_str());
        return (float)atof(buf);
    };
    auto readInt = [&](const char* sec, const char* key, int def) -> int {
        return (int)GetPrivateProfileIntA(sec, key, def, fullPath.c_str());
    };
    auto readColor = [&](const char* sec, const char* key, float c[4]) {
        char buf[128];
        char defBuf[128];
        sprintf_s(defBuf, "%.3f,%.3f,%.3f,%.3f", c[0], c[1], c[2], c[3]);
        GetPrivateProfileStringA(sec, key, defBuf, buf, sizeof(buf), fullPath.c_str());
        float r = 0, g = 0, b = 0, a = 1.0f;
        if (sscanf_s(buf, "%f,%f,%f,%f", &r, &g, &b, &a) >= 3) {
            c[0] = r; c[1] = g; c[2] = b; c[3] = a;
        }
    };

    espEnabled = readBool("ESP", "Enabled", espEnabled);
    espBox = readBool("ESP", "Box2D", espBox);
    esp3DBox = readBool("ESP", "Box3D", esp3DBox);
    espSkeleton = readBool("ESP", "Skeleton", espSkeleton);
    espSkeletonThickness = readFloat("ESP", "SkeletonThickness", espSkeletonThickness);
    espSnaplines = readBool("ESP", "Snaplines", espSnaplines);
    espNames = readBool("ESP", "Names", espNames);
    espDistance = readBool("ESP", "Distance", espDistance);
    espHealthBar = readBool("ESP", "HealthBar", espHealthBar);
    espHeadPoint = readBool("ESP", "HeadPoint", espHeadPoint);
    espHeadRadius = readFloat("ESP", "HeadRadius", espHeadRadius);
    espOffscreenArrows = readBool("ESP", "OffscreenArrows", espOffscreenArrows);
    offscreenRadius = readFloat("ESP", "OffscreenRadius", offscreenRadius);
    espMaxDistance = readFloat("ESP", "MaxDistance", espMaxDistance);
    espFilterDowned = readBool("ESP", "FilterDowned", espFilterDowned);
    espShowDownedState = readBool("ESP", "ShowDownedTag", espShowDownedState);
    espShowKitTag = readBool("ESP", "ShowKitTag", espShowKitTag);

    espWorldVehicles = readBool("World", "Vehicles", espWorldVehicles);
    espWorldSupplies = readBool("World", "Supplies", espWorldSupplies);
    espWorldControlPoints = readBool("World", "ControlPoints", espWorldControlPoints);
    espHazards = readBool("World", "Hazards", espHazards);
    espWorldMaxDist = readFloat("World", "MaxDist", espWorldMaxDist);

    aimWarningEnabled = readBool("AimWarning", "Enabled", aimWarningEnabled);
    aimWarningMaxDist = readFloat("AimWarning", "MaxDist", aimWarningMaxDist);
    aimWarningAngleDeg = readFloat("AimWarning", "AngleDeg", aimWarningAngleDeg);

    aimAssistEnabled = readBool("AimAssist", "Enabled", aimAssistEnabled);
    aimAssistSmooth = readFloat("AimAssist", "Smooth", aimAssistSmooth);
    espAimbotFov = readFloat("AimAssist", "FOV", espAimbotFov);
    aimAssistKey = readInt("AimAssist", "Key", aimAssistKey);
    aimAssistTargetBone = readInt("AimAssist", "TargetBone", aimAssistTargetBone);
    aimAssistSnapline = readBool("AimAssist", "Snapline", aimAssistSnapline);
    aimAssistFilterDowned = readBool("AimAssist", "FilterDowned", aimAssistFilterDowned);

    antiRecoilEnabled = readBool("Combat", "AntiRecoil", antiRecoilEnabled);
    antiRecoilStrength = readFloat("Combat", "RecoilStrength", antiRecoilStrength);
    antiRecoilHorizontal = readFloat("Combat", "RecoilHorizontal", antiRecoilHorizontal);
    antiRecoilDelay = readFloat("Combat", "RecoilDelay", antiRecoilDelay);
    antiRecoilOnlyWhenScoped = readBool("Combat", "RecoilOnlyScoped", antiRecoilOnlyWhenScoped);

    radarEnabled = readBool("Radar", "Enabled", radarEnabled);
    radarRange = readFloat("Radar", "Range", radarRange);
    radarSize = readFloat("Radar", "Size", radarSize);
    radarViewCones = readBool("Radar", "ViewCones", radarViewCones);
    radarShowStances = readBool("Radar", "ShowStances", radarShowStances);
    radarShowVehicles = readBool("Radar", "ShowVehicles", radarShowVehicles);
    radarShowSupplies = readBool("Radar", "ShowSupplies", radarShowSupplies);

    readColor("Colors", "Enemy", colEnemy);
    readColor("Colors", "Ally", colAlly);
    readColor("Colors", "Downed", colDowned);
    readColor("Colors", "Skeleton", colSkel);
    readColor("Colors", "Hazard", colHazard);
    readColor("Colors", "Warning", colWarn);
}
