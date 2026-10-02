#pragma once
#include <windows.h>
#include "imgui.h"
#include "settings.h"

void DrawOffscreenArrow(float screenCenterX, float screenCenterY, float angleRad, float radius, ImU32 color, ImDrawList* draw);
void Draw3DBox(const Vector3& origin, float width, float depth, float height, float yOffset, ImDrawList* draw, ImU32 color, int screenWidth, int screenHeight);
void UpdateFrameDataSnapshot();
void RenderVisualESP(int screenWidth, int screenHeight);
void RenderRadar();
void RenderAmmoHud(int screenWidth, int screenHeight);
void RenderWatermark();
void RenderMenu();
void ApplyModernTheme();
