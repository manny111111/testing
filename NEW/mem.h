#pragma once
// ============================================================
//  mem.h  --  Process attach, RPM wrapper, module resolver
// ============================================================

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <cstdint>

class Memory
{
public:
    HANDLE      hProcess   = nullptr;
    DWORD       processId  = 0;
    uintptr_t   clientBase = 0;  // client.dll base

    // ── Attach to cs2.exe ────────────────────────────────────
    bool Attach()
    {
        processId = GetProcId("cs2.exe");
        if (!processId) return false;

        hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId);
        if (!hProcess || hProcess == INVALID_HANDLE_VALUE) return false;

        clientBase = GetModuleBase("client.dll");
        return clientBase != 0;
    }

    void Detach()
    {
        if (hProcess) CloseHandle(hProcess);
        hProcess   = nullptr;
        processId  = 0;
        clientBase = 0;
    }

    bool IsValid() const { return hProcess && hProcess != INVALID_HANDLE_VALUE && clientBase; }

    // ── Generic RPM ─────────────────────────────────────────
    template<typename T>
    T Read(uintptr_t address) const
    {
        T value{};
        ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address), &value, sizeof(T), nullptr);
        return value;
    }

    // ── Read null-terminated string ──────────────────────────
    std::string ReadString(uintptr_t address, size_t maxLen = 256) const
    {
        std::string buf(maxLen, '\0');
        ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address),
                          buf.data(), maxLen, nullptr);
        buf.resize(strnlen(buf.c_str(), maxLen));
        return buf;
    }

    // ── Read a block of bytes ────────────────────────────────
    bool ReadRaw(uintptr_t address, void* buffer, size_t size) const
    {
        SIZE_T bytesRead = 0;
        return ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address),
                                 buffer, size, &bytesRead) && bytesRead == size;
    }

private:
    // ── Walk process list ────────────────────────────────────
    DWORD GetProcId(const char* procName) const
    {
        DWORD pid = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32 pe{};
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe))
        {
            do {
                if (_stricmp(pe.szExeFile, procName) == 0)
                {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
        return pid;
    }

    // ── Walk module list ─────────────────────────────────────
    uintptr_t GetModuleBase(const char* modName) const
    {
        uintptr_t base = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32 me{};
        me.dwSize = sizeof(me);
        if (Module32First(snap, &me))
        {
            do {
                if (_stricmp(me.szModule, modName) == 0)
                {
                    base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                    break;
                }
            } while (Module32Next(snap, &me));
        }
        CloseHandle(snap);
        return base;
    }
};

// Global memory instance
inline Memory gMem;
