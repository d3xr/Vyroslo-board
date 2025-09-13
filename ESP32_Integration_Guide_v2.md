# ESP32 Greenhouse IoT Integration Guide

## 🌱 Vyroslo IoT Platform - ESP32 Device Integration

**Production API Base URL:** `https://vyroslo.replit.app`

---

## 📋 Table of Contents

1. [Device Registration](#device-registration)
2. [Authentication Flow](#authentication-flow)
3. [API Endpoints](#api-endpoints)
4. [Device Manifest System](#device-manifest-system)
5. [WebSocket Communication](#websocket-communication)
6. [Data Schemas](#data-schemas)
7. [Code Examples](#code-examples)
8. [Error Handling](#error-handling)
9. [Best Practices](#best-practices)

---

## 🔧 Device Registration

### Step 1: Web Interface Registration

1. **Login to Admin Panel:**
   - URL: `https://vyroslo.replit.app/login`
   - Credentials: username=`test`, password=`test123`

2. **Add New Device:**
   - Navigate to: `https://vyroslo.replit.app/devices`
   - Click "Add Device" button
   - Fill in device details:
     - **Device ID**: Unique identifier (e.g., `gh-01`, `gh-02`)
     - **Name**: Human readable name (e.g., `Main Greenhouse`)
     - **Description**: Device description
     - **Timezone**: Device timezone (e.g., `Europe/Moscow`)

3. **Save Device Secret:**
   - After creation, system generates a **device secret**
   - **⚠️ CRITICAL:** Save this secret immediately - it won't be shown again
   - Format: UUID (e.g., `daad861d-e1df-4fb3-9668-c4382e437f71`)

### Step 2: ESP32 Configuration

```cpp
// ESP32 Configuration
#define DEVICE_ID "gh-01"  // Your device ID from web interface
#define DEVICE_SECRET "daad861d-e1df-4fb3-9668-c4382e437f71"  // Generated secret
#define BASE_URL "https://vyroslo.replit.app"
#define FIRMWARE_VERSION "v1.1.0"
#define HARDWARE_VERSION "ESP32-Wroom-32"
```

---

## 🔐 Authentication Flow

### Device Authentication Sequence

```
ESP32 → POST /api/v1/device/auth → JWT Token → All subsequent API calls
```

### Authentication Request

**Endpoint:** `POST https://vyroslo.replit.app/api/v1/device/auth`

**Headers:**
```
Content-Type: application/json
```

**Request Body:**
```json
{
  "device_id": "gh-01",
  "secret": "daad861d-e1df-4fb3-9668-c4382e437f71",
  "fw": "v1.1.0",
  "hw": "ESP32-Wroom-32"
}
```

**Success Response (200):**
```json
{
  "success": true,
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "expires_in": 604800,
  "device": {
    "id": "gh-01",
    "name": "Main Greenhouse",
    "type": "greenhouse"
  }
}
```

**Error Response (401):**
```json
{
  "error": "Invalid device credentials"
}
```

---

## 📡 API Endpoints

### 1. Device Telemetry

**Endpoint:** `POST https://vyroslo.replit.app/api/v1/device/telemetry`

**Headers:**
```
Content-Type: application/json
Authorization: Bearer <JWT_TOKEN>
```

**Request Body:**
```json
{
  "device_id": "gh-esp32-001",
  "ts_ms": 1736723456000,
  // Канонично: ts_ms (number), ISO timestamp также принимается
  "air": [
    {
      "id": "air_top",
      "t_c": 19.5,
      "rh": 62.3
    },
    {
      "id": "air_bot", 
      "t_c": 18.9,
      "rh": 65.1
    }
  ],
  "soil": [
    {
      "id": "soil_01",
      "percentage": 45.2,
      "raw": 2048
    },
    {
      "id": "soil_02",
      "percentage": 38.7,
      "raw": 2156
    }
  ],
  "relays": [
    {
      "id": "humidifier",
      "state": true,
      "duration_ms": 30000
    },
    {
      "id": "exhaust_low",
      "state": false,
      "duration_ms": 0
    }
  ],
  "water": {
    "low": false
  }
}
```

**Success Response (200):**
```json
{
  "status": "ok"
}
```

### 2. Device Heartbeat

**🔄 ПРОСТОЙ HEARTBEAT (РЕКОМЕНДУЕТСЯ):**

**Endpoint:** `HEAD https://vyroslo.replit.app/api/`

**Описание:** ESP32 отправляет простой HEAD запрос каждые 1-5 секунд для поддержания онлайн статуса. Это самый эффективный способ heartbeat - минимальный трафик, без JSON.

**Headers:**
```
(Никаких заголовков не требуется)
```

**Success Response (200):** Пустое тело ответа

**🔗 АЛЬТЕРНАТИВНЫЙ HEARTBEAT:**

**Endpoint:** `POST https://vyroslo.replit.app/api/v1/device/heartbeat`

**Headers:**
```
Content-Type: application/json
Authorization: Bearer <JWT_TOKEN>
```

**Request Body:**
```json
{}
```

**Success Response (200):**
```json
{
  "status": "ok"
}
```

### 3. Device Configuration

**Endpoint:** `GET https://vyroslo.replit.app/api/v1/device/config`

**Headers:**
```
Authorization: Bearer <JWT_TOKEN>
If-None-Match: <etag>  # Optional for caching
```

**Success Response (200):**
```
HTTP/1.1 200 OK
ETag: "manifest-v2-abc123def456"
Content-Type: application/json

{
  "timezone": "Europe/Moscow",
  "targets": {
    "temp_c": [18, 20],
    "rh_pct": [50, 60]
  },
  "relays": [
    {
      "id": "light_top",
      "gpio": 16,
      "failsafe": {
        "min_on_ms": 60000,
        "min_off_ms": 60000
      }
    },
    {
      "id": "humidifier",
      "gpio": 19
    },
    {
      "id": "exhaust_low", 
      "gpio": 32
    }
  ],
  "schedules": [
    {
      "relay_id": "light_top",
      "mode": "timer",
      "follow": false,
      "periods": [
        {
          "start": "06:00",
          "end": "22:00"
        }
      ]
    }
  ],
  "rules": [
    {
      "id": "temp-control",
      "when": {
        "air.t_c": {
          "gt": 22
        }
      },
      "then": {
        "relay.exhaust_low": true
      },
      "release": {
        "air.t_c": {
          "lt": 20
        }
      },
      "hysteresis": 30000
    }
  ]
}
```

---

## 📋 Device Manifest System

The Device Manifest System provides a standardized way for ESP32 devices to describe their hardware capabilities, sensor configurations, GPIO assignments, and operational parameters. This manifest data is used by the platform to automatically generate appropriate user interfaces and validation rules.

### 🎯 Overview

The manifest system enables:
- **Hardware Discovery**: Automatic detection of connected sensors, relays, and components
- **UI Auto-Generation**: Dynamic creation of device control interfaces based on manifest
- **GPIO Validation**: Automatic validation of pin assignments and hardware constraints
- **Version Control**: Tracking of configuration changes with ETag-based caching
- **Configuration Validation**: Schema-based validation of device capabilities

### 📡 Manifest API Endpoints

#### 1. Get Device Manifest

**Endpoint:** `GET https://vyroslo.replit.app/api/v1/device/manifest`

**Headers:**
```
Authorization: Bearer <JWT_TOKEN>
If-None-Match: <etag>  # Optional for caching
```

**Success Response (200):**
```
HTTP/1.1 200 OK
ETag: "manifest-v3-abc123def456"
Content-Type: application/json

{
  "schema": "1.0",
  "device": {
    "id": "gh-esp32-001",
    "hw": "ESP32-Wroom-32",
    "fw": "v1.2.0"
  },
  "i2c": {
    "bus0": {
      "sda": 21,
      "scl": 22,
      "devices": ["SHT31_0x44", "SHT31_0x45"]
    }
  },
  "sensors": {
    "air": [
      {
        "id": "air_top",
        "type": "SHT31",
        "address": "0x44",
        "measures": ["temperature", "humidity"],
        "units": {
          "temperature": "C",
          "humidity": "%"
        }
      }
    ],
    "soil": [
      {
        "id": "soil_01",
        "type": "analog",
        "mux_channel": 0,
        "calibration": {
          "air_value": 4095,
          "water_value": 1500
        }
      }
    ]
  },
  "relays": [
    {
      "id": "humidifier",
      "gpio": 19,
      "active": "high",
      "type": "auxiliary",
      "note": "Main humidifier relay"
    }
  ],
  "manifest_version": 3,
  "updated_at": 1704067200000
}
```

**Cache Hit Response (304):**
- No body, manifest unchanged since last request

**Error Responses:**
- `404`: Manifest not found for device
- `429`: Rate limit exceeded (max 1 request/minute)

#### 2. Update Device Manifest

**Endpoint:** `PUT https://vyroslo.replit.app/api/v1/device/manifest`

**Headers:**
```
Content-Type: application/json
Authorization: Bearer <JWT_TOKEN>
```

**Request Body:**
```json
{
  "schema": "1.0",
  "device": {
    "id": "gh-esp32-001",
    "hw": "ESP32-Wroom-32",
    "fw": "v1.2.0"
  },
  "i2c": {
    "bus0": {
      "sda": 21,
      "scl": 22,
      "devices": ["SHT31_0x44", "SHT31_0x45", "BME280_0x76"]
    }
  },
  "sensors": {
    "air": [
      {
        "id": "air_top",
        "type": "SHT31",
        "address": "0x44",
        "measures": ["temperature", "humidity"]
      },
      {
        "id": "air_bot",
        "type": "BME280", 
        "address": "0x76",
        "measures": ["temperature", "humidity", "pressure"]
      }
    ],
    "soil": [
      {
        "id": "soil_01",
        "type": "capacitive",
        "mux_channel": 0
      },
      {
        "id": "soil_02", 
        "type": "analog",
        "mux_channel": 1,
        "calibration": {
          "air_value": 4095,
          "water_value": 1500
        }
      }
    ],
    "water": {
      "id": "water_level",
      "type": "digital",
      "mux_channel": 15,
      "threshold": 2048
    }
  },
  "relays": [
    {
      "id": "humidifier",
      "gpio": 19,
      "active": "high",
      "type": "auxiliary"
    },
    {
      "id": "exhaust_fan",
      "gpio": 18,
      "active": "low", 
      "type": "fan"
    },
    {
      "id": "grow_light",
      "gpio": 16,
      "active": "high",
      "type": "light"
    }
  ],
  "multiplexer": {
    "type": "CD74HC4067",
    "pins": {
      "signal": 36,
      "s0": 4,
      "s1": 5,
      "s2": 15,
      "s3": 2
    },
    "channels_used": [0, 1, 15]
  },
  "display": {
    "type": "SSD1306",
    "resolution": "128x64"
  },
  "controls": {
    "encoder": {
      "clk": 25,
      "dt": 26,
      "sw": 27
    }
  }
}
```

**Success Response (200):**
```json
{
  "ok": true,
  "manifest_version": 4,
  "etag": "AbCdEf123456",
  "warnings": [
    "GPIO 36 в multiplexer.pins.signal является input-only пином (34-39). Рекомендуется использовать другой пин."
  ]
}
```

**Error Responses:**

**Validation Error (400):**
```json
{
  "error": "validation_error", 
  "detail": "Manifest validation failed",
  "errors": [
    {
      "field": "sensors.air[0].address",
      "message": "Invalid I2C address format",
      "received": "44",
      "expected": "0x44"
    },
    {
      "field": "relays[0].gpio",
      "message": "Number must be greater than or equal to 0",
      "received": -1
    }
  ]
}
```

**Rate Limit Error (429):**
```
HTTP/1.1 429 Too Many Requests
Retry-After: 60
Content-Type: application/json

{
  "error": "rate_limited",
  "detail": "Too many requests", 
  "retry_after_sec": 60
}
```

### 📐 Complete Schema Documentation

#### Device Information
```json
{
  "device": {
    "id": "string (1-64 chars)",     // Device identifier
    "hw": "string (1-32 chars)",     // Hardware version (e.g., "ESP32-Wroom-32")
    "fw": "string (1-32 chars)"      // Firmware version (e.g., "v1.2.0")
  }
}
```

#### I2C Bus Configuration
```json
{
  "i2c": {
    "bus0": {                        // Bus name pattern: /^bus\d+$/
      "sda": "number (0-39)",        // SDA GPIO pin
      "scl": "number (0-39)",        // SCL GPIO pin  
      "devices": ["string"]          // Connected device identifiers
    }
  }
}
```

#### Air Sensors
```json
{
  "sensors": {
    "air": [
      {
        "id": "string (1-32 chars)",           // Unique sensor identifier
        "type": "enum",                        // SHT31|BME280|DHT20|DHT22|SHT20|SHT25|SHT30|SHT35|SHT40|SHT45
        "address": "string (/^0x[0-9A-Fa-f]{2}$/)", // I2C address (e.g., "0x44")
        "measures": ["temperature", "humidity", "pressure"], // Available measurements
        "units": {                             // Optional measurement units
          "temperature": "C|F",                // Temperature unit
          "humidity": "%",                     // Humidity unit
          "pressure": "Pa|hPa|kPa|mmHg"       // Pressure unit
        }
      }
    ]
  }
}
```

#### Soil Sensors
```json
{
  "sensors": {
    "soil": [
      {
        "id": "string (1-32 chars)",          // Unique sensor identifier
        "type": "analog|capacitive|modbus",   // Sensor type
        "mux_channel": "number (0-15)",       // Optional multiplexer channel
        "calibration": {                      // Optional calibration values
          "air_value": "number",              // Dry/air reading
          "water_value": "number"             // Wet/water reading
        }
      }
    ]
  }
}
```

#### Water Level Sensor
```json
{
  "sensors": {
    "water": {
      "id": "string (1-32 chars)",           // Sensor identifier
      "type": "analog|digital",              // Sensor type
      "mux_channel": "number (0-15)",        // Optional multiplexer channel
      "threshold": "number"                  // Optional threshold value
    }
  }
}
```

#### Relay Configuration
```json
{
  "relays": [
    {
      "id": "string (1-32 chars)",           // Unique relay identifier
      "gpio": "number (0-39)",               // GPIO pin number
      "active": "low|high",                  // Active logic level
      "type": "pump|fan|light|heater|auxiliary", // Relay purpose
      "note": "string (max 128 chars)"      // Optional description
    }
  ]
}
```

#### Analog Multiplexer (CD74HC4067)
```json
{
  "multiplexer": {
    "type": "CD74HC4067",                    // Fixed multiplexer type
    "pins": {
      "signal": "number (0-39)",             // Common signal pin
      "s0": "number (0-39)",                 // Select pin S0
      "s1": "number (0-39)",                 // Select pin S1  
      "s2": "number (0-39)",                 // Select pin S2
      "s3": "number (0-39)"                  // Select pin S3
    },
    "channels_used": ["number (0-15)"]      // Active multiplexer channels
  }
}
```

#### OLED Display
```json
{
  "display": {
    "type": "SSD1306|SH1106",               // Display controller type
    "resolution": "128x64|128x32|64x48"     // Display resolution
  }
}
```

#### User Controls
```json
{
  "controls": {
    "encoder": {                            // Rotary encoder (optional)
      "clk": "number (0-39)",               // Clock pin
      "dt": "number (0-39)",                // Data pin
      "sw": "number (0-39)"                 // Switch/button pin
    }
  }
}
```

### ⚠️ GPIO Pin Validation

The system automatically validates GPIO assignments and provides warnings for common issues:

**Input-Only Pins (34-39):**
- Cannot be used for: relays, multiplexer control, I2C, encoder
- Can be used for: analog sensors (ADC), multiplexer signal input
- Warning generated if used inappropriately

**Validation Examples:**
```json
{
  "warnings": [
    "GPIO 34 в relays[0] является input-only пином (34-39). Рекомендуется использовать другой пин.",
    "GPIO 39 в multiplexer.pins.s0 является input-only пином (34-39). Рекомендуется использовать другой пин."
  ]
}
```

### 🔄 ETag Caching System

The manifest system uses ETags for efficient caching:

**ETag Generation:**
- SHA-256 hash of canonical JSON representation
- Base64URL encoded (RFC 4648 Section 5)
- Automatically computed on manifest save

**Client Usage:**
```cpp
// Store ETag from previous response
String storedETag = "AbCdEf123456";

// Send conditional request
http.addHeader("If-None-Match", storedETag);
int responseCode = http.GET();

if (responseCode == 304) {
  Serial.println("Manifest unchanged, using cached version");
} else if (responseCode == 200) {
  String newETag = http.header("ETag");
  // Process updated manifest and store new ETag
}
```

### 🛠️ Integration Workflow

#### Step 1: Device Startup Sequence
```cpp
void setup() {
  // 1. Connect to WiFi
  // 2. Authenticate device → get JWT token
  // 3. Fetch current manifest (with ETag caching)
  // 4. Upload updated manifest if hardware changed
  // 5. Start normal telemetry and command processing
}
```

#### Step 2: Manifest Generation
```cpp
void generateManifest() {
  DynamicJsonDocument manifest(4096);
  
  // Device info
  manifest["schema"] = "1.0";
  JsonObject device = manifest.createNestedObject("device");
  device["id"] = DEVICE_ID;
  device["hw"] = HARDWARE_VERSION;
  device["fw"] = FIRMWARE_VERSION;
  
  // I2C configuration
  JsonObject i2c = manifest.createNestedObject("i2c");
  JsonObject bus0 = i2c.createNestedObject("bus0");
  bus0["sda"] = I2C_SDA_PIN;
  bus0["scl"] = I2C_SCL_PIN;
  JsonArray devices = bus0.createNestedArray("devices");
  devices.add("SHT31_0x44");
  
  // Air sensors
  JsonObject sensors = manifest.createNestedObject("sensors");
  JsonArray airSensors = sensors.createNestedArray("air");
  JsonObject sht31 = airSensors.createNestedObject();
  sht31["id"] = "air_top";
  sht31["type"] = "SHT31";
  sht31["address"] = "0x44";
  JsonArray measures = sht31.createNestedArray("measures");
  measures.add("temperature");
  measures.add("humidity");
  
  // Relays
  JsonArray relays = manifest.createNestedArray("relays");
  JsonObject humidifier = relays.createNestedObject();
  humidifier["id"] = "humidifier";
  humidifier["gpio"] = HUMIDIFIER_PIN;
  humidifier["active"] = "high";
  humidifier["type"] = "auxiliary";
  
  // Send manifest to server
  uploadManifest(manifest);
}
```

#### Step 3: Manifest Upload
```cpp
bool uploadManifest(const JsonDocument& manifest) {
  HTTPClient http;
  http.begin(String(BASE_URL) + "/api/v1/device/manifest");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + jwtToken);
  
  String payload;
  serializeJson(manifest, payload);
  
  int httpResponseCode = http.PUT(payload);
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    
    String newETag = doc["etag"];
    int manifestVersion = doc["manifest_version"];
    
    Serial.printf("Manifest uploaded: version %d, ETag: %s\n", 
                  manifestVersion, newETag.c_str());
    
    // Check for GPIO warnings
    if (doc.containsKey("warnings")) {
      JsonArray warnings = doc["warnings"];
      for (const char* warning : warnings) {
        Serial.printf("⚠️  GPIO Warning: %s\n", warning);
      }
    }
    
    return true;
  } else if (httpResponseCode == 400) {
    Serial.println("❌ Manifest validation failed");
    String response = http.getString();
    Serial.println(response);
  } else if (httpResponseCode == 429) {
    // Получаем Retry-After из заголовка
    String retryAfter = http.header("Retry-After");
    int waitSec = retryAfter.length() ? retryAfter.toInt() : 60;
    Serial.println("⏳ Rate limited - retry in " + String(waitSec) + " seconds");
    delay(waitSec * 1000);
  }
  
  http.end();
  return false;
}
```

### 🎨 UI Auto-Generation Features

The manifest drives automatic UI generation in the platform:

**Generated Components:**
- **Relay Control Panel**: Switches and timers for each relay
- **Sensor Monitoring**: Real-time graphs and current values
- **I2C Configuration**: Device scanning and address management
- **GPIO Pinout Diagram**: Visual representation of pin assignments
- **Device Information**: Hardware and firmware details

**Real-Time Updates:**
- Frontend polls manifest endpoint every 30 seconds
- Changes in manifest automatically refresh UI components
- Tabbed interface organizes different device aspects
- Validation warnings displayed in configuration panel

### ✅ Best Practices

**Manifest Design:**
1. **Use descriptive IDs**: `air_top`, `soil_bed_1` instead of `sensor1`, `s1`
2. **Include calibration data**: Provide air/water values for soil sensors
3. **Document GPIO usage**: Add notes explaining relay purposes
4. **Version incrementally**: Update firmware version on manifest changes
5. **Validate locally**: Check GPIO constraints before uploading

**Error Handling:**
```cpp
// Always handle rate limiting
if (httpResponseCode == 429) {
  // Получаем время ожидания из Retry-After заголовка
  String retryAfter = http.header("Retry-After");
  int waitSec = retryAfter.length() ? retryAfter.toInt() : 60;
  Serial.println("Manifest rate limited, waiting " + String(waitSec) + " seconds");
  delay(waitSec * 1000);
  return uploadManifest(manifest);
}

// Handle validation errors gracefully
if (httpResponseCode == 400) {
  Serial.println("Manifest validation failed - check GPIO assignments");
  // Log errors and continue with cached configuration
}
```

**Performance Optimization:**
```cpp
// Use ETag caching to minimize bandwidth
String lastETag = preferences.getString("manifest_etag", "");
http.addHeader("If-None-Match", lastETag);

// Only upload manifest when hardware actually changes
bool hardwareChanged = checkHardwareChanges();
if (hardwareChanged) {
  uploadManifest(generateCurrentManifest());
}
```

**Security Considerations:**
- Never expose GPIO pin assignments in public manifests
- Validate all sensor addresses before manifest upload
- Use device JWT authentication for all manifest operations
- Implement manifest version rollback for failed deployments

---

## 🔌 WebSocket Communication

### Connection

**URL:** `wss://vyroslo.replit.app/ws?token=<JWT_TOKEN>`

### Incoming Commands

**Command Structure (рекомендуемый батч-формат):**
```json
{
  "type": "relay_control",
  "cmd_id": "cmd_12345",
  "actions": [
    {
      "relay": "humidifier",
      "state": true,
      "timeout_sec": 30
    }
  ]
}
```

**Legacy Single Command Format (временная поддержка):**
```json
{
  "cmd_id": "cmd_12345",
  "type": "relay_control",
  "relay_id": "humidifier",
  "state": true,
  "duration_ms": 30000
}
```

**Command Types:**
- `relay_control` - Control relay state
- `config_update` - Update device configuration

### Command Acknowledgment

**Send back to server:**
```json
{
  "type": "ack",
  "cmd_id": "cmd_12345",
  "status": "ok",
  "ts": 1736723456000
}
```

### Fallback Communication

**⚠️ ВАЖНО: Fallback через HTTP polling НЕ ПОДДЕРЖИВАЕТСЯ**

WebSocket является единственным каналом для получения команд управления устройством. При недоступности WebSocket соединения:
- Команды relay_control и config_update недоступны
- Устройство должно пытаться переподключиться к WebSocket
- Endpoint `/api/v1/device/inbox` отсутствует в API
- Рекомендуется реализовать автоматическое переподключение с экспоненциальной задержкой

---

## 📊 Data Schemas

### Air Sensor Data
```cpp
typedef struct {
  char id[16];        // "air_top", "air_bot"
  float t_c;         // Temperature in Celsius (-40 to 125)
  float rh;          // Relative humidity (0-100%)
} AirSensorData;
```

### Soil Sensor Data
```cpp
typedef struct {
  char id[16];        // "soil_01" to "soil_16"
  float percentage;   // Moisture percentage (0-100%)
  uint16_t raw;      // Raw ADC value (0-4095)
} SoilSensorData;
```

### Relay State Data
```cpp
typedef struct {
  char id[32];        // "humidifier", "exhaust_low", etc.
  bool state;         // true = ON, false = OFF
  uint32_t duration_ms;  // How long it's been in current state
} RelayStateData;
```

### Telemetry Timestamp Format

**Канонический формат времени:**
- **ts_ms**: `number` - Epoch timestamp в миллисекундах (предпочтительно для ESP32)
- **timestamp**: `string` - ISO 8601 format (принимается как совместимый формат)

```cpp
// Получение epoch времени в миллисекундах
unsigned long getEpochTimeMs() {
  return WiFi.getTime() * 1000UL + (millis() % 1000);
}

// Использование в телеметрии
doc["ts_ms"] = getEpochTimeMs();
```

---

## 💻 Arduino Code Examples

### 1. Complete ESP32 Client

```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

// Configuration
#define DEVICE_ID "gh-esp32-001"
#define DEVICE_SECRET "device_secret_your_secret_here"
#define BASE_URL "https://vyroslo.replit.app"
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

String jwtToken = "";
WebSocketsClient webSocket;

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected!");
  
  // Authenticate device
  if (authenticateDevice()) {
    Serial.println("Device authenticated successfully!");
    
    // Connect WebSocket for commands
    connectWebSocket();
    
    // Start telemetry loop
    // Call sendTelemetry() every 10 seconds
  } else {
    Serial.println("Authentication failed!");
  }
}

bool authenticateDevice() {
  HTTPClient http;
  http.begin(String(BASE_URL) + "/api/v1/device/auth");
  http.addHeader("Content-Type", "application/json");
  
  // Create auth payload
  DynamicJsonDocument doc(1024);
  doc["device_id"] = DEVICE_ID;
  doc["secret"] = DEVICE_SECRET;
  doc["fw"] = "v1.0.0";
  doc["hw"] = "ESP32-Wroom-32";
  
  String payload;
  serializeJson(doc, payload);
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    
    DynamicJsonDocument responseDoc(1024);
    deserializeJson(responseDoc, response);
    
    jwtToken = responseDoc["token"].as<String>();
    return true;
  }
  
  http.end();
  return false;
}

void sendTelemetry() {
  if (jwtToken.length() == 0) return;
  
  HTTPClient http;
  http.begin(String(BASE_URL) + "/api/v1/device/telemetry");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + jwtToken);
  
  // Create telemetry payload
  DynamicJsonDocument doc(2048);
  doc["device_id"] = DEVICE_ID;
  doc["ts_ms"] = getEpochTimeMs(); // Canonical timestamp format
  
  // Air sensors (SHT31)
  JsonArray airArray = doc.createNestedArray("air");
  JsonObject air1 = airArray.createNestedObject();
  air1["id"] = "air_top";
  air1["t_c"] = readTemperature(0); // Your sensor reading function
  air1["rh"] = readHumidity(0);
  
  // Soil sensors
  JsonArray soilArray = doc.createNestedArray("soil");
  for (int i = 0; i < 4; i++) {  // Example: 4 soil sensors
    JsonObject soil = soilArray.createNestedObject();
    soil["id"] = "soil_" + String(i + 1, 10);
    soil["percentage"] = readSoilMoisture(i);
    soil["raw"] = readSoilRaw(i);
  }
  
  // Relay states
  JsonArray relayArray = doc.createNestedArray("relays");
  JsonObject relay1 = relayArray.createNestedObject();
  relay1["id"] = "humidifier";
  relay1["state"] = digitalRead(HUMIDIFIER_PIN);
  relay1["duration_ms"] = getRelayDuration("humidifier");
  
  // Water level
  JsonObject water = doc.createNestedObject("water");
  water["low"] = digitalRead(WATER_LEVEL_PIN) == LOW;
  
  String payload;
  serializeJson(doc, payload);
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode == 200) {
    Serial.println("Telemetry sent successfully");
  } else if (httpResponseCode == 401) {
    Serial.println("JWT expired, re-authenticating...");
    authenticateDevice();
  } else {
    Serial.println("Telemetry failed: " + String(httpResponseCode));
  }
  
  http.end();
}

void connectWebSocket() {
  // Convert HTTPS to WSS
  String wsUrl = String(BASE_URL).substring(8); // Remove "https://"
  webSocket.beginSSL(wsUrl, 443, "/ws?token=" + jwtToken);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_CONNECTED:
      Serial.println("WebSocket Connected");
      break;
      
    case WStype_TEXT:
      Serial.println("WebSocket message received:");
      Serial.println((char*)payload);
      handleCommand((char*)payload);
      break;
      
    case WStype_DISCONNECTED:
      Serial.println("WebSocket Disconnected");
      break;
  }
}

void handleCommand(const char* commandJson) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, commandJson);
  
  String cmdId = doc["cmd_id"];
  String type = doc["type"];
  
  if (type == "relay_control") {
    // Поддержка батч-формата (рекомендуемый)
    if (doc.containsKey("actions") && doc["actions"].is<JsonArray>()) {
      JsonArray actions = doc["actions"];
      for (JsonObject action : actions) {
        String relay = action["relay"];
        bool state = action["state"];
        uint32_t timeout = action["timeout_sec"] | 0;
        controlRelay(relay, state, timeout * 1000); // Convert to ms
      }
    } else {
      // Legacy одиночный формат (временная поддержка)
      String relayId = doc["relay_id"];
      bool state = doc["state"];
      uint32_t duration = doc["duration_ms"];
      controlRelay(relayId, state, duration);
    }
    
    // Send acknowledgment
    sendCommandAck(cmdId, "ok");
  }
}

void sendCommandAck(String cmdId, String status) {
  DynamicJsonDocument doc(256);
  doc["type"] = "ack";
  doc["cmd_id"] = cmdId;
  doc["status"] = status;
  doc["ts"] = millis(); // Можно использовать NTP timestamp
  
  String payload;
  serializeJson(doc, payload);
  
  webSocket.sendTXT(payload);
}

void loop() {
  webSocket.loop();
  
  // Send telemetry every 10 seconds
  static unsigned long lastTelemetry = 0;
  if (millis() - lastTelemetry >= 10000) {
    sendTelemetry();
    lastTelemetry = millis();
  }
  
  // Send simple heartbeat every 2 seconds (HEAD /api)
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 2000) {
    sendSimpleHeartbeat();
    lastHeartbeat = millis();
  }
  
  // Check WiFi connectivity
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

// ПРОСТОЙ HEARTBEAT - рекомендуемый метод
void sendSimpleHeartbeat() {
  HTTPClient http;
  http.begin(String(BASE_URL) + "/api/");
  
  // HEAD запрос - минимальный трафик
  int httpResponseCode = http.sendRequest("HEAD");
  
  if (httpResponseCode == 200) {
    // Heartbeat успешный - устройство онлайн
  } else if (httpResponseCode < 0) {
    Serial.println("Heartbeat failed: " + http.errorToString(httpResponseCode));
  }
  
  http.end();
}

// АЛЬТЕРНАТИВНЫЙ HEARTBEAT с JWT авторизацией
void sendAuthenticatedHeartbeat() {
  if (jwtToken.length() == 0) return;
  
  HTTPClient http;
  http.begin(String(BASE_URL) + "/api/v1/device/heartbeat");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + jwtToken);
  
  int httpResponseCode = http.POST("{}");
  
  if (httpResponseCode == 401) {
    Serial.println("JWT expired, re-authenticating...");
    authenticateDevice();
  }
  
  http.end();
}
```

### 2. Sensor Reading Functions

```cpp
// SHT31 Temperature/Humidity sensor
float readTemperature(int sensorIndex) {
  // Implement I2C reading for SHT31
  // Return temperature in Celsius
  return 19.5; // Example
}

float readHumidity(int sensorIndex) {
  // Implement I2C reading for SHT31  
  // Return humidity percentage (0-100)
  return 62.3; // Example
}

// Soil moisture sensors via multiplexer
float readSoilMoisture(int sensorIndex) {
  // Select multiplexer channel
  selectMuxChannel(sensorIndex);
  
  // Read ADC value
  int rawValue = analogRead(SOIL_ADC_PIN);
  
  // Convert to percentage (calibrate these values)
  float percentage = map(rawValue, 4095, 1024, 0, 100);
  return constrain(percentage, 0, 100);
}

uint16_t readSoilRaw(int sensorIndex) {
  selectMuxChannel(sensorIndex);
  return analogRead(SOIL_ADC_PIN);
}

void selectMuxChannel(int channel) {
  // Control multiplexer select pins
  // Example for 74HC4051 (3-bit address)
  digitalWrite(MUX_S0, channel & 0x01);
  digitalWrite(MUX_S1, (channel >> 1) & 0x01);
  digitalWrite(MUX_S2, (channel >> 2) & 0x01);
  delay(10); // Settling time
}

// Relay control
void controlRelay(String relayId, bool state, uint32_t duration) {
  int pin = getRelayPin(relayId);
  if (pin > 0) {
    digitalWrite(pin, state ? HIGH : LOW);
    Serial.println("Relay " + relayId + " set to " + (state ? "ON" : "OFF"));
  }
}

int getRelayPin(String relayId) {
  if (relayId == "humidifier") return 19;
  if (relayId == "exhaust_low") return 32;
  if (relayId == "exhaust_high") return 33;
  if (relayId == "light_top") return 16;
  if (relayId == "light_mid") return 17;
  if (relayId == "light_bot") return 18;
  return 0; // Invalid
}
```

---

## ⚠️ Error Handling

### HTTP Status Codes

- **200 OK** - Request successful
- **304 Not Modified** - Config unchanged (use cached version)
- **400 Bad Request** - Invalid request format
- **401 Unauthorized** - Invalid/expired JWT or device credentials
- **404 Not Found** - Device not found
- **429 Too Many Requests** - Rate limited (всегда с заголовком `Retry-After`)
- **500 Internal Server Error** - Server error

### Retry-After Headers

**Все 429 ответы включают заголовок `Retry-After`:**
```
HTTP/1.1 429 Too Many Requests
Retry-After: <seconds>
```

**Примеры для разных эндпойнтов:**
- `/api/v1/device/auth`: `Retry-After: 10` (rate limit: 5/min)
- `/api/v1/device/telemetry`: `Retry-After: 5` (rate limit: 12/min)
- `/api/v1/device/config`: `Retry-After: 30` (rate limit: 2/min)
- `/api/v1/device/manifest`: `Retry-After: 60` (rate limit: 1/min)

### 401 Error Semantics

**Четкая семантика 401 ошибок:**

1. **401 на `/api/v1/device/auth`** = неверный device secret
   - Устройство переходит в состояние `authBlocked` до перезагрузки
   - Не пытаться повторять запросы авторизации
   - Требуется проверка device_id и secret

2. **401 на остальных эндпойнтах** = протухший/невалидный JWT
   - Клиент обязан пере-авторизоваться через `/auth`
   - После получения нового токена - повторить исходный запрос
   - Нормальное поведение при истечении 7-дневного токена

```cpp
if (httpResponseCode == 401) {
  if (currentEndpoint == "/api/v1/device/auth") {
    Serial.println("AUTH BLOCKED: Invalid device credentials");
    goToSleep(); // Блокируемся до ребута
  } else {
    Serial.println("JWT expired, re-authenticating...");
    if (authenticateDevice()) {
      // Retry the failed request with new token
    }
  }
}
```

### JWT Token Expiration

JWT tokens expire after **7 days**. Handle 401 responses by re-authenticating (см. выше).

### Network Connectivity

```cpp
void checkConnectivity() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(1000);
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi reconnected");
      // Re-authenticate and reconnect WebSocket
      authenticateDevice();
      connectWebSocket();
    }
  }
}
```

---

## 🔄 Best Practices

### 1. Timing Intervals

- **Telemetry**: Every 10 seconds (отправка данных датчиков)
- **Simple Heartbeat**: Every 1-5 seconds (HEAD /api - поддержание онлайн статуса)
- **Authenticated Heartbeat**: Every 30 seconds (если используете POST с JWT)
- **Config Polling**: Every 60 seconds (получение новой конфигурации)
- **Sensor Reading**: 100ms intervals for stability

### 2. Memory Management

```cpp
// Use StaticJsonDocument for fixed-size buffers
StaticJsonDocument<2048> telemetryDoc;

// Clear documents after use
telemetryDoc.clear();

// Use String reservation for JWT
jwtToken.reserve(512);
```

### 3. Watchdog Timer

```cpp
#include "esp_task_wdt.h"

void setup() {
  // Enable watchdog with 30 second timeout
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
}

void loop() {
  // Reset watchdog
  esp_task_wdt_reset();
  
  // Your main loop code
}
```

### 4. OTA Updates Support

```cpp
#include <ArduinoOTA.h>

void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_ID);
  ArduinoOTA.setPassword("your_ota_password");
  
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update Starting...");
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA Update Complete!");
  });
  
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  // ... rest of loop
}
```

### 5. Persistent Configuration

```cpp
#include <Preferences.h>

Preferences prefs;

void saveJWT(String token) {
  prefs.begin("greenhouse", false);
  prefs.putString("jwt", token);
  prefs.end();
}

String loadJWT() {
  prefs.begin("greenhouse", true);
  String token = prefs.getString("jwt", "");
  prefs.end();
  return token;
}
```

---

## 🚀 Production Deployment Checklist

### Hardware Setup

- [ ] ESP32 board with sufficient GPIO pins
- [ ] SHT31 I2C sensors for air temperature/humidity
- [ ] Soil moisture sensors with multiplexer (74HC4051)
- [ ] Relay board for actuator control
- [ ] Water level sensor
- [ ] Stable 5V power supply
- [ ] WiFi antenna (external recommended)

### Software Configuration

- [ ] Set unique DEVICE_ID for each unit
- [ ] Configure device secret from web interface
- [ ] Set correct WiFi credentials
- [ ] Configure GPIO pin assignments
- [ ] Calibrate soil moisture sensors
- [ ] Test all relay outputs
- [ ] Verify sensor readings
- [ ] Enable watchdog timer
- [ ] Configure OTA updates

### Network Requirements

- [ ] Stable WiFi connection
- [ ] Internet access to vyroslo.replit.app
- [ ] NTP time synchronization
- [ ] WebSocket support (port 443)
- [ ] No corporate firewall blocking HTTPS/WSS

### Testing Procedure

1. **Device Registration**: Create device via web interface
2. **Authentication Test**: Verify JWT token retrieval
3. **Telemetry Test**: Send sample data, verify in dashboard
4. **Command Test**: Send relay command from web interface
5. **Disconnection Test**: Verify auto-reconnection
6. **Long-term Test**: Run for 24+ hours

---

## 📞 Support & Troubleshooting

### Common Issues

**1. Authentication Failed (401)**
- Check device ID matches exactly
- Verify device secret is correct
- Ensure device exists in web interface

**2. Telemetry Not Appearing**
- Check JWT token validity
- Verify JSON payload format
- Check device is online in dashboard

**3. WebSocket Connection Failed**
- Verify JWT token in URL
- Check firewall/network restrictions
- Test HTTPS connectivity first

**4. Sensor Reading Issues**
- Check I2C connections and pull-up resistors
- Verify multiplexer channel selection
- Calibrate ADC readings

### Debug Logging

```cpp
#define DEBUG_LEVEL 1  // 0=none, 1=basic, 2=verbose

void debugPrint(String message, int level = 1) {
  if (DEBUG_LEVEL >= level) {
    Serial.println("[DEBUG] " + message);
  }
}
```

### Production Monitoring

Monitor these metrics in your ESP32 code:

- WiFi signal strength (RSSI)
- Free heap memory
- Uptime counter
- Failed request count
- Last successful telemetry timestamp

---

## 📋 API Rate Limits

- **Authentication**: 5 requests/minute per device
- **Telemetry**: 12 requests/minute per device (every 5s max)
- **Simple Heartbeat (HEAD /api)**: Неограниченно (рекомендуется каждые 1-5 секунд)
- **Authenticated Heartbeat**: 4 requests/minute per device (every 15s max)
- **Config**: 2 requests/minute per device

---

**End of Documentation**

For technical support or API updates, contact the development team or check the web interface for system status.

Built with ❤️ for sustainable greenhouse automation 🌱