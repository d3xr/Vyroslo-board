# Vyroslo-board ESP32 Greenhouse Controller

ESP32-based smart greenhouse controller firmware for the Vyroslo ecosystem.

## 📋 Overview

This firmware implements a thin client for ESP32 designed to control and monitor smart greenhouses. The device synchronizes with the Vyroslo server, publishes its manifest, sends telemetry, receives configuration, and provides real-time control via WebSocket with fallback REST interface.

## 🚀 Features

### Core Functionality
- **Device Authentication**: JWT-based authentication with automatic token management
- **Manifest Synchronization**: Auto-generated device manifest with ETag caching
- **Real-time Telemetry**: Sensor data transmission every ~10 seconds
- **Configuration Management**: Server-driven configuration with automatic updates
- **WebSocket Control**: Real-time relay control and command processing
- **Fallback REST API**: Backup control interface when WebSocket unavailable
- **NVS Storage**: Persistent storage for tokens, configuration, and state

### Hardware Support
- **ESP32 Development Board**: Main microcontroller
- **SHT31 Sensor**: Temperature and humidity monitoring
- **Soil Moisture Sensors**: Multi-channel soil monitoring
- **Water Level Sensors**: Reservoir monitoring
- **8-Channel Relay Module**: Device control (pumps, fans, lights)
- **SSD1306 OLED Display**: 128x64 status display
- **Rotary Encoder**: User interface navigation
- **LED Indicators**: System status indication

### User Interface
- **Multi-screen Navigation**: Home, Menu, Relays, Sensors, Server status
- **Real-time Display**: Live sensor readings and relay states
- **Server Testing**: Built-in authentication and connectivity testing
- **Encoder Control**: Intuitive navigation and control

## 🛠️ Hardware Setup

### Pin Configuration
```cpp
// Display (I2C)
#define DISPLAY_SDA 21
#define DISPLAY_SCL 22

// Encoder
#define ENCODER_CLK 18
#define ENCODER_DT 19
#define ENCODER_SW 5

// Sensors (I2C shared with display)
// SHT31: Uses I2C pins 21 (SDA), 22 (SCL)

// Relays (8-channel module)
#define RELAY_PINS {13, 12, 14, 27, 26, 25, 33, 32}

// Status LEDs
#define LED_GREEN 2
#define LED_YELLOW 4
#define LED_RED 16

// Analog inputs for soil/water sensors
#define SOIL_SENSOR_1 A0
#define SOIL_SENSOR_2 A3
#define WATER_LEVEL A6
```

### Wiring Diagram
```
ESP32          SSD1306 OLED    SHT31 Sensor    Rotary Encoder
GPIO21  -----> SDA             SDA
GPIO22  -----> SCL             SCL
3.3V    -----> VCC             VIN
GND     -----> GND             GND
GPIO18  -----> -----------------> CLK
GPIO19  -----> -----------------> DT
GPIO5   -----> -----------------> SW
```

## 🔧 Installation

### Prerequisites
- [PlatformIO](https://platformio.org/) installed
- ESP32 development board
- Required hardware components (see Hardware Setup)

### Dependencies
The following libraries are automatically installed via PlatformIO:
```ini
lib_deps =
  bblanchon/ArduinoJson @ ^6.21.5
  adafruit/Adafruit Unified Sensor @ ^1.1.14
  adafruit/Adafruit SHT31 Library @ ^2.2.2
  adafruit/Adafruit SSD1306 @ ^2.5.9
  links2004/WebSockets @ ^2.4.1
```

### Build and Upload

1. **Clone the repository**:
   ```bash
   git clone https://github.com/d3xr/vyroslo-board.git
   cd vyroslo-board
   ```

2. **Configure WiFi and Device Credentials**:
   Edit `src/main.cpp` and update:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   const char* deviceId = "YOUR_DEVICE_ID";
   const char* deviceSecret = "YOUR_DEVICE_SECRET";
   ```

3. **Build and upload**:
   ```bash
   platformio run --target upload
   ```

4. **Monitor serial output**:
   ```bash
   platformio device monitor
   ```

## 🖥️ User Interface

### Navigation
- **Rotate Encoder**: Navigate through menu items
- **Short Press**: Select/confirm action
- **Long Press**: Back/exit to previous screen

### Screen Layout
1. **Home Screen**: System status, WiFi, server connection
2. **Relays Screen**: Manual relay control and status
3. **Sensors Screen**: Temperature, humidity readings
4. **Soil Screen**: Soil moisture levels
5. **Server Screen**: Connection status and testing

### LED Status Indicators
- **Green**: Online (WebSocket connected)
- **Yellow**: Connecting/waiting/backoff
- **Red**: Authentication blocked/error

## 🌐 Server Integration

### API Endpoints
- `POST /api/v1/device/auth` - Device authentication
- `GET /api/v1/device/manifest` - Manifest synchronization
- `PUT /api/v1/device/manifest` - Manifest publication
- `POST /api/v1/device/telemetry` - Telemetry transmission
- `GET /api/v1/device/config` - Configuration retrieval
- `HEAD /api` - Heartbeat (health check)
- `GET /api/v1/device/inbox` - Fallback command retrieval

### WebSocket Protocol
- **URL**: `wss://vyroslo.replit.app/ws?token=<JWT>`
- **Relay Control**: `{"type":"relay_control", "relay_id":1, "action":"on", "timeout":30}`
- **Config Update**: `{"type":"config_update"}`
- **ACK Response**: `{"type":"ack", "command_id":"...", "status":"success"}`

### Telemetry Format
```json
{
  "device_id": "greenhouse_001",
  "ts": 1704067200,
  "fw": "1.0.0",
  "hw": "esp32_v1",
  "sensors": {
    "air": {"temp": 25.5, "humidity": 65.2},
    "soil": [{"moisture": 45}, {"moisture": 52}],
    "water": {"level": 75}
  },
  "relays": [0, 1, 0, 0, 0, 0, 0, 0],
  "rssi_dbm": -45,
  "uptime_sec": 3600
}
```

## 🔍 Configuration

### Device Configuration
The device receives configuration from the server including:
- Telemetry intervals
- Relay schedules and timeouts
- Sensor thresholds and calibration
- Display settings
- Network parameters

### NVS Storage
Persistent data stored in ESP32 NVS:
- JWT authentication token
- Manifest ETag and timestamp
- Configuration ETag and last config
- Network credentials
- Device state

## 🐛 Troubleshooting

### Common Issues

**Compilation Errors**:
```bash
# Clean and rebuild
platformio run --target clean
platformio run
```

**WiFi Connection Issues**:
- Verify SSID/password in `main.cpp`
- Check WiFi signal strength
- Monitor serial output for connection logs

**Server Authentication**:
- Verify device_id and device_secret
- Check server availability
- Use built-in TEST function in Server menu

**Display Issues**:
- Check I2C wiring (SDA/SCL pins)
- Verify 3.3V power supply
- Test with I2C scanner

**Sensor Reading Issues**:
- Check sensor wiring and power
- Verify I2C address conflicts
- Monitor serial output for sensor errors

### Debug Output
Enable verbose logging by monitoring serial output:
```bash
platformio device monitor --baud 115200
```

## 📁 Project Structure

```
vyroslo-board/
├── src/
│   └── main.cpp              # Main firmware (monolithic implementation)
├── include/
│   └── README                # PlatformIO include directory
├── lib/
│   └── README                # Local libraries directory
├── test/
│   └── README                # Unit tests directory
├── platformio.ini            # PlatformIO configuration
├── PROJECT_DESCRIPTION.md    # Detailed project description (Russian)
└── README.md                 # This file
```

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Commit changes: `git commit -m 'Add amazing feature'`
4. Push to branch: `git push origin feature/amazing-feature`
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🔗 Related Projects

- [Vyroslo Server](https://github.com/vyroslo/server) - Backend server for the ecosystem
- [Vyroslo Web UI](https://github.com/vyroslo/web) - Web interface for greenhouse management

## 👨‍💻 Author

**d3xr** - [GitHub](https://github.com/d3xr)

## 🙏 Acknowledgments

- Vyroslo team for the server infrastructure and protocol design
- Adafruit for excellent sensor libraries
- Arduino and ESP32 communities for continuous support