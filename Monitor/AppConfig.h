#pragma once

#include <Arduino.h>

// =============================================================
// SMRAD CONFIGURATION
// =============================================================

#define SERIAL_DEBUG 1

// =============================================================
// WIFI CONFIGURATION
// =============================================================

#define WIFI_SSID       "TamNET_Guest"
#define WIFI_PASS       "BeMyGuest"

// =============================================================
// MQTT / THINGSPEAK CONFIGURATION
// =============================================================

#define MQTT_SERVER     "mqtt3.thingspeak.com"
#define MQTT_PORT       1883

// =============================================================
// THINGSPEAK CHANNELS
// =============================================================

#define CHANNEL_ID_1    "3389737"
#define CHANNEL_ID_2    "3394344"
#define CHANNEL_ID_3    "3394347"

// =============================================================
// RGB STATUS LED
// =============================================================

#define LED_PIN         38

// =============================================================
// SD CARD SPI CONFIGURATION
// =============================================================

#define SD_CS_PIN       05
#define SD_MOSI_PIN     23
#define SD_SCK_PIN      18
#define SD_MISO_PIN     19
#define SD_SPI_SPEED    4000000UL

// =============================================================
// ENVIRONMENTAL REFERENCE VALUES
// =============================================================

#define SEALEVELPRESSURE_HPA 1013.25f

// =============================================================
// SENSOR FIELD CONFIGURATION
// =============================================================

#define FIELD_COUNT         24
#define FIELDS_PER_CHANNEL  8
#define MAX_CHANNELS        3

// =============================================================
// SENSOR TIMING
// =============================================================

#define SENSOR_PERIOD_MS    20000UL

// =============================================================
// MQTT PUBLISH SPACING
// =============================================================
//
// Delay between ThingSpeak channel publishes.
//
// Prevents:
// - broker overload
// - aggressive burst traffic
// - occasional publish failures
//
// =============================================================

#define MQTT_SPACING_MS     1200UL

// =============================================================
// WIFI / MQTT RETRY TIMERS
// =============================================================
//
// Controls reconnect retry pacing.
//
// =============================================================

#define WIFI_RETRY_MS       15000UL
#define MQTT_RETRY_MS       8000UL

// =============================================================
// NTP TIME SYNCHRONIZATION
// =============================================================

#define TIME_SYNC_INTERVAL_MS   3600000UL
#define TIME_SYNC_TIMEOUT_MS    10000UL

// =============================================================
// WATCHDOG TIMING
// =============================================================

#define WATCHDOG_PERIOD_MS             5000UL
#define DIAGNOSTIC_LOG_PERIOD_MS       60000UL

// =============================================================
// TASK HEARTBEAT TIMEOUTS
// =============================================================

#define SENSOR_TASK_TIMEOUT_MS         90000UL
#define NETWORK_TASK_TIMEOUT_MS        45000UL
#define LED_TASK_TIMEOUT_MS            15000UL
#define TIME_TASK_TIMEOUT_MS           60000UL
#define SD_TASK_TIMEOUT_MS             60000UL

// =============================================================
// WIFI RECOVERY POLICY
// =============================================================

#define WIFI_OFFLINE_RECOVERY_MS       180000UL
#define WIFI_OFFLINE_RESTART_MS        900000UL

// =============================================================
// MQTT RECOVERY POLICY
// =============================================================

#define MQTT_OFFLINE_RECOVERY_MS       180000UL
#define MQTT_OFFLINE_RESTART_MS        1200000UL

// =============================================================
// PUBLISH / SENSOR SUPERVISION
// =============================================================

#define NO_PUBLISH_RESTART_MS          1800000UL
#define RS485_NO_VALID_WARN_MS          600000UL

// =============================================================
// SD LOGGER QUEUE
// =============================================================

#define SD_QUEUE_LENGTH                4

// =============================================================
// POWER TELEMETRY
// =============================================================
//
// Optional analog voltage measurements.
//
// These are useful in field deployments for:
// - UPS monitoring
// - battery diagnostics
// - brownout analysis
// - cable / power supply troubleshooting
//
// Voltage divider:
//
// measured source voltage
//      |
//     rTop
//      |
//   ADC pin
//      |
//    rBottom
//      |
//     GND
//
// =============================================================

#define POWER_VIN_PIN       A0 // A2
#define POWER_BAT_PIN       A0 // A3

#define POWER_VIN_RTOP      100000.0f
#define POWER_VIN_RBOTTOM   22000.0f

#define POWER_BAT_RTOP      100000.0f
#define POWER_BAT_RBOTTOM   22000.0f