#pragma once
// ============================================================
//  game.h  --  Entity system, bone cache, W2S, view matrix
// ============================================================

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DirectXMath.h>
#include <cmath>
#include <array>
#include <string>
#include <vector>

#include "mem.h"
#include "offsets.h"

// ── Math helpers ─────────────────────────────────────────────
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };

// CS2 4x4 row-major view matrix
struct ViewMatrix4x4
{
    float m[4][4];
};

// ── Bone IDs for CS2 skeleton (standard 27-bone) ─────────────
// See: https://github.com/a2x/cs2-dumper / bone enums
namespace BoneID
{
    enum : int
    {
        head        =  6,
        neck_0      =  5,
        spine_1     =  4,
        spine_2     =  2,
        pelvis      =  0,
        arm_upper_L = 11,
        arm_lower_L = 12,
        hand_L      = 13,
        arm_upper_R =  8,
        arm_lower_R =  9,
        hand_R      = 10,
        leg_upper_L = 23,
        leg_lower_L = 24,
        foot_L      = 25,
        leg_upper_R = 20,
        leg_lower_R = 21,
        foot_R      = 22,
    };
}

// Full skeleton bone array (uses raw pointer read from CModelState+0x80)
struct BoneArray
{
    Vec3 bones[32]; // we only need ~27 bones
};

// ── Entity data cached per-frame ──────────────────────────────
struct PlayerData
{
    bool        valid       = false;
    bool        dormant     = false;
    bool        alive       = false;
    int         health      = 0;
    int         team        = 0;         // 2 = T, 3 = CT
    Vec3        origin      = {};
    Vec3        headPos     = {};
    Vec3        eyeAngles   = {};
    std::string name;
    std::string weaponName;
    uintptr_t   pawnPtr     = 0;
    uintptr_t   ctrlPtr     = 0;

    // Bone world positions
    Vec3        bonePos[32] = {};

    // Grenade-specific (only valid if entity is a grenade projectile)
    std::vector<Vec3> grenadeTrail;
};

// ── Bomb data ─────────────────────────────────────────────────
struct BombData
{
    bool    planted    = false;
    bool    defused    = false;
    bool    ticking    = false;
    bool    beingDefused = false;
    float   blowTime   = 0.f;      // absolute game time
    float   timerLen   = 0.f;
    float   defuseLen  = 0.f;
    float   defuseCountdown = 0.f; // absolute game time
    int     site       = 0;        // 0 = A, 1 = B
    Vec3    pos        = {};
};

// ── Global frame state ────────────────────────────────────────
inline ViewMatrix4x4 gViewMatrix{};
inline PlayerData    gLocalPlayer{};
inline PlayerData    gPlayers[64]{};
inline int           gPlayerCount = 0;
inline BombData      gBomb{};

// ── World-to-Screen ───────────────────────────────────────────
// Returns false when the point is behind the camera.
inline bool WorldToScreen(const Vec3& world, Vec2& screen,
                           float screenW, float screenH)
{
    const float* m = &gViewMatrix.m[0][0];

    float clip_w = m[3] * world.x + m[7] * world.y + m[11] * world.z + m[15];
    if (clip_w < 0.001f) return false;

    float clip_x = m[0] * world.x + m[4] * world.y + m[8]  * world.z + m[12];
    float clip_y = m[1] * world.x + m[5] * world.y + m[9]  * world.z + m[13];

    screen.x = (screenW / 2.f) + (screenW / 2.f) * (clip_x / clip_w);
    screen.y = (screenH / 2.f) - (screenH / 2.f) * (clip_y / clip_w);
    return true;
}

// ── Game data updater ─────────────────────────────────────────
// Call once per render frame (or on a dedicated thread at lower rate).
class GameUpdater
{
public:
    void Update(float screenW, float screenH)
    {
        if (!gMem.IsValid()) return;

        // 1. View matrix
        {
            ViewMatrix4x4 vm{};
            gMem.ReadRaw(gMem.clientBase + off::dwViewMatrix, &vm, sizeof(vm));
            gViewMatrix = vm;
        }

        // 2. Local player pawn pointer
        uintptr_t localPawnAddr = gMem.Read<uintptr_t>(gMem.clientBase + off::dwLocalPlayerPawn);
        uintptr_t localCtrlAddr = gMem.Read<uintptr_t>(gMem.clientBase + off::dwLocalPlayerController);

        gLocalPlayer = {};
        if (localPawnAddr)
        {
            gLocalPlayer.pawnPtr = localPawnAddr;
            gLocalPlayer.ctrlPtr = localCtrlAddr;
            ReadPawnData(localPawnAddr, localCtrlAddr, gLocalPlayer);
        }

        // 3. Entity list
        uintptr_t entityList = gMem.Read<uintptr_t>(gMem.clientBase + off::dwEntityList);
        if (!entityList) return;

        // CS2 entity list: first chunk at [entityList + 0x10] contains 512 entries,
        // each chunk pointer holds 512 / 512 = sub-lists of 512 entities.
        // High-entity-index: list[chunkIdx * 8 + 8] -> chunk base -> entry
        uintptr_t listChunk = gMem.Read<uintptr_t>(entityList + 0x10);
        if (!listChunk) return;

        gPlayerCount = 0;
        for (int i = 0; i < 64; i++)
        {
            gPlayers[i] = {};
            uintptr_t ctrlEntry = gMem.Read<uintptr_t>(listChunk + (i * 0x78));
            if (!ctrlEntry) continue;

            // Skip local player slot if addresses match
            if (ctrlEntry == localCtrlAddr) continue;

            PlayerData p;
            p.ctrlPtr = ctrlEntry;

            // Get pawn handle
            uint32_t pawnHandle = gMem.Read<uint32_t>(ctrlEntry + off::m_hPlayerPawn);
            if (!pawnHandle || pawnHandle == 0xFFFFFFFF) continue;

            // Resolve pawn pointer from handle
            uintptr_t listChunk2 = gMem.Read<uintptr_t>(entityList + 0x10 + ((pawnHandle & 0x7FFF) / 512) * 8);
            if (!listChunk2) continue;
            uintptr_t pawnPtr = gMem.Read<uintptr_t>(listChunk2 + (pawnHandle & 0x1FF) * 0x78);
            if (!pawnPtr) continue;

            p.pawnPtr = pawnPtr;
            ReadPawnData(pawnPtr, ctrlEntry, p);

            if (!p.valid) continue;

            gPlayers[i] = p;
            gPlayerCount++;
        }

        // 4. Bomb
        UpdateBomb(entityList);
    }

private:
    // ── Read all data for a single player pawn ────────────────
    void ReadPawnData(uintptr_t pawnPtr, uintptr_t ctrlPtr, PlayerData& out)
    {
        out.pawnPtr = pawnPtr;
        out.ctrlPtr = ctrlPtr;

        // Lifestate / health
        uint8_t lifeState = gMem.Read<uint8_t>(pawnPtr + off::m_lifeState);
        out.health  = gMem.Read<int32_t>(pawnPtr + off::m_iHealth);
        out.alive   = (lifeState == 0) && (out.health > 0);
        out.team    = gMem.Read<uint8_t>(pawnPtr + off::m_iTeamNum);

        // Scene node -> origin
        uintptr_t sceneNode = gMem.Read<uintptr_t>(pawnPtr + off::m_pGameSceneNode);
        if (!sceneNode) return;

        out.dormant = gMem.Read<uint8_t>(sceneNode + off::m_bDormant) != 0;
        Vec3 absOrigin = gMem.Read<Vec3>(sceneNode + off::m_vecAbsOrigin);
        out.origin = absOrigin;

        // Eye angles
        out.eyeAngles = gMem.Read<Vec3>(pawnPtr + off::m_angEyeAngles);

        // Bones
        ReadBones(sceneNode, out);
        out.headPos = out.bonePos[BoneID::head];

        // Player name
        if (ctrlPtr)
            out.name = gMem.ReadString(ctrlPtr + off::m_iszPlayerName, 128);

        // Weapon name
        uintptr_t weapSvcs = gMem.Read<uintptr_t>(pawnPtr + off::m_pWeaponServices);
        if (weapSvcs)
        {
            uint32_t activeHandle = gMem.Read<uint32_t>(weapSvcs + off::m_hActiveWeapon);
            if (activeHandle && activeHandle != 0xFFFFFFFF)
            {
                uintptr_t entityList2 = gMem.Read<uintptr_t>(gMem.clientBase + off::dwEntityList);
                uintptr_t listChunk2  = gMem.Read<uintptr_t>(entityList2 + 0x10 +
                    ((activeHandle & 0x7FFF) / 512) * 8);
                if (listChunk2)
                {
                    uintptr_t weapPtr = gMem.Read<uintptr_t>(listChunk2 + (activeHandle & 0x1FF) * 0x78);
                    if (weapPtr)
                    {
                        // pVData -> szName (CGlobalSymbol stores a const char*)
                        uintptr_t pVData = gMem.Read<uintptr_t>(weapPtr + off::m_pVData);
                        if (pVData)
                        {
                            uintptr_t szNamePtr = gMem.Read<uintptr_t>(pVData + off::m_szName);
                            if (szNamePtr)
                                out.weaponName = gMem.ReadString(szNamePtr, 64);
                        }
                    }
                }
            }
        }

        out.valid = true;
    }

    // ── Read bone world positions ─────────────────────────────
    void ReadBones(uintptr_t sceneNode, PlayerData& out)
    {
        // CSkeletonInstance is the derived type of the game scene node for players.
        // Bone positions are at CModelState + 0x80 (pointer to float[32*3] array)
        uintptr_t modelState = sceneNode + off::m_modelState; // offset 0x140 in CSkeletonInstance
        uintptr_t boneArrayPtr = gMem.Read<uintptr_t>(modelState + off::m_boneArray); // +0x80
        if (!boneArrayPtr) return;

        // Read 32 bone positions at once
        struct BonePos { float x, y, z; float pad; };
        BonePos raw[32]{};
        gMem.ReadRaw(boneArrayPtr, raw, sizeof(raw));
        for (int b = 0; b < 32; b++)
        {
            out.bonePos[b] = { raw[b].x, raw[b].y, raw[b].z };
        }
    }

    // ── Update planted bomb data ──────────────────────────────
    void UpdateBomb(uintptr_t entityList)
    {
        gBomb = {};
        uintptr_t plantedC4 = gMem.Read<uintptr_t>(gMem.clientBase + off::dwPlantedC4);
        if (!plantedC4) return;

        // dwPlantedC4 is a pointer-to-pointer in CS2
        uintptr_t c4Ptr = gMem.Read<uintptr_t>(plantedC4);
        if (!c4Ptr) return;

        gBomb.planted        = true;
        gBomb.ticking        = gMem.Read<bool>(c4Ptr + off::m_bBombTicking);
        gBomb.defused        = gMem.Read<bool>(c4Ptr + off::m_bBombDefused);
        gBomb.beingDefused   = gMem.Read<bool>(c4Ptr + off::m_bBeingDefused);
        gBomb.blowTime       = gMem.Read<float>(c4Ptr + off::m_flC4Blow);
        gBomb.timerLen       = gMem.Read<float>(c4Ptr + off::m_flTimerLength);
        gBomb.defuseLen      = gMem.Read<float>(c4Ptr + off::m_flDefuseLength);
        gBomb.defuseCountdown = gMem.Read<float>(c4Ptr + off::m_flDefuseCountDown);
        gBomb.site           = gMem.Read<int32_t>(c4Ptr + off::m_nBombSite);

        // Bomb position from scene node
        uintptr_t sceneNode = gMem.Read<uintptr_t>(c4Ptr + off::m_pGameSceneNode);
        if (sceneNode)
            gBomb.pos = gMem.Read<Vec3>(sceneNode + off::m_vecAbsOrigin);
    }
};

inline GameUpdater gUpdater;
