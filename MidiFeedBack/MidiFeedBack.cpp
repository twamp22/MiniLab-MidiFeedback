#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <vector>

#pragma comment(lib, "Winmm.lib")

const std::wstring REQUIRED_INPUT_DEVICE = L"Arturia loopMIDI Port";
const std::wstring REQUIRED_OUTPUT_DEVICE = L"Arturia MiniLab mkII";

struct ButtonConfig {
    int noteNumber;
    std::string name;
    BYTE defaultColor;
    BYTE muteColor;
};

struct ButtonState {
    BYTE color;
    std::atomic<bool> isMuted;
    std::atomic<bool> isBlinking;
    std::thread* blinkThread;

    ButtonState(BYTE col, bool muted, bool blinking)
        : color(col), isMuted(muted), isBlinking(blinking), blinkThread(nullptr) {
    }
    ButtonState() : ButtonState(0x01, false, false) {}

    ~ButtonState() {
        if (blinkThread && blinkThread->joinable()) {
            isBlinking = false;
            blinkThread->join();
            delete blinkThread;
        }
    }
};

std::unordered_map<int, std::unique_ptr<ButtonState>> buttonStates;
std::vector<ButtonConfig> buttonConfigs;
HMIDIOUT hMidiOut = nullptr;
std::atomic<bool> isRunning(true);

// Console output functions removed for headless operation

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
    while (buttonStates[buttonIndex]->isBlinking && isRunning) {
        SetButtonColor(buttonIndex, 0x00);
        Sleep(500);
        if (!buttonStates[buttonIndex]->isBlinking || !isRunning) break;
        SetButtonColor(buttonIndex, color);
        Sleep(500);
    }
}

void HandleButtonPress(int buttonIndex, BYTE data2) {
    if (buttonIndex >= buttonConfigs.size()) {
        return;
    }

    ButtonConfig& config = buttonConfigs[buttonIndex];

    if (data2 == 127 && !buttonStates[buttonIndex]->isMuted) {
        buttonStates[buttonIndex]->isMuted = true;
        buttonStates[buttonIndex]->isBlinking = true;

        if (buttonStates[buttonIndex]->blinkThread && buttonStates[buttonIndex]->blinkThread->joinable()) {
            buttonStates[buttonIndex]->blinkThread->join();
            delete buttonStates[buttonIndex]->blinkThread;
        }
        buttonStates[buttonIndex]->blinkThread = new std::thread(BlinkButton, buttonIndex, config.muteColor);
        buttonStates[buttonIndex]->blinkThread->detach();
    }
    else if (data2 == 0 && buttonStates[buttonIndex]->isMuted) {
        buttonStates[buttonIndex]->isMuted = false;
        buttonStates[buttonIndex]->isBlinking = false;
        Sleep(100);
        SetButtonColor(buttonIndex, config.defaultColor);
    }
}

void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (wMsg == MIM_DATA) {
        BYTE status = dwParam1 & 0xFF;
        BYTE data1 = (dwParam1 >> 8) & 0xFF;
        BYTE data2 = (dwParam1 >> 16) & 0xFF;

        if (status == 153 && data1 >= 48 && data1 <= 55) {
            int buttonIndex = data1 - 48;
            HandleButtonPress(buttonIndex, data2);
        }
    }
}

void InitializeButtonConfigs() {
    buttonConfigs = {
        {48, "Microphone", 0x01, 0x01},
        {49, "PlayStation", 0x10, 0x10},
        {50, "Spotify", 0x04, 0x04},
        {51, "Chrome", 0x05, 0x05},
        {52, "Console 2", 0x14, 0x14},
        {53, "Default Channel", 0x7F, 0x7F},
        {54, "Game Channel", 0x7F, 0x7F},
        {55, "VOIP Channel", 0x7F, 0x7F}
    };
}

bool InitializeMidiDevices() {
    bool hasLoopMIDI = false, hasMiniLab = false;
    UINT numInputDevices = midiInGetNumDevs();

    for (UINT i = 0; i < numInputDevices; ++i) {
        MIDIINCAPS caps;
        if (midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS)) == MMSYSERR_NOERROR) {
            if (std::wstring(caps.szPname) == REQUIRED_INPUT_DEVICE) {
                hasLoopMIDI = true;
            }
            if (std::wstring(caps.szPname) == REQUIRED_OUTPUT_DEVICE) {
                hasMiniLab = true;
            }
        }
    }

    if (!hasLoopMIDI || !hasMiniLab) {
        return false;
    }

    return true;
}

bool OpenMidiInputDevice() {
    HMIDIIN hMidiIn = nullptr;
    UINT numInputDevices = midiInGetNumDevs();

    for (UINT i = 0; i < numInputDevices; ++i) {
        MIDIINCAPS caps;
        if (midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS)) == MMSYSERR_NOERROR) {
            if (std::wstring(caps.szPname) == REQUIRED_INPUT_DEVICE) {
                if (midiInOpen(&hMidiIn, i, (DWORD_PTR)MidiInProc, 0, CALLBACK_FUNCTION) == MMSYSERR_NOERROR) {
                    midiInStart(hMidiIn);
                    return true;
                }
            }
        }
    }

    return false;
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

    std::vector<BYTE> defaultColors = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x06, 0x06 };
    for (int i = 0; i < 8; ++i) {
        buttonStates[i] = std::make_unique<ButtonState>(defaultColors[i], false, false);
    }

    if (!InitializeMidiDevices()) {
        return 1;
    }

    if (!OpenMidiInputDevice() || !OpenMidiOutputDevice()) {
        return 1;
    }

    for (int i = 0; i < 8; ++i) {
        SetButtonColor(i, buttonStates[i]->color);
        Sleep(50);
    }

    // Run indefinitely in the background
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    isRunning = false;

    if (hMidiOut) {
        midiOutClose(hMidiOut);
    }

    return 0;
}