# sense360
An assistive wearable system that detects surrounding sounds, identifies sound intensity and direction, and provides haptic alerts to help hearing-impaired users become aware of nearby calls, alarms, or hazardous noises.

📌 Project Overview

This project uses:

STM32F401 Black Pill

MAX9814 Analog MEMS Microphone

ESP8266 WiFi Module

The system reads sound intensity using the MAX9814 microphone and transmits the data to a remote server using the ESP8266 WiFi module.

🧠 System Architecture
MAX9814 → ADC (STM32) → Processing → UART → ESP8266 → WiFi

🔌 Hardware Connections
🎤 3 MEMS Microphone Connections (MAX9814)
✅ STM32F401 ADC Pins Used
Microphone Position	MAX9814 OUT → STM32 Pin	ADC Channel
Left Mic	PA0	ADC1_IN0
Right Mic	PA1	ADC1_IN1
Back Mic	PA4	ADC1_IN4
🔌 Full Wiring Details
🟢 Power Connections (All 3 Mics)
MAX9814 Pin	Connect To
VCC	3.3V
GND	GND

⚠️ All grounds must be common with STM32.

🟢 Signal Connections
Mic 1 OUT → PA0
Mic 2 OUT → PA1
Mic 3 OUT → PA4

📌 Notes:

ADC resolution: 12-bit

Input voltage range: 0–3.3V

PA0 configured as ADC1 Channel 0

🔹 2️⃣ ESP8266 WiFi Module Connections

Using UART2 (USART2)

ESP8266 Pin	STM32F401 Pin	Description
VCC	3.3V (Stable)	Power supply
GND	GND	Ground
TX	PA3	USART2_RX
RX	PA2	USART2_TX
EN	3.3V	Enable pin
RST	3.3V (Pull-up)	Reset
⚠️ Important Power Notes

ESP8266 requires stable 3.3V supply.

Do NOT power ESP8266 from 5V.

If unstable, use external 3.3V regulator.

🛠 STM32 Configuration (CubeIDE / CubeMX)
🔹 ADC Configuration

ADC1 Enabled

Channel: IN0 (PA0)

Resolution: 12-bit

Continuous Conversion Mode: Enabled

🔹 UART Configuration (USART2)

Mode: Asynchronous

Baud Rate: 115200

TX: PA2

RX: PA3

🔹 LED Configuration

PC13 configured as GPIO Output

Used for debugging / sound detection indication

📡 Working Principle

MAX9814 converts sound into analog voltage.

STM32 reads voltage via ADC.

ADC value processed in firmware.

Data transmitted to ESP8266 via UART.

ESP8266 sends data over WiFi.
🔍 Optional Debugging

To view serial output:

Use USB-TTL converter

Connect to PA2 (TX)

Baud rate: 115200

📦 Components Used

STM32F401 Black Pill

MAX9814 Microphone Module

ESP8266 WiFi Module

Breadboard

Jumper wires

3.3V power source
