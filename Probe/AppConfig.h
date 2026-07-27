#pragma once

#include <Arduino.h>

// =============================================================
// SMRAD CONFIGURATION
// =============================================================


// =============================================================
// DEBUG CONFIGURATION
// =============================================================

#define SERIAL_DEBUG 1
#define SERIAL_MONITOR 1

// =============================================================
// I2C BUS CONFIGURATION
// =============================================================

#define SDA_PIN         8
#define SCL_PIN         9

// =============================================================
// RGB STATUS LED
// =============================================================

#define LED_PIN         38

// =============================================================
// EXPLORIR CO2 UART
// =============================================================

#define CO2_RX          16
#define CO2_TX          17

// =============================================================
// ANALOG SENSOR INPUTS
// =============================================================

#define MQ4_PIN         A0
#define H2S_PIN         A1

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

#define SENSOR_PERIOD_MS    10000UL

// =============================================================
// WATCHDOG
// =============================================================

#define PROBE_SENSOR_WATCHDOG_MS   120000UL
#define PROBE_SEND_WATCHDOG_MS     120000UL
#define PROBE_LOOP_WATCHDOG_MS     180000UL

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

#define POWER_VIN_PIN       A2
#define POWER_BAT_PIN       A3

#define POWER_VIN_RTOP      100000.0f
#define POWER_VIN_RBOTTOM   22000.0f

#define POWER_BAT_RTOP      100000.0f
#define POWER_BAT_RBOTTOM   22000.0f