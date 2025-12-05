#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <TM1637.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <qrcode.h>

// GPIO Pins for ESP32-S3
#define CLK_PIN 12  // TM1637 CLK
#define DIO_PIN 13  // TM1637 DIO
#define SDA_PIN 21  // I2C SDA
#define SCL_PIN 20  // I2C SCL

// FreeRTOS Core definitions
#define CORE_WIFI 0      // Core 0: WiFi, NTP, Web Server
#define CORE_DISPLAY 1   // Core 1: Display, RTC, Animation

// Preferences (ESP32 alternative to EEPROM)
Preferences preferences;

// Global objects
TM1637 display(CLK_PIN, DIO_PIN);
RTC_DS1307 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);
WebServer server(80);

// Global variables
int timezoneOffset = 0; // in hours
volatile bool wifiConnected = false;
volatile bool syncRequested = false;
volatile bool timeReady = false; // True when time is synced and ready to display
volatile unsigned long lastDisplayUpdate = 0;
volatile bool colonState = true;

// FreeRTOS task handles
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;

// Mutex for shared resources
SemaphoreHandle_t timeMutex;
SemaphoreHandle_t displayMutex;

// HTML page for timezone selection
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 20px; }
    h1 { color: #333; }
    select { padding: 10px; font-size: 16px; margin: 10px; }
    button { padding: 10px 30px; font-size: 16px; background-color: #4CAF50; color: white; border: none; cursor: pointer; }
    button:hover { background-color: #45a049; }
    .info { margin: 20px; padding: 10px; background-color: #f0f0f0; }
  </style>
</head>
<body>
  <h1>ESP32-S3 Clock Setup</h1>
  <div class="info">
    <p>Current Time: <span id="time">Loading...</span></p>
    <p>WiFi Status: <span id="wifi">Connected</span></p>
  </div>
  <form action="/setTimezone" method="POST">
    <label for="timezone">Select Timezone:</label><br>
    <select name="timezone" id="timezone">
      <option value="-12">UTC-12:00</option>
      <option value="-11">UTC-11:00</option>
      <option value="-10">UTC-10:00</option>
      <option value="-9">UTC-09:00</option>
      <option value="-8">UTC-08:00</option>
      <option value="-7">UTC-07:00</option>
      <option value="-6">UTC-06:00</option>
      <option value="-5">UTC-05:00</option>
      <option value="-4">UTC-04:00</option>
      <option value="-3">UTC-03:00</option>
      <option value="-2">UTC-02:00</option>
      <option value="-1">UTC-01:00</option>
      <option value="0" selected>UTC+00:00</option>
      <option value="1">UTC+01:00</option>
      <option value="2">UTC+02:00 (EET)</option>
      <option value="3">UTC+03:00 (EEST)</option>
      <option value="4">UTC+04:00</option>
      <option value="5">UTC+05:00</option>
      <option value="6">UTC+06:00</option>
      <option value="7">UTC+07:00</option>
      <option value="8">UTC+08:00</option>
      <option value="9">UTC+09:00</option>
      <option value="10">UTC+10:00</option>
      <option value="11">UTC+11:00</option>
      <option value="12">UTC+12:00</option>
      <option value="13">UTC+13:00</option>
      <option value="14">UTC+14:00</option>
    </select><br><br>
    <button type="submit">Update Timezone & Sync Time</button>
  </form>
  <script>
    setInterval(function() {
      fetch('/getTime').then(r => r.text()).then(t => {
        document.getElementById('time').innerText = t;
      });
    }, 1000);
  </script>
</body>
</html>
)rawliteral";

// Function to save timezone to Preferences
void saveTimezone(int tz) {
  Serial.print("[CONFIG] Saving timezone to preferences: UTC");
  if (tz >= 0) Serial.print("+");
  Serial.println(tz);

  if (preferences.begin("clock", false)) {
    preferences.putInt("timezone", tz);
    preferences.end();
    timezoneOffset = tz;
    Serial.println("[CONFIG] ✓ Timezone saved successfully");
  } else {
    Serial.println("[CONFIG] ✗ Failed to save timezone - preferences could not be initialized");
    Serial.println("[CONFIG] → Using in-memory value only");
    timezoneOffset = tz; // Still update in memory
  }
}

// Function to load timezone from Preferences
void loadTimezone() {
  Serial.println("[CONFIG] Loading timezone from preferences...");

  // Try to open in read-write mode first to create namespace if it doesn't exist
  if (preferences.begin("clock", false)) {
    // Check if timezone key exists
    if (preferences.isKey("timezone")) {
      timezoneOffset = preferences.getInt("timezone", 0);
      Serial.print("[CONFIG] ✓ Timezone loaded: UTC");
      if (timezoneOffset >= 0) Serial.print("+");
      Serial.println(timezoneOffset);
    } else {
      // First time - initialize with default
      Serial.println("[CONFIG] → First time setup - no saved timezone");
      timezoneOffset = 0;
      preferences.putInt("timezone", timezoneOffset);
      Serial.println("[CONFIG] ✓ Initialized with default: UTC+0");
    }
    preferences.end();
  } else {
    Serial.println("[CONFIG] ✗ Failed to open preferences namespace");
    Serial.println("[CONFIG] → Using default: UTC+0");
    timezoneOffset = 0;
  }
}

// Function to print WiFi QR code to serial console
void printWiFiQR(const char* ssid, const char* password) {
  // Create WiFi QR code string in format: WIFI:T:WPA;S:ssid;P:password;;
  String qrData = "WIFI:T:WPA;S:";
  qrData += ssid;
  qrData += ";P:";
  qrData += password;
  qrData += ";;";

  // Create QR Code
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)]; // Version 3 QR code
  qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, qrData.c_str());

  // Print QR code to serial with border
  Serial.println("\n╔════════════════════════════════════════════════╗");
  Serial.println("║       WiFi AP - Scan to Connect               ║");
  Serial.println("╠════════════════════════════════════════════════╣");
  Serial.print("║ SSID: ");
  Serial.print(ssid);
  for (int i = strlen(ssid); i < 38; i++) Serial.print(" ");
  Serial.println("║");
  Serial.print("║ Password: ");
  Serial.print(password);
  for (int i = strlen(password); i < 32; i++) Serial.print(" ");
  Serial.println("║");
  Serial.println("╠════════════════════════════════════════════════╣");

  // Print QR code
  for (uint8_t y = 0; y < qrcode.size; y++) {
    Serial.print("║ ");
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        Serial.print("██");
      } else {
        Serial.print("  ");
      }
    }
    Serial.println(" ║");
  }

  Serial.println("╚════════════════════════════════════════════════╝");
  Serial.println();
}

// Function to sync time from NTP and update RTC
bool syncTimeFromNTP() {
  if (!wifiConnected) {
    Serial.println("[NTP] ✗ Cannot sync - WiFi not connected");
    return false;
  }

  Serial.println();
  Serial.println("[NTP] ═══════════════════════════════════════");
  Serial.println("[NTP] Starting NTP time synchronization...");
  Serial.print("[NTP] → NTP Server: ");
  Serial.println("pool.ntp.org");
  Serial.print("[NTP] → Timezone Offset: UTC");
  if (timezoneOffset >= 0) Serial.print("+");
  Serial.print(timezoneOffset);
  Serial.println(" hours");

  timeClient.setTimeOffset(timezoneOffset * 3600);

  Serial.println("[NTP] → Sending time request...");
  if (timeClient.update()) {
    unsigned long epochTime = timeClient.getEpochTime();
    Serial.print("[NTP] → Received epoch time: ");
    Serial.println(epochTime);

    // Acquire mutex before updating RTC
    if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
      // Convert to DateTime and set RTC
      DateTime dt = DateTime(epochTime);
      rtc.adjust(dt);

      Serial.println("[NTP] → Updating RTC module...");
      Serial.print("[NTP] ✓ Time synchronized: ");
      char timeStr[20];
      sprintf(timeStr, "%02d:%02d:%02d", dt.hour(), dt.minute(), dt.second());
      Serial.println(timeStr);
      Serial.print("[NTP] → Date: ");
      sprintf(timeStr, "%04d-%02d-%02d", dt.year(), dt.month(), dt.day());
      Serial.println(timeStr);

      xSemaphoreGive(timeMutex);

      // Signal that time is ready to display
      timeReady = true;

      Serial.println("[NTP] ═══════════════════════════════════════");
      Serial.println();
      return true;
    } else {
      Serial.println("[NTP] ✗ Failed to acquire mutex");
    }
  } else {
    Serial.println("[NTP] ✗ Failed to receive time from NTP server");
    Serial.println("[NTP] → This may be due to network issues");
    Serial.println("[NTP] ═══════════════════════════════════════");
    Serial.println();
  }
  return false;
}

// Function to show one frame of the spinning animation
// Returns the next pattern index
int showSpinningFrame(int patternIndex) {
  // Segment patterns for circle animation (rotating around the display)
  // Segment mapping: A=0x01, B=0x02, C=0x04, D=0x08, E=0x10, F=0x20, G=0x40
  const uint8_t circlePatterns[] = {
    0b00000001,  // A - top
    0b00000010,  // B - top-right
    0b00000100,  // C - bottom-right
    0b00001000,  // D - bottom
    0b00010000,  // E - bottom-left
    0b00100000   // F - top-left
  };

  const int numPatterns = 6;

  // Buffer to hold 4 digits (all showing the same pattern)
  uint8_t buffer[4];

  // Fill buffer with the same pattern for all 4 digits
  for (int digit = 0; digit < 4; digit++) {
    buffer[digit] = circlePatterns[patternIndex];
  }
  display.displayRawBytes(buffer, 4);

  // Return next pattern index (wrap around)
  return (patternIndex + 1) % numPatterns;
}

// Function to display time on TM1637
void displayTime(int hour, int minute, bool showColon) {
  // Acquire display mutex
  if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
    // Format: HHMM with leading zeros
    String timeStr = "";
    if (hour < 10) timeStr += "0";
    timeStr += String(hour);
    if (minute < 10) timeStr += "0";
    timeStr += String(minute);

    // Display the time
    display.display(timeStr);

    // Control the colon based on showColon parameter
    if (showColon) {
      display.colonOn();
    } else {
      display.colonOff();
    }

    xSemaphoreGive(displayMutex);
  }
}

// WiFi Task - Runs on Core 0
void wifiTask(void *parameter) {
  Serial.println("[WiFi] Task starting on Core 0...");
  Serial.println("[WiFi] Initializing WiFiManager...");

  // Initialize WiFiManager
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180); // 3 minutes timeout
  Serial.println("[WiFi] Configuration portal timeout: 180 seconds");

  // Set callback for when entering AP mode
  wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
    Serial.println();
    Serial.println("╔════════════════════════════════════════════════╗");
    Serial.println("║     WiFi Configuration Portal Started          ║");
    Serial.println("╚════════════════════════════════════════════════╝");
    Serial.println();
    Serial.println("[WiFi] No saved credentials or connection failed");
    Serial.println("[WiFi] Starting Access Point mode...");

    // Print QR code for easy connection
    printWiFiQR("ClockSetup", "clock1234");

    Serial.println("[WiFi] → Connect to the WiFi AP to configure");
    Serial.print("[WiFi] → AP SSID: ClockSetup\n");
    Serial.print("[WiFi] → AP Password: clock1234\n");
    Serial.print("[WiFi] → AP IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("[WiFi] → Portal will timeout after 3 minutes");
    Serial.println();
  });

  // Try to connect to WiFi
  Serial.println();
  Serial.println("[WiFi] Attempting to connect to WiFi...");
  Serial.println("[WiFi] Checking for saved credentials...");

  if (wifiManager.autoConnect("ClockSetup", "clock1234")) {
    Serial.println();
    Serial.println("[WiFi] ✓ Successfully connected to WiFi!");
    Serial.print("[WiFi] → SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("[WiFi] → IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] → Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("[WiFi] → Subnet: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("[WiFi] → DNS: ");
    Serial.println(WiFi.dnsIP());
    Serial.print("[WiFi] → Signal Strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println();
    wifiConnected = true;

    // Initialize NTP client
    Serial.println("[NTP] Initializing NTP client...");
    timeClient.begin();
    Serial.println("[NTP] ✓ NTP client initialized");

    // Sync time from NTP
    syncTimeFromNTP();
  } else {
    Serial.println();
    Serial.println("[WiFi] ✗ Failed to connect to WiFi");
    Serial.println("[WiFi] Portal timeout or connection failed");
    Serial.println("[WiFi] Continuing with RTC time only...");
    Serial.println();
    wifiConnected = false;

    // Even without WiFi, signal that we should display the RTC time
    timeReady = true;
  }

  // Setup web server
  if (wifiConnected) {
    Serial.println("[WebServer] Initializing web server...");

    // Root page
    server.on("/", HTTP_GET, []() {
      Serial.println("[WebServer] GET / - Serving configuration page");
      server.send_P(200, "text/html", index_html);
    });

    // Get current time endpoint
    server.on("/getTime", HTTP_GET, []() {
      if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
        DateTime now = rtc.now();
        char timeStr[20];
        sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        xSemaphoreGive(timeMutex);
        server.send(200, "text/plain", timeStr);
      }
    });

    // Set timezone endpoint
    server.on("/setTimezone", HTTP_POST, []() {
      Serial.println("[WebServer] POST /setTimezone - Timezone change request");
      if (server.hasArg("timezone")) {
        String tzStr = server.arg("timezone");
        int tz = tzStr.toInt();

        if (tz >= -12 && tz <= 14) {
          Serial.println();
          Serial.println("[CONFIG] ═══════════════════════════════════════");
          Serial.println("[CONFIG] Timezone Change Requested");
          Serial.print("[CONFIG] → Old timezone: UTC");
          if (timezoneOffset >= 0) Serial.print("+");
          Serial.println(timezoneOffset);
          Serial.print("[CONFIG] → New timezone: UTC");
          if (tz >= 0) Serial.print("+");
          Serial.println(tz);

          saveTimezone(tz);
          Serial.println("[CONFIG] ✓ Timezone saved to preferences");

          // Request sync
          syncRequested = true;
          Serial.println("[CONFIG] → Requesting time sync with new timezone...");
          Serial.println("[CONFIG] ═══════════════════════════════════════");

          server.send(200, "text/html", "<html><body><h1>Timezone updated! Syncing time...</h1><a href='/'>Back</a></body></html>");
        } else {
          Serial.print("[WebServer] ✗ Invalid timezone value: ");
          Serial.println(tz);
          server.send(400, "text/html", "<html><body><h1>Invalid timezone</h1><a href='/'>Back</a></body></html>");
        }
      } else {
        Serial.println("[WebServer] ✗ Missing timezone parameter");
        server.send(400, "text/html", "<html><body><h1>Missing timezone parameter</h1><a href='/'>Back</a></body></html>");
      }
    });

    server.begin();
    Serial.println("[WebServer] ✓ Web server started");
    Serial.print("[WebServer] → Access at: http://");
    Serial.println(WiFi.localIP());
    Serial.println();
  }

  // Main WiFi task loop
  unsigned long loopCount = 0;
  while (true) {
    if (wifiConnected) {
      server.handleClient();

      // Handle sync requests
      if (syncRequested) {
        syncRequested = false;
        Serial.println("[WiFi] Processing time sync request...");
        syncTimeFromNTP();
      }

      // Periodic NTP sync every hour
      static unsigned long lastSync = 0;
      if (millis() - lastSync > 3600000) {
        lastSync = millis();
        Serial.println();
        Serial.println("[NTP] ═══════════════════════════════════════");
        Serial.println("[NTP] Periodic sync triggered (hourly)");
        Serial.println("[NTP] ═══════════════════════════════════════");
        syncTimeFromNTP();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
    loopCount++;
  }
}

// Display Task - Runs on Core 1
void displayTask(void *parameter) {
  Serial.println("[Display] Task starting on Core 1...");
  Serial.println();

  // Initialize I2C
  Serial.println("[I2C] Initializing I2C bus...");
  Serial.print("[I2C] → SDA Pin: ");
  Serial.println(SDA_PIN);
  Serial.print("[I2C] → SCL Pin: ");
  Serial.println(SCL_PIN);
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("[I2C] ✓ I2C initialized");
  Serial.println();

  // Initialize TM1637 display
  Serial.println("[Display] Initializing TM1637 display...");
  display.begin();
  display.setBrightness(7); // 0-7 brightness level
  Serial.println("[Display] → Brightness level: 7/7");
  display.clearScreen();
  Serial.println("[Display] ✓ TM1637 display initialized");
  Serial.println();

  // Initialize RTC
  Serial.println("[RTC] Initializing DS1307 RTC module...");
  if (!rtc.begin()) {
    Serial.println("[RTC] ✗ ERROR: Couldn't find RTC module!");
    Serial.println("[RTC] → Check I2C connections");
    Serial.println("[RTC] → Expected address: 0x68");
    while (1) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  Serial.println("[RTC] ✓ RTC module found");

  if (!rtc.isrunning()) {
    Serial.println("[RTC] ⚠ RTC is NOT running");
    Serial.println("[RTC] → Setting default time: 2024-01-01 00:00:00");
    // Set to Jan 1, 2024 00:00:00 as default
    rtc.adjust(DateTime(2024, 1, 1, 0, 0, 0));
    Serial.println("[RTC] ✓ Default time set");
  } else {
    Serial.println("[RTC] ✓ RTC is running");
    if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
      DateTime now = rtc.now();
      char timeStr[20];
      sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
              now.year(), now.month(), now.day(),
              now.hour(), now.minute(), now.second());
      Serial.print("[RTC] → Current RTC time: ");
      Serial.println(timeStr);
      xSemaphoreGive(timeMutex);
    }
  }

  Serial.println();
  Serial.println("[Display] ═══════════════════════════════════════");
  Serial.println("[Display] All hardware initialized successfully!");
  Serial.println("[Display] ═══════════════════════════════════════");
  Serial.println();

  // Animation state
  int animationFrame = 0;
  unsigned long lastAnimationUpdate = 0;
  const int animationDelay = 80; // milliseconds between animation frames

  // Keep showing spinning animation until time is ready
  Serial.println("[Display] Starting loading animation...");
  Serial.println("[Display] → Waiting for WiFi connection and time sync...");
  while (!timeReady) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastAnimationUpdate >= animationDelay) {
      lastAnimationUpdate = currentMillis;

      // Acquire display mutex and show next animation frame
      if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
        animationFrame = showSpinningFrame(animationFrame);
        xSemaphoreGive(displayMutex);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
  }

  // Clear display and prepare for time display
  Serial.println();
  Serial.println("[Display] ═══════════════════════════════════════");
  Serial.println("[Display] Time ready! Starting clock display...");
  Serial.println("[Display] ═══════════════════════════════════════");
  Serial.println();
  if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
    display.clearScreen();
    xSemaphoreGive(displayMutex);
  }
  delay(200);

  // Main display task loop - show time
  while (true) {
    // Update display every 500ms
    unsigned long currentMillis = millis();
    if (currentMillis - lastDisplayUpdate >= 500) {
      lastDisplayUpdate = currentMillis;

      // Get current time from RTC
      if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
        DateTime now = rtc.now();
        xSemaphoreGive(timeMutex);

        // Toggle colon state
        colonState = !colonState;

        // Display time
        displayTime(now.hour(), now.minute(), colonState);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
  }
}

void setup() {
  // Initialize UART Serial
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize

  // Very first diagnostic - if you don't see this, there's a hardware/boot issue
  Serial.println("\n\n>>> ESP32-S3 Boot OK <<<");
  Serial.flush();
  delay(100);

  Serial.println("\n\n╔════════════════════════════════════════════════╗");
  Serial.println("║         ESP32-S3 Clock - Starting Up           ║");
  Serial.println("╚════════════════════════════════════════════════╝");
  Serial.println();
  Serial.flush();

  // System Information
  Serial.println("[SYSTEM] Hardware Information:");
  Serial.flush();
  Serial.print("  → Chip Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("  → CPU Cores: ");
  Serial.println(ESP.getChipCores());
  Serial.print("  → CPU Frequency: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");
  Serial.print("  → Flash Size: ");
  Serial.print(ESP.getFlashChipSize() / 1024 / 1024);
  Serial.println(" MB");
  Serial.print("  → Free Heap: ");
  Serial.print(ESP.getFreeHeap() / 1024);
  Serial.println(" KB");
  Serial.flush();

  // Check PSRAM
  Serial.println("[SYSTEM] Checking PSRAM...");
  Serial.flush();
  if (psramFound()) {
    Serial.print("  → PSRAM: ");
    Serial.print(ESP.getPsramSize() / 1024);
    Serial.print(" KB (Free: ");
    Serial.print(ESP.getFreePsram() / 1024);
    Serial.println(" KB)");
  } else {
    Serial.println("  → PSRAM: NOT FOUND");
  }
  Serial.flush();

  Serial.println();
  Serial.println("[SYSTEM] FreeRTOS Configuration:");
  Serial.println("  → Core 0: WiFi, NTP, Web Server");
  Serial.println("  → Core 1: Display, RTC, Animation");
  Serial.println();
  Serial.flush();

  // Initialize NVS (Non-Volatile Storage)
  Serial.println("[NVS] Initializing Non-Volatile Storage...");
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    Serial.println("[NVS] ⚠ Partition needs to be erased");
    Serial.println("[NVS] → Erasing NVS flash...");
    err = nvs_flash_erase();
    if (err != ESP_OK) {
      Serial.print("[NVS] ✗ Erase failed with error: 0x");
      Serial.println(err, HEX);
    } else {
      Serial.println("[NVS] ✓ Erase successful");
    }
    Serial.println("[NVS] → Re-initializing...");
    err = nvs_flash_init();
  }

  if (err == ESP_OK) {
    Serial.println("[NVS] ✓ Initialized successfully");

    // Verify NVS is working by getting stats
    nvs_stats_t nvs_stats;
    if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK) {
      Serial.print("[NVS] → Used entries: ");
      Serial.print(nvs_stats.used_entries);
      Serial.print(" / ");
      Serial.println(nvs_stats.total_entries);
      Serial.print("[NVS] → Free entries: ");
      Serial.println(nvs_stats.free_entries);
      Serial.print("[NVS] → Namespace count: ");
      Serial.println(nvs_stats.namespace_count);
    }
  } else {
    Serial.print("[NVS] ✗ Initialization failed with error: 0x");
    Serial.println(err, HEX);
    Serial.println("[NVS] → Preferences will not work!");
  }

  // Load timezone from Preferences
  Serial.println();
  Serial.println("[CONFIG] Loading configuration...");
  loadTimezone();
  Serial.print("[CONFIG] Timezone: UTC");
  if (timezoneOffset >= 0) Serial.print("+");
  Serial.println(timezoneOffset);

  // Create mutexes
  Serial.println();
  Serial.println("[RTOS] Creating synchronization primitives...");
  timeMutex = xSemaphoreCreateMutex();
  displayMutex = xSemaphoreCreateMutex();

  if (timeMutex == NULL || displayMutex == NULL) {
    Serial.println("[RTOS] ✗ ERROR: Failed to create mutexes!");
    while (1) delay(10);
  }
  Serial.println("[RTOS] ✓ Mutexes created successfully");

  // Create WiFi task on Core 0
  Serial.println();
  Serial.println("[RTOS] Creating tasks...");
  Serial.println("[RTOS] → Creating WiFi Task on Core 0...");
  xTaskCreatePinnedToCore(
      wifiTask,         // Task function
      "WiFi Task",      // Task name
      8192,             // Stack size (bytes)
      NULL,             // Task parameters
      1,                // Priority
      &wifiTaskHandle,  // Task handle
      CORE_WIFI         // Core 0
  );

  // Create Display task on Core 1
  Serial.println("[RTOS] → Creating Display Task on Core 1...");
  xTaskCreatePinnedToCore(
      displayTask,         // Task function
      "Display Task",      // Task name
      4096,                // Stack size (bytes)
      NULL,                // Task parameters
      1,                   // Priority
      &displayTaskHandle,  // Task handle
      CORE_DISPLAY         // Core 1
  );

  Serial.println("[RTOS] ✓ All tasks created successfully!");
  Serial.println();
  Serial.println("╔════════════════════════════════════════════════╗");
  Serial.println("║          Initialization Complete!              ║");
  Serial.println("╚════════════════════════════════════════════════╝");
  Serial.println();
}

void loop() {
  // Empty loop - all work is done in FreeRTOS tasks
  vTaskDelay(pdMS_TO_TICKS(1000));
}

