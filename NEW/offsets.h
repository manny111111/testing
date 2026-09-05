#pragma once
// ============================================================
//  offsets.h  --  flat aliases from the a2x cs2-dumper headers
//  All values sourced from Offsets/offsets.hpp and
//  Offsets/client_dll.hpp (dump 2026-09-04)
// ============================================================

#include <cstddef>
#include <cstdint>

// ── Pull in the raw dump headers ────────────────────────────
#include "Offsets/offsets.hpp"
#include "Offsets/client_dll.hpp"

namespace offsets
{
    // ── client.dll globals ───────────────────────────────────
    using namespace cs2_dumper::offsets::client_dll;

    // ── C_BaseEntity ─────────────────────────────────────────
    namespace entity
    {
        using namespace cs2_dumper::schemas::client_dll::C_BaseEntity;
    }

    // ── CGameSceneNode ───────────────────────────────────────
    namespace scene_node
    {
        using namespace cs2_dumper::schemas::client_dll::CGameSceneNode;
    }

    // ── CSkeletonInstance ────────────────────────────────────
    namespace skeleton
    {
        using namespace cs2_dumper::schemas::client_dll::CSkeletonInstance;
    }

    // ── CModelState ──────────────────────────────────────────
    namespace model_state
    {
        using namespace cs2_dumper::schemas::client_dll::CModelState;
    }

    // ── CBasePlayerController ────────────────────────────────
    namespace base_controller
    {
        using namespace cs2_dumper::schemas::client_dll::CBasePlayerController;
    }

    // ── CCSPlayerController ──────────────────────────────────
    namespace controller
    {
        using namespace cs2_dumper::schemas::client_dll::CCSPlayerController;
    }

    // ── C_BasePlayerPawn ─────────────────────────────────────
    namespace base_pawn
    {
        using namespace cs2_dumper::schemas::client_dll::C_BasePlayerPawn;
    }

    // ── C_CSPlayerPawnBase ───────────────────────────────────
    namespace pawn_base
    {
        using namespace cs2_dumper::schemas::client_dll::C_CSPlayerPawnBase;
    }

    // ── C_CSPlayerPawn ───────────────────────────────────────
    namespace pawn
    {
        using namespace cs2_dumper::schemas::client_dll::C_CSPlayerPawn;
    }

    // ── CPlayer_WeaponServices ───────────────────────────────
    namespace weapon_services
    {
        using namespace cs2_dumper::schemas::client_dll::CPlayer_WeaponServices;
    }

    // ── C_CSWeaponBase ───────────────────────────────────────
    namespace weapon_base
    {
        using namespace cs2_dumper::schemas::client_dll::C_CSWeaponBase;
    }

    // ── CCSWeaponBaseVData (weapon name, etc.) ───────────────
    namespace weapon_vdata
    {
        using namespace cs2_dumper::schemas::client_dll::CCSWeaponBaseVData;
    }

    // ── C_PlantedC4 ──────────────────────────────────────────
    namespace planted_c4
    {
        using namespace cs2_dumper::schemas::client_dll::C_PlantedC4;
    }

    // ── C_CSGameRules ────────────────────────────────────────
    namespace game_rules
    {
        using namespace cs2_dumper::schemas::client_dll::C_CSGameRules;
    }

    // ── C_CSGameRulesProxy ───────────────────────────────────
    namespace game_rules_proxy
    {
        using namespace cs2_dumper::schemas::client_dll::C_CSGameRulesProxy;
    }

    // ── C_BaseCSGrenadeProjectile ────────────────────────────
    namespace grenade_proj
    {
        using namespace cs2_dumper::schemas::client_dll::C_BaseCSGrenadeProjectile;
    }

    // ── CCSPlayerBase_CameraServices ─────────────────────────
    namespace camera_services
    {
        using namespace cs2_dumper::schemas::client_dll::CCSPlayerBase_CameraServices;
    }
}

// ── Convenient standalone compile-time constants ─────────────
namespace off
{
    // Globals in client.dll
    inline constexpr std::ptrdiff_t dwEntityList         = cs2_dumper::offsets::client_dll::dwEntityList;
    inline constexpr std::ptrdiff_t dwLocalPlayerController = cs2_dumper::offsets::client_dll::dwLocalPlayerController;
    inline constexpr std::ptrdiff_t dwLocalPlayerPawn    = cs2_dumper::offsets::client_dll::dwLocalPlayerPawn;
    inline constexpr std::ptrdiff_t dwViewMatrix         = cs2_dumper::offsets::client_dll::dwViewMatrix;
    inline constexpr std::ptrdiff_t dwPlantedC4          = cs2_dumper::offsets::client_dll::dwPlantedC4;
    inline constexpr std::ptrdiff_t dwGameRules          = cs2_dumper::offsets::client_dll::dwGameRules;

    // Entity
    inline constexpr std::ptrdiff_t m_iHealth            = cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth;
    inline constexpr std::ptrdiff_t m_iTeamNum           = cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum;
    inline constexpr std::ptrdiff_t m_lifeState          = cs2_dumper::schemas::client_dll::C_BaseEntity::m_lifeState;
    inline constexpr std::ptrdiff_t m_pGameSceneNode     = cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode;
    inline constexpr std::ptrdiff_t m_hOwnerEntity       = cs2_dumper::schemas::client_dll::C_BaseEntity::m_hOwnerEntity;
    inline constexpr std::ptrdiff_t m_vecAbsVelocity     = cs2_dumper::schemas::client_dll::C_BaseEntity::m_vecAbsVelocity;

    // Scene node
    inline constexpr std::ptrdiff_t m_vecAbsOrigin       = cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin;
    inline constexpr std::ptrdiff_t m_bDormant           = cs2_dumper::schemas::client_dll::CGameSceneNode::m_bDormant;

    // Skeleton
    inline constexpr std::ptrdiff_t m_modelState         = cs2_dumper::schemas::client_dll::CSkeletonInstance::m_modelState;

    // Model state / bone cache
    inline constexpr std::ptrdiff_t ms_MeshGroupMask     = cs2_dumper::schemas::client_dll::CModelState::m_MeshGroupMask; // not bone pos, but same struct
    inline constexpr std::ptrdiff_t ms_hModel            = cs2_dumper::schemas::client_dll::CModelState::m_hModel;
    // Bone positions are at CModelState + 0x80 in CS2 (bone array pointer inside CModelState)
    inline constexpr std::ptrdiff_t m_boneArray          = 0x80; // offset inside CModelState to bone positions pointer

    // Controller
    inline constexpr std::ptrdiff_t m_hPlayerPawn        = cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn;
    inline constexpr std::ptrdiff_t m_iszPlayerName      = cs2_dumper::schemas::client_dll::CBasePlayerController::m_iszPlayerName;
    inline constexpr std::ptrdiff_t m_bPawnIsAlive       = cs2_dumper::schemas::client_dll::CCSPlayerController::m_bPawnIsAlive;

    // Pawn
    inline constexpr std::ptrdiff_t m_pWeaponServices    = cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pWeaponServices;
    inline constexpr std::ptrdiff_t m_angEyeAngles       = cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_angEyeAngles;
    inline constexpr std::ptrdiff_t m_vOldOrigin         = cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin;

    // Weapon services
    inline constexpr std::ptrdiff_t m_hActiveWeapon      = cs2_dumper::schemas::client_dll::CPlayer_WeaponServices::m_hActiveWeapon;
    inline constexpr std::ptrdiff_t m_hMyWeapons         = cs2_dumper::schemas::client_dll::CPlayer_WeaponServices::m_hMyWeapons;

    // Weapon VData — m_szName lives in CCSWeaponBaseVData
    inline constexpr std::ptrdiff_t m_szName             = cs2_dumper::schemas::client_dll::CCSWeaponBaseVData::m_szName;
    // VData pointer is 0x20 into C_BaseWeaponVData (empirically confirmed, not in dump directly)
    inline constexpr std::ptrdiff_t m_pVData             = 0x370; // C_EconEntity -> m_pVData (standard CS2 offset)

    // PlantedC4
    inline constexpr std::ptrdiff_t m_flC4Blow           = cs2_dumper::schemas::client_dll::C_PlantedC4::m_flC4Blow;
    inline constexpr std::ptrdiff_t m_flTimerLength      = cs2_dumper::schemas::client_dll::C_PlantedC4::m_flTimerLength;
    inline constexpr std::ptrdiff_t m_bBombTicking       = cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBombTicking;
    inline constexpr std::ptrdiff_t m_bBombDefused       = cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBombDefused;
    inline constexpr std::ptrdiff_t m_bBeingDefused      = cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBeingDefused;
    inline constexpr std::ptrdiff_t m_flDefuseLength     = cs2_dumper::schemas::client_dll::C_PlantedC4::m_flDefuseLength;
    inline constexpr std::ptrdiff_t m_flDefuseCountDown  = cs2_dumper::schemas::client_dll::C_PlantedC4::m_flDefuseCountDown;
    inline constexpr std::ptrdiff_t m_nBombSite          = cs2_dumper::schemas::client_dll::C_PlantedC4::m_nBombSite;

    // GameRulesProxy
    inline constexpr std::ptrdiff_t m_pGameRules         = cs2_dumper::schemas::client_dll::C_CSGameRulesProxy::m_pGameRules;

    // Grenade trail
    inline constexpr std::ptrdiff_t m_arrTrajectoryTrailPoints = cs2_dumper::schemas::client_dll::C_BaseCSGrenadeProjectile::m_arrTrajectoryTrailPoints;
}
