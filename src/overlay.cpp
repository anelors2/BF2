#include "overlay.h"
#include "game_memory.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

inline ImU32 ToImU32(const float c[4]) {
    return IM_COL32((int)(c[0] * 255.0f), (int)(c[1] * 255.0f), (int)(c[2] * 255.0f), (int)(c[3] * 255.0f));
}

void DrawOffscreenArrow(float screenCenterX, float screenCenterY, float angleRad, float radius, ImU32 color, ImDrawList* draw) {
    float cosA = cosf(angleRad);
    float sinA = sinf(angleRad);

    ImVec2 tip = ImVec2(screenCenterX + radius * cosA, screenCenterY + radius * sinA);
    float arrowSize = 14.0f;
    float arrowWidth = 8.0f;

    float backX = tip.x - arrowSize * cosA;
    float backY = tip.y - arrowSize * sinA;

    float perpX = -sinA * arrowWidth;
    float perpY =  cosA * arrowWidth;

    ImVec2 leftCorner  = ImVec2(backX + perpX, backY + perpY);
    ImVec2 rightCorner = ImVec2(backX - perpX, backY - perpY);

    draw->AddTriangleFilled(tip, leftCorner, rightCorner, color);
    draw->AddTriangle(tip, leftCorner, rightCorner, IM_COL32(0, 0, 0, 200), 1.2f);
}

void Draw3DBox(const Vector3& origin, float width, float depth, float height, float yOffset, ImDrawList* draw, ImU32 color, int screenWidth, int screenHeight) {
    float hw = width * 0.5f;
    float hd = depth * 0.5f;
    float yBottom = origin.y + yOffset;
    float yTop = yBottom + height;

    Vector3 corners[8] = {
        { origin.x - hw, yBottom, origin.z - hd },
        { origin.x + hw, yBottom, origin.z - hd },
        { origin.x + hw, yBottom, origin.z + hd },
        { origin.x - hw, yBottom, origin.z + hd },
        { origin.x - hw, yTop,    origin.z - hd },
        { origin.x + hw, yTop,    origin.z - hd },
        { origin.x + hw, yTop,    origin.z + hd },
        { origin.x - hw, yTop,    origin.z + hd }
    };

    Vector2 s[8];
    for (int i = 0; i < 8; i++) {
        if (!GameMemory::WorldToScreen(corners[i], s[i], screenWidth, screenHeight)) return;
    }

    draw->AddLine(ImVec2(s[0].x, s[0].y), ImVec2(s[1].x, s[1].y), color, 1.5f);
    draw->AddLine(ImVec2(s[1].x, s[1].y), ImVec2(s[2].x, s[2].y), color, 1.5f);
    draw->AddLine(ImVec2(s[2].x, s[2].y), ImVec2(s[3].x, s[3].y), color, 1.5f);
    draw->AddLine(ImVec2(s[3].x, s[3].y), ImVec2(s[0].x, s[0].y), color, 1.5f);

    draw->AddLine(ImVec2(s[4].x, s[4].y), ImVec2(s[5].x, s[5].y), color, 1.5f);
    draw->AddLine(ImVec2(s[5].x, s[5].y), ImVec2(s[6].x, s[6].y), color, 1.5f);
    draw->AddLine(ImVec2(s[6].x, s[6].y), ImVec2(s[7].x, s[7].y), color, 1.5f);
    draw->AddLine(ImVec2(s[7].x, s[7].y), ImVec2(s[4].x, s[4].y), color, 1.5f);

    draw->AddLine(ImVec2(s[0].x, s[0].y), ImVec2(s[4].x, s[4].y), color, 1.5f);
    draw->AddLine(ImVec2(s[1].x, s[1].y), ImVec2(s[5].x, s[5].y), color, 1.5f);
    draw->AddLine(ImVec2(s[2].x, s[2].y), ImVec2(s[6].x, s[6].y), color, 1.5f);
    draw->AddLine(ImVec2(s[3].x, s[3].y), ImVec2(s[7].x, s[7].y), color, 1.5f);
}

static std::vector<PlayerData> s_FramePlayers;
static std::vector<WorldObjectData> s_FrameWorldObjects;
static PlayerData s_LocalPlayer;
static bool s_HasLocalPlayer = false;

void UpdateFrameDataSnapshot() {
    std::lock_guard<std::mutex> lock(g_DataMutex);
    s_FramePlayers = g_Players;
    s_FrameWorldObjects = g_WorldObjects;

    s_HasLocalPlayer = false;
    for (const auto& pl : s_FramePlayers) {
        if (pl.isLocal) {
            s_LocalPlayer = pl;
            s_HasLocalPlayer = true;
            break;
        }
    }
    if (!s_HasLocalPlayer) {
        for (const auto& pl : s_FramePlayers) {
            if (pl.ai == 0 && pl.hasPos) {
                s_LocalPlayer = pl;
                s_HasLocalPlayer = true;
                break;
            }
        }
    }
}

static const PlayerData* GetLocalPlayer() {
    return s_HasLocalPlayer ? &s_LocalPlayer : nullptr;
}

void RenderVisualESP(int screenWidth, int screenHeight) {
    if (!g_Settings.espEnabled) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    const PlayerData* local = GetLocalPlayer();

    float screenBottomX = (float)screenWidth * 0.5f;
    float screenBottomY = (float)screenHeight;

    for (const auto& pl : s_FramePlayers) {
        if (!pl.hasPos || pl.alive == 0) continue;
        if (pl.isLocal || (local && &pl == local)) continue;
        if (!g_Settings.showBots && pl.ai == 1) continue;

        bool isTeam = (local && pl.team == local->team);
        if (!g_Settings.showTeammates && isTeam) continue;

        ImU32 boxCol = isTeam ? ToImU32(g_Settings.colAlly) : ToImU32(g_Settings.colEnemy);
        ImU32 snapCol = isTeam ? IM_COL32((int)(g_Settings.colAlly[0]*255), (int)(g_Settings.colAlly[1]*255), (int)(g_Settings.colAlly[2]*255), 160)
                               : IM_COL32((int)(g_Settings.colEnemy[0]*255), (int)(g_Settings.colEnemy[1]*255), (int)(g_Settings.colEnemy[2]*255), 160);

        float dist = 0.0f;
        if (local) {
            float dx = pl.pos.x - local->pos.x;
            float dy = pl.pos.y - local->pos.y;
            float dz = pl.pos.z - local->pos.z;
            dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist > g_Settings.espMaxDistance) continue;
        }

        if (g_Settings.espFilterDowned && pl.isDowned) continue;

        if (pl.isDowned) {
            boxCol = ToImU32(g_Settings.colDowned);
            snapCol = IM_COL32((int)(g_Settings.colDowned[0]*255), (int)(g_Settings.colDowned[1]*255), (int)(g_Settings.colDowned[2]*255), 160);
        }

        Vector2 feet2D, head2D;
        Vector3 feetPos = { pl.pos.x, pl.pos.y + g_Settings.espBaseOffsetY, pl.pos.z };
        Vector3 headPos = { pl.pos.x, feetPos.y + g_Settings.espBoxHeight, pl.pos.z };

        bool feetOk = GameMemory::WorldToScreen(feetPos, feet2D, screenWidth, screenHeight);
        bool headOk = GameMemory::WorldToScreen(headPos, head2D, screenWidth, screenHeight);

        if (g_Settings.espHeadPoint && pl.hasPos) {
            Vector2 headDot2D;
            if (GameMemory::WorldToScreen(pl.headPos, headDot2D, screenWidth, screenHeight)) {
                ImU32 dotCol = pl.isDowned ? IM_COL32(255, 180, 50, 255) : (isTeam ? IM_COL32(80, 200, 255, 255) : IM_COL32(255, 230, 80, 255));
                draw->AddCircleFilled(ImVec2(headDot2D.x, headDot2D.y), g_Settings.espHeadRadius, dotCol, 12);
                draw->AddCircle(ImVec2(headDot2D.x, headDot2D.y), g_Settings.espHeadRadius + 0.5f, IM_COL32(0, 0, 0, 220), 12, 1.2f);
            }
        }

        if (g_Settings.espOffscreenArrows && (!feetOk || !headOk)) {
            float dx = pl.pos.x - g_CamEye.x;
            float dy = pl.pos.y - g_CamEye.y;
            float dz = pl.pos.z - g_CamEye.z;

            float localX = dx * g_CamRight.x + dy * g_CamRight.y + dz * g_CamRight.z;
            float localForward = dx * g_CamForward.x + dy * g_CamForward.y + dz * g_CamForward.z;
            float screenAngle = atan2f(-localForward, localX);

            float scX = (float)screenWidth * 0.5f;
            float scY = (float)screenHeight * 0.5f;
            DrawOffscreenArrow(scX, scY, screenAngle, g_Settings.offscreenRadius, boxCol, draw);
        }

        if (g_Settings.esp3DBox) {
            float boxDepth = g_Settings.espBoxHeight * g_Settings.espWidthScale;
            float boxWidth = g_Settings.espBoxHeight * g_Settings.espWidthScale;
            Draw3DBox(pl.pos, boxWidth, boxDepth, g_Settings.espBoxHeight, g_Settings.espBaseOffsetY, draw, boxCol, screenWidth, screenHeight);
        }

        if (g_Settings.espSkeleton && pl.hasSkeleton && !pl.isVehicle && pl.alive) {
            ImU32 skelCol = pl.isDowned ? ToImU32(g_Settings.colDowned) : (isTeam ? ToImU32(g_Settings.colAlly) : ToImU32(g_Settings.colSkel));
            ImU32 jointCol = IM_COL32(255, 255, 255, 230);

            for (const auto& seg : pl.skeletonSegments) {
                Vector2 sA, sB;
                if (GameMemory::WorldToScreen(seg.first, sA, screenWidth, screenHeight) &&
                    GameMemory::WorldToScreen(seg.second, sB, screenWidth, screenHeight)) {
                    if (sA.x >= -60.0f && sA.x <= (float)screenWidth + 60.0f &&
                        sA.y >= -60.0f && sA.y <= (float)screenHeight + 60.0f &&
                        sB.x >= -60.0f && sB.x <= (float)screenWidth + 60.0f &&
                        sB.y >= -60.0f && sB.y <= (float)screenHeight + 60.0f) {
                        draw->AddLine(ImVec2(sA.x, sA.y), ImVec2(sB.x, sB.y), skelCol, g_Settings.espSkeletonThickness);
                        draw->AddCircleFilled(ImVec2(sA.x, sA.y), 1.8f, jointCol);
                        draw->AddCircleFilled(ImVec2(sB.x, sB.y), 1.8f, jointCol);
                    }
                }
            }
        }

        if (feetOk && headOk) {
            float top    = (std::min)(head2D.y, feet2D.y);
            float bottom = (std::max)(head2D.y, feet2D.y);
            float midX   = feet2D.x;

            float boxHeight = bottom - top;
            if (boxHeight < 2.0f) boxHeight = 2.0f;
            if (boxHeight > (float)screenHeight * 0.98f) continue;
            float boxWidth = boxHeight * g_Settings.espWidthScale;
            if (boxWidth < 2.0f) boxWidth = 2.0f;

            float x1 = midX - boxWidth * 0.5f;
            float x2 = midX + boxWidth * 0.5f;
            float y1 = top;
            float y2 = bottom;

            if (g_Settings.espBox && !g_Settings.esp3DBox) {
                draw->AddRect(ImVec2(x1 - 1, y1 - 1), ImVec2(x2 + 1, y2 + 1), IM_COL32(0, 0, 0, 180), 0.0f, 0, 1.0f);
                draw->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), boxCol, 0.0f, 0, 1.5f);
            }

            if (g_Settings.espSnaplines) {
                draw->AddLine(ImVec2(screenBottomX, screenBottomY), ImVec2(feet2D.x, feet2D.y), snapCol, 1.0f);
            }

            char tagBuf[128];
            const char* vehTag = pl.isVehicle ? "[VEHICLE] " : "";
            const char* stateTag = (g_Settings.espShowDownedState && pl.isDowned) ? "[DOWNED] " : "";
            char kitBuf[64] = "";
            if (g_Settings.espShowKitTag && !pl.kitName.empty() && !pl.isVehicle) {
                sprintf_s(kitBuf, "{%s} ", pl.kitName.c_str());
            }

            if (g_Settings.espNames && g_Settings.espDistance && local) {
                sprintf_s(tagBuf, "%s%s%s%s [%.0fm]", stateTag, vehTag, kitBuf, pl.name.c_str(), dist);
            } else if (g_Settings.espNames) {
                sprintf_s(tagBuf, "%s%s%s%s", stateTag, vehTag, kitBuf, pl.name.c_str());
            } else if (g_Settings.espDistance && local) {
                sprintf_s(tagBuf, "%s%s%s[%.0fm]", stateTag, vehTag, kitBuf, dist);
            } else if (pl.isDowned) {
                sprintf_s(tagBuf, "[DOWNED]");
            } else if (pl.isVehicle) {
                sprintf_s(tagBuf, "[VEHICLE]");
            } else {
                tagBuf[0] = '\0';
            }

            if (tagBuf[0] != '\0') {
                ImVec2 txtSz = ImGui::CalcTextSize(tagBuf);
                draw->AddText(ImVec2(midX - txtSz.x * 0.5f, y1 - txtSz.y - 2.0f), IM_COL32(255, 255, 255, 220), tagBuf);
            }

            if (g_Settings.espHealthBar) {
                float barWidth = 3.5f;
                float barX1 = x1 - barWidth - 4.0f;
                float barX2 = x1 - 4.0f;
                float barYTop = (y1 < y2) ? y1 : y2;
                float barYBottom = (y1 < y2) ? y2 : y1;
                float totalH = barYBottom - barYTop;

                if (totalH > 4.0f) {
                    draw->AddRectFilled(ImVec2(barX1 - 1, barYTop - 1), ImVec2(barX2 + 1, barYBottom + 1), IM_COL32(10, 10, 10, 220));

                    float hpFraction = pl.health / (pl.maxHealth > 0.0f ? pl.maxHealth : 100.0f);
                    if (hpFraction > 1.0f) hpFraction = 1.0f;
                    if (hpFraction < 0.0f) hpFraction = 0.0f;

                    float fillH = totalH * hpFraction;
                    if (fillH < 2.0f && pl.health > 0.0f) fillH = 2.0f;
                    float fillY1 = barYBottom - fillH;

                    ImU32 hpColor;
                    if (hpFraction > 0.50f) {
                        hpColor = IM_COL32(40, 255, 70, 255);
                    } else if (hpFraction > 0.25f) {
                        hpColor = IM_COL32(255, 210, 30, 255);
                    } else {
                        hpColor = IM_COL32(255, 40, 40, 255);
                    }

                    draw->AddRectFilled(ImVec2(barX1, fillY1), ImVec2(barX2, barYBottom), hpColor);
                    draw->AddRect(ImVec2(barX1 - 1, barYTop - 1), ImVec2(barX2 + 1, barYBottom + 1), IM_COL32(0, 0, 0, 240), 0.0f, 0, 1.0f);
                }
            }
        }
    }

    if (g_Settings.espEnabled && (g_Settings.espWorldVehicles || g_Settings.espWorldSupplies || g_Settings.espWorldControlPoints || g_Settings.espHazards)) {
        for (const auto& wo : s_FrameWorldObjects) {
            if (!wo.hasPos) continue;

            float dist = 0.0f;
            if (local) {
                float dx = wo.pos.x - local->pos.x;
                float dy = wo.pos.y - local->pos.y;
                float dz = wo.pos.z - local->pos.z;
                dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist > g_Settings.espWorldMaxDist) continue;
            }

            Vector2 screenPt;
            if (!GameMemory::WorldToScreen(wo.pos, screenPt, screenWidth, screenHeight)) continue;

            if (wo.type == WorldObjectType::Vehicle && g_Settings.espWorldVehicles) {
                bool isOccupied = !wo.occupantNames.empty();
                ImU32 vCol = isOccupied ? (wo.team == (local ? local->team : 1) ? IM_COL32(60, 160, 255, 240) : IM_COL32(255, 60, 60, 240)) : IM_COL32(255, 215, 0, 230);

                char vBuf[128];
                if (wo.speedKmh > 2.0f) {
                    sprintf_s(vBuf, "[VEH] %s (%.0f km/h) [%.0fm]", wo.name.c_str(), wo.speedKmh, dist);
                } else if (dist > 0.0f) {
                    sprintf_s(vBuf, "[VEH] %s [%.0fm]", wo.name.c_str(), dist);
                } else {
                    sprintf_s(vBuf, "[VEH] %s", wo.name.c_str());
                }

                ImVec2 tSz = ImGui::CalcTextSize(vBuf);
                draw->AddRectFilled(ImVec2(screenPt.x - 4.0f, screenPt.y - 4.0f), ImVec2(screenPt.x + 4.0f, screenPt.y + 4.0f), vCol);
                draw->AddRect(ImVec2(screenPt.x - 4.0f, screenPt.y - 4.0f), ImVec2(screenPt.x + 4.0f, screenPt.y + 4.0f), IM_COL32(0, 0, 0, 220), 0.0f, 0, 1.0f);
                draw->AddText(ImVec2(screenPt.x - tSz.x * 0.5f, screenPt.y - tSz.y - 6.0f), vCol, vBuf);

                if (wo.maxHealth > 0.0f) {
                    char hpBuf[32];
                    sprintf_s(hpBuf, "HP: %.0f/%.0f", wo.health, wo.maxHealth);
                    ImVec2 hpSz = ImGui::CalcTextSize(hpBuf);
                    draw->AddText(ImVec2(screenPt.x - hpSz.x * 0.5f, screenPt.y + 6.0f), IM_COL32(230, 230, 230, 210), hpBuf);
                }
            } else if (wo.type == WorldObjectType::SupplyCrate && g_Settings.espWorldSupplies) {
                ImU32 sCol = IM_COL32(80, 240, 120, 240);
                char sBuf[64];
                sprintf_s(sBuf, "[CRATE] %s [%.0fm]", wo.name.c_str(), dist);
                ImVec2 tSz = ImGui::CalcTextSize(sBuf);
                draw->AddCircleFilled(ImVec2(screenPt.x, screenPt.y), 4.0f, sCol);
                draw->AddCircle(ImVec2(screenPt.x, screenPt.y), 4.5f, IM_COL32(0, 0, 0, 220), 12, 1.0f);
                draw->AddText(ImVec2(screenPt.x - tSz.x * 0.5f, screenPt.y - tSz.y - 6.0f), sCol, sBuf);
            } else if (wo.type == WorldObjectType::ControlPoint && g_Settings.espWorldControlPoints) {
                ImU32 cpCol = IM_COL32(200, 100, 255, 240);
                char cpBuf[64];
                sprintf_s(cpBuf, "[FLAG] %s [%.0fm]", wo.name.c_str(), dist);
                ImVec2 tSz = ImGui::CalcTextSize(cpBuf);
                draw->AddText(ImVec2(screenPt.x - tSz.x * 0.5f, screenPt.y - tSz.y - 5.0f), cpCol, cpBuf);
            } else if (wo.type == WorldObjectType::Hazard && g_Settings.espHazards) {
                if (dist <= g_Settings.espHazardsMaxDist) {
                    ImU32 hazCol = ToImU32(g_Settings.colHazard);
                    char hazBuf[64];
                    sprintf_s(hazBuf, "[!] %s [%.0fm]", wo.name.c_str(), dist);
                    ImVec2 tSz = ImGui::CalcTextSize(hazBuf);

                    float sz = 6.0f;
                    ImVec2 pts[3] = {
                        ImVec2(screenPt.x, screenPt.y - sz),
                        ImVec2(screenPt.x - sz, screenPt.y + sz),
                        ImVec2(screenPt.x + sz, screenPt.y + sz)
                    };
                    draw->AddPolyline(pts, 3, hazCol, ImDrawFlags_Closed, 1.8f);
                    draw->AddText(ImVec2(screenPt.x - tSz.x * 0.5f, screenPt.y + sz + 2.0f), hazCol, hazBuf);
                }
            }
        }
    }

    if (g_Settings.aimWarningEnabled && local && local->alive) {
        int enemiesAimedAtMe = 0;
        float closestDist = 9999.0f;

        for (const auto& pl : s_FramePlayers) {
            if (!pl.hasPos || pl.alive == 0 || pl.isDowned || pl.isLocal || &pl == local) continue;
            if (pl.team == local->team) continue;

            float toMeX = local->pos.x - pl.pos.x;
            float toMeZ = local->pos.z - pl.pos.z;
            float dist2D = std::sqrt(toMeX * toMeX + toMeZ * toMeZ);
            if (dist2D > g_Settings.aimWarningMaxDist || dist2D < 1.0f) continue;

            toMeX /= dist2D;
            toMeZ /= dist2D;

            float enemyFwdX = sinf(pl.yaw);
            float enemyFwdZ = cosf(pl.yaw);

            float dot = enemyFwdX * toMeX + enemyFwdZ * toMeZ;
            float thresholdDot = cosf(g_Settings.aimWarningAngleDeg * (3.14159265f / 180.0f));

            if (dot >= thresholdDot) {
                enemiesAimedAtMe++;
                if (dist2D < closestDist) closestDist = dist2D;
            }
        }

        if (enemiesAimedAtMe > 0) {
            char warnBuf[128];
            if (enemiesAimedAtMe == 1) {
                sprintf_s(warnBuf, "WARNING: ENEMY AIMING AT YOU! [%.0fm]", closestDist);
            } else {
                sprintf_s(warnBuf, "WARNING: %d ENEMIES AIMING AT YOU! [%.0fm]", enemiesAimedAtMe, closestDist);
            }

            ImVec2 txtSz = ImGui::CalcTextSize(warnBuf);
            float bannerW = txtSz.x + 36.0f;
            float bannerH = txtSz.y + 14.0f;
            float bx1 = ((float)screenWidth - bannerW) * 0.5f;
            float by1 = (float)screenHeight * 0.20f;
            float bx2 = bx1 + bannerW;
            float by2 = by1 + bannerH;

            ImU32 wCol = ToImU32(g_Settings.colWarn);
            draw->AddRectFilled(ImVec2(bx1, by1), ImVec2(bx2, by2), IM_COL32(20, 0, 0, 210), 6.0f);
            draw->AddRect(ImVec2(bx1, by1), ImVec2(bx2, by2), wCol, 6.0f, 0, 2.0f);
            draw->AddText(ImVec2(bx1 + 18.0f, by1 + 7.0f), wCol, warnBuf);
        }
    }

    float bestFovDist = g_Settings.espAimbotFov;
    bool targetFound = false;
    Vector2 bestScreenPoint = { 0.0f, 0.0f };

    float screenCenterX = (float)screenWidth * 0.5f;
    float screenCenterY = (float)screenHeight * 0.5f;

    if (g_Settings.aimAssistEnabled && local && local->alive) {
        for (const auto& pl : s_FramePlayers) {
            if (!pl.hasPos || pl.alive == 0) continue;
            if (pl.isLocal || &pl == local) continue;
            if (!g_Settings.showBots && pl.ai == 1) continue;
            if (pl.team == local->team) continue;
            if (g_Settings.aimAssistFilterDowned && pl.isDowned) continue;

            Vector3 aimTarget3D = pl.headPos;
            if (g_Settings.aimAssistTargetBone == 1) {
                aimTarget3D.y = pl.pos.y + g_Settings.espBaseOffsetY + (g_Settings.espBoxHeight * 0.65f);
            }

            Vector2 screenPoint;
            if (GameMemory::WorldToScreen(aimTarget3D, screenPoint, screenWidth, screenHeight)) {
                float dx = screenPoint.x - screenCenterX;
                float dy = screenPoint.y - screenCenterY;
                float distToCrosshair = std::sqrt(dx * dx + dy * dy);

                if (distToCrosshair <= bestFovDist) {
                    bestFovDist = distToCrosshair;
                    bestScreenPoint = screenPoint;
                    targetFound = true;
                }
            }
        }
    }

    if (targetFound) {
        g_HasAimTarget.store(true);
        g_AimTargetDeltaX.store(bestScreenPoint.x - screenCenterX);
        g_AimTargetDeltaY.store(bestScreenPoint.y - screenCenterY);
        g_AimTarget2D = bestScreenPoint;

        bool keyHeld = (GetAsyncKeyState(g_Settings.aimAssistKey) & 0x8000) != 0;
        ImU32 lockColor = keyHeld ? IM_COL32(255, 40, 40, 255) : IM_COL32(255, 200, 40, 220);

        draw->AddCircle(ImVec2(bestScreenPoint.x, bestScreenPoint.y), 6.5f, lockColor, 16, 1.5f);
        draw->AddCircle(ImVec2(bestScreenPoint.x, bestScreenPoint.y), 2.5f, IM_COL32(255, 255, 255, 255), 8, 1.0f);

        if (g_Settings.aimAssistSnapline) {
            ImU32 lineCol = keyHeld ? IM_COL32(255, 60, 60, 200) : IM_COL32(255, 220, 60, 130);
            draw->AddLine(ImVec2(screenCenterX, screenCenterY), ImVec2(bestScreenPoint.x, bestScreenPoint.y), lineCol, 1.2f);
        }
    } else {
        g_HasAimTarget.store(false);
        g_AimTargetDeltaX.store(0.0f);
        g_AimTargetDeltaY.store(0.0f);
    }

    if (g_DebugMatrixOnScreen && g_CameraValid) {
        char dbuf[256];
        float y = 60.0f;
        draw->AddRectFilled(ImVec2(4, y - 2), ImVec2(400, y + 90), IM_COL32(0, 0, 0, 175));
        draw->AddText(ImVec2(8, y), IM_COL32(100, 200, 255, 255), "--- BF2 Active Camera (+0xFC ViewProj) ---");
        sprintf_s(dbuf, "Eye:     %.1f, %.1f, %.1f", g_CamEye.x, g_CamEye.y, g_CamEye.z);
        draw->AddText(ImVec2(8, y + 16), IM_COL32(255, 255, 255, 230), dbuf);
        sprintf_s(dbuf, "FOV:     %.2f rad (%.1f deg)", g_CamFov, g_CamFov * 180.0f / 3.14159265f);
        draw->AddText(ImVec2(8, y + 32), IM_COL32(100, 255, 100, 230), dbuf);
        sprintf_s(dbuf, "Aspect:  %.3f | Near: %.2f | Far: %.1f", g_CamAspect, g_CamNear, g_CamFar);
        draw->AddText(ImVec2(8, y + 48), IM_COL32(255, 200, 50, 230), dbuf);
    }
}

void RenderRadar() {
    if (!g_Settings.radarEnabled) return;

    const PlayerData* local = GetLocalPlayer();

    ImGui::SetNextWindowSize(ImVec2(g_Settings.radarSize, g_Settings.radarSize + 30.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, 50), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Tactical 2D Radar", &g_Settings.radarEnabled, ImGuiWindowFlags_NoCollapse)) {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 sz = ImGui::GetContentRegionAvail();
        float r = (sz.x < sz.y ? sz.x : sz.y) * 0.5f;
        ImVec2 center = ImVec2(p0.x + r, p0.y + r);

        ImDrawList* draw = ImGui::GetWindowDrawList();

        draw->AddCircleFilled(center, r, IM_COL32(15, 18, 22, 220), 48);
        draw->AddCircle(center, r, IM_COL32(60, 75, 95, 255), 48, 1.5f);
        draw->AddCircle(center, r * 0.66f, IM_COL32(40, 50, 65, 150), 36, 1.0f);
        draw->AddCircle(center, r * 0.33f, IM_COL32(40, 50, 65, 150), 36, 1.0f);
        draw->AddLine(ImVec2(center.x - r, center.y), ImVec2(center.x + r, center.y), IM_COL32(40, 50, 65, 180));
        draw->AddLine(ImVec2(center.x, center.y - r), ImVec2(center.x, center.y + r), IM_COL32(40, 50, 65, 180));

        draw->AddCircleFilled(center, 4.0f, IM_COL32(0, 255, 100, 255));

        if (local) {
            for (auto& pl : s_FramePlayers) {
                if (pl.isLocal || &pl == local || !pl.hasPos || pl.alive == 0) continue;
                if (!g_Settings.showBots && pl.ai == 1) continue;

                bool isTeam = (pl.team == local->team);
                if (!g_Settings.showTeammates && isTeam) continue;

                if (g_Settings.espFilterDowned && pl.isDowned) continue;

                float dx = pl.pos.x - local->pos.x;
                float dz = pl.pos.z - local->pos.z;

                float px = (dx / g_Settings.radarRange) * r;
                float py = (dz / g_Settings.radarRange) * r;

                float dist = sqrtf(px * px + py * py);
                if (dist > r - 4.0f) {
                    px = (px / dist) * (r - 4.0f);
                    py = (py / dist) * (r - 4.0f);
                }

                ImVec2 blip = ImVec2(center.x + px, center.y - py);
                ImU32 col = pl.isDowned ? IM_COL32(255, 160, 40, 255) : (isTeam ? IM_COL32(60, 150, 255, 240) : IM_COL32(255, 50, 50, 255));

                if (g_Settings.radarShowStances && !pl.isDowned) {
                    if (pl.pose == 2) {
                        draw->AddLine(ImVec2(blip.x - 4, blip.y), ImVec2(blip.x + 4, blip.y), col, 2.5f);
                    } else if (pl.pose == 1) {
                        ImVec2 pts[4] = { ImVec2(blip.x, blip.y - 4), ImVec2(blip.x + 4, blip.y), ImVec2(blip.x, blip.y + 4), ImVec2(blip.x - 4, blip.y) };
                        draw->AddPolyline(pts, 4, col, ImDrawFlags_Closed, 1.5f);
                    } else {
                        draw->AddCircleFilled(blip, 3.5f, col);
                        draw->AddCircle(blip, 3.5f, IM_COL32(0, 0, 0, 180), 12, 1.0f);
                    }
                } else {
                    draw->AddCircleFilled(blip, 3.5f, col);
                    draw->AddCircle(blip, 3.5f, IM_COL32(0, 0, 0, 180), 12, 1.0f);
                }

                if (g_Settings.radarViewCones && !pl.isDowned) {
                    float lineLen = 9.0f;
                    float fwdX = sinf(pl.yaw);
                    float fwdZ = cosf(pl.yaw);
                    ImVec2 lineEnd = ImVec2(blip.x + fwdX * lineLen, blip.y - fwdZ * lineLen);
                    draw->AddLine(blip, lineEnd, col, 1.5f);

                    float coneLen = 6.0f;
                    float aLeft = pl.yaw - 0.40f;
                    float aRight = pl.yaw + 0.40f;
                    ImVec2 pLeft = ImVec2(blip.x + sinf(aLeft) * coneLen, blip.y - cosf(aLeft) * coneLen);
                    ImVec2 pRight = ImVec2(blip.x + sinf(aRight) * coneLen, blip.y - cosf(aRight) * coneLen);
                    draw->AddLine(blip, pLeft, IM_COL32(255, 255, 255, 120), 1.0f);
                    draw->AddLine(blip, pRight, IM_COL32(255, 255, 255, 120), 1.0f);
                }
            }

            for (const auto& wo : s_FrameWorldObjects) {
                if (!wo.hasPos) continue;

                float dx = wo.pos.x - local->pos.x;
                float dz = wo.pos.z - local->pos.z;

                float px = (dx / g_Settings.radarRange) * r;
                float py = (dz / g_Settings.radarRange) * r;

                float dist = sqrtf(px * px + py * py);
                if (dist > r - 4.0f) {
                    px = (px / dist) * (r - 4.0f);
                    py = (py / dist) * (r - 4.0f);
                }

                ImVec2 blip = ImVec2(center.x + px, center.y - py);

                if (wo.type == WorldObjectType::Vehicle && g_Settings.radarShowVehicles) {
                    bool occupied = !wo.occupantNames.empty();
                    ImU32 vCol = occupied ? (wo.team == local->team ? IM_COL32(60, 160, 255, 255) : IM_COL32(255, 60, 60, 255)) : IM_COL32(255, 215, 0, 240);
                    draw->AddRectFilled(ImVec2(blip.x - 3.5f, blip.y - 3.5f), ImVec2(blip.x + 3.5f, blip.y + 3.5f), vCol);
                    draw->AddRect(ImVec2(blip.x - 3.5f, blip.y - 3.5f), ImVec2(blip.x + 3.5f, blip.y + 3.5f), IM_COL32(0, 0, 0, 200), 0.0f, 0, 1.0f);
                } else if (wo.type == WorldObjectType::SupplyCrate && g_Settings.radarShowSupplies) {
                    draw->AddCircleFilled(blip, 3.0f, IM_COL32(80, 240, 120, 255));
                    draw->AddCircle(blip, 3.0f, IM_COL32(0, 0, 0, 200), 8, 1.0f);
                }
            }

            if (g_Settings.radarViewCones) {
                float localHeading = local->yaw;
                float lineLen = 12.0f;
                ImVec2 lineEnd = ImVec2(center.x + sinf(localHeading) * lineLen, center.y - cosf(localHeading) * lineLen);
                draw->AddLine(center, lineEnd, IM_COL32(0, 255, 120, 255), 2.0f);
            }

            draw->AddText(ImVec2(center.x - 4.0f, center.y - r + 3.0f), IM_COL32(255, 70, 70, 240), "N");
        } else {
            draw->AddText(ImVec2(p0.x + 10, p0.y + 10), IM_COL32(255, 200, 50, 200), "Waiting for local player...");
        }
    }
    ImGui::End();
}

void RenderAmmoHud(int screenWidth, int screenHeight) {
    if (!g_Settings.ammoHudEnabled) return;

    ImDrawList* bgDraw = ImGui::GetBackgroundDrawList();
    float hudW = 145.0f;
    float hudH = 50.0f;
    float defX = (float)screenWidth - hudW - 25.0f;
    float defY = (float)screenHeight - hudH - 30.0f;
    float hudX = (g_Settings.ammoHudPosX > 0.0f) ? g_Settings.ammoHudPosX : defX;
    float hudY = (g_Settings.ammoHudPosY > 0.0f) ? g_Settings.ammoHudPosY : defY;

    bgDraw->AddRectFilled(ImVec2(hudX, hudY), ImVec2(hudX + hudW, hudY + hudH), IM_COL32(12, 14, 18, 225), 6.0f);
    bgDraw->AddRect(ImVec2(hudX, hudY), ImVec2(hudX + hudW, hudY + hudH), IM_COL32(60, 75, 95, 255), 6.0f, 0, 1.2f);
    bgDraw->AddText(ImVec2(hudX + 8.0f, hudY + 4.0f), IM_COL32(140, 160, 185, 220), "WEAPON AMMO");

    if (g_Settings.localHasAmmoData) {
        char curBuf[32];
        char totBuf[32];
        sprintf_s(curBuf, "%d", g_Settings.localCurAmmo);
        sprintf_s(totBuf, "/ %d", g_Settings.localTotalAmmo);

        ImU32 curCol = (g_Settings.localCurAmmo <= 5) ? IM_COL32(255, 60, 60, 255) : IM_COL32(40, 255, 120, 255);
        bgDraw->AddText(ImVec2(hudX + 10.0f, hudY + 20.0f), curCol, curBuf);

        ImVec2 curSz = ImGui::CalcTextSize(curBuf);
        bgDraw->AddText(ImVec2(hudX + 14.0f + curSz.x, hudY + 22.0f), IM_COL32(200, 210, 225, 230), totBuf);
    } else {
        bgDraw->AddText(ImVec2(hudX + 10.0f, hudY + 22.0f), IM_COL32(255, 200, 50, 220), "DETECTING...");
    }
}

void RenderWatermark() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::Begin("PRBF2_Indicator", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground);
    if (g_Settings.menuOpen) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[PRBF2 External Overlay (DX11) - Press INSERT to Close Menu]");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 0.7f), "[PRBF2 External Overlay - Press INSERT to Open Menu]");
    }
    ImGui::End();
}

void ApplyModernTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 5.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 6.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;

    style.WindowPadding     = ImVec2(14.0f, 14.0f);
    style.FramePadding      = ImVec2(10.0f, 6.0f);
    style.ItemSpacing       = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    style.IndentSpacing     = 18.0f;
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 10.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.94f, 0.97f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.55f, 0.62f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.08f, 0.09f, 0.12f, 0.97f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.12f, 0.14f, 0.18f, 0.60f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.10f, 0.11f, 0.15f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.24f, 0.32f, 0.70f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.14f, 0.17f, 0.23f, 0.85f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.20f, 0.26f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.25f, 0.32f, 0.44f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.11f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.07f, 0.08f, 0.10f, 0.80f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.11f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.09f, 0.10f, 0.13f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.22f, 0.27f, 0.36f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.28f, 0.36f, 0.48f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.35f, 0.46f, 0.60f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.18f, 0.86f, 0.58f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.18f, 0.65f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.32f, 0.78f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.16f, 0.22f, 0.30f, 0.90f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.22f, 0.32f, 0.46f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.28f, 0.42f, 0.62f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.18f, 0.26f, 0.36f, 0.80f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.24f, 0.36f, 0.50f, 0.90f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.30f, 0.44f, 0.62f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.18f, 0.22f, 0.29f, 0.85f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.26f, 0.36f, 0.50f, 1.00f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.32f, 0.48f, 0.66f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.18f, 0.25f, 0.35f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.28f, 0.40f, 0.56f, 0.80f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.38f, 0.52f, 0.72f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.12f, 0.14f, 0.19f, 0.90f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.22f, 0.31f, 0.44f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.18f, 0.28f, 0.40f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.10f, 0.12f, 0.16f, 0.85f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.14f, 0.19f, 0.27f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.13f, 0.16f, 0.22f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.22f, 0.27f, 0.36f, 0.90f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.16f, 0.20f, 0.27f, 0.60f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
}

void RenderMenu() {
    if (!g_Settings.menuOpen) return;

    ImGui::SetNextWindowPos(ImVec2(240, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(820, 620), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(740, 520), ImVec2(1920, 1200));

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("PRBF2 External Toolkit - Professional Control", &g_Settings.menuOpen, winFlags)) {
        ImGui::End();
        return;
    }

    {
        ImGui::TextColored(ImVec4(0.18f, 0.80f, 1.0f, 1.0f), "PROJECT REALITY BF2");
        ImGui::SameLine();
        ImGui::TextDisabled("| EXTERNAL TOOLKIT v2.1");

        float rightPos = ImGui::GetWindowWidth() - 320.0f;
        if (rightPos > 240.0f) {
            ImGui::SameLine(rightPos);
        }

        if (g_hGameWnd && IsWindow(g_hGameWnd)) {
            ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.45f, 1.0f), "[ONLINE]");
        } else {
            ImGui::TextColored(ImVec4(1.00f, 0.65f, 0.15f, 1.0f), "[SEARCHING]");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%.0f FPS | %zu ents", ImGui::GetIO().Framerate, s_FramePlayers.size());
        ImGui::SameLine();
        if (ImGui::SmallButton(" X ")) {
            g_Settings.menuOpen = false;
        }
        ImGui::Separator();
    }

    if (ImGui::BeginTabBar("ToolkitTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Visuals (ESP)")) {
            float availX = ImGui::GetContentRegionAvail().x;
            float colW = (availX - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
            float childH = ImGui::GetContentRegionAvail().y - 4.0f;

            ImGui::BeginChild("ESP_PlayerCore", ImVec2(colW, childH), true);
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 1.00f, 1.0f), "PLAYER VISUALS");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Master ESP Enabled", &g_Settings.espEnabled);
            ImGui::Spacing();

            ImGui::Checkbox("2D Bounding Boxes", &g_Settings.espBox);
            ImGui::Checkbox("3D Bounding Boxes", &g_Settings.esp3DBox);

            ImGui::Checkbox("Skeleton ESP", &g_Settings.espSkeleton);
            if (g_Settings.espSkeleton) {
                ImGui::Indent(18.0f);
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##SkelThick", &g_Settings.espSkeletonThickness, 1.0f, 4.0f, "Thickness: %.1f px");
                ImGui::Unindent(18.0f);
            }

            ImGui::Checkbox("Tactical Head Point", &g_Settings.espHeadPoint);
            if (g_Settings.espHeadPoint) {
                ImGui::Indent(18.0f);
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##HRad", &g_Settings.espHeadRadius, 2.0f, 10.0f, "Head Radius: %.1f px");
                ImGui::Unindent(18.0f);
            }

            ImGui::Checkbox("Snaplines (Screen Bottom)", &g_Settings.espSnaplines);
            ImGui::Checkbox("Player Names", &g_Settings.espNames);
            ImGui::Checkbox("Distance Tag", &g_Settings.espDistance);
            ImGui::Checkbox("Dynamic Health Bar", &g_Settings.espHealthBar);
            ImGui::Checkbox("Vehicle Name Tags", &g_Settings.espVehicles);

            ImGui::Checkbox("Offscreen Radar Arrows", &g_Settings.espOffscreenArrows);
            if (g_Settings.espOffscreenArrows) {
                ImGui::Indent(18.0f);
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##OffRadius", &g_Settings.offscreenRadius, 100.0f, 600.0f, "Arrow Radius: %.0f px");
                ImGui::Unindent(18.0f);
            }

            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("ESP_FiltersCalib", ImVec2(colW, childH), true);
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 1.00f, 1.0f), "FILTERS & DISTANCE");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Show Teammates (Blue)", &g_Settings.showTeammates);
            ImGui::Checkbox("Show AI Bots", &g_Settings.showBots);
            ImGui::Checkbox("Show Kit Tag {Medic/LAT...}", &g_Settings.espShowKitTag);
            ImGui::Checkbox("Show [DOWNED] State Tag", &g_Settings.espShowDownedState);
            ImGui::Checkbox("Hide Downed Targets (Caídos)", &g_Settings.espFilterDowned);

            ImGui::Spacing();
            ImGui::TextDisabled("Max Player ESP Range:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##MaxEspDist", &g_Settings.espMaxDistance, 50.0f, 3000.0f, "%.0f meters");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.25f, 1.0f), "BOX TUNING & CALIBRATION");
            ImGui::Spacing();

            ImGui::TextDisabled("Feet Y Offset:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##FeetOffset", &g_Settings.espBaseOffsetY, -2.5f, 0.5f, "%.2f m");

            ImGui::TextDisabled("Box Total Height:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##BoxHeight", &g_Settings.espBoxHeight, 0.8f, 2.8f, "%.2f m");

            ImGui::TextDisabled("Box Width Scale:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##WidthScale", &g_Settings.espWidthScale, 0.2f, 0.8f, "%.2f");

            ImGui::Spacing();
            if (ImGui::Button("Reset Calibration Defaults", ImVec2(-1, 26))) {
                g_Settings.espBaseOffsetY = -0.90f;
                g_Settings.espBoxHeight   = 1.80f;
                g_Settings.espWidthScale  = 0.40f;
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Combat & Recoil")) {
            float availX = ImGui::GetContentRegionAvail().x;
            float colW = (availX - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

            float topH = 265.0f;

            ImGui::BeginChild("Combat_Recoil", ImVec2(colW, topH), true);
            ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.40f, 1.0f), "DYNAMIC ANTI-RECOIL");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Enable Anti-Recoil", &g_Settings.antiRecoilEnabled);
            ImGui::Checkbox("Active Only When Scoped (RMB)", &g_Settings.antiRecoilOnlyWhenScoped);

            ImGui::Spacing();
            ImGui::TextDisabled("Vertical Pull Strength:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##RecoilVert", &g_Settings.antiRecoilStrength, 0.5f, 12.0f, "%.1f px");

            ImGui::TextDisabled("Horizontal Drift Compensation:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##RecoilHoriz", &g_Settings.antiRecoilHorizontal, -4.0f, 4.0f, "%.1f px");

            ImGui::TextDisabled("Compensation Interval:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##RecoilDelay", &g_Settings.antiRecoilDelay, 5.0f, 40.0f, "%.0f ms");

            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("Combat_AimAssist", ImVec2(colW, topH), true);
            ImGui::TextColored(ImVec4(0.35f, 1.00f, 0.70f, 1.0f), "SMOOTH AIM ASSIST");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Enable Aim Assist", &g_Settings.aimAssistEnabled);
            ImGui::SameLine();
            ImGui::Checkbox("Show Snapline", &g_Settings.aimAssistSnapline);

            ImGui::Checkbox("Ignore Downed Targets", &g_Settings.aimAssistFilterDowned);

            const char* bones[] = { "Head Point", "Chest / Torso" };
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##Hitbox", &g_Settings.aimAssistTargetBone, bones, IM_ARRAYSIZE(bones));

            const char* keys[] = { "Right Mouse Button (RMB)", "Left Alt Key (ALT)", "Left Mouse Button (LMB)" };
            static int keyIdx = 0;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##AimKey", &keyIdx, keys, IM_ARRAYSIZE(keys))) {
                if (keyIdx == 0) g_Settings.aimAssistKey = VK_RBUTTON;
                else if (keyIdx == 1) g_Settings.aimAssistKey = VK_MENU;
                else if (keyIdx == 2) g_Settings.aimAssistKey = VK_LBUTTON;
            }

            ImGui::TextDisabled("Aim FOV Radius:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##AimFOV", &g_Settings.espAimbotFov, 20.0f, 500.0f, "%.0f px");

            ImGui::TextDisabled("Smooth Factor (Softness):");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##AimSmooth", &g_Settings.aimAssistSmooth, 2.0f, 25.0f, "%.1f");

            ImGui::EndChild();

            float bottomH = ImGui::GetContentRegionAvail().y - 4.0f;
            ImGui::BeginChild("Combat_HUDAlerts", ImVec2(availX, bottomH), true);
            ImGui::TextColored(ImVec4(1.00f, 0.80f, 0.20f, 1.0f), "TACTICAL HUD & AIM WARNING");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Columns(2, "HudColumns", false);

            ImGui::Checkbox("Screen Center Crosshair", &g_Settings.espCrosshair);
            ImGui::Checkbox("Weapon Ammo Counter HUD", &g_Settings.ammoHudEnabled);
            ImGui::Checkbox("Screen Center FOV Radius Circle", &g_Settings.espFOVRadius);

            ImGui::NextColumn();

            ImGui::Checkbox("Enable Aimed-At Warning Alert", &g_Settings.aimWarningEnabled);
            if (g_Settings.aimWarningEnabled) {
                ImGui::SetNextItemWidth(140.0f);
                ImGui::SliderFloat("Warning Angle##WarnAngle", &g_Settings.aimWarningAngleDeg, 5.0f, 45.0f, "%.0f deg");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(160.0f);
                ImGui::SliderFloat("Max Dist##WarnDist", &g_Settings.aimWarningMaxDist, 50.0f, 800.0f, "%.0f m");
            }

            ImGui::Columns(1);
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("World & Radar")) {
            float availX = ImGui::GetContentRegionAvail().x;
            float colW = (availX - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
            float childH = ImGui::GetContentRegionAvail().y - 4.0f;

            ImGui::BeginChild("World_RadarCard", ImVec2(colW, childH), true);
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 1.00f, 1.0f), "TACTICAL 2D RADAR");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Enable 2D Radar Window", &g_Settings.radarEnabled);
            ImGui::Spacing();

            ImGui::Checkbox("View Direction Cones", &g_Settings.radarViewCones);
            ImGui::Checkbox("Pose Shapes (Prone/Crouch/Stand)", &g_Settings.radarShowStances);
            ImGui::Checkbox("Show Vehicles on Radar", &g_Settings.radarShowVehicles);
            ImGui::Checkbox("Show Supplies on Radar", &g_Settings.radarShowSupplies);
            ImGui::Checkbox("Ocultar Caídos no Radar", &g_Settings.espFilterDowned);

            ImGui::Spacing();
            ImGui::TextDisabled("Radar Scan Range:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##RRange", &g_Settings.radarRange, 50.0f, 1500.0f, "%.0f meters");

            ImGui::TextDisabled("Radar Window Size:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##RSize", &g_Settings.radarSize, 180.0f, 450.0f, "%.0f px");

            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("World_ObjectsCard", ImVec2(colW, childH), true);
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 1.00f, 1.0f), "WORLD OBJECTS & DEPLOYABLES");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("World Vehicles (Empty & Occupied)", &g_Settings.espWorldVehicles);
            ImGui::Checkbox("Supply Crates", &g_Settings.espWorldSupplies);
            ImGui::Checkbox("Control Points", &g_Settings.espWorldControlPoints);
            ImGui::Checkbox("Hazard ESP (Mines / IED / C4)", &g_Settings.espHazards);

            ImGui::Spacing();
            ImGui::TextDisabled("Hazard Detection Distance:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##HazDist", &g_Settings.espHazardsMaxDist, 50.0f, 600.0f, "%.0f meters");

            ImGui::TextDisabled("World Objects Max Distance:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##WorldDist", &g_Settings.espWorldMaxDist, 100.0f, 3000.0f, "%.0f meters");

            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Entity List")) {
            const PlayerData* local = GetLocalPlayer();

            if (local) {
                ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.45f, 1.0f), "LOCAL PLAYER: %s", local->name.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("| Team: %u | Pos: (%.1f, %.1f, %.1f)", local->team, local->pos.x, local->pos.y, local->pos.z);
            } else {
                ImGui::TextDisabled("Local Player: Not Detected in Memory");
            }
            ImGui::SameLine(ImGui::GetWindowWidth() - 220.0f);
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 1.00f, 1.0f), "Tracked Entities: %zu", s_FramePlayers.size());

            ImGui::Spacing();

            static ImGuiTextFilter filter;
            filter.Draw("Filter by Name##EntityFilter", 240.0f);

            float tableH = ImGui::GetContentRegionAvail().y - 4.0f;
            if (ImGui::BeginTable("PlayersTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable, ImVec2(0, tableH))) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Team", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("Health", ImGuiTableColumnFlags_WidthFixed, 65.0f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Position (X, Y, Z)", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Classification", ImGuiTableColumnFlags_WidthFixed, 85.0f);
                ImGui::TableHeadersRow();

                for (auto& pl : s_FramePlayers) {
                    if (!filter.PassFilter(pl.name.c_str())) continue;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (pl.isLocal) {
                        ImGui::TextColored(ImVec4(0.20f, 1.00f, 0.50f, 1.0f), "%s [YOU]", pl.name.empty() ? "(Local)" : pl.name.c_str());
                    } else {
                        ImGui::Text("%s", pl.name.empty() ? "(Unknown)" : pl.name.c_str());
                    }

                    ImGui::TableSetColumnIndex(1);
                    if (local && pl.team == local->team) {
                        ImGui::TextColored(ImVec4(0.30f, 0.70f, 1.00f, 1.0f), "%u (Ally)", pl.team);
                    } else if (local && pl.team != local->team) {
                        ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.0f), "%u (Enemy)", pl.team);
                    } else {
                        ImGui::Text("%u", pl.team);
                    }

                    ImGui::TableSetColumnIndex(2);
                    if (pl.alive) {
                        ImGui::TextColored(pl.health > 50.0f ? ImVec4(0.20f, 1.00f, 0.40f, 1.0f) : ImVec4(1.00f, 0.30f, 0.30f, 1.0f), "%.0f HP", pl.health);
                    } else {
                        ImGui::TextDisabled("0 HP");
                    }

                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%s", pl.ai ? "AI Bot" : "Human");

                    ImGui::TableSetColumnIndex(4);
                    if (pl.alive) {
                        ImGui::TextColored(ImVec4(0.20f, 1.00f, 0.40f, 1.0f), "Alive");
                    } else {
                        ImGui::TextDisabled("Dead");
                    }

                    ImGui::TableSetColumnIndex(5);
                    if (pl.hasPos) {
                        ImGui::Text("%.1f, %.1f, %.1f", pl.pos.x, pl.pos.y, pl.pos.z);
                    } else {
                        ImGui::TextDisabled("N/A");
                    }

                    ImGui::TableSetColumnIndex(6);
                    if (pl.isLocal) {
                        ImGui::TextColored(ImVec4(0.20f, 1.00f, 0.50f, 1.0f), "LOCAL");
                    } else if (pl.isDowned) {
                        ImGui::TextColored(ImVec4(1.00f, 0.65f, 0.20f, 1.0f), "DOWNED");
                    } else if (pl.isVehicle) {
                        ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.20f, 1.0f), "VEHICLE");
                    } else {
                        ImGui::Text("Player");
                    }
                }
                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Colors & Palette")) {
            float availX = ImGui::GetContentRegionAvail().x;
            float colW = (availX - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
            float childH = ImGui::GetContentRegionAvail().y - 4.0f;

            ImGui::BeginChild("Colors_PickerCard", ImVec2(colW, childH), true);
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 1.00f, 1.0f), "TACTICAL COLOR SCHEME");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::ColorEdit4("Enemy Target (Inimigos)", g_Settings.colEnemy);
            ImGui::ColorEdit4("Friendly Target (Aliados)", g_Settings.colAlly);
            ImGui::ColorEdit4("Downed Target (Caídos)", g_Settings.colDowned);
            ImGui::ColorEdit4("Skeleton Bones (Esqueleto)", g_Settings.colSkel);
            ImGui::ColorEdit4("Hazard Explosive (Minas/IED)", g_Settings.colHazard);
            ImGui::ColorEdit4("Aim Threat Warning (Alerta)", g_Settings.colWarn);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Reset to Default Tactical Palette", ImVec2(-1, 28))) {
                g_Settings.colEnemy[0] = 1.0f; g_Settings.colEnemy[1] = 0.2f; g_Settings.colEnemy[2] = 0.2f; g_Settings.colEnemy[3] = 0.9f;
                g_Settings.colAlly[0]  = 0.3f; g_Settings.colAlly[1]  = 0.65f; g_Settings.colAlly[2]  = 1.0f; g_Settings.colAlly[3]  = 0.9f;
                g_Settings.colDowned[0] = 1.0f; g_Settings.colDowned[1] = 0.65f; g_Settings.colDowned[2] = 0.15f; g_Settings.colDowned[3] = 0.9f;
                g_Settings.colSkel[0]   = 0.2f; g_Settings.colSkel[1]   = 1.0f; g_Settings.colSkel[2]   = 0.5f; g_Settings.colSkel[3]   = 0.9f;
                g_Settings.colHazard[0] = 1.0f; g_Settings.colHazard[1] = 0.15f; g_Settings.colHazard[2] = 0.15f; g_Settings.colHazard[3] = 1.0f;
                g_Settings.colWarn[0]   = 1.0f; g_Settings.colWarn[1]   = 0.1f; g_Settings.colWarn[2]   = 0.1f; g_Settings.colWarn[3]   = 1.0f;
            }

            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("Colors_InfoCard", ImVec2(colW, childH), true);
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 1.00f, 1.0f), "PALETTE GUIDELINES");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::BulletText("Enemy: Highlights hostile combatants & boxes.");
            ImGui::BulletText("Ally: Blue tint for teammates and squad members.");
            ImGui::BulletText("Downed: Amber tint indicating incapacitated states.");
            ImGui::BulletText("Skeleton: High-visibility bone connector links.");
            ImGui::BulletText("Hazard: Sharp red beacon for explosive devices.");
            ImGui::BulletText("Warning: Pulsing red indicator when aimed at.");

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Config & System")) {
            float availX = ImGui::GetContentRegionAvail().x;
            float colW = (availX - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
            float childH = ImGui::GetContentRegionAvail().y - 4.0f;

            static char configName[64] = "toolkit_config.ini";
            static char feedbackMsg[128] = "";
            static DWORD feedbackTick = 0;

            ImGui::BeginChild("Config_FilesCard", ImVec2(colW, childH), true);
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 1.00f, 1.0f), "PROFILE CONFIGURATION");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled("Configuration Filename (INI):");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##ConfigFile", configName, IM_ARRAYSIZE(configName));

            ImGui::Spacing();
            if (ImGui::Button("Save Configuration Profile", ImVec2(-1, 28))) {
                g_Settings.SaveConfig(configName);
                snprintf(feedbackMsg, sizeof(feedbackMsg), "Config saved to '%s' successfully.", configName);
                feedbackTick = GetTickCount();
            }

            ImGui::Spacing();
            if (ImGui::Button("Load Configuration Profile", ImVec2(-1, 28))) {
                g_Settings.LoadConfig(configName);
                snprintf(feedbackMsg, sizeof(feedbackMsg), "Config loaded from '%s' successfully.", configName);
                feedbackTick = GetTickCount();
            }

            if (feedbackTick != 0 && (GetTickCount() - feedbackTick < 4000)) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.45f, 1.0f), "%s", feedbackMsg);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Configuration files are stored directly alongside the executable.");

            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("Config_ActionsCard", ImVec2(colW, childH), true);
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 1.00f, 1.0f), "HOTKEYS & RUNTIME ACTIONS");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::BulletText("Toggle Menu: [INSERT]");
            ImGui::BulletText("Alternative Toggle: [F11]");
            ImGui::BulletText("Click-through: Automatic when menu closed.");
            ImGui::BulletText("Input Focus: Returned to game on toggle.");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Close Menu Window", ImVec2(-1, 28))) {
                g_Settings.menuOpen = false;
            }

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.12f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.16f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.00f, 0.22f, 0.22f, 1.0f));
            if (ImGui::Button("Exit External Overlay Process", ImVec2(-1, 30))) {
                g_Settings.menuOpen = false;
                g_Settings.shouldQuit = true;
            }
            ImGui::PopStyleColor(3);

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

