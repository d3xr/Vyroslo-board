# ESP32 Device gh-02 - Подробная временная схема работы

**Устройство:** `gh-02` с секретом `daad861d-e1df-4fb3-9668-c4382e437f71`  
**Сервер:** `https://vyroslo.replit.app`  
**Прошивка:** v1.1.0 ESP32-Wroom-32

---

## 📅 **Временная шкала первых 10 минут работы**

### **T+00:00 - Загрузка устройства**

🔄 **ESP32 подключается к WiFi, синхронизирует время, инициализирует аппаратуру**

---

### **T+00:15 - ПЕРВАЯ АУТЕНТИФИКАЦИЯ**

#### 1️⃣ **POST /api/v1/device/auth** (ПЕРВЫЙ РАЗ)

**📤 ЗАПРОС ESP32:**
```http
POST https://vyroslo.replit.app/api/v1/device/auth
Content-Type: application/json

{
  "device_id": "gh-02",
  "secret": "daad861d-e1df-4fb3-9668-c4382e437f71",
  "fw": "v1.1.0",
  "hw": "ESP32-Wroom-32"
}
```

**📥 ОТВЕТ СЕРВЕРА (200 OK):**
```json
{
  "success": true,
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJkZXZpY2VfaWQiOiJnaC0wMiIsInR5cGUiOiJkZXZpY2UiLCJpYXQiOjE3NTc3OTE4NTIsImV4cCI6MTc1ODM5NjY1Mn0.hVmZBlUzHpA1MCntiWknQK3tkbgco5aruWXf4fW3rkg",
  "expires_in": 604800,
  "device": {
    "id": "gh-02",
    "name": "gh-02",
    "type": "greenhouse"
  },
  "polling": {
    "telemetry_sec": 60,
    "config_sec": 300
  }
}
```

**🔍 ESP32 сохраняет:**
- JWT токен на 7 дней (604800 сек)
- Интервалы: телеметрия 60 сек, конфиг 300 сек
- Device ID подтвержден как "gh-02"

---

### **T+00:20 - WebSocket подключение**

#### 2️⃣ **WebSocket Connection**

**📤 ЗАПРОС ESP32:**
```
wss://vyroslo.replit.app/ws?token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

**📥 ОТВЕТ СЕРВЕРА:**
```
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
```

**📋 ЛОГИ СЕРВЕРА:**
```
[WS][DEBUG] Decoded token payload: {
  "device_id": "gh-02",
  "type": "device",
  "iat": 1757791852,
  "exp": 1758396652
}
[WS] Device gh-02 connected via WebSocket
```

**✅ Результат:** ESP32 подключен для получения команд реле в реальном времени

---

### **T+00:25 - Первый heartbeat**

#### 3️⃣ **HEAD /api/** (ПЕРВЫЙ РАЗ)

**📤 ЗАПРОС ESP32:**
```http
HEAD https://vyroslo.replit.app/api/
```

**📥 ОТВЕТ СЕРВЕРА (200 OK):**
```http
HTTP/1.1 200 OK
Content-Type: application/json
Cache-Control: no-store, no-cache, must-revalidate, proxy-revalidate
Access-Control-Allow-Origin: *
Content-Length: 0
```

**📋 ЛОГИ СЕРВЕРА:**
```
[HEARTBEAT] ESP32 heartbeat detected
```

**✅ Результат:** Устройство теперь показывается как **ONLINE** в дашборде

---

### **T+00:30 - Первая телеметрия**

#### 4️⃣ **POST /api/v1/device/telemetry** (ПЕРВЫЙ РАЗ)

**📤 ЗАПРОС ESP32:**
```http
POST https://vyroslo.replit.app/api/v1/device/telemetry
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
Content-Type: application/json

{
  "device_id": "gh-02",
  "ts_ms": 1757791850000,
  "air": [
    {
      "id": "air_top",
      "t_c": 19.2,
      "rh": 61.5
    },
    {
      "id": "air_bot", 
      "t_c": 18.8,
      "rh": 63.2
    }
  ],
  "soil": [
    {
      "id": "soil_01",
      "percentage": 42.3,
      "raw": 2148
    },
    {
      "id": "soil_02",
      "percentage": 38.7,
      "raw": 2298
    }
  ],
  "relays": [
    {
      "id": "light_top",
      "state": false,
      "duration_ms": 0
    },
    {
      "id": "humidifier",
      "state": true,
      "duration_ms": 30000
    }
  ],
  "water": {
    "low": false
  }
}
```

**📥 ОТВЕТ СЕРВЕРА (200 OK):**
```json
{
  "status": "ok"
}
```

**📋 ЛОГИ СЕРВЕРА:**
```
[RATE-LIMIT-DEBUG] Telemetry keyGenerator called for device: gh-02
7:30:55 PM [express] POST /api/v1/device/telemetry 200 in 1132ms :: {"status":"ok"}
```

---

### **T+00:35 - Загрузка манифеста**

#### 5️⃣ **PUT /api/v1/device/manifest** (ПЕРВЫЙ РАЗ)

**📤 ЗАПРОС ESP32:**
```http
PUT https://vyroslo.replit.app/api/v1/device/manifest
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
Content-Type: application/json

{
  "device_id": "gh-02", 
  "hw_version": "ESP32-Wroom-32",
  "fw_version": "v1.1.0",
  "relays": [
    {
      "id": "light_top",
      "name": "Top Light",
      "type": "light",
      "gpio": 13,
      "active_level": "low",
      "max_duration_sec": 3600
    },
    {
      "id": "light_mid", 
      "name": "Mid Light",
      "type": "light",
      "gpio": 14,
      "active_level": "low", 
      "max_duration_sec": 3600
    },
    {
      "id": "humidifier",
      "name": "Humidifier",
      "type": "humidifier",
      "gpio": 26,
      "active_level": "low",
      "max_duration_sec": 1800
    }
  ],
  "sensors": {
    "air": [
      {
        "id": "air_top",
        "name": "Top Air Sensor",
        "type": "sht31",
        "i2c_address": "0x44",
        "bus": 0,
        "parameters": ["temperature", "humidity"]
      },
      {
        "id": "air_bot",
        "name": "Bottom Air Sensor", 
        "type": "sht31",
        "i2c_address": "0x45",
        "bus": 0,
        "parameters": ["temperature", "humidity"]
      }
    ],
    "soil": [
      {
        "id": "soil_01",
        "name": "Soil Sensor 1",
        "type": "analog", 
        "mux_channel": 0,
        "air_value": 4095,
        "water_value": 1500
      },
      {
        "id": "soil_02",
        "name": "Soil Sensor 2",
        "type": "analog",
        "mux_channel": 1, 
        "air_value": 4095,
        "water_value": 1500
      }
    ]
  },
  "display": {
    "type": "ssd1306",
    "size": "128x64",
    "i2c_address": "0x3c",
    "bus": 1
  },
  "controls": {
    "encoder": {
      "clk_pin": 15,
      "dt_pin": 2,
      "sw_pin": 4
    }
  }
}
```

**📥 ОТВЕТ СЕРВЕРА (200 OK):**
```json
{
  "status": "saved",
  "etag": "manifest-gh-02-v1.1.0-20250913T193055Z",
  "version": 1,
  "message": "Device manifest saved successfully"
}
```

**✅ Результат:** Манифест UI перестает показывать ошибку "Failed to load device manifest"

---

### **T+01:30 - Регулярные циклы начинаются**

---

#### **Каждые 5 секунд - Heartbeat** 

**📤 ЗАПРОС ESP32:**
```http
HEAD https://vyroslo.replit.app/api/
```
**📥 ОТВЕТ:** `200 OK` (поддерживает online статус)

---

#### **Каждые 60 секунд - Телеметрия**

**📤 ЗАПРОС ESP32:**
```http
POST https://vyroslo.replit.app/api/v1/device/telemetry
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```
**📥 ОТВЕТ:** `{"status":"ok"}` (данные сохранены)

---

#### **Каждые 300 секунд (5 мин) - Конфигурация**

**📤 ЗАПРОС ESP32:**
```http
GET https://vyroslo.replit.app/api/v1/device/config
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
If-None-Match: "config-gh-02-v1"
```

**📥 ОТВЕТ СЕРВЕРА (304 Not Modified):**
```http
HTTP/1.1 304 Not Modified
ETag: "config-gh-02-v1"
Cache-Control: no-store
```

**ИЛИ при изменениях (200 OK):**
```json
{
  "device_id": "gh-02",
  "etag": "config-gh-02-v2", 
  "config": {
    "polling": {
      "telemetry_sec": 30,
      "config_sec": 120,
      "heartbeat_sec": 5
    },
    "relays": {
      "default_timeout_sec": 300
    }
  },
  "updated_at": "2025-09-13T19:35:00Z"
}
```

---

### **T+05:00 - Получение команды от WebSocket**

#### **WebSocket: Команда управления реле**

**📥 ОТ СЕРВЕРА ЧЕРЕЗ WebSocket:**
```json
{
  "type": "relay_control",
  "cmd_id": "cmd-20250913-193500-001",
  "actions": [
    {
      "relay": "humidifier",
      "state": true,
      "timeout_sec": 30
    }
  ]
}
```

**📤 ESP32 ОТПРАВЛЯЕТ ACK:**
```json
{
  "type": "ack", 
  "cmd_id": "cmd-20250913-193500-001",
  "status": "ok",
  "ts": 1757791900000
}
```

**📋 ЛОГИ СЕРВЕРА:**
```
[WS] Sent batch command to gh-02: 1 actions
[CMD-DISPATCH] Sent relay_control command cmd-20250913-193500-001 to gh-02
[WS] Command cmd-20250913-193500-001 acknowledged by gh-02
```

---

### **T+10:00 - Итоговое состояние**

**✅ Устройство полностью интегрировано:**

- 🟢 **Статус:** ONLINE (heartbeat каждые 5 сек)
- 📊 **Телеметрия:** Поступает каждые 60 сек
- ⚡ **WebSocket:** Подключен, команды работают
- 🔧 **Конфигурация:** Синхронизируется каждые 5 мин  
- 📋 **Манифест:** Загружен, UI показывает устройство правильно
- 🔐 **JWT токен:** Действителен 7 дней

---

## 🚨 **Возможные ошибки и решения**

### **401 Unauthorized**
```json
{"error": "unauthorized"}
```
**Причина:** Истек JWT или неверный секрет  
**Решение:** Повторить POST /api/v1/device/auth

### **429 Too Many Requests**  
```json
{
  "error": "rate_limited",
  "detail": "Too many requests", 
  "retry_after_sec": 60
}
```
**Причина:** Превышен лимит запросов  
**Решение:** Ждать указанное время + экспоненциальный backoff

### **500 Internal Server Error**
```json
{"error": "Internal server error"}
```
**Причина:** Проблема на сервере  
**Решение:** Повторить через 30-60 сек с backoff

---

## 📊 **Лимиты запросов**

- **Auth:** 5 запросов/минуту на IP
- **Telemetry:** 12 запросов/минуту на устройство  
- **Config:** 2 запроса/минуту на устройство
- **Manifest:** 1 запрос/минуту на устройство
- **Heartbeat:** Без ограничений

---

## 🔄 **Важные детали реализации**

1. **JWT токен действует 7 дней** - сохранять в NVS памяти ESP32
2. **WebSocket переподключение** при разрыве связи 
3. **ETag кеширование** для config и manifest
4. **Обработка 401** - всегда повторная аутентификация
5. **Retry-After соблюдение** при 429 ошибках
6. **TLS/SSL обязательно** для production сервера
7. **Heartbeat критичен** для online статуса устройства

---

**🎯 Этот протокол обеспечивает надежную работу ESP32 устройства gh-02 с полной интеграцией в систему мониторинга теплицы.**