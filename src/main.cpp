#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <TM1637.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// GPIO Pins
#define CLK_PIN 14  // D5
#define DIO_PIN 12  // D6
#define SDA_PIN 4   // D2
#define SCL_PIN 5   // D1

// EEPROM settings
#define EEPROM_SIZE 512
#define TIMEZONE_ADDR 0
#define TIMEZONE_MAGIC 0xAA

// Global objects
TM1637 display(CLK_PIN, DIO_PIN);
RTC_DS1307 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);
ESP8266WebServer server(80);

// Global variables
int timezoneOffset = 0; // in hours
bool wifiConnected = false;
unsigned long lastDisplayUpdate = 0;
bool colonState = true;

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
  <h1>ESP8266 Clock Setup</h1>
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

// Function to save timezone to EEPROM
void saveTimezone(int tz) {
  EEPROM.write(TIMEZONE_ADDR, TIMEZONE_MAGIC);
  EEPROM.write(TIMEZONE_ADDR + 1, (byte)((tz + 12) & 0xFF)); // Store as 0-26 range
  EEPROM.commit();
  timezoneOffset = tz;
}

// Function to load timezone from EEPROM
void loadTimezone() {
  byte magic = EEPROM.read(TIMEZONE_ADDR);
  if (magic == TIMEZONE_MAGIC) {
    byte tzValue = EEPROM.read(TIMEZONE_ADDR + 1);
    timezoneOffset = (int)tzValue - 12; // Convert back to -12 to +14 range
  } else {
    timezoneOffset = 0; // Default to UTC
  }
}

// Function to sync time from NTP and update RTC
bool syncTimeFromNTP() {
  if (!wifiConnected) {
    Serial.println("WiFi not connected, cannot sync NTP");
    return false;
  }
  
  Serial.println("Syncing time from NTP...");
  timeClient.setTimeOffset(timezoneOffset * 3600);
  
  if (timeClient.update()) {
    unsigned long epochTime = timeClient.getEpochTime();
    
    // Convert to DateTime and set RTC
    DateTime dt = DateTime(epochTime);
    rtc.adjust(dt);
    
    Serial.print("Time synced: ");
    Serial.print(dt.hour());
    Serial.print(":");
    Serial.print(dt.minute());
    Serial.print(":");
    Serial.println(dt.second());
    
    return true;
  } else {
    Serial.println("Failed to sync time from NTP");
    return false;
  }
}

// Function to run boot animation (rotating circle)
void bootAnimation() {
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
  const int cycles = 3;  // Number of complete rotations
  const int delayMs = 80;  // Delay between animation frames

  // Buffer to hold 4 digits (all showing the same pattern)
  uint8_t buffer[4];

  // Run the animation for specified cycles
  for (int cycle = 0; cycle < cycles; cycle++) {
    for (int i = 0; i < numPatterns; i++) {
      // Fill buffer with the same pattern for all 4 digits
      for (int digit = 0; digit < 4; digit++) {
        buffer[digit] = circlePatterns[i];
      }
      display.displayRawBytes(buffer, 4);
      delay(delayMs);
    }
  }

  // Clear the display after animation
  display.clearScreen();
  delay(200);
}

// Function to display time on TM1637
void displayTime(int hour, int minute, bool showColon) {
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
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nESP8266 Clock Starting...");
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadTimezone();
  Serial.print("Loaded timezone offset: UTC");
  if (timezoneOffset >= 0) Serial.print("+");
  Serial.println(timezoneOffset);
  
  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  
  // Initialize TM1637 display
  display.begin();
  display.setBrightness(7); // 0-7 brightness level
  display.clearScreen();

  // Run boot animation
  bootAnimation();
  
  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC!");
    while (1) delay(10);
  }
  
  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running, setting default time");
    // Set to Jan 1, 2024 00:00:00 as default
    rtc.adjust(DateTime(2024, 1, 1, 0, 0, 0));
  } else {
    Serial.println("RTC is running");
  }
  
  // Initialize WiFiManager
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180); // 3 minutes timeout
  
  // Try to connect to WiFi
  Serial.println("Connecting to WiFi...");
  if (wifiManager.autoConnect("ClockSetup", "clock1234")) {
    Serial.println("Connected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
    
    // Initialize NTP client
    timeClient.begin();
    
    // Sync time from NTP
    syncTimeFromNTP();
  } else {
    Serial.println("Failed to connect to WiFi, using RTC time only");
    wifiConnected = false;
  }
  
  // Setup web server
  if (wifiConnected) {
    // Root page
    server.on("/", HTTP_GET, [](){
      server.send_P(200, "text/html", index_html);
    });
    
    // Get current time endpoint
    server.on("/getTime", HTTP_GET, [](){
      DateTime now = rtc.now();
      char timeStr[20];
      sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
      server.send(200, "text/plain", timeStr);
    });
    
    // Set timezone endpoint
    server.on("/setTimezone", HTTP_POST, [](){
      if (server.hasArg("timezone")) {
        String tzStr = server.arg("timezone");
        int tz = tzStr.toInt();
        
        if (tz >= -12 && tz <= 14) {
          saveTimezone(tz);
          Serial.print("New timezone set: UTC");
          if (tz >= 0) Serial.print("+");
          Serial.println(tz);
          
          // Sync time with new timezone
          if (syncTimeFromNTP()) {
            server.send(200, "text/html", "<html><body><h1>Timezone updated and time synced!</h1><a href='/'>Back</a></body></html>");
          } else {
            server.send(200, "text/html", "<html><body><h1>Timezone updated but NTP sync failed</h1><a href='/'>Back</a></body></html>");
          }
        } else {
          server.send(400, "text/html", "<html><body><h1>Invalid timezone</h1><a href='/'>Back</a></body></html>");
        }
      } else {
        server.send(400, "text/html", "<html><body><h1>Missing timezone parameter</h1><a href='/'>Back</a></body></html>");
      }
    });
    
    server.begin();
    Serial.println("Web server started");
  }
  
  Serial.println("Clock ready!");
}

void loop() {
  // Handle web server requests
  if (wifiConnected) {
    server.handleClient();
  }
  
  // Update display every 500ms
  if (millis() - lastDisplayUpdate >= 500) {
    lastDisplayUpdate = millis();
    
    // Get current time from RTC
    DateTime now = rtc.now();
    
    // Toggle colon state
    colonState = !colonState;
    
    // Display time
    displayTime(now.hour(), now.minute(), colonState);
  }
  
  yield();
}

