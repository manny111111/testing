# CS2 External Overlay

A fully external CS2 overlay built in C++17 / MSVC x64 using only `ReadProcessMemory`.  
No DLL injection. No driver. Pure Win32 + DX11 + ImGui.

---

## Features

| Feature | Details |
|---|---|
| **ESP Boxes** | Corner boxes OR full rectangles (toggle in config) |
| **Health Bars** | Left-side vertical bar, red→yellow→green gradient |
| **Skeleton** | 16-bone line skeleton drawn with world-to-screen |
| **Player Names** | Read from `CBasePlayerController::m_iszPlayerName` |
| **Weapon Names** | Read from `CCSWeaponBaseVData::m_szName` via active weapon handle |
| **Bomb Timer** | HUD panel (top-center) + world-space marker on planted bomb |
| **Grenade Trails** | Line segments drawn from trajectory point cache |
| **Aimbot** | Dedicated 2000 Hz thread, SendInput, FOV filter, smooth |
| **Silent Aim** | Stub ready; requires `PROCESS_VM_WRITE` if you add it |
| **Team Check** | Independent toggle for ESP and aimbot |
| **Hotkey picker** | Click "Change hotkey" in config and press any key |
| **Window tracking** | Overlay auto-repositions/resizes when CS2 moves |

---

## Dependencies – you must supply these

### ImGui (Dear ImGui)

Download from: https://github.com/ocornut/imgui/releases (v1.90+)

Copy the following files into an `imgui/` folder at the project root:
```
imgui/
  imgui.h
  imgui.cpp
  imgui_internal.h
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
  imgui_impl_dx11.h
  imgui_impl_dx11.cpp
  imgui_impl_win32.h
  imgui_impl_win32.cpp
  imconfig.h
  imstb_rectpack.h
  imstb_textedit.h
  imstb_truetype.h
```

---

## Project Layout

```
NEW/
├── Offsets/
│   ├── offsets.hpp          ← a2x dumper globals
│   ├── client_dll.hpp       ← a2x dumper schemas
│   └── buttons.hpp
│
├── imgui/                   ← YOU supply this (see above)
│
├── offsets.h                ← flat alias namespace
├── mem.h                    ← RPM wrapper
├── game.h                   ← entity system, W2S, view matrix
├── esp.h                    ← all ESP rendering
├── aimbot.h                 ← aimbot + SendInput
├── main.cpp                 ← overlay window + DX11 + render loop
└── CS2Overlay.vcxproj       ← MSVC x64 project
```

---

## Building

1. Open `CS2Overlay.vcxproj` in **Visual Studio 2022** (or 2019 with v143 toolset).
2. Ensure `imgui/` folder exists with all files listed above.
3. Set configuration to **Release | x64**.
4. Build → the `.exe` is in `x64/Release/`.

### Manual compiler command (no VS IDE)

```bat
cl /std:c++17 /EHsc /O2 /W3 /MT /DWIN32 /D_WINDOWS /DNDEBUG ^
   /I. /I.\imgui ^
   main.cpp ^
   imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp ^
   imgui\imgui_widgets.cpp ^
   imgui\imgui_impl_dx11.cpp imgui\imgui_impl_win32.cpp ^
   /link d3d11.lib dxgi.lib dwmapi.lib user32.lib gdi32.lib ^
   /SUBSYSTEM:WINDOWS /OUT:CS2Overlay.exe
```

---

## Usage

1. Launch **CS2** first.
2. Run `CS2Overlay.exe` as **Administrator** (needed for `OpenProcess` + `ReadProcessMemory`).
3. The overlay attaches automatically and follows the CS2 window.
4. Press **INSERT** to open/close the config menu.
5. Press **END** to exit.

---

## Key Controls

| Key | Action |
|---|---|
| `INSERT` | Toggle config/menu window |
| `END` | Exit overlay |
| Aimbot hotkey | Hold to aim (default: LMB) |

---

## Notes

- **Silent aim** writes view angles directly — requires `PROCESS_VM_WRITE` added to `OpenProcess` call and the commented-out write in `aimbot.h::Tick()`.  
- Bone indices assume the standard CS2 27-bone rig. If Valve changes the skeleton, update `BoneID` in `game.h`.
- `dwPlantedC4` is a **pointer-to-pointer** in CS2 — `game.h` dereferences twice.
- The bomb timer uses `ImGui::GetTime()` as a clock proxy. For pixel-perfect timing, read `dwGlobalVars` and add `m_flCurTime` (offset from engine2.dll).
