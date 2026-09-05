// ============================================================
//  main.cpp  --  DX11 transparent overlay, ImGui init,
//                render loop, game window tracking
//
//  Compile:  MSVC x64
//  Link:     d3d11.lib dxgi.lib dwmapi.lib
// ============================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <cstdio>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>

// ImGui
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

// Our modules
#include "mem.h"
#include "offsets.h"
#include "game.h"
#include "esp.h"
#include "aimbot.h"

// ── Forward declarations ──────────────────────────────────────
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ── Globals ───────────────────────────────────────────────────
static ID3D11Device*            g_pd3dDevice           = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext    = nullptr;
static IDXGISwapChain*          g_pSwapChain           = nullptr;
static ID3D11RenderTargetView*  g_pRTV                 = nullptr;
static HWND                     g_hOverlay             = nullptr;
static HWND                     g_hTarget              = nullptr; // cs2 window

static bool                     g_overlayRunning       = true;
static bool                     g_showConfig           = true;    // toggle with INSERT
static std::atomic<bool>        g_gameAttached         { false };

static float                    g_screenW              = 1920.f;
static float                    g_screenH              = 1080.f;

// ── Hotkey name table ─────────────────────────────────────────
static const struct { int vk; const char* name; } kHotkeyMap[] = {
    { VK_LBUTTON,  "LMB"    }, { VK_RBUTTON,  "RMB"    },
    { VK_MBUTTON,  "MMB"    }, { VK_XBUTTON1, "M4"     },
    { VK_XBUTTON2, "M5"     }, { VK_LSHIFT,   "L.Shift"},
    { VK_LCONTROL, "L.Ctrl" }, { VK_LMENU,    "L.Alt"  },
    { VK_CAPITAL,  "CapsLk" }, { 0, nullptr }
};

static const char* HotkeyName(int vk)
{
    for (int i = 0; kHotkeyMap[i].name; i++)
        if (kHotkeyMap[i].vk == vk) return kHotkeyMap[i].name;
    static char buf[8];
    snprintf(buf, sizeof(buf), "0x%02X", vk);
    return buf;
}

// ── DX11 helpers ─────────────────────────────────────────────
static bool CreateDeviceAndSwapChain(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hWnd;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, levels, 1, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel,
        &g_pd3dDeviceContext);

    if (FAILED(hr)) return false;

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (!pBackBuffer) return false;
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRTV);
    pBackBuffer->Release();
    return true;
}

static void CleanupDevice()
{
    if (g_pRTV)              { g_pRTV->Release();              g_pRTV             = nullptr; }
    if (g_pSwapChain)        { g_pSwapChain->Release();        g_pSwapChain       = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)        { g_pd3dDevice->Release();        g_pd3dDevice       = nullptr; }
}

// ── Window procedure ──────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_DESTROY:
        g_overlayRunning = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

// ── Create transparent topmost overlay window ─────────────────
static HWND CreateOverlayWindow(HINSTANCE hInstance)
{
    WNDCLASSEXA wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "CS2Overlay";
    RegisterClassExA(&wc);

    // Get desktop size
    RECT desktop{};
    GetWindowRect(GetDesktopWindow(), &desktop);
    g_screenW = static_cast<float>(desktop.right  - desktop.left);
    g_screenH = static_cast<float>(desktop.bottom - desktop.top);

    HWND hWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        "CS2Overlay", "CS2Overlay",
        WS_POPUP,
        desktop.left, desktop.top,
        static_cast<int>(g_screenW), static_cast<int>(g_screenH),
        nullptr, nullptr, hInstance, nullptr);

    // Make window fully transparent to mouse (click-through)
    SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 255, LWA_ALPHA);

    // DWM transparency (no title bar, no chrome)
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hWnd, &margins);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    return hWnd;
}

// ── When config menu is open, re-enable mouse input ──────────
static void SetOverlayInputMode(bool inputEnabled)
{
    LONG exStyle = GetWindowLongA(g_hOverlay, GWL_EXSTYLE);
    if (inputEnabled)
        exStyle &= ~WS_EX_TRANSPARENT;
    else
        exStyle |=  WS_EX_TRANSPARENT;
    SetWindowLongA(g_hOverlay, GWL_EXSTYLE, exStyle);
}

// ── Track and snap to game window ────────────────────────────
static void SnapToGameWindow()
{
    if (!g_hTarget || !IsWindow(g_hTarget)) return;

    RECT r{};
    GetWindowRect(g_hTarget, &r);
    int wx = r.left, wy = r.top;
    int ww = r.right - r.left;
    int wh = r.bottom - r.top;

    if (ww < 1 || wh < 1) return;

    g_screenW = static_cast<float>(ww);
    g_screenH = static_cast<float>(wh);

    SetWindowPos(g_hOverlay, HWND_TOPMOST, wx, wy, ww, wh, SWP_NOACTIVATE);

    // Resize swap chain if needed
    if (g_pRTV) { g_pRTV->Release(); g_pRTV = nullptr; }
    g_pSwapChain->ResizeBuffers(0, ww, wh, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* pBB = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBB));
    if (pBB) { g_pd3dDevice->CreateRenderTargetView(pBB, nullptr, &g_pRTV); pBB->Release(); }
}

// ── Config/menu rendering ─────────────────────────────────────
static void RenderConfigWindow()
{
    if (!g_showConfig) return;

    ImGui::SetNextWindowSize({ 420.f, 540.f }, ImGuiCond_Once);
    ImGui::SetNextWindowPos({ 40.f, 40.f }, ImGuiCond_Once);

    ImGui::Begin("CS2 Overlay  [INSERT to toggle]",
                 &g_showConfig,
                 ImGuiWindowFlags_NoCollapse);

    // ── Status ────────────────────────────────────────────────
    ImGui::SeparatorText("Status");
    if (g_gameAttached)
        ImGui::TextColored({ 0.2f, 1.f, 0.3f, 1.f }, "Game attached");
    else
        ImGui::TextColored({ 1.f, 0.3f, 0.2f, 1.f }, "Waiting for cs2.exe...");

    // ── ESP ───────────────────────────────────────────────────
    ImGui::SeparatorText("ESP");
    ImGui::Checkbox("Enable ESP",        &gEsp.enabled);
    ImGui::Checkbox("Team check (ESP)",  &gEsp.teamCheck);
    ImGui::Checkbox("Full box",          &gEsp.showBoxFull);
    ImGui::Checkbox("Health bar",        &gEsp.showHealthBar);
    ImGui::Checkbox("Skeleton",          &gEsp.showSkeleton);
    ImGui::Checkbox("Player name",       &gEsp.showName);
    ImGui::Checkbox("Weapon name",       &gEsp.showWeapon);
    ImGui::Checkbox("Bomb timer",        &gEsp.showBomb);
    ImGui::Checkbox("Grenade trails",    &gEsp.showGrenadeTrail);

    ImGui::SliderFloat("Box thickness",  &gEsp.boxThickness, 0.5f, 4.f, "%.1f");
    ImGui::SliderFloat("Corner length",  &gEsp.cornerLen,    0.1f, 0.5f, "%.2f");

    ImGui::ColorEdit4("Enemy color",     &gEsp.colorEnemy.x);
    ImGui::ColorEdit4("Team color",      &gEsp.colorTeam.x);
    ImGui::ColorEdit4("Bomb color",      &gEsp.colorBomb.x);
    ImGui::ColorEdit4("Grenade color",   &gEsp.colorGrenade.x);

    // ── Aimbot ────────────────────────────────────────────────
    ImGui::SeparatorText("Aimbot");

    bool abEnabled = gAimbot.enabled.load();
    if (ImGui::Checkbox("Enable Aimbot",   &abEnabled)) gAimbot.enabled = abEnabled;

    bool abTeam = gAimbot.teamCheck.load();
    if (ImGui::Checkbox("Team check (AB)", &abTeam)) gAimbot.teamCheck = abTeam;

    bool abSilent = gAimbot.silentAim.load();
    if (ImGui::Checkbox("Silent aim",      &abSilent)) gAimbot.silentAim = abSilent;

    // FOV slider
    float abFov = gAimbot.fov.load();
    if (ImGui::SliderFloat("FOV##AB", &abFov, 0.5f, 45.f, "%.1f deg"))
        gAimbot.fov = abFov;

    // Smooth slider
    float abSmooth = gAimbot.smooth.load();
    if (ImGui::SliderFloat("Smooth##AB", &abSmooth, 1.f, 20.f, "%.1f"))
        gAimbot.smooth = abSmooth;

    // Target bone
    static const char* boneNames[] = { "Head", "Neck" };
    static int boneChoice = 0;
    if (ImGui::Combo("Target bone", &boneChoice, boneNames, 2))
        gAimbot.targetBone = (boneChoice == 0) ? (float)BoneID::head : (float)BoneID::neck_0;

    // Hotkey picker
    ImGui::Text("Hotkey: %s", HotkeyName(gAimbot.hotkey.load()));
    ImGui::SameLine();
    static bool waitingForKey = false;
    if (waitingForKey)
    {
        ImGui::TextColored({ 1.f, 1.f, 0.f, 1.f }, "[Press any key...]");
        for (int vk = 1; vk < 0xFE; vk++)
        {
            if (vk == VK_ESCAPE) { waitingForKey = false; break; }
            if (GetAsyncKeyState(vk) & 0x8000)
            {
                gAimbot.hotkey = vk;
                waitingForKey  = false;
                break;
            }
        }
    }
    else
    {
        if (ImGui::Button("Change hotkey"))
            waitingForKey = true;
    }

    ImGui::End();
}

// ── Game attach thread ────────────────────────────────────────
static void AttachThread()
{
    while (g_overlayRunning)
    {
        if (!gMem.IsValid())
        {
            g_gameAttached = false;
            gMem.Detach();
            if (gMem.Attach())
            {
                g_gameAttached = true;
                g_hTarget = FindWindowA(nullptr, "Counter-Strike 2");
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

// ── WinMain ───────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Create overlay window
    g_hOverlay = CreateOverlayWindow(hInstance);
    if (!g_hOverlay) return 1;

    // Init DX11
    if (!CreateDeviceAndSwapChain(g_hOverlay))
    {
        MessageBoxA(nullptr, "DX11 init failed", "Error", MB_ICONERROR);
        return 1;
    }

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename  = "cs2_overlay.ini";

    // Dark theme with slight transparency
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 8.f;
    style.FrameRounding     = 4.f;
    style.GrabRounding      = 4.f;
    style.WindowBorderSize  = 1.f;
    style.Alpha             = 0.92f;

    // Accent colors
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_TitleBgActive]  = { 0.15f, 0.15f, 0.20f, 1.f };
    colors[ImGuiCol_Header]         = { 0.20f, 0.40f, 0.80f, 0.4f };
    colors[ImGuiCol_HeaderHovered]  = { 0.20f, 0.60f, 1.00f, 0.6f };
    colors[ImGuiCol_CheckMark]      = { 0.30f, 0.90f, 0.30f, 1.0f };
    colors[ImGuiCol_SliderGrab]     = { 0.20f, 0.70f, 1.00f, 1.0f };
    colors[ImGuiCol_Button]         = { 0.20f, 0.40f, 0.80f, 0.6f };
    colors[ImGuiCol_ButtonHovered]  = { 0.30f, 0.60f, 1.00f, 0.8f };

    ImGui_ImplWin32_Init(g_hOverlay);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load a nicer font if available
    ImFontConfig fc;
    fc.OversampleH = 2; fc.OversampleV = 2;
    // Try loading Windows Segoe UI — silently falls back to default if missing
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 15.f, &fc);
    io.Fonts->Build();
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_CreateDeviceObjects();

    // Start game attach thread
    std::thread attachThread(AttachThread);

    // Start aimbot thread
    gAimbotWorker.Start();

    // Main loop
    LARGE_INTEGER freq{}, prev{}, cur{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    int  targetFps     = 144;
    RECT lastGameRect  = {};
    bool lastConfigVis = g_showConfig;

    MSG msg{};
    while (g_overlayRunning)
    {
        // Process window messages
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (msg.message == WM_QUIT)
            {
                g_overlayRunning = false;
                break;
            }
        }

        // INSERT toggle
        static bool insertPrev = false;
        bool insertNow = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (insertNow && !insertPrev)
        {
            g_showConfig = !g_showConfig;
            SetOverlayInputMode(g_showConfig);
        }
        insertPrev = insertNow;

        // END to quit
        if (GetAsyncKeyState(VK_END) & 0x8000)
            g_overlayRunning = false;

        // Track game window
        if (g_hTarget && IsWindow(g_hTarget))
        {
            RECT r{};
            GetWindowRect(g_hTarget, &r);
            if (memcmp(&r, &lastGameRect, sizeof(RECT)) != 0)
            {
                lastGameRect = r;
                SnapToGameWindow();
            }
        }

        // Update game data
        if (g_gameAttached)
            gUpdater.Update(g_screenW, g_screenH);

        // ── Render ────────────────────────────────────────────
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ESP
        gESP.Render(g_screenW, g_screenH);

        // Config window
        RenderConfigWindow();

        ImGui::Render();

        // Clear with transparent black
        float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pRTV, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_pRTV, clearColor);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0); // vsync

        // Cap frame rate
        QueryPerformanceCounter(&cur);
        double elapsed = (double)(cur.QuadPart - prev.QuadPart) / (double)freq.QuadPart;
        double target  = 1.0 / targetFps;
        if (elapsed < target)
        {
            DWORD sleepMs = (DWORD)((target - elapsed) * 1000.0);
            if (sleepMs > 0) Sleep(sleepMs);
        }
        prev = cur;
    }

    // Shutdown
    gAimbotWorker.Stop();
    if (attachThread.joinable()) attachThread.join();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDevice();
    DestroyWindow(g_hOverlay);
    UnregisterClassA("CS2Overlay", hInstance);
    return 0;
}
