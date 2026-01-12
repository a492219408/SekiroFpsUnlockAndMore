// SekiroCameraFix - DLL for ModEngine injection
// Disables camera auto-rotate on movement in Sekiro: Shadows Die Twice
// 
// Usage: Load via ModEngine by placing this DLL in the mods folder

#include <windows.h>
#include <psapi.h>
#include <vector>
#include <cstdint>

#pragma comment(lib, "psapi.lib")

// Maximum relative jump range for x64 JMP rel32 instruction
// The rel32 offset is a signed 32-bit value, so the range is -2GB to +2GB
// We use 0x70000000 (~1.87GB) to leave a safety margin
constexpr intptr_t MAX_RELATIVE_JUMP_RANGE = 0x70000000;

// Initialization retry settings
constexpr int INIT_DELAY_MS = 1000;      // Initial delay before first attempt
constexpr int INIT_MAX_RETRIES = 30;     // Maximum number of retry attempts
constexpr int INIT_RETRY_DELAY_MS = 500; // Delay between retry attempts

// Pattern definitions from original GameData.cs
// Camera adjustment patterns for disabling auto-rotate on movement

// Controls camera pitch
// 000000014073AF86 | 0F29A5 70080000 | movaps xmmword ptr ss:[rbp+870],xmm4
const char* PATTERN_CAMADJUST_PITCH = "0F 29 ?? ?? ?? 00 00 0F 29 ?? ?? ?? 00 00 0F 29 ?? ?? ?? 00 00 EB ?? F3";
const int INJECT_CAMADJUST_PITCH_OVERWRITE_LENGTH = 7;
const uint8_t INJECT_CAMADJUST_PITCH_SHELLCODE[] = {
    0x0F, 0x28, 0xA6, 0x70, 0x01, 0x00, 0x00,   // movaps xmm4,xmmword ptr ds:[rsi+170]
    0x0F, 0x29, 0xA5, 0x70, 0x08, 0x00, 0x00    // movaps xmmword ptr ss:[rbp+870],xmm4
};

// Controls automatic camera yaw adjust on move on Z-axis
// 000000014073AFB1 | F3:0F1186 74010000 | movss dword ptr ds:[rsi+174],xmm0
const char* PATTERN_CAMADJUST_YAW_Z = "E8 ?? ?? ?? ?? F3 ?? ?? ?? ?? ?? 00 00 80 ?? ?? ?? 00 00 00 0F 84";
const int PATTERN_CAMADJUST_YAW_Z_OFFSET = 5;
const int INJECT_CAMADJUST_YAW_Z_OVERWRITE_LENGTH = 8;
const uint8_t INJECT_CAMADJUST_YAW_Z_SHELLCODE[] = {
    0xF3, 0x0F, 0x10, 0x86, 0x74, 0x01, 0x00, 0x00, // movss xmm0,dword ptr ds:[rsi+174]
    0xF3, 0x0F, 0x11, 0x86, 0x74, 0x01, 0x00, 0x00  // movss dword ptr ds:[rsi+174],xmm0
};

// Controls automatic camera pitch adjust on move on XY-axis
// 000000014073B4D6 | F3:0F1000 | movss xmm0,dword ptr ds:[rax]
const char* PATTERN_CAMADJUST_PITCH_XY = "F3 ?? ?? ?? F3 ?? ?? ?? 70 01 00 00 F3 ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 0F";
const int INJECT_CAMADJUST_PITCH_XY_OVERWRITE_LENGTH = 12;
const uint8_t INJECT_CAMADJUST_PITCH_XY_SHELLCODE[] = {
    0xF3, 0x0F, 0x10, 0x86, 0x70, 0x01, 0x00, 0x00, // movss xmm0,dword ptr ds:[rsi+170]
    0xF3, 0x0F, 0x11, 0x00,                         // movss dword ptr ds:[rax],xmm0
    0xF3, 0x0F, 0x10, 0x00,                         // movss xmm0,dword ptr ds:[rax]
    0xF3, 0x0F, 0x11, 0x86, 0x70, 0x01, 0x00, 0x00  // movss dword ptr ds:[rsi+170],xmm0
};

// Controls automatic camera yaw adjust on move on XY-axis
// 000000014073B5C9 | F3:0F1186 74010000 | movss dword ptr ds:[rsi+174],xmm0
const char* PATTERN_CAMADJUST_YAW_XY = "E8 ?? ?? ?? ?? F3 0F 11 86 ?? ?? 00 00 E9";
const int PATTERN_CAMADJUST_YAW_XY_OFFSET = 5;
const int INJECT_CAMADJUST_YAW_XY_OVERWRITE_LENGTH = 8;
const uint8_t INJECT_CAMADJUST_YAW_XY_SHELLCODE[] = {
    0xF3, 0x0F, 0x10, 0x86, 0x74, 0x01, 0x00, 0x00, // movss xmm0,dword ptr ds:[rsi+174]
    0xF3, 0x0F, 0x11, 0x86, 0x74, 0x01, 0x00, 0x00  // movss dword ptr ds:[rsi+174],xmm0
};

// Helper function to parse pattern string into bytes and mask
bool ParsePattern(const char* pattern, std::vector<uint8_t>& bytes, std::vector<bool>& mask) {
    bytes.clear();
    mask.clear();
    
    const char* p = pattern;
    while (*p) {
        // Skip spaces
        while (*p == ' ') p++;
        if (!*p) break;
        
        if (p[0] == '?' && p[1] == '?') {
            bytes.push_back(0);
            mask.push_back(false);
            p += 2;
        } else {
            char hex[3] = { p[0], p[1], 0 };
            bytes.push_back((uint8_t)strtol(hex, nullptr, 16));
            mask.push_back(true);
            p += 2;
        }
    }
    return !bytes.empty();
}

// Pattern scanning function
uintptr_t FindPattern(uintptr_t baseAddress, size_t moduleSize, const char* pattern) {
    std::vector<uint8_t> patternBytes;
    std::vector<bool> patternMask;
    
    if (!ParsePattern(pattern, patternBytes, patternMask)) {
        return 0;
    }
    
    uint8_t* base = (uint8_t*)baseAddress;
    size_t patternSize = patternBytes.size();
    
    for (size_t i = 0; i < moduleSize - patternSize; i++) {
        bool found = true;
        for (size_t j = 0; j < patternSize; j++) {
            if (patternMask[j] && base[i + j] != patternBytes[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return baseAddress + i;
        }
    }
    
    return 0;
}

// Allocate memory near an address for code cave
void* AllocateNearAddress(void* targetAddr, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    
    uintptr_t target = (uintptr_t)targetAddr;
    uintptr_t minAddr = target - MAX_RELATIVE_JUMP_RANGE;
    uintptr_t maxAddr = target + MAX_RELATIVE_JUMP_RANGE;
    
    if (minAddr < (uintptr_t)si.lpMinimumApplicationAddress) {
        minAddr = (uintptr_t)si.lpMinimumApplicationAddress;
    }
    if (maxAddr > (uintptr_t)si.lpMaximumApplicationAddress) {
        maxAddr = (uintptr_t)si.lpMaximumApplicationAddress;
    }
    
    // Align to allocation granularity
    minAddr = (minAddr + si.dwAllocationGranularity - 1) & ~((uintptr_t)si.dwAllocationGranularity - 1);
    
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t addr = minAddr;
    
    while (addr < maxAddr) {
        if (VirtualQuery((void*)addr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            if (mbi.State == MEM_FREE && mbi.RegionSize >= size) {
                // Allocate as read-write first, will change to execute after writing
                void* allocated = VirtualAlloc((void*)mbi.BaseAddress, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                if (allocated) {
                    return allocated;
                }
            }
            addr = (uintptr_t)mbi.BaseAddress + si.dwAllocationGranularity;
        } else {
            break;
        }
    }
    
    return nullptr;
}

// Create and activate a code cave
bool CreateCodeCave(uintptr_t instructionAddr, int overwriteLength, const uint8_t* shellcode, size_t shellcodeSize) {
    // Calculate total cave size: shellcode + jump back (5 bytes)
    size_t caveSize = shellcodeSize + 5;
    
    // Allocate memory for the code cave
    void* caveAddr = AllocateNearAddress((void*)instructionAddr, caveSize);
    if (!caveAddr) {
        return false;
    }
    
    // Calculate relative jump offset from cave back to original code
    intptr_t jumpBackOffset = (intptr_t)(instructionAddr + overwriteLength) - (intptr_t)((uintptr_t)caveAddr + shellcodeSize) - 5;
    
    // Build cave code: shellcode + jmp back
    std::vector<uint8_t> caveCode(shellcodeSize + 5);
    memcpy(caveCode.data(), shellcode, shellcodeSize);
    caveCode[shellcodeSize] = 0xE9; // JMP rel32
    *(int32_t*)&caveCode[shellcodeSize + 1] = (int32_t)jumpBackOffset;
    
    // Write cave code (memory is already PAGE_READWRITE from allocation)
    memcpy(caveAddr, caveCode.data(), caveCode.size());
    
    // Change protection to execute-read (no write) for security
    DWORD oldProtect;
    VirtualProtect(caveAddr, caveSize, PAGE_EXECUTE_READ, &oldProtect);
    
    // Calculate relative jump offset from instruction to cave
    intptr_t jumpToCaveOffset = (intptr_t)caveAddr - (intptr_t)instructionAddr - 5;
    
    // Build jump instruction with NOPs for remaining bytes
    std::vector<uint8_t> jumpInstr(overwriteLength);
    jumpInstr[0] = 0xE9; // JMP rel32
    *(int32_t*)&jumpInstr[1] = (int32_t)jumpToCaveOffset;
    for (int i = 5; i < overwriteLength; i++) {
        jumpInstr[i] = 0x90; // NOP
    }
    
    // Write jump instruction to original location
    VirtualProtect((void*)instructionAddr, overwriteLength, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)instructionAddr, jumpInstr.data(), jumpInstr.size());
    VirtualProtect((void*)instructionAddr, overwriteLength, oldProtect, &oldProtect);
    
    return true;
}

// Main injection function
void ApplyCameraFix() {
    // Initial delay to let the game start loading
    Sleep(INIT_DELAY_MS);
    
    // Get module info with retry logic
    HMODULE hModule = nullptr;
    MODULEINFO moduleInfo = {0};
    
    for (int retry = 0; retry < INIT_MAX_RETRIES; retry++) {
        hModule = GetModuleHandle(nullptr);
        if (hModule && GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo))) {
            // Verify the module is fully loaded by checking for a reasonable size
            if (moduleInfo.SizeOfImage > 0x1000000) { // At least 16MB (Sekiro is much larger)
                break;
            }
        }
        Sleep(INIT_RETRY_DELAY_MS);
    }
    
    if (!hModule || moduleInfo.SizeOfImage == 0) {
        return;
    }
    
    uintptr_t baseAddress = (uintptr_t)hModule;
    size_t moduleSize = moduleInfo.SizeOfImage;
    
    // Find all camera adjustment patterns and apply code caves
    
    // 1. Camera pitch adjustment
    uintptr_t pitchAddr = FindPattern(baseAddress, moduleSize, PATTERN_CAMADJUST_PITCH);
    if (pitchAddr) {
        CreateCodeCave(pitchAddr, INJECT_CAMADJUST_PITCH_OVERWRITE_LENGTH, 
                       INJECT_CAMADJUST_PITCH_SHELLCODE, sizeof(INJECT_CAMADJUST_PITCH_SHELLCODE));
    }
    
    // 2. Camera yaw Z-axis adjustment
    uintptr_t yawZAddr = FindPattern(baseAddress, moduleSize, PATTERN_CAMADJUST_YAW_Z);
    if (yawZAddr) {
        yawZAddr += PATTERN_CAMADJUST_YAW_Z_OFFSET;
        CreateCodeCave(yawZAddr, INJECT_CAMADJUST_YAW_Z_OVERWRITE_LENGTH,
                       INJECT_CAMADJUST_YAW_Z_SHELLCODE, sizeof(INJECT_CAMADJUST_YAW_Z_SHELLCODE));
    }
    
    // 3. Camera pitch XY-axis adjustment (may break controller input)
    // Note: This is commented out by default as it can cause issues with controllers
    // Uncomment if using mouse input only
    /*
    uintptr_t pitchXYAddr = FindPattern(baseAddress, moduleSize, PATTERN_CAMADJUST_PITCH_XY);
    if (pitchXYAddr) {
        CreateCodeCave(pitchXYAddr, INJECT_CAMADJUST_PITCH_XY_OVERWRITE_LENGTH,
                       INJECT_CAMADJUST_PITCH_XY_SHELLCODE, sizeof(INJECT_CAMADJUST_PITCH_XY_SHELLCODE));
    }
    */
    
    // 4. Camera yaw XY-axis adjustment
    uintptr_t yawXYAddr = FindPattern(baseAddress, moduleSize, PATTERN_CAMADJUST_YAW_XY);
    if (yawXYAddr) {
        yawXYAddr += PATTERN_CAMADJUST_YAW_XY_OFFSET;
        CreateCodeCave(yawXYAddr, INJECT_CAMADJUST_YAW_XY_OVERWRITE_LENGTH,
                       INJECT_CAMADJUST_YAW_XY_SHELLCODE, sizeof(INJECT_CAMADJUST_YAW_XY_SHELLCODE));
    }
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        // Create a new thread to apply the camera fix
        // The thread handle is closed immediately as we don't need to track it
        // The thread will run to completion on its own
        {
            HANDLE hThread = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
                ApplyCameraFix();
                return 0;
            }, nullptr, 0, nullptr);
            if (hThread) {
                CloseHandle(hThread);
            }
        }
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
