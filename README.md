# Arturia MiniLab mkII RGB LED Feedback System

A headless Windows application that provides real-time RGB LED visual feedback on the Arturia MiniLab mkII MIDI controller based on VoiceMeeter audio routing states.

## Features

- **Silent Background Operation** - Runs headlessly without any console window
- **Real-Time LED Control** - Updates button colors instantly based on channel states
- **Visual Mute Feedback** - Buttons blink at 500ms intervals when channels are muted
- **8-Channel Support** - Controls 8 programmable buttons on the MiniLab mkII
- **Color-Coded Channels** - Each audio source has a unique LED color
- **Zero Configuration** - Automatically detects MIDI devices on startup
- **Lightweight** - Minimal resource usage (~177KB executable)

## Requirements

### Hardware
- **Arturia MiniLab mkII** MIDI controller
- Windows PC with available USB ports

### Software
- **Windows 10/11** (x64)
- **VoiceMeeter** (Standard, Banana, or Potato) - [Download here](https://vb-audio.com/Voicemeeter/)
- **loopMIDI** virtual MIDI port driver - [Download here](https://www.tobias-erichsen.de/software/loopmidi.html)

## Installation

### 1. Install Required Software

1. **Install VoiceMeeter** (if not already installed)
   - Download from https://vb-audio.com/Voicemeeter/
   - Follow the installation wizard
   - Restart your computer after installation

2. **Install loopMIDI**
   - Download from https://www.tobias-erichsen.de/software/loopmidi.html
   - Install the application
   - Launch loopMIDI and create a new virtual port named: **`Arturia loopMIDI Port`**
   - Keep loopMIDI running in the background

### 2. Configure VoiceMeeter MIDI Output

1. Open VoiceMeeter
2. Go to **Menu → MIDI Mapping**
3. Set the MIDI Output device to: **`Arturia loopMIDI Port`**
4. Configure button mappings to send Note On/Off messages on notes 48-55 (C3-G#3)

### 3. Connect Your MiniLab mkII

1. Connect the Arturia MiniLab mkII via USB
2. Ensure Windows recognizes the device (check Device Manager if needed)
3. The device should appear as **"Arturia MiniLab mkII"** in MIDI device lists

### 4. Run the Application

1. Download the latest release from the [Releases](../../releases) page
2. Extract `MidiFeedBack.exe` to a folder of your choice
3. Double-click `MidiFeedBack.exe` to start
4. The application runs silently in the background (no window appears)

**To verify it's running:**
- Open Task Manager (Ctrl+Shift+Esc)
- Look for `MidiFeedBack.exe` in the Processes tab

**To stop the application:**
- Open Task Manager and end the `MidiFeedBack.exe` process

## Button Configuration

The application controls the 8 bottom buttons (notes 48-55) on the MiniLab mkII:

| Button | MIDI Note | Channel Name     | LED Color | Hex Code |
|--------|-----------|------------------|-----------|----------|
| 1      | 48 (C3)   | Microphone       | Red       | 0x01     |
| 2      | 49 (C#3)  | PlayStation      | Green     | 0x10     |
| 3      | 50 (D3)   | Spotify          | Cyan      | 0x04     |
| 4      | 51 (D#3)  | Chrome           | Blue      | 0x05     |
| 5      | 52 (E3)   | Console 2        | Pink      | 0x14     |
| 6      | 53 (F3)   | Default Channel  | White     | 0x7F     |
| 7      | 54 (F#3)  | Game Channel     | White     | 0x7F     |
| 8      | 55 (G3)   | VOIP Channel     | White     | 0x7F     |

### LED Behavior

- **Active State**: Button shows its default color
- **Muted State**: Button blinks on/off every 500ms
- **Trigger**: VoiceMeeter sends MIDI Note On (127) to mute, Note Off (0) to unmute

## How It Works

```
VoiceMeeter Audio Mixer
         ↓
  MIDI Messages (Notes 48-55)
         ↓
  loopMIDI Virtual Port
         ↓
  MidiFeedBack Application
         ↓
  Arturia MiniLab mkII LEDs
```

1. VoiceMeeter monitors audio channel states (mute/unmute)
2. When a channel state changes, VoiceMeeter sends a MIDI message through loopMIDI
3. MidiFeedBack receives the MIDI message and interprets the button state
4. MidiFeedBack sends proprietary SysEx commands to the MiniLab mkII
5. The corresponding button's RGB LED updates color or begins blinking

## Building from Source

### Prerequisites

- **Visual Studio 2022** (Community Edition or higher)
- **Windows 10/11 SDK**
- **MSVC v143 toolset**

### Build Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/twamp22/MiniLab-MidiFeedback.git
   cd MiniLab-MidiFeedback
   ```

2. Open `MidiFeedBack.sln` in Visual Studio 2022

3. Select your build configuration:
   - **Debug** (x64) - For development with debug symbols
   - **Release** (x64) - Optimized for production use

4. Build the solution:
   - Press `Ctrl+Shift+B` or
   - Go to **Build → Build Solution**

5. The executable will be generated in:
   - Debug: `x64\Debug\MidiFeedBack.exe`
   - Release: `x64\Release\MidiFeedBack.exe`

### Build via Command Line

```bash
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" MidiFeedBack.sln -p:Configuration=Release -p:Platform=x64
```

## Customization

To customize button colors or channel names, edit `MidiFeedBack.cpp`:

```cpp
void InitializeButtonConfigs() {
    buttonConfigs = {
        {48, "Your Channel Name", 0x01, 0x01},  // Button 1
        // ... modify as needed
    };
}
```

**Available LED color codes:**
- `0x01` - Red
- `0x04` - Cyan
- `0x05` - Blue
- `0x10` - Green
- `0x14` - Pink
- `0x7F` - White
- `0x00` - Off

After making changes, rebuild the project.

## Troubleshooting

### Application doesn't start or immediately closes

- **Check MIDI devices**: Ensure both "Arturia loopMIDI Port" and "Arturia MiniLab mkII" are visible in Windows Device Manager
- **Verify loopMIDI**: Open loopMIDI and confirm the virtual port "Arturia loopMIDI Port" exists and is active
- **Check Task Manager**: The app runs silently - verify it's not already running

### LEDs don't update

- **VoiceMeeter MIDI Output**: Verify VoiceMeeter's MIDI output is set to "Arturia loopMIDI Port"
- **MIDI Mapping**: Ensure VoiceMeeter buttons are mapped to send Note On/Off on notes 48-55
- **USB Connection**: Try reconnecting the MiniLab mkII

### Wrong buttons light up

- Check that VoiceMeeter MIDI mappings use MIDI Channel 10 (status byte 153/0x99)
- Verify note numbers match the configuration (48-55 for buttons 1-8)

### Application won't close

- Open Task Manager (Ctrl+Shift+Esc)
- Find `MidiFeedBack.exe` under Processes
- Right-click and select "End Task"

## Technical Details

- **Language**: C++17
- **API**: Win32 API with Windows Multimedia Extensions (MME)
- **Subsystem**: Windows (headless, no console)
- **Threading**: Multi-threaded with atomic operations for LED blinking
- **MIDI Protocol**: Arturia proprietary SysEx messages for RGB LED control

### SysEx Message Format

```
F0 00 20 6B 7F 42 02 00 10 [button] [color] F7
```

- `F0` - SysEx start
- `00 20 6B` - Arturia manufacturer ID
- `7F 42` - Device ID
- `02 00 10` - Command header
- `[button]` - Button index (0x70-0x77)
- `[color]` - RGB color code
- `F7` - SysEx end

## Auto-Start on Windows Boot (Optional)

To run MidiFeedBack automatically when Windows starts:

1. Press `Win+R` and type: `shell:startup`
2. Create a shortcut to `MidiFeedBack.exe` in this folder
3. The application will start silently on every boot

## License

This project is provided as-is for personal and educational use.

## Acknowledgments

- Built with [Claude Code](https://claude.com/claude-code)
- Arturia for the MiniLab mkII hardware
- VB-Audio for VoiceMeeter
- Tobias Erichsen for loopMIDI

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## Support

For issues, questions, or feature requests, please open an issue on the [GitHub Issues](../../issues) page.
