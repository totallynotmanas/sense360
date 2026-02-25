#include "stm32f4xx.h"
#include <stdio.h>

void delay(volatile uint32_t t)
{
    while(t--);
}

void USART1_Init(void)
{
    RCC->APB2ENR |= (1 << 4);     // Enable USART1 clock
    RCC->AHB1ENR |= (1 << 0);     // Enable GPIOA clock

    // PA9 = TX, PA10 = RX 
    GPIOA->MODER &= ~(0xF << 18);  // Clear MODER for PA9 & PA10
    GPIOA->MODER |=  (0xA << 18);  // Set to Alternate mode (10)

    // Clear the Alternate Function bits first, THEN set them to AF7 (0x07)
    GPIOA->AFR[1] &= ~((0xF << 4) | (0xF << 8)); 
    GPIOA->AFR[1] |=  ((0x7 << 4) | (0x7 << 8));

    // Note: Assuming your 84MHz baud rate worked!
    USART1->BRR = 0x8B;         // 9600 baud @ 84MHz APB2 Clock
    USART1->CR1 |= (1 << 13);     // UE enable
    USART1->CR1 |= (1 << 3);      // TE enable
}

void USART1_SendChar(char c)
{
    while(!(USART1->SR & (1 << 7)));
    USART1->DR = c;
}

void USART1_SendString(char *str)
{
    while(*str)
    {
        USART1_SendChar(*str++);
    }
}

void USART2_Init(void) {
    RCC->APB1ENR |= (1 << 17);    // Enable USART2 clock
    RCC->AHB1ENR |= (1 << 0);     // Enable GPIOA clock

    // PA2 = TX (Connect to ESP RX), PA3 = RX (Connect to ESP TX)
    GPIOA->MODER &= ~(0xF << 4);  
    GPIOA->MODER |=  (0xA << 4);  

    GPIOA->AFR[0] &= ~((0xF << 8) | (0xF << 12)); 
    GPIOA->AFR[0] |=  ((0x7 << 8) | (0x7 << 12)); 

    // Baud rate for 9600 @ 42MHz APB1
    USART2->BRR = 0x8B;         
    USART2->CR1 |= (1 << 13) | (1 << 3) | (1 << 2); // UE, TE, RE (Receive Enable)
}

// Helper to check if ESP sent anything AND clear Overrun Errors
void Debug_Bridge(void) {
    // If USART2 (WiFi) has received data (RXNE) OR an Overrun Error (ORE) occurred
    if (USART2->SR & ((1 << 5) | (1 << 3))) {
        char c = USART2->DR; // Reading DR automatically clears both flags!
        USART1_SendChar(c);  // Forward it to PC
    }
}

void ESP_Send(char *str) {
    while(*str) {
        while(!(USART2->SR & (1 << 7)));
        USART2->DR = *str++;
    }
}

// A much more reliable delay that keeps checking the ESP
void ESP_Wait_And_Debug(uint32_t ms_estimate) {
    for(uint32_t i = 0; i < ms_estimate; i++) {
        for(uint32_t j = 0; j < 8400; j++) { // Roughly 1ms at 84MHz
            Debug_Bridge();
        }
    }
}

// Listens to the UART line and exits only when it has been completely silent
void Flush_ESP(void) {
    uint32_t idle_timer = 0;
    
    // INCREASED: 8,000,000 empty loops is roughly 500ms of pure silence
    while(idle_timer < 8000000) { 
        if (USART2->SR & ((1 << 5) | (1 << 3))) { // Check RXNE and ORE
            char c = USART2->DR;     
            USART1_SendChar(c);      
            idle_timer = 0;          // Reset the silence timer
        }
        idle_timer++;
    }
}

// Helper to initialize Wi-Fi connection
void ESP_Wifi_Setup(void) {
    USART1_SendString("\r\n--- Initializing WiFi ---\r\n");
    
    ESP_Send("AT+RST\r\n");
    ESP_Wait_And_Debug(1000); 

    ESP_Send("AT+CWMODE=1\r\n"); 
    ESP_Wait_And_Debug(500); 

    ESP_Send("AT+CIPMUX=0\r\n");
    ESP_Wait_And_Debug(500);
    
    USART1_SendString("\r\n--- Connecting to Router ---\r\n");
    ESP_Send("AT+CWJAP=\"EdgeNode\",\"s6@23337\"\r\n");
    ESP_Wait_And_Debug(2000); 
    
    USART1_SendString("\r\n--- Setup Complete ---\r\n");
}

void ADC1_Init(void)
{
    RCC->APB2ENR |= (1 << 8);     // Enable ADC1 clock
    RCC->AHB1ENR |= (1 << 0);     // Enable GPIOA clock

    // Configure PA0, PA1, and PA4 to Analog mode (11 in binary is 3)
    GPIOA->MODER |= (3 << 0);     // PA0 (Channel 0)
    GPIOA->MODER |= (3 << 2);     // PA1 (Channel 1)
    GPIOA->MODER |= (3 << 8);     // PA4 (Channel 4)

    ADC1->CR2 = 0;
    
    ADC1->SMPR2 |= (7 << 0) | (7 << 3) | (7 << 12);  
    ADC1->CR2 |= (1 << 0);        // ADON enable
}

uint16_t ADC1_Read(uint8_t channel)
{
    ADC1->SQR3 &= ~(0x1F);        
    ADC1->SQR3 |= channel;        
    
    ADC1->CR2 |= (1 << 30);       // Start conversion
    while(!(ADC1->SR & (1 << 1)));// Wait for conversion to finish
    return ADC1->DR;
}

// Smart Wait: Exits immediately when 'expected' string is received, or when timeout hits.
uint8_t ESP_Wait_For(const char* expected, uint32_t timeout_ms) {
    uint32_t timer = 0;
    uint8_t match_idx = 0;
    
    while(timer < (timeout_ms * 15000)) { 
        if (USART2->SR & ((1 << 5) | (1 << 3))) { // Check RXNE and ORE
            char c = USART2->DR;
            USART1_SendChar(c); 
            
            if (c == expected[match_idx]) {
                match_idx++;
                if (expected[match_idx] == '\0') {
                    return 1; // Found the exact word! Exit immediately.
                }
            } else {
                if (c == expected[0]) match_idx = 1; 
                else match_idx = 0;
            }
        }
        timer++;
    }
    return 0; // Timeout reached 
}

int main(void) {
    USART1_Init(); 
    USART2_Init(); 
    ADC1_Init();
    
    ESP_Wifi_Setup(); 

    char json_payload[256];
    char http_request[512]; 
    char wifi_cmd[30];

    int loop_counter = 0;
    int event_id = 1000;
    uint32_t simulated_time_ms = 0; 
    int cooldown_loops = 0; 

    while(1) {
        if (cooldown_loops > 0) cooldown_loops--;

        uint16_t max0 = 0, min0 = 4095;
        uint16_t max1 = 0, min1 = 4095;
        uint16_t max2 = 0, min2 = 4095;
        uint16_t sample;
        
        for(int i = 0; i < 8000; i++) {
            sample = ADC1_Read(0);
            if(sample > max0) max0 = sample; 
            if(sample < min0) min0 = sample; 

            sample = ADC1_Read(1);
            if(sample > max1) max1 = sample; 
            if(sample < min1) min1 = sample; 

            sample = ADC1_Read(4);
            if(sample > max2) max2 = sample; 
            if(sample < min2) min2 = sample; 
        }
                
        Debug_Bridge();
        
        uint16_t vol0 = max0 - min0; 
        uint16_t vol1 = max1 - min1; 
        uint16_t vol2 = max2 - min2; 
        
        loop_counter++;
        simulated_time_ms += 820; 

        uint16_t peak_vol = vol0;
        char* direction = "LEFT";
        if (vol1 > peak_vol) { peak_vol = vol1; direction = "FRONT"; }
        if (vol2 > peak_vol) { peak_vol = vol2; direction = "RIGHT"; }

        char debug_msg[64];
        sprintf(debug_msg, "Loop: %d | Peak Vol: %d | Cooldown: %d\r\n", loop_counter, peak_vol, cooldown_loops);
        USART1_SendString(debug_msg);

        int trigger_high = (peak_vol > 3500) && (cooldown_loops == 0);
        int trigger_periodic = (loop_counter >= 20); 

        if (trigger_high || trigger_periodic) {
            
            char* priority = trigger_high ? "HIGH" : "MEDIUM";
            int intensity_pct = (peak_vol * 100) / 4095; 

            int json_len = sprintf(json_payload, 
                "{\"device_id\":\"STM32_A1\",\"event_id\":%d,\"event_time\":%lu,\"intensity\":0.%02d,\"duration_ms\":820,\"direction\":\"%s\",\"priority\":\"%s\",\"confidence\":0.86}", 
                event_id++, simulated_time_ms, intensity_pct, direction, priority);

            int req_len = sprintf(http_request, 
                "POST /api/v1/p3mlal8ckdbugnttgmnm/telemetry HTTP/1.1\r\n"
                "Host: thingsboard.cloud\r\n"
                "Content-Type: application/json\r\n"
                "Connection: close\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s", 
                json_len, json_payload);

            // 1. Clean jammed sockets
            ESP_Send("AT+CIPCLOSE\r\n");
            ESP_Wait_And_Debug(1000); 

            // 2. Ping the ESP to ensure it is awake and listening!
            ESP_Send("AT\r\n");
            ESP_Wait_For("OK", 1000);
            Flush_ESP();

            // 3. Send to ThingsBoard
            USART1_SendString("\r\n--- Sending Telemetry to Cloud ---\r\n");
            ESP_Send("AT+CIPSTART=\"TCP\",\"thingsboard.cloud\",80\r\n");
            
            // Increased to 8 seconds to handle slow network resolution
            if (ESP_Wait_For("CONNECT", 8000)) { 
                sprintf(wifi_cmd, "AT+CIPSEND=%d\r\n", req_len);
                ESP_Send(wifi_cmd);
                ESP_Wait_For(">", 1000);
                ESP_Send(http_request);

                if (ESP_Wait_For("200", 3000)) {
                    Flush_ESP(); // Now safely waits 500ms for all HTTP headers to finish!
                    USART1_SendString("\r\n--- Cloud Data Accepted! ---\r\n");
                } else {
                    USART1_SendString("\r\n--- Cloud Data Failed/Timeout ---\r\n");
                    Flush_ESP(); 
                }
            } else {
                USART1_SendString("\r\n--- Cloud TCP Connection Failed ---\r\n");
                Flush_ESP();
            }

            // Close ThingsBoard connection
            ESP_Send("AT+CIPCLOSE\r\n");
            ESP_Wait_And_Debug(1000); 

            // Only trigger MacroDroid if it was a HIGH priority event
            if (trigger_high) {
                int macro_req_len = sprintf(http_request, 
                    "GET /STM32_ALERT HTTP/1.1\r\n"
                    "Host: 10.104.109.200\r\n" 
                    "Connection: close\r\n"
                    "\r\n");

                USART1_SendString("\r\n--- Triggering Local Haptic Alert ---\r\n");
                
                ESP_Send("AT+CIPCLOSE\r\n");
                ESP_Wait_And_Debug(500); 
                
                ESP_Send("AT+CIPSTART=\"TCP\",\"10.104.109.200\",8080\r\n");
                
                if (ESP_Wait_For("CONNECT", 10000)) { 
                    sprintf(wifi_cmd, "AT+CIPSEND=%d\r\n", macro_req_len);
                    ESP_Send(wifi_cmd);
                    
                    if (ESP_Wait_For(">", 1000)) {
                        ESP_Send(http_request);
                        ESP_Wait_For("CLOSED", 2000); 
                    }
                } else {
                    USART1_SendString("\r\n--- Haptic Alert TCP Failed ---\r\n");
                    Flush_ESP();
                }
                
                cooldown_loops = 6; 
            }

            loop_counter = 0; 
            ESP_Wait_And_Debug(1000); 
        }
    }
}