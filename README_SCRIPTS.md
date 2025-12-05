# ESP8266 Clock - CLI Tools

Interactive command-line tools for building, uploading, and monitoring the ESP8266 Clock project using PlatformIO.

## Prerequisites

- [PlatformIO CLI](https://platformio.org/install/cli) must be installed and available in PATH

## Usage

### macOS/Linux

```bash
cd mac
./clock.sh
```

### Windows

```cmd
cd windows
clock.bat
```

## Features

Both scripts provide an interactive menu with arrow key navigation:

1. **Build** - Compile the project
2. **Upload** - Upload firmware to the device
3. **Monitor** - Open serial monitor (115200 baud)
4. **Build + Upload + Monitor** - Complete workflow
5. **Clean** - Clean build files
6. **Device Info** - List connected devices
7. **Exit** - Close the tool

## Navigation

- **Arrow Keys** (↑/↓) - Navigate menu
- **Enter** - Select option
- **Number Keys** (1-7) - Quick select

## Notes

- The serial monitor runs at 115200 baud rate
- Press `Ctrl+C` to exit the monitor
- Make sure your ESP8266 device is connected before uploading
