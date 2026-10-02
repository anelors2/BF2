#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

struct Vector3 {
    float x, y, z;
};

struct Vector2 {
    float x, y;
};

struct Matrix4x4 {
    float m[4][4];
};

struct PlayerData {
    uintptr_t address;
    std::string name;
    uint8_t ai;
    uint8_t alive;
    uint32_t team;
    Vector3 pos;
    Vector3 headPos;
    bool hasPos;
    float health;
    float maxHealth;
    bool isVehicle;
    std::string vehicleName;
    bool isDowned;
    float yaw;
    bool hasSkeleton = false;
    std::vector<std::pair<Vector3, Vector3>> skeletonSegments;
    int pose = 0;
    std::string kitName;
    bool isLocal = false;
};

enum class WorldObjectType {
    Vehicle,
    SupplyCrate,
    ControlPoint,
    Deployable,
    Hazard
};

struct WorldObjectData {
    uintptr_t address;
    std::string name;
    WorldObjectType type;
    Vector3 pos;
    bool hasPos;
    float health;
    float maxHealth;
    float speedKmh;
    bool hasVelocity;
    uint32_t team;
    std::vector<std::string> occupantNames;
};

struct Settings {
    bool menuOpen = false;
    bool shouldQuit = false;

    bool espEnabled = true;
    bool espBox = true;
    bool esp3DBox = true;
    bool espSkeleton = true;
    float espSkeletonThickness = 1.5f;
    bool espSnaplines = false;
    bool espNames = true;
    bool espDistance = true;
    bool espCrosshair = true;
    bool espFOVRadius = false;
    float espAimbotFov = 100.0f;
    float espMaxDistance = 1000.0f;
    bool espOffscreenArrows = true;
    float offscreenRadius = 260.0f;
    bool espHealthBar = true;
    bool espVehicles = true;
    int espHealthOffset = 0x140;

    bool espWorldVehicles = true;
    bool espWorldSupplies = true;
    bool espWorldControlPoints = true;
    float espWorldMaxDist = 1500.0f;
    bool espShowKitTag = true;

    bool espHeadPoint = true;
    float espHeadRadius = 4.0f;
    bool radarViewCones = true;
    bool radarShowVehicles = true;
    bool radarShowSupplies = true;
    bool radarShowStances = true;
    bool espShowDownedState = true;
    bool espFilterDowned = false;

    bool ammoHudEnabled = true;
    float ammoHudPosX = 0.0f;
    float ammoHudPosY = 0.0f;
    int localCurAmmo = 0;
    int localTotalAmmo = 0;
    bool localHasAmmoData = false;

    bool antiRecoilEnabled = false;
    float antiRecoilStrength = 2.5f;
    float antiRecoilHorizontal = 0.0f;
    float antiRecoilDelay = 15.0f;
    bool antiRecoilOnlyWhenScoped = false;
    bool localPlayerAlive = false;

    bool aimAssistEnabled = false;
    float aimAssistSmooth = 8.0f;
    int aimAssistKey = VK_RBUTTON;
    int aimAssistTargetBone = 0;
    bool aimAssistSnapline = true;
    bool aimAssistFilterDowned = true;

    float espBaseOffsetY = -0.90f;
    float espBoxHeight = 1.80f;
    float espWidthScale = 0.40f;

    bool radarEnabled = true;
    float radarRange = 300.0f;
    float radarSize = 250.0f;
    bool showBots = true;
    bool showTeammates = false;

    bool aimWarningEnabled = true;
    float aimWarningMaxDist = 350.0f;
    float aimWarningAngleDeg = 18.0f;

    bool espHazards = true;
    float espHazardsMaxDist = 300.0f;

    float colEnemy[4] = { 1.0f, 0.2f, 0.2f, 0.9f };
    float colAlly[4]  = { 0.3f, 0.65f, 1.0f, 0.9f };
    float colDowned[4] = { 1.0f, 0.65f, 0.15f, 0.9f };
    float colSkel[4]   = { 0.2f, 1.0f, 0.5f, 0.9f };
    float colHazard[4] = { 1.0f, 0.15f, 0.15f, 1.0f };
    float colWarn[4]   = { 1.0f, 0.1f, 0.1f, 1.0f };

    void SaveConfig(const char* filename = "toolkit_config.ini");
    void LoadConfig(const char* filename = "toolkit_config.ini");

    bool freezeTeam1 = false;
    uint32_t freezeTeam1Value = 9999;
    bool freezeTeam2 = false;
    uint32_t freezeTeam2Value = 0;
};

extern Settings g_Settings;
extern std::vector<PlayerData> g_Players;
extern std::vector<WorldObjectData> g_WorldObjects;
extern std::mutex g_DataMutex;

extern std::atomic<bool> g_Running;
extern std::atomic<bool> g_HasAimTarget;
extern std::atomic<float> g_AimTargetDeltaX;
extern std::atomic<float> g_AimTargetDeltaY;
extern Vector2 g_AimTarget2D;

extern float g_ViewProj[16];
extern bool g_CameraValid;
extern bool g_DebugMatrixOnScreen;
extern float g_CamFov;
extern float g_CamAspect;
extern float g_CamNear;
extern float g_CamFar;
extern Vector3 g_CamEye;
extern Vector3 g_CamRight;
extern Vector3 g_CamForward;

extern HWND g_hGameWnd;
