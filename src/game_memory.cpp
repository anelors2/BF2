#include "game_memory.h"
#include "prbf2_offsets.h"
#include "prbf2.h"
#include <cmath>
#include <cstring>
#include <vector>

void GameMemory::UpdateCamera(const Memory& mem) {
    if (!mem.processHandle) {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        g_CameraValid = false;
        return;
    }

    static uintptr_t s_RendBase = 0;
    if (!s_RendBase) {
        s_RendBase = mem.rendDX9Base;
        if (!s_RendBase) {
            s_RendBase = const_cast<Memory&>(mem).GetModuleBase(L"RendDX9.dll");
            const_cast<Memory&>(mem).rendDX9Base = s_RendBase;
        }
    }

    if (!s_RendBase) {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        g_CameraValid = false;
        return;
    }

    prbf2::ExternalMem extMem{ mem.processHandle };
    prbf2::Walker<prbf2::ExternalMem> w(extMem);

    prbf2::addr_t cam = w.camera(static_cast<prbf2::addr_t>(s_RendBase));
    if (!cam) {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        g_CameraValid = false;
        return;
    }

    float m[16] = {};
    if (extMem.read(cam + prbf2::rend::camera::kViewProj, m)) {
        float eye[3] = {};
        extMem.read(cam + prbf2::rend::camera::kEye, eye);

        float camParams[4] = {};
        extMem.readRaw(cam + prbf2::rend::camera::kFov, camParams, sizeof(camParams));

        std::lock_guard<std::mutex> lock(g_DataMutex);
        memcpy(g_ViewProj, m, sizeof(m));
        g_CamEye = { eye[0], eye[1], eye[2] };
        g_CamFov = camParams[0];
        g_CamNear = camParams[1];
        g_CamFar = camParams[2];
        g_CamAspect = camParams[3];

        g_CamRight = { m[0], m[4], m[8] };
        g_CamForward = { m[2], m[6], m[10] };

        g_CameraValid = true;
    } else {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        g_CameraValid = false;
    }
}

bool GameMemory::WorldToScreen(const Vector3& world, Vector2& screen, int screenWidth, int screenHeight) {
    if (!g_CameraValid || screenWidth <= 0 || screenHeight <= 0) return false;

    const float* m = g_ViewProj;
    float cw = world.x * m[3] + world.y * m[7] + world.z * m[11] + m[15];
    if (cw < 0.001f) return false;

    float cx = world.x * m[0] + world.y * m[4] + world.z * m[8]  + m[12];
    float cy = world.x * m[1] + world.y * m[5] + world.z * m[9]  + m[13];

    float invW = 1.0f / cw;
    float ndcX = cx * invW;
    float ndcY = cy * invW;

    screen.x = ((ndcX + 1.0f) * 0.5f * (float)screenWidth);
    screen.y = ((1.0f - ndcY) * 0.5f * (float)screenHeight);
    return true;
}

void GameMemory::ProcessPlayerNode(const Memory& mem, uintptr_t playerPtr) {
}

void GameMemory::ReadPlayers(const Memory& mem) {
    if (!mem.processHandle) {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        g_Players.clear();
        return;
    }

    prbf2::ExternalMem extMem{ mem.processHandle };
    prbf2::Walker<prbf2::ExternalMem> w(extMem);

    prbf2::addr_t localPl = w.localPlayer();
    std::vector<prbf2::addr_t> playerList = w.players(256);

    if (localPl) {
        bool found = false;
        for (auto p : playerList) {
            if (p == localPl) {
                found = true;
                break;
            }
        }
        if (!found) {
            playerList.push_back(localPl);
        }
    }

    std::vector<PlayerData> playersList;
    playersList.reserve(playerList.size());

    Vector3 camEyeLocal;
    {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        camEyeLocal = g_CamEye;
    }

    bool localAlive = false;
    int localCurAmmo = 0;
    int localTotalAmmo = 0;
    bool localHasAmmo = false;

    for (prbf2::addr_t plAddr : playerList) {
        if (!plAddr) continue;

        prbf2::PlayerInfo info = w.playerInfo(plAddr);
        prbf2::addr_t sol = w.soldierOf(plAddr);
        prbf2::addr_t ctl = w.controlledOf(plAddr);
        bool inVeh = w.inVehicle(plAddr);

        PlayerData pd = {};
        pd.address = plAddr;
        pd.isLocal = (localPl != 0 && plAddr == localPl);
        pd.name = info.name;
        pd.ai = info.ai ? 1 : 0;
        pd.alive = info.alive ? 1 : 0;
        pd.team = static_cast<uint32_t>(info.team);
        pd.isVehicle = inVeh;
        pd.health = info.hp;
        pd.maxHealth = info.maxHp > 0.0f ? info.maxHp : 100.0f;
        pd.isDowned = (info.manDown || (pd.alive && !pd.isVehicle && pd.health <= 0.0f));
        pd.hasPos = false;
        pd.yaw = 0.0f;

        pd.kitName = info.kit;
        pd.pose = sol ? w.pose(sol) : 0;

        prbf2::addr_t targetObj = inVeh ? (ctl ? ctl : sol) : sol;
        if (targetObj) {
            float p[3] = {};
            if (w.position(targetObj, p)) {
                pd.pos = { p[0], p[1], p[2] };
                pd.hasPos = true;
            }

            float yawDeg = 0.0f;
            if (w.yawDegrees(targetObj, yawDeg)) {
                pd.yaw = yawDeg * (3.14159265f / 180.0f);
            }

            if (inVeh) {
                pd.vehicleName = w.templateName(targetObj);
                if (pd.vehicleName.empty() && ctl) {
                    prbf2::addr_t root = w.rootOf(ctl);
                    if (root) pd.vehicleName = w.templateName(root);
                }

                float vHp = 0.0f, vMax = 0.0f;
                if (w.health(targetObj, vHp, vMax) && vMax > 0.0f) {
                    pd.health = vHp;
                    pd.maxHealth = vMax;
                }
            }

            bool gotHead = false;
            float distSq = (pd.pos.x - camEyeLocal.x) * (pd.pos.x - camEyeLocal.x) +
                           (pd.pos.y - camEyeLocal.y) * (pd.pos.y - camEyeLocal.y) +
                           (pd.pos.z - camEyeLocal.z) * (pd.pos.z - camEyeLocal.z);

            if (sol && pd.alive && !inVeh && (g_Settings.espSkeleton || g_Settings.espHeadPoint) && distSq < (150.0f * 150.0f)) {
                std::vector<prbf2::BoneInfo> bonesList;
                bool posed = false;
                if (w.bones(sol, bonesList, 1, &posed) && posed) {
                    if (g_Settings.espSkeleton) {
                        pd.hasSkeleton = true;
                        int count = static_cast<int>(bonesList.size());
                        for (int i = 0; i < count && i < 48; ++i) {
                            int parentIdx = bonesList[i].parent;
                            if (parentIdx >= 0 && parentIdx < count && parentIdx < 48) {
                                Vector3 pA = { bonesList[i].world[0], bonesList[i].world[1], bonesList[i].world[2] };
                                Vector3 pB = { bonesList[parentIdx].world[0], bonesList[parentIdx].world[1], bonesList[parentIdx].world[2] };
                                float dx = pA.x - pB.x;
                                float dy = pA.y - pB.y;
                                float dz = pA.z - pB.z;
                                float lenSq = dx * dx + dy * dy + dz * dz;
                                if (lenSq > 0.0001f && lenSq < 1.44f) {
                                    pd.skeletonSegments.push_back({ pA, pB });
                                }
                            }
                        }
                    }

                    int headIdx = prbf2::Walker<prbf2::ExternalMem>::findBone(bonesList, "head");
                    if (headIdx >= 0) {
                        pd.headPos = { bonesList[headIdx].world[0], bonesList[headIdx].world[1], bonesList[headIdx].world[2] };
                        gotHead = true;
                    }
                }
            }

            if (!gotHead && pd.hasPos) {
                int pose = pd.pose;
                float headOffset = 0.85f;
                if (pose == 1) headOffset = 0.45f;
                else if (pose == 2) headOffset = 0.15f;
                if (pd.isDowned) headOffset = 0.15f;
                pd.headPos = { pd.pos.x, pd.pos.y + headOffset, pd.pos.z };
            }
        }

        if (pd.isLocal || (localPl == 0 && pd.ai == 0 && !localHasAmmo)) {
            localAlive = (pd.alive != 0 && !pd.isDowned);
            if (pd.alive) {
                prbf2::addr_t wpn = w.activeWeapon(ctl ? ctl : sol, prbf2::kPrimaryWeaponSlot);
                prbf2::AmmoInfo ammo;
                if (w.ammoOf(wpn, ammo)) {
                    localCurAmmo = ammo.inMag;
                    int capacity = ammo.magSize > 0 ? ammo.magSize : 30;
                    localTotalAmmo = ammo.magsLeft * capacity;
                    localHasAmmo = true;
                }
            }
        }

        playersList.push_back(std::move(pd));
    }

    g_Settings.localPlayerAlive = localAlive;
    g_Settings.localCurAmmo = localCurAmmo;
    g_Settings.localTotalAmmo = localTotalAmmo;
    g_Settings.localHasAmmoData = localHasAmmo;

    {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        g_Players = std::move(playersList);
    }
}

#pragma pack(push, 1)
struct RawObjHead {
    prbf2::addr_t vtable;
    uint8_t pad[0x24];
    prbf2::addr_t parent;
};
#pragma pack(pop)

void GameMemory::ReadWorldObjects(const Memory& mem) {
    if (!mem.processHandle) {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        g_WorldObjects.clear();
        return;
    }

    prbf2::ExternalMem extMem{ mem.processHandle };
    prbf2::Walker<prbf2::ExternalMem> w(extMem);

    std::vector<prbf2::addr_t> objs = w.objects(5000);
    std::vector<prbf2::addr_t> plist = w.players(128);

    std::vector<WorldObjectData> worldObjs;
    worldObjs.reserve(64);

    for (prbf2::addr_t obj : objs) {
        if (!obj) continue;

        RawObjHead head = {};
        if (!extMem.readRaw(obj, &head, sizeof(head))) continue;
        if (head.parent != 0) continue;

        WorldObjectType objType;
        if (head.vtable == prbf2::object_class::kPlayerControlObject) {
            objType = WorldObjectType::Vehicle;
        } else if (head.vtable == prbf2::object_class::kSupplyDepot || head.vtable == prbf2::object_class::kSupplyObject) {
            objType = WorldObjectType::SupplyCrate;
        } else if (head.vtable == prbf2::object_class::kControlPoint) {
            objType = WorldObjectType::ControlPoint;
        } else if (head.vtable == prbf2::object_class::kGenericProjectile || head.vtable == prbf2::object_class::kItem) {
            std::string tName = w.templateName(obj);
            if (tName.empty()) continue;
            std::string lowerName = tName;
            for (auto& c : lowerName) c = (char)tolower((unsigned char)c);

            bool isHazard = (lowerName.find("mine") != std::string::npos ||
                             lowerName.find("ied") != std::string::npos ||
                             lowerName.find("c4") != std::string::npos ||
                             lowerName.find("claymore") != std::string::npos ||
                             lowerName.find("tripwire") != std::string::npos ||
                             lowerName.find("at_mine") != std::string::npos ||
                             lowerName.find("ap_mine") != std::string::npos);
            if (!isHazard) continue;
            objType = WorldObjectType::Hazard;
        } else {
            continue;
        }

        WorldObjectData wod = {};
        wod.address = obj;
        wod.type = objType;
        wod.name = w.templateName(obj);
        wod.hasPos = false;
        wod.hasVelocity = false;
        wod.speedKmh = 0.0f;
        wod.health = 0.0f;
        wod.maxHealth = 0.0f;
        wod.team = 0;

        float p[3] = {};
        if (w.position(obj, p)) {
            wod.pos = { p[0], p[1], p[2] };
            wod.hasPos = true;
        }

        if (!wod.hasPos) continue;

        if (objType == WorldObjectType::Vehicle) {
            w.health(obj, wod.health, wod.maxHealth);
            float vel[3] = {};
            if (w.velocity(obj, vel)) {
                wod.hasVelocity = true;
                float spdMs = std::sqrt(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);
                wod.speedKmh = spdMs * 3.6f;
            }

            bool likelyVeh = (wod.maxHealth > 0.0f && wod.hasVelocity);
            if (!likelyVeh && wod.name.find("deployable") == std::string::npos &&
                wod.name.find("supply") == std::string::npos) {
                bool hasOcc = false;
                for (prbf2::addr_t pl : plist) {
                    prbf2::addr_t ctl = w.controlledOf(pl);
                    if (ctl && ctl != w.soldierOf(pl) && w.topParent(ctl) == obj) {
                        hasOcc = true;
                        break;
                    }
                }
                if (!hasOcc) continue;
            }

            for (prbf2::addr_t pl : plist) {
                prbf2::addr_t ctl = w.controlledOf(pl);
                if (ctl && ctl != w.soldierOf(pl) && w.topParent(ctl) == obj) {
                    wod.occupantNames.push_back(w.playerInfo(pl).name);
                    if (wod.team == 0) wod.team = static_cast<uint32_t>(w.team(pl));
                }
            }
        }

        worldObjs.push_back(std::move(wod));
    }

    {
        std::lock_guard<std::mutex> lock(g_DataMutex);
        g_WorldObjects = std::move(worldObjs);
    }
}

DWORD WINAPI MemoryReaderWorker(LPVOID lpParam) {
    PRBF2Interface* pTickets = reinterpret_cast<PRBF2Interface*>(lpParam);
    if (!pTickets) return 0;
    Memory& mem = pTickets->mem;

    DWORD lastWorldObjTick = 0;

    while (g_Running.load()) {
        if (pTickets->TryConnect()) {
            pTickets->Update();
            GameMemory::UpdateCamera(mem);
            GameMemory::ReadPlayers(mem);

            DWORD now = GetTickCount();
            if (now - lastWorldObjTick > 1500) {
                lastWorldObjTick = now;
                GameMemory::ReadWorldObjects(mem);
            }
        } else {
            {
                std::lock_guard<std::mutex> lock(g_DataMutex);
                g_CameraValid = false;
                g_Players.clear();
                g_WorldObjects.clear();
            }
            Sleep(250);
            continue;
        }

        Sleep(12);
    }
    return 0;
}

