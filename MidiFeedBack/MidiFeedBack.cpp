#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <vector>

#pragma comment(lib, "Winmm.lib")

// ============================================================================
// Voicemeeter Remote API
// ============================================================================

typedef long(__stdcall* T_VBVMR_Login)(void);
typedef long(__stdcall* T_VBVMR_Logout)(void);
typedef long(__stdcall* T_VBVMR_IsParametersDirty)(void);
typedef long(__stdcall* T_VBVMR_GetParameterFloat)(char* szParamName, float* pValue);

T_VBVMR_Login VBVMR_Login = nullptr;
T_VBVMR_Logout VBVMR_Logout = nullptr;
T_VBVMR_IsParametersDirty VBVMR_IsParametersDirty = nullptr;
T_VBVMR_GetParameterFloat VBVMR_GetParameterFloat = nullptr;

HMODULE hVoicemeeterDLL = nullptr;

bool InitializeVoicemeeterAPI() {
    // Try to find VoicemeeterRemote64.dll in Voicemeeter installation path
    const wchar_t* paths[] = {
        L"C:\\Program Files (x86)\\VB\\Voicemeeter\\VoicemeeterRemote64.dll",
        L"C:\\Program Files\\VB\\Voicemeeter\\VoicemeeterRemote64.dll"
    };

    for (const auto& path : paths) {
        hVoicemeeterDLL = LoadLibraryW(path);
        if (hVoicemeeterDLL) break;
    }

    if (!hVoicemeeterDLL) {
        return false;
    }

    VBVMR_Login = (T_VBVMR_Login)GetProcAddress(hVoicemeeterDLL, "VBVMR_Login");
    VBVMR_Logout = (T_VBVMR_Logout)GetProcAddress(hVoicemeeterDLL, "VBVMR_Logout");
    VBVMR_IsParametersDirty = (T_VBVMR_IsParametersDirty)GetProcAddress(hVoicemeeterDLL, "VBVMR_IsParametersDirty");
    VBVMR_GetParameterFloat = (T_VBVMR_GetParameterFloat)GetProcAddress(hVoicemeeterDLL, "VBVMR_GetParameterFloat");

    if (!VBVMR_Login || !VBVMR_Logout || !VBVMR_IsParametersDirty || !VBVMR_GetParameterFloat) {
        FreeLibrary(hVoicemeeterDLL);
        hVoicemeeterDLL = nullptr;
        return false;
    }

    long result = VBVMR_Login();
    if (result < 0) {
        FreeLibrary(hVoicemeeterDLL);
        hVoicemeeterDLL = nullptr;
        return false;
    }

    return true;
}

void ShutdownVoicemeeterAPI() {
    if (VBVMR_Logout) {
        VBVMR_Logout();
    }
    if (hVoicemeeterDLL) {
        FreeLibrary(hVoicemeeterDLL);
        hVoicemeeterDLL = nullptr;
    }
}

bool GetVoicemeeterMuteState(int stripIndex) {
    if (!VBVMR_GetParameterFloat) return false;

    char paramName[64];
    sprintf_s(paramName, "Strip[%d].Mute", stripIndex);

    float value = 0.0f;
    long result = VBVMR_GetParameterFloat(paramName, &value);

    if (result == 0) {
        return value >= 1.0f;  // 1.0 = muted, 0.0 = unmuted
    }
    return false;
}

// ============================================================================
// MIDI and LED Control
// ============================================================================

const std::wstring REQUIRED_OUTPUT_DEVICE = L"Arturia MiniLab mkII";

struct ButtonConfig {
    int noteNumber;
    std::string name;
    int voicemeeterStrip;  // Which Voicemeeter strip this button controls
    BYTE defaultColor;
    BYTE muteColor;
};

struct ButtonState {
    BYTE defaultColor;
    BYTE muteColor;
    std::atomic<bool> isMuted;
    std::atomic<bool> isBlinking;
    std::thread* blinkThread;

    ButtonState(BYTE defCol, BYTE mutCol)
        : defaultColor(defCol), muteColor(mutCol), isMuted(false), isBlinking(false), blinkThread(nullptr) {
    }
    ButtonState() : ButtonState(0x01, 0x01) {}

    ~ButtonState() {
        isBlinking = false;
        if (blinkThread && blinkThread->joinable()) {
            blinkThread->join();
            delete blinkThread;
        }
    }
};

std::unordered_map<int, std::unique_ptr<ButtonState>> buttonStates;
std::vector<ButtonConfig> buttonConfigs;
HMIDIOUT hMidiOut = nullptr;
std::atomic<bool> isRunning(true);

void SetButtonColor(int buttonIndex, BYTE color) {
    if (!hMidiOut) return;

    BYTE sysexMessage[] = {
        0xF0, 0x00, 0x20, 0x6B, 0x7F, 0x42, 0x02, 0x00,
        0x10, static_cast<BYTE>(0x70 + buttonIndex), color, 0xF7
    };

    MIDIHDR midiHdr;
    memset(&midiHdr, 0, sizeof(MIDIHDR));
    midiHdr.lpData = reinterpret_cast<LPSTR>(sysexMessage);
    midiHdr.dwBufferLength = sizeof(sysexMessage);
    midiHdr.dwFlags = 0;

    if (midiOutPrepareHeader(hMidiOut, &midiHdr, sizeof(MIDIHDR)) == MMSYSERR_NOERROR) {
        midiOutLongMsg(hMidiOut, &midiHdr, sizeof(MIDIHDR));
        midiOutUnprepareHeader(hMidiOut, &midiHdr, sizeof(MIDIHDR));
    }
}

void BlinkButton(int buttonIndex, BYTE color) {
    while (buttonStates[buttonIndex]->isBlinking && buttonStates[buttonIndex]->isMuted && isRunning) {
        SetButtonColor(buttonIndex, 0x00);
        Sleep(500);
        if (!buttonStates[buttonIndex]->isBlinking || !buttonStates[buttonIndex]->isMuted || !isRunning) break;
        SetButtonColor(buttonIndex, color);
        Sleep(500);
    }
}

void StartBlinking(int buttonIndex) {
    ButtonState* state = buttonStates[buttonIndex].get();

    // Stop existing blink thread if running
    state->isBlinking = false;
    if (state->blinkThread && state->blinkThread->joinable()) {
        state->blinkThread->join();
        delete state->blinkThread;
        state->blinkThread = nullptr;
    }

    state->isBlinking = true;
    state->blinkThread = new std::thread(BlinkButton, buttonIndex, state->muteColor);
}

void StopBlinking(int buttonIndex) {
    ButtonState* state = buttonStates[buttonIndex].get();
    state->isBlinking = false;

    if (state->blinkThread && state->blinkThread->joinable()) {
        state->blinkThread->join();
        delete state->blinkThread;
        state->blinkThread = nullptr;
    }

    SetButtonColor(buttonIndex, state->defaultColor);
}

void UpdateButtonFromVoicemeeter(int buttonIndex) {
    if (buttonIndex >= buttonConfigs.size()) return;

    int stripIndex = buttonConfigs[buttonIndex].voicemeeterStrip;
    bool isMuted = GetVoicemeeterMuteState(stripIndex);
    ButtonState* state = buttonStates[buttonIndex].get();

    // Only update if state changed
    if (isMuted != state->isMuted.load()) {
        state->isMuted = isMuted;

        if (isMuted) {
            StartBlinking(buttonIndex);
        } else {
            StopBlinking(buttonIndex);
        }
    }
}

void VoicemeeterPollingThread() {
    while (isRunning) {
        // Check if any parameters changed
        if (VBVMR_IsParametersDirty && VBVMR_IsParametersDirty() > 0) {
            // Update all buttons from Voicemeeter state
            for (int i = 0; i < 8; ++i) {
                UpdateButtonFromVoicemeeter(i);
            }
        }
        Sleep(50);  // Poll every 50ms
    }
}

void InitializeButtonConfigs() {
    // Map each button to a Voicemeeter strip (0-7 for Potato's 8 strips)
    buttonConfigs = {
        {48, "Microphone",      0, 0x01, 0x01},  // Strip 0
        {49, "PlayStation",     1, 0x10, 0x10},  // Strip 1
        {50, "Spotify",         2, 0x04, 0x04},  // Strip 2
        {51, "Chrome",          3, 0x05, 0x05},  // Strip 3
        {52, "Console 2",       4, 0x14, 0x14},  // Strip 4
        {53, "Default Channel", 5, 0x7F, 0x7F},  // Strip 5
        {54, "Game Channel",    6, 0x7F, 0x7F},  // Strip 6
        {55, "VOIP Channel",    7, 0x7F, 0x7F}   // Strip 7
    };
}

bool OpenMidiOutputDevice() {
    UINT numOutputDevices = midiOutGetNumDevs();

    for (UINT i = 0; i < numOutputDevices; ++i) {
        MIDIOUTCAPS caps;
        if (midiOutGetDevCaps(i, &caps, sizeof(MIDIOUTCAPS)) == MMSYSERR_NOERROR) {
            if (std::wstring(caps.szPname) == REQUIRED_OUTPUT_DEVICE) {
                if (midiOutOpen(&hMidiOut, i, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
                    return true;
                }
            }
        }
    }

    return false;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitializeButtonConfigs();

    // Initialize button states with colors from config
    for (int i = 0; i < 8; ++i) {
        buttonStates[i] = std::make_unique<ButtonState>(
            buttonConfigs[i].defaultColor,
            buttonConfigs[i].muteColor
        );
    }

    // Initialize Voicemeeter API
    if (!InitializeVoicemeeterAPI()) {
        return 1;
    }

    // Open MIDI output device
    if (!OpenMidiOutputDevice()) {
        ShutdownVoicemeeterAPI();
        return 1;
    }

    // Set initial LED colors
    for (int i = 0; i < 8; ++i) {
        SetButtonColor(i, buttonStates[i]->defaultColor);
        Sleep(50);
    }

    // Do initial sync with Voicemeeter state
    for (int i = 0; i < 8; ++i) {
        UpdateButtonFromVoicemeeter(i);
    }

    // Start Voicemeeter polling thread
    std::thread pollThread(VoicemeeterPollingThread);

    // Run indefinitely in the background
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    isRunning = false;

    // Wait for polling thread to finish
    if (pollThread.joinable()) {
        pollThread.join();
    }

    // Cleanup
    if (hMidiOut) {
        midiOutClose(hMidiOut);
    }

    ShutdownVoicemeeterAPI();

    return 0;
}
