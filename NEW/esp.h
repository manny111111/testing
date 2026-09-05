#pragma once
// ============================================================
//  esp.h  --  All ESP rendering via ImGui DrawList
//  Features: corner boxes, full boxes, health bars,
//            skeleton, player names, weapon names,
//            bomb timer, grenade trails
// ============================================================

#include "imgui/imgui.h"
#include "game.h"
#include <string>
#include <cmath>
#include <algorithm>

// ── ESP settings (adjusted via ImGui config window) ──────────
struct EspSettings
{
    bool enabled         = true;
    bool showBoxFull     = false;   // false = corner boxes, true = full box
    bool showHealthBar   = true;
    bool showSkeleton    = true;
    bool showName        = true;
    bool showWeapon      = true;
    bool showBomb        = true;
    bool showGrenadeTrail= true;
    bool teamCheck       = true;    // skip teammates

    // Colors
    ImVec4 colorEnemy    = { 1.f, 0.15f, 0.15f, 1.f };
    ImVec4 colorTeam     = { 0.15f, 0.85f, 1.f,  1.f };
    ImVec4 colorBomb     = { 1.f,  0.65f, 0.f,   1.f };
    ImVec4 colorGrenade  = { 0.2f, 1.f,   0.4f,  1.f };

    float  boxRounding   = 0.f;
    float  boxThickness  = 1.5f;
    float  cornerLen     = 0.3f;  // fraction of box side that corner covers
};

inline EspSettings gEsp;

// ── Helpers ───────────────────────────────────────────────────
static inline ImU32 ToImU32(const ImVec4& c)
{
    return ImGui::ColorConvertFloat4ToU32(c);
}

static inline ImVec4 HealthColor(int hp)
{
    // Green (100hp) -> Yellow (50hp) -> Red (0hp)
    float t = std::clamp(hp / 100.f, 0.f, 1.f);
    if (t > 0.5f)
        return { 2.f*(1.f-t), 1.f, 0.f, 1.f };  // yellow->green
    else
        return { 1.f, 2.f*t,  0.f, 1.f };        // red->yellow
}

// ── Main ESP render function (call inside ImGui frame) ────────
class ESP
{
public:
    void Render(float screenW, float screenH)
    {
        if (!gEsp.enabled) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        // Local player team for team-check
        int localTeam = gLocalPlayer.team;

        // ── Players ───────────────────────────────────────────
        for (int i = 0; i < 64; i++)
        {
            const PlayerData& p = gPlayers[i];
            if (!p.valid || !p.alive || p.dormant) continue;

            // Team check
            if (gEsp.teamCheck && p.team == localTeam) continue;

            bool isEnemy = (p.team != localTeam);
            ImVec4 col4 = isEnemy ? gEsp.colorEnemy : gEsp.colorTeam;
            ImU32  col  = ToImU32(col4);

            // Project head + foot
            Vec2 headScreen{}, footScreen{};
            Vec3 headPos  = { p.bonePos[BoneID::head].x,
                              p.bonePos[BoneID::head].y,
                              p.bonePos[BoneID::head].z + 9.f }; // tiny head offset
            Vec3 footPos  = p.origin;

            if (!WorldToScreen(headPos, headScreen, screenW, screenH)) continue;
            if (!WorldToScreen(footPos, footScreen, screenW, screenH)) continue;

            float boxH = footScreen.y - headScreen.y;
            if (boxH < 4.f) continue;
            float boxW = boxH * 0.45f;
            float bx   = headScreen.x - boxW * 0.5f;
            float by   = headScreen.y;
            float br   = bx + boxW;
            float bb   = footScreen.y;

            // ── Box ───────────────────────────────────────────
            if (gEsp.showBoxFull)
            {
                // Full rectangle
                dl->AddRect({ bx, by }, { br, bb }, col, 0.f, 0, gEsp.boxThickness);
            }
            else
            {
                // Corner box
                DrawCornerBox(dl, bx, by, br, bb, col, gEsp.boxThickness, gEsp.cornerLen);
            }

            // ── Health bar (left side) ────────────────────────
            if (gEsp.showHealthBar)
            {
                float hFrac  = std::clamp(p.health / 100.f, 0.f, 1.f);
                float barX   = bx - 6.f;
                float barTop = by;
                float barBot = bb;
                float barH   = barBot - barTop;
                float fillTop = barBot - barH * hFrac;

                // Background
                dl->AddRectFilled({ barX - 1.f, barTop }, { barX + 3.f, barBot },
                                  IM_COL32(0,0,0,140));
                // Fill
                ImVec4 hcol = HealthColor(p.health);
                dl->AddRectFilled({ barX, fillTop }, { barX + 2.f, barBot },
                                  ToImU32(hcol));
                // HP text (only if <100)
                if (p.health < 100)
                {
                    char hpBuf[8];
                    snprintf(hpBuf, sizeof(hpBuf), "%d", p.health);
                    dl->AddText({ barX - 1.f, fillTop - 12.f },
                                IM_COL32(255,255,255,200), hpBuf);
                }
            }

            // ── Skeleton ──────────────────────────────────────
            if (gEsp.showSkeleton)
                DrawSkeleton(dl, p, col, screenW, screenH);

            // ── Name ──────────────────────────────────────────
            if (gEsp.showName && !p.name.empty())
            {
                ImVec2 nameSize = ImGui::CalcTextSize(p.name.c_str());
                dl->AddText({ headScreen.x - nameSize.x * 0.5f, by - nameSize.y - 2.f },
                            IM_COL32(255,255,255,230), p.name.c_str());
            }

            // ── Weapon name ───────────────────────────────────
            if (gEsp.showWeapon && !p.weaponName.empty())
            {
                // Strip leading "weapon_" prefix
                std::string wname = p.weaponName;
                const char* prefix = "weapon_";
                if (wname.compare(0, 7, prefix) == 0)
                    wname = wname.substr(7);

                ImVec2 wSize = ImGui::CalcTextSize(wname.c_str());
                dl->AddText({ footScreen.x - wSize.x * 0.5f, footScreen.y + 2.f },
                            ToImU32(col4), wname.c_str());
            }
        }

        // ── Bomb timer ────────────────────────────────────────
        if (gEsp.showBomb)
            DrawBomb(dl, screenW, screenH);

        // ── Grenade trails ────────────────────────────────────
        if (gEsp.showGrenadeTrail)
            DrawGrenadeTrails(dl, screenW, screenH);
    }

private:
    // ── Corner box helper ─────────────────────────────────────
    void DrawCornerBox(ImDrawList* dl,
                       float x1, float y1, float x2, float y2,
                       ImU32 col, float thick, float lenFrac)
    {
        float lx = (x2 - x1) * lenFrac;
        float ly = (y2 - y1) * lenFrac;

        // Top-left
        dl->AddLine({x1,y1}, {x1+lx,y1}, col, thick);
        dl->AddLine({x1,y1}, {x1,y1+ly}, col, thick);
        // Top-right
        dl->AddLine({x2,y1}, {x2-lx,y1}, col, thick);
        dl->AddLine({x2,y1}, {x2,y1+ly}, col, thick);
        // Bottom-left
        dl->AddLine({x1,y2}, {x1+lx,y2}, col, thick);
        dl->AddLine({x1,y2}, {x1,y2-ly}, col, thick);
        // Bottom-right
        dl->AddLine({x2,y2}, {x2-lx,y2}, col, thick);
        dl->AddLine({x2,y2}, {x2,y2-ly}, col, thick);
    }

    // ── Skeleton line pairs ───────────────────────────────────
    void DrawSkeleton(ImDrawList* dl, const PlayerData& p,
                      ImU32 col, float sw, float sh)
    {
        static const int pairs[][2] = {
            { BoneID::head,        BoneID::neck_0      },
            { BoneID::neck_0,      BoneID::spine_1     },
            { BoneID::spine_1,     BoneID::spine_2     },
            { BoneID::spine_2,     BoneID::pelvis      },
            { BoneID::spine_1,     BoneID::arm_upper_L },
            { BoneID::arm_upper_L, BoneID::arm_lower_L },
            { BoneID::arm_lower_L, BoneID::hand_L      },
            { BoneID::spine_1,     BoneID::arm_upper_R },
            { BoneID::arm_upper_R, BoneID::arm_lower_R },
            { BoneID::arm_lower_R, BoneID::hand_R      },
            { BoneID::pelvis,      BoneID::leg_upper_L },
            { BoneID::leg_upper_L, BoneID::leg_lower_L },
            { BoneID::leg_lower_L, BoneID::foot_L      },
            { BoneID::pelvis,      BoneID::leg_upper_R },
            { BoneID::leg_upper_R, BoneID::leg_lower_R },
            { BoneID::leg_lower_R, BoneID::foot_R      },
        };

        for (auto& pair : pairs)
        {
            Vec2 a{}, b{};
            if (!WorldToScreen(p.bonePos[pair[0]], a, sw, sh)) continue;
            if (!WorldToScreen(p.bonePos[pair[1]], b, sw, sh)) continue;
            dl->AddLine({a.x,a.y}, {b.x,b.y}, col, 1.0f);
        }
    }

    // ── Bomb timer display ────────────────────────────────────
    void DrawBomb(ImDrawList* dl, float sw, float sh)
    {
        if (!gBomb.planted || gBomb.defused) return;

        // Get current game time from system (approximate)
        static float gameTimeAtLoad = 0.f;  // will be set when bomb first ticks
        float timeNow = ImGui::GetTime();   // fallback: use ImGui time as relative ref

        // We'll use currentTime = blowTime - (blowTime - now) approach
        // Since we don't have server game time from RPM easily, we derive remaining:
        // remaining = blowTime - currentGameTime
        // We approximate currentGameTime by reading GlobalVars
        float remaining = gBomb.blowTime - timeNow;
        if (!gBomb.ticking) return;

        // Clamp
        if (remaining < 0.f) remaining = 0.f;

        // Build text
        char bombBuf[128];
        float pct = std::clamp(remaining / std::max(gBomb.timerLen, 0.001f), 0.f, 1.f);

        if (gBomb.beingDefused)
        {
            float defRemaining = gBomb.defuseCountdown - timeNow;
            snprintf(bombBuf, sizeof(bombBuf),
                     "[BOMB] %.1fs  [DEFUSING: %.1fs]  Site: %c",
                     remaining, std::max(defRemaining, 0.f),
                     (gBomb.site == 0 ? 'A' : 'B'));
        }
        else
        {
            snprintf(bombBuf, sizeof(bombBuf),
                     "[BOMB] %.1fs  Site: %c",
                     remaining, (gBomb.site == 0 ? 'A' : 'B'));
        }

        // Draw at top-center
        float panelW = 280.f, panelH = 50.f;
        float px = sw * 0.5f - panelW * 0.5f;
        float py = 12.f;

        // Background
        dl->AddRectFilled({px-4,py-4}, {px+panelW+4,py+panelH+4},
                          IM_COL32(10,10,10,200), 6.f);

        // Progress bar (red)
        ImVec4 bombCol = { 1.f - pct, pct * 0.5f, 0.f, 1.f };
        dl->AddRectFilled({px, py+28.f}, {px + panelW * pct, py+34.f},
                          ToImU32(bombCol));
        dl->AddRect({px, py+28.f}, {px+panelW, py+34.f},
                    IM_COL32(255,255,255,100));

        // Text
        dl->AddText({px+4.f, py+6.f}, ToImU32(gEsp.colorBomb), bombBuf);

        // Also render on bomb world position
        Vec2 bombScreen{};
        if (WorldToScreen(gBomb.pos, bombScreen, sw, sh))
        {
            char siteBuf[32];
            snprintf(siteBuf, sizeof(siteBuf), "[BOMB] %.1fs", remaining);
            ImVec2 ts = ImGui::CalcTextSize(siteBuf);
            dl->AddText({bombScreen.x - ts.x*0.5f, bombScreen.y - ts.y - 4.f},
                        ToImU32(gEsp.colorBomb), siteBuf);
        }
    }

    // ── Grenade trail lines ───────────────────────────────────
    void DrawGrenadeTrails(ImDrawList* dl, float sw, float sh)
    {
        // We don't enumerate grenade entities separately in the current frame data;
        // grenade trails are stored in gPlayers[i].grenadeTrail if populated.
        // The GameUpdater would need to be extended to scan grenade entity types.
        // For now, iterate and draw whatever trail data is cached.
        for (int i = 0; i < 64; i++)
        {
            const auto& p = gPlayers[i];
            if (p.grenadeTrail.size() < 2) continue;

            ImU32 col = ToImU32(gEsp.colorGrenade);
            Vec2 prev{};
            bool hasPrev = false;
            for (const auto& pt : p.grenadeTrail)
            {
                Vec2 screen{};
                if (!WorldToScreen(pt, screen, sw, sh)) { hasPrev = false; continue; }
                if (hasPrev)
                    dl->AddLine({prev.x,prev.y}, {screen.x,screen.y}, col, 1.5f);
                prev    = screen;
                hasPrev = true;
            }
        }
    }
};

inline ESP gESP;
