# ESP32-S3 Clock - Dual Core Architecture

This project has been rewritten for ESP32-S3 with dual-core FreeRTOS architecture and 8MB PSRAM support.

## Hardware Requirements

- **ESP32-S3 DevKit** with 8MB PSRAM
- **TM1637 4-digit 7-segment display**
- **DS1307 RTC module** (I2C)

## Pin Configuration

### TM1637 Display
- **CLK**: GPIO 12
- **DIO**: GPIO 13

### DS1307 RTC (I2C)
- **SDA**: GPIO 21
- **SCL**: GPIO 20

## Architecture Overview

### Dual Core Design

**Core 0 (CORE_WIFI)** - Network Tasks:
- WiFi connection management
- NTP time synchronization
- Web server for configuration
- Periodic time sync (every hour)

**Core 1 (CORE_DISPLAY)** - Display Tasks:
- TM1637 display control
- RTC communication
- Boot animation
- Clock display with blinking colon

### Thread Safety

The project uses FreeRTOS mutexes to ensure thread-safe access to shared resources:
- **timeMutex**: Protects RTC read/write operations
- **displayMutex**: Protects TM1637 display operations

### Key Features

1. **Boot Animation**: Rotating circle effect on startup
2. **Blinking Colon**: Updates every 500ms for clock effect
3. **WiFi Portal**: Auto-connects or creates "ClockSetup" AP
4. **Web Interface**: Configure timezone via web browser
5. **NTP Sync**: Automatic time synchronization
6. **PSRAM Support**: Optimized for 8MB PSRAM
7. **USB CDC**: Serial output over USB

## Configuration

### WiFi Setup
1. On first boot, ESP32 creates "ClockSetup" AP
2. Password: `clock1234`
3. Connect and configure your WiFi credentials

### Timezone Configuration
1. Connect to the same WiFi network
2. Open web browser to ESP32 IP address
3. Select timezone from dropdown
4. Click "Update Timezone & Sync Time"

## Web API Endpoints

- `GET /` - Configuration web interface
- `GET /getTime` - Returns current time as text
- `POST /setTimezone` - Update timezone (param: `timezone`)

## Memory Configuration

- **Flash**: 8MB
- **PSRAM**: OPI mode (Octal)
- **Flash Mode**: QIO
- **USB CDC**: Enabled on boot

## Building and Uploading

### Using CLI Tools

**macOS/Linux:**
```bash
cd mac
./clock.sh
```

**Windows:**
```cmd
cd windows
clock.bat
```

### Manual PlatformIO Commands

```bash
# Build
pio run

# Upload
pio run --target upload

# Monitor
pio device monitor --baud 115200
```

## FreeRTOS Task Details

### WiFi Task
- **Stack Size**: 8192 bytes
- **Priority**: 1
- **Core**: 0
- **Loop Delay**: 10ms

### Display Task
- **Stack Size**: 4096 bytes
- **Priority**: 1
- **Core**: 1
- **Loop Delay**: 10ms

## Storage

Uses ESP32 **Preferences** library (NVS) instead of EEPROM:
- Namespace: `clock`
- Key: `timezone`
- Type: Integer (-12 to +14)

## Troubleshooting

### PSRAM Not Found
If you see "Warning: PSRAM not found!" but your board has PSRAM:
- Check `board_build.psram_type` in `platformio.ini`
- Verify your board variant supports OPI PSRAM

### Display Not Working
- Verify pin connections (GPIO 12, 13)
- Check TM1637 power supply (3.3V or 5V)
- Some displays may not support colon feature

### RTC Not Found
- Check I2C connections (GPIO 21 SDA, GPIO 20 SCL)
- Verify DS1307 has backup battery
- Try I2C scanner sketch to detect address

### WiFi Connection Issues
- Hold reset to restart WiFi portal
- Check AP password: `clock1234`
- Portal timeout: 3 minutes

## Serial Output Example

```
========================================
ESP32-S3 Clock Starting...
Dual Core Configuration:
  Core 0: WiFi, NTP, Web Server
  Core 1: Display, RTC
========================================

PSRAM found: 8192 KB
Loaded timezone offset: UTC+0
Tasks created successfully!
WiFi Task starting on Core 0...
Display Task starting on Core 1...
Connecting to WiFi...
Connected to WiFi!
IP address: 192.168.1.100
Syncing time from NTP...
Time synced: 14:23:15
Web server started
RTC is running
Display ready!
```

## Performance Notes

- Display updates: 500ms interval
- NTP sync: Every 60 minutes
- Web server: Non-blocking
- Task yields: 10ms intervals

## Power Consumption

Dual-core configuration allows:
- Core 0: Can sleep during non-WiFi operations
- Core 1: Always active for smooth display
- Future enhancement: Dynamic power management

## License

See main project LICENSE file.
