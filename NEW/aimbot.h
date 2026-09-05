#pragma once
// ============================================================
//  aimbot.h  --  Aimbot logic + SendInput mouse movement
//                Runs on a dedicated 2000 Hz thread
// ============================================================

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cmath>
#include <thread>
#include <atomic>
#include <algorithm>
#include <chrono>

#include "game.h"
#include "mem.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Aimbot settings ───────────────────────────────────────────
struct AimbotSettings
{
    // Disabled by default for safety — user must explicitly enable aimbot.
    std::atomic<bool>  enabled      { false };
    std::atomic<bool>  silentAim    { false };
    std::atomic<bool>  teamCheck    { true };
    // No default hotkey (0 = none). Prevents accidental LMB triggering while clicking UI.
    std::atomic<int>   hotkey       { 0 };
    std::atomic<float> fov          { 5.f };           // degrees
    std::atomic<float> smooth       { 5.f };           // higher = slower
    std::atomic<float> targetBone   { (float)BoneID::head };
};

inline AimbotSettings gAimbot;

// ── Math helpers ──────────────────────────────────────────────
namespace AimMath
{
    inline float ToRad(float deg) { return deg * (float)(M_PI / 180.0); }
    inline float ToDeg(float rad) { return rad * (float)(180.0 / M_PI); }

    // Clamp angle to [-180, 180]
    inline float NormalizeAngle(float a)
    {
        while (a >  180.f) a -= 360.f;
        while (a < -180.f) a += 360.f;
        return a;
    }

    // Pitch / yaw from local to target
    struct Angles { float pitch, yaw; };

    inline Angles CalcAngle(const Vec3& from, const Vec3& to)
    {
        Vec3 delta = { to.x - from.x, to.y - from.y, to.z - from.z };
        float dist = std::sqrtf(delta.x*delta.x + delta.y*delta.y + delta.z*delta.z);
        if (dist < 0.001f) return { 0.f, 0.f };
        Angles a;
        a.pitch = ToDeg(-std::asinf(delta.z / dist));
        a.yaw   = ToDeg(std::atan2f(delta.y, delta.x));
        return a;
    }

    // FOV distance in degrees between current viewangles and target
    inline float GetFov(const Vec3& viewAngles, const Angles& targetAngles)
    {
        float dp = NormalizeAngle(targetAngles.pitch - viewAngles.x);
        float dy = NormalizeAngle(targetAngles.yaw   - viewAngles.y);
        return std::sqrtf(dp*dp + dy*dy);
    }

    // Angle delta -> pixel delta
    // For SendInput: we move relative pixels.
    // Formula: pixels = (angleDelta / sensitivity) * (screenW / fovDeg)
    // We use a simple multiplier based on 1000 DPI / sensitivity 1.0 baseline.
    inline float AngleToPix(float angleDelta, float sensitivity = 1.f)
    {
        // CS2: 1 degree = ~5.86 raw counts at sens 1.0, 400 DPI
        // We use a tunable factor; adjust in config if needed.
        constexpr float kFactor = 4.8f;
        return angleDelta * kFactor / sensitivity;
    }
}

// ── SendInput mouse movement (no injection needed) ────────────
inline void MoveMouse(int dx, int dy)
{
    if (dx == 0 && dy == 0) return;
    INPUT inp{};
    inp.type           = INPUT_MOUSE;
    inp.mi.dwFlags     = MOUSEEVENTF_MOVE;
    inp.mi.dx          = dx;
    inp.mi.dy          = dy;
    SendInput(1, &inp, sizeof(INPUT));
}

// ── Aimbot worker ─────────────────────────────────────────────
class Aimbot
{
public:
    void Start()
    {
        m_running = true;
        m_thread  = std::thread(&Aimbot::ThreadFunc, this);
    }

    void Stop()
    {
        m_running = false;
        if (m_thread.joinable()) m_thread.join();
    }

private:
    std::thread      m_thread;
    std::atomic<bool> m_running { false };

    void ThreadFunc()
    {
        // Target 2000 Hz -> 500 µs per iteration
        constexpr auto kInterval = std::chrono::microseconds(500);

        while (m_running)
        {
            auto t0 = std::chrono::high_resolution_clock::now();

            // Only call Tick when enabled and memory interface valid.
            if (gAimbot.enabled.load() && gMem.IsValid())
                Tick();

            auto elapsed = std::chrono::high_resolution_clock::now() - t0;
            auto sleep   = kInterval - elapsed;
            if (sleep.count() > 0)
                std::this_thread::sleep_for(sleep);
        }
    }

    void Tick()
    {
        // Safety: require a configured hotkey
        int hk = gAimbot.hotkey.load();
        if (hk == 0) return;

        // Only operate when the game window is foreground to avoid stealing desktop input.
        HWND fg = GetForegroundWindow();
        HWND target = FindWindowA(nullptr, "Counter-Strike 2");
        if (!target || fg != target) return;

        // Check hotkey state
        if (!(GetAsyncKeyState(hk) & 0x8000)) return;

        // Validate local player
        if (!gLocalPlayer.valid || !gLocalPlayer.alive) return;

        // Rate limiter: avoid calling SendInput too often (protect system responsiveness)
        static auto lastMove = std::chrono::high_resolution_clock::time_point{};
        auto now = std::chrono::high_resolution_clock::now();
        constexpr auto kMinInterval = std::chrono::milliseconds(2); // >=2ms between moves
        if (lastMove.time_since_epoch().count() != 0 && (now - lastMove) < kMinInterval)
            return;
        lastMove = now;

        // Local eye position (origin + eye height ~64 units in CS2)
        Vec3 eyePos = gLocalPlayer.origin;
        eyePos.z += 64.f;
        Vec3 viewAngles = gLocalPlayer.eyeAngles; // pitch, yaw, roll

        int   bestIdx    = -1;
        float bestFov    = gAimbot.fov.load();

        for (int i = 0; i < 64; i++)
        {
            const PlayerData& p = gPlayers[i];
            if (!p.valid || !p.alive || p.dormant) continue;
            if (gAimbot.teamCheck.load() && p.team == gLocalPlayer.team) continue;

            int boneIdx = static_cast<int>(gAimbot.targetBone.load());
            Vec3 bonePos = p.bonePos[boneIdx];

            AimMath::Angles angles = AimMath::CalcAngle(eyePos, bonePos);
            float fovDist = AimMath::GetFov(viewAngles, angles);

            if (fovDist < bestFov)
            {
                bestFov = fovDist;
                bestIdx = i;
            }
        }

        if (bestIdx < 0) return;

        // Calculate angle delta
        int boneIdx = static_cast<int>(gAimbot.targetBone.load());
        Vec3 targetPos = gPlayers[bestIdx].bonePos[boneIdx];
        Vec3 eyePos2 = gLocalPlayer.origin;
        eyePos2.z += 64.f;

        AimMath::Angles targetAng = AimMath::CalcAngle(eyePos2, targetPos);
        float dPitch = AimMath::NormalizeAngle(targetAng.pitch - viewAngles.x);
        float dYaw   = AimMath::NormalizeAngle(targetAng.yaw   - viewAngles.y);

        // Apply smoothing
        float smooth = std::max(gAimbot.smooth.load(), 1.f);
        dPitch /= smooth;
        dYaw   /= smooth;

        // Convert to pixel delta
        int pxY = static_cast<int>(AimMath::AngleToPix(dPitch));
        int pxX = static_cast<int>(AimMath::AngleToPix(dYaw));

        if (gAimbot.silentAim.load())
        {
            // Silent aim: would write view angles directly if write access were available.
            // We intentionally do not perform memory writes here.
        }

        MoveMouse(pxX, pxY);
    }
};

inline Aimbot gAimbotWorker;
