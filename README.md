# sense360
An Assistive Wearable Sound Awareness System

An assistive wearable system that detects surrounding sounds, identifies sound intensity and direction, and provides haptic alerts to help hearing-impaired users become aware of nearby calls, alarms, or hazardous noises.

------------------------------------------------------------
PROJECT OVERVIEW
------------------------------------------------------------

This project uses:

- STM32F401 Black Pill
- 3 × MAX9814 Analog MEMS Microphones
- ESP8266 WiFi Module

The system reads sound intensity from multiple microphones using the STM32 internal ADC, processes direction and intensity, and transmits data to a remote server via the ESP8266 WiFi module.

------------------------------------------------------------
SYSTEM ARCHITECTURE
------------------------------------------------------------

MAX9814 (x3) → ADC (STM32) → Processing → UART → ESP8266 → WiFi

------------------------------------------------------------
HARDWARE CONNECTIONS
------------------------------------------------------------

3 MEMS MICROPHONE CONNECTIONS (MAX9814)

STM32F401 ADC Pins Used:

Left Mic   → PA0  → ADC1_IN0
Right Mic  → PA1  → ADC1_IN1
Back Mic   → PA4  → ADC1_IN4

Power Connections (All 3 Mics):

MAX9814 VCC → 3.3V
MAX9814 GND → GND

IMPORTANT:
- All grounds must be common with STM32.
- ADC resolution: 12-bit
- Input voltage range: 0–3.3V
- All microphones must use same GAIN and AR configuration.
- Physically space microphones for direction detection.

Signal Connections:

Mic 1 OUT → PA0
Mic 2 OUT → PA1
Mic 3 OUT → PA4

------------------------------------------------------------
ESP8266 WIFI MODULE CONNECTIONS
------------------------------------------------------------

Using UART2 (USART2)

ESP8266 VCC → 3.3V (Stable)
ESP8266 GND → GND
ESP8266 TX  → PA3 (USART2_RX)
ESP8266 RX  → PA2 (USART2_TX)
ESP8266 EN  → 3.3V
ESP8266 RST → 3.3V (Pull-up)

IMPORTANT POWER NOTES:

- ESP8266 requires stable 3.3V supply.
- Do NOT power ESP8266 from 5V.
- Use external 3.3V regulator if unstable.
- Ensure common ground between ESP8266 and STM32.

------------------------------------------------------------
STM32 CONFIGURATION (CubeIDE / CubeMX)
------------------------------------------------------------

ADC Configuration:

- ADC1 Enabled
- Channels: IN0 (PA0), IN1 (PA1), IN4 (PA4)
- Resolution: 12-bit
- Scan Conversion Mode: Enabled
- Continuous Conversion Mode: Enabled

UART Configuration (USART2):

- Mode: Asynchronous
- Baud Rate: 115200
- TX: PA2
- RX: PA3
- Word Length: 8 bits
- Parity: None
- Stop Bits: 1

LED Configuration:

- PC13 configured as GPIO Output
- Used for debugging and sound detection indication

------------------------------------------------------------
WORKING PRINCIPLE
------------------------------------------------------------

1. MAX9814 converts sound into analog voltage.
2. STM32 reads voltage using ADC.
3. Firmware processes intensity and direction.
4. Processed data transmitted via UART.
5. ESP8266 sends data over WiFi.

------------------------------------------------------------
OPTIONAL DEBUGGING
------------------------------------------------------------

To view serial output:

- Use USB-TTL converter
- Connect USB-TTL RX → PA2 (STM32 TX)
- Connect GND → GND
- Baud Rate: 115200

------------------------------------------------------------
COMPONENTS USED
------------------------------------------------------------

- STM32F401 Black Pill
- 3 × MAX9814 Microphone Modules
- ESP8266 WiFi Module (ESP-01)
- Breadboard
- Jumper wires
- Stable 3.3V power source
