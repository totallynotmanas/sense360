#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

#include "ff.h" 

void delay(volatile uint32_t t) {
    while(t--);
}

void SPI1_Init(void) {
    RCC->APB2ENR |= (1 << 12); 
    RCC->AHB1ENR |= (1 << 0);  

    GPIOA->MODER &= ~((0x3 << 10) | (0x3 << 12) | (0x3 << 14) | (0x3 << 16));
    GPIOA->MODER |=  ((0x2 << 10) | (0x2 << 12) | (0x2 << 14) | (0x1 << 16));

    GPIOA->AFR[0] &= ~((0xF << 20) | (0xF << 24) | (0xF << 28));
    GPIOA->AFR[0] |=  ((0x5 << 20) | (0x5 << 24) | (0x5 << 28));

    GPIOA->ODR |= (1 << 8);

    SPI1->CR1 = (1 << 2) | (0x3 << 3) | (1 << 9) | (1 << 8);
    SPI1->CR1 |= (1 << 6); 
}

void USART1_Init(void) {
    RCC->APB2ENR |= (1 << 4);     
    RCC->AHB1ENR |= (1 << 0);     
    GPIOA->MODER &= ~(0xF << 18);  
    GPIOA->MODER |=  (0xA << 18);  
    GPIOA->AFR[1] &= ~((0xF << 4) | (0xF << 8)); 
    GPIOA->AFR[1] |=  ((0x7 << 4) | (0x7 << 8));
    USART1->BRR = 0x8B;         
    USART1->CR1 |= (1 << 13) | (1 << 3);     
}

void USART1_SendChar(char c) {
    while(!(USART1->SR & (1 << 7)));
    USART1->DR = c;
}

void USART1_SendString(char *str) {
    while(*str) USART1_SendChar(*str++);
}

void USART2_Init(void) {
    RCC->APB1ENR |= (1 << 17);    
    RCC->AHB1ENR |= (1 << 0);     
    GPIOA->MODER &= ~(0xF << 4);  
    GPIOA->MODER |=  (0xA << 4);  
    GPIOA->AFR[0] &= ~((0xF << 8) | (0xF << 12)); 
    GPIOA->AFR[0] |=  ((0x7 << 8) | (0x7 << 12)); 
    USART2->BRR = 0x8B;         
    USART2->CR1 |= (1 << 13) | (1 << 3) | (1 << 2); 
}

void Debug_Bridge(void) {
    if (USART2->SR & ((1 << 5) | (1 << 3))) {
        char c = USART2->DR; 
        USART1_SendChar(c);  
    }
}

void ESP_Send(char *str) {
    while(*str) {
        while(!(USART2->SR & (1 << 7)));
        USART2->DR = *str++;
    }
}

void ESP_Wait_And_Debug(uint32_t ms_estimate) {
    for(uint32_t i = 0; i < ms_estimate; i++) {
        for(uint32_t j = 0; j < 8400; j++) { Debug_Bridge(); }
    }
}

void Flush_ESP(void) {
    uint32_t idle_timer = 0;
    // OPTIMIZED: Reduced from 8 million to 3 million (~200ms flush)
    while(idle_timer < 3000000) { 
        if (USART2->SR & ((1 << 5) | (1 << 3))) { 
            char c = USART2->DR;     
            USART1_SendChar(c);      
            idle_timer = 0;          
        }
        idle_timer++;
    }
}

uint8_t ESP_Wait_For(const char* expected, uint32_t timeout_ms) {
    uint32_t timer = 0;
    uint8_t match_idx = 0;
    while(timer < (timeout_ms * 15000)) { 
        if (USART2->SR & ((1 << 5) | (1 << 3))) { 
            char c = USART2->DR;
            USART1_SendChar(c); 
            if (c == expected[match_idx]) {
                match_idx++;
                if (expected[match_idx] == '\0') return 1; 
            } else {
                if (c == expected[0]) match_idx = 1; 
                else match_idx = 0;
            }
        }
        timer++;
    }
    return 0; 
}

void ESP_Wifi_Setup(void) {
    USART1_SendString("\r\n--- Initializing WiFi ---\r\n");
    ESP_Send("AT+RST\r\n");
    ESP_Wait_And_Debug(200); 
    ESP_Send("AT+CWMODE=1\r\n"); 
    ESP_Wait_And_Debug(250); 
    ESP_Send("AT+CIPMUX=0\r\n");
    ESP_Wait_And_Debug(250);
    
    USART1_SendString("\r\n--- Connecting to Router ---\r\n");
    ESP_Send("AT+CWJAP=\"EdgeNode\",\"s6@23337\"\r\n");
    ESP_Wait_And_Debug(250); 
    USART1_SendString("\r\n--- Setup Complete ---\r\n");
}

void ADC1_Init(void) {
    RCC->APB2ENR |= (1 << 8);     
    RCC->AHB1ENR |= (1 << 0);     
    GPIOA->MODER |= (3 << 0) | (3 << 2) | (3 << 8);     
    ADC1->CR2 = 0;
    ADC1->SMPR2 |= (7 << 0) | (7 << 3) | (7 << 12);  
    ADC1->CR2 |= (1 << 0);        
}

uint16_t ADC1_Read(uint8_t channel) {
    ADC1->SQR3 &= ~(0x1F);        
    ADC1->SQR3 |= channel;        
    ADC1->CR2 |= (1 << 30);       
    while(!(ADC1->SR & (1 << 1)));
    return ADC1->DR;
}

// Global Variables
FATFS fs;      
FIL fil;       
char json_payload[256];
char http_request[512]; 
char wifi_cmd[30];

int main(void) {
    USART1_Init(); 
    USART2_Init(); 
    ADC1_Init();
    SPI1_Init(); 
    
    ESP_Wifi_Setup(); 
  
    FRESULT fres;  

    fres = f_mount(&fs, "", 1); 
    if (fres == FR_OK) {
        USART1_SendString("\r\n--- SD Card Mounted Successfully! ---\r\n");
    } else {
        USART1_SendString("\r\n--- SD Card Mount FAILED! Check Wiring/FatFs. ---\r\n");
    }

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
        
        // OPTIMIZED: Reduced window to 3000 loops (~300ms reaction time)
        for(int i = 0; i < 3000; i++) {
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
        simulated_time_ms += 300; // Updated to match the new 300ms window

        uint16_t peak_vol = vol0;
        char* direction = "LEFT";
        if (vol1 > peak_vol) { peak_vol = vol1; direction = "FRONT"; }
        if (vol2 > peak_vol) { peak_vol = vol2; direction = "RIGHT"; }

        char debug_msg[64];
        sprintf(debug_msg, "Loop: %d | Peak Vol: %d | Cooldown: %d\r\n", loop_counter, peak_vol, cooldown_loops);
        USART1_SendString(debug_msg);

        int trigger_high = (peak_vol > 3000) && (cooldown_loops == 0);
        
        // Because loops are 2.5x faster, we wait 50 loops (~15 seconds) for periodic uploads
        int trigger_periodic = (loop_counter >= 50); 

        if (trigger_high || trigger_periodic) {
            
            char* priority = trigger_high ? "HIGH" : "MEDIUM";
            int intensity_pct = (peak_vol * 100) / 4095; 

            int json_len = sprintf(json_payload, 
                "{\"device_id\":\"STM32_A1\",\"event_id\":%d,\"event_time\":%u,\"intensity\":0.%02d,\"duration_ms\":300,\"direction\":\"%s\",\"priority\":\"%s\",\"confidence\":0.86}", 
                event_id++, simulated_time_ms, intensity_pct, direction, priority);

            fres = f_open(&fil, "log.txt", FA_OPEN_APPEND | FA_WRITE);
            if (fres == FR_OK) {
                f_printf(&fil, "%s\n", json_payload);
                f_close(&fil); 
                USART1_SendString("\r\n--- Saved to MicroSD Card ---\r\n");
            } else {
                USART1_SendString("\r\n--- SD Card Write Error! ---\r\n");
            }

            if (trigger_high) {
                int macro_req_len = sprintf(http_request, 
                    "GET /STM32_ALERT HTTP/1.1\r\n"
                    "Host: 10.119.171.247\r\n" 
                    "Connection: close\r\n"
                    "\r\n");

                USART1_SendString("\r\n--- Triggering Local Haptic Alert ---\r\n");
                
                ESP_Send("AT+CIPCLOSE\r\n");
                ESP_Wait_And_Debug(100); // OPTIMIZED: Shorter breather
                
                ESP_Send("AT+CIPSTART=\"TCP\",\"10.119.171.247\",8080\r\n");
                
                if (ESP_Wait_For("CONNECT", 10000)) { 
                    sprintf(wifi_cmd, "AT+CIPSEND=%d\r\n", macro_req_len);
                    ESP_Send(wifi_cmd);
                    
                    if (ESP_Wait_For(">", 1000)) {
                        ESP_Send(http_request); 
                        ESP_Wait_For("CLOSED", 500); 
                    }
                } else {
                    USART1_SendString("\r\n--- Haptic Alert TCP Failed ---\r\n");
                    Flush_ESP();
                }
                // Because loops are 2.5x faster, cooldown needs to be 15 loops to equal ~5 seconds
                cooldown_loops = 15; 
            }

            int req_len = sprintf(http_request, 
                "POST /api/v1/p3mlal8ckdbugnttgmnm/telemetry HTTP/1.1\r\n"
                "Host: thingsboard.cloud\r\n"
                "Content-Type: application/json\r\n"
                "Connection: close\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s", 
                json_len, json_payload);

            ESP_Send("AT+CIPCLOSE\r\n");
            ESP_Wait_And_Debug(250); // OPTIMIZED
            
            ESP_Send("AT\r\n");
            ESP_Wait_For("OK", 500); // OPTIMIZED
            Flush_ESP();

            USART1_SendString("\r\n--- Sending Telemetry to Cloud ---\r\n");
            ESP_Send("AT+CIPSTART=\"TCP\",\"thingsboard.cloud\",80\r\n");
            
            if (ESP_Wait_For("CONNECT", 8000)) { 
                sprintf(wifi_cmd, "AT+CIPSEND=%d\r\n", req_len);
                ESP_Send(wifi_cmd);
                ESP_Wait_For(">", 1000);
                ESP_Send(http_request); 

                if (ESP_Wait_For("200", 3000)) {
                    Flush_ESP(); 
                    USART1_SendString("\r\n--- Cloud Data Accepted! ---\r\n");
                } else {
                    USART1_SendString("\r\n--- Cloud Data Failed/Timeout ---\r\n");
                    Flush_ESP(); 
                }
            } else {
                USART1_SendString("\r\n--- Cloud TCP Connection Failed ---\r\n");
                Flush_ESP();
            }

            ESP_Send("AT+CIPCLOSE\r\n");
            loop_counter = 0; 
            ESP_Wait_And_Debug(250); // OPTIMIZED
        }
    }
}