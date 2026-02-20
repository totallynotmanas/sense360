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
    USART1->BRR = 0x683;         // 9600 baud @ 84MHz APB2 Clock
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

// --- Add these to your USART functions ---

void USART2_Init(void) {
    RCC->APB1ENR |= (1 << 17);    // Enable USART2 clock
    RCC->AHB1ENR |= (1 << 0);     // Enable GPIOA clock

    // PA2 = TX (Connect to ESP RX), PA3 = RX (Connect to ESP TX)
    GPIOA->MODER &= ~(0xF << 4);  
    GPIOA->MODER |=  (0xA << 4);  

    GPIOA->AFR[0] &= ~((0xF << 8) | (0xF << 12)); 
    GPIOA->AFR[0] |=  ((0x7 << 8) | (0x7 << 12)); 

    // Baud rate for 9600 @ 42MHz APB1
    USART2->BRR = 0x1117;         
    USART2->CR1 |= (1 << 13) | (1 << 3) | (1 << 2); // UE, TE, RE (Receive Enable)
}

// Helper to check if ESP sent anything and forward it to PC
void Debug_Bridge(void) {
    // If USART2 (WiFi) has received data
    if (USART2->SR & (1 << 5)) {
        char c = USART2->DR;
        USART1_SendChar(c); // Forward it to PC
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

// Helper to initialize Wi-Fi connection
void ESP_Wifi_Setup(void) {
    USART1_SendString("\r\n--- Initializing WiFi ---\r\n");
    
    // 1. Reset Module (Good practice to clear previous hangs)
    ESP_Send("AT+RST\r\n");
    for(int i=0; i<2000000; i++) Debug_Bridge(); 

    // 2. Set Station Mode
    ESP_Send("AT+CWMODE=1\r\n"); 
    for(int i=0; i<1000000; i++) Debug_Bridge();
    
    // 3. Connect to WiFi - THIS TAKES TIME
    USART1_SendString("Connecting to Router...\r\n");
    ESP_Send("AT+CWJAP=\"Alana\",\"12345679\"\r\n");
    
    // Wait about 10 seconds for connection
    for(int i=0; i<15000000; i++) Debug_Bridge(); 
    
    // 4. Start UDP
    USART1_SendString("Opening UDP Port...\r\n");
    // Verify this IP matches your Laptop's Wi-Fi IP!
    ESP_Send("AT+CIPSTART=\"UDP\",\"192.168.x.x\",5005\r\n"); 
    for(int i=0; i<2000000; i++) Debug_Bridge();
    
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
    
    // Set sampling time for Channels 0, 1, and 4
    ADC1->SMPR2 |= (7 << 0) | (7 << 3) | (7 << 12);  
    
    ADC1->CR2 |= (1 << 0);        // ADON enable
}

// Updated to accept a channel parameter
uint16_t ADC1_Read(uint8_t channel)
{
    // Clear the first 5 bits of SQR3 (the 1st conversion slot)
    ADC1->SQR3 &= ~(0x1F);        
    
    // Insert our target channel into SQR3
    ADC1->SQR3 |= channel;        
    
    ADC1->CR2 |= (1 << 30);       // Start conversion
    while(!(ADC1->SR & (1 << 1)));// Wait for conversion to finish
    return ADC1->DR;
}

int main(void) {
    USART1_Init(); // Debug/Serial Monitor
    USART2_Init(); // Wi-Fi Module
    ADC1_Init();
    
    ESP_Wifi_Setup(); // Run once to connect

    char buffer[100];
    char wifi_cmd[20];
		uint16_t sample;

    while(1) {
			
			
        // Setup Max/Min trackers for all 3 microphones
        uint16_t max0 = 0, min0 = 4095;
        uint16_t max1 = 0, min1 = 4095;
        uint16_t max2 = 0, min2 = 4095;
        
        // Sample all three repeatedly for the "window"
        for(int i = 0; i < 8000; i++) 
        {
            // Read Mic 0 (PA0 -> Channel 0)
            sample = ADC1_Read(0);
            if(sample > max0) { max0 = sample; }
            if(sample < min0) { min0 = sample; }

            // Read Mic 1 (PA1 -> Channel 1)
            sample = ADC1_Read(1);
            if(sample > max1) { max1 = sample; }
            if(sample < min1) { min1 = sample; }

            // Read Mic 2 (PA4 -> Channel 4)
            sample = ADC1_Read(4);
            if(sample > max2) { max2 = sample; }
            if(sample < min2) { min2 = sample; }
        }
				
				Debug_Bridge();
        
        uint16_t vol0 = max0 - min0;
        uint16_t vol1 = max1 - min1;
        uint16_t vol2 = max2 - min2;

        // Prepare the data string
        int len = sprintf(buffer, "M0:%d,M1:%d,M2:%d\n", vol0, vol1, vol2);
        
        // 1. Send to local PC via USB (USART1)
        USART1_SendString(buffer);

        // 2. Send to Laptop via Wi-Fi (USART2)
        sprintf(wifi_cmd, "AT+CIPSEND=%d\r\n", len);
        ESP_Send(wifi_cmd);
        delay(50000); // Small wait for ESP to be ready
        ESP_Send(buffer);

        delay(500000); 
    }
}