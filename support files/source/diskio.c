#include "ff.h"
#include "diskio.h"
#include "stm32f4xx.h"

#define CS_LOW()  (GPIOA->BSRR = (1 << 24)) 
#define CS_HIGH() (GPIOA->BSRR = (1 << 8))  

static uint8_t SPI_TxRx(uint8_t data) {
    while(!(SPI1->SR & (1 << 1))); 
    SPI1->DR = data;               
    while(!(SPI1->SR & (1 << 0))); 
    return SPI1->DR;               
}

static uint8_t SD_SendCommand(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t res;
    int n = 10;
    
    SPI_TxRx(cmd | 0x40);    
    SPI_TxRx((uint8_t)(arg >> 24)); 
    SPI_TxRx((uint8_t)(arg >> 16)); 
    SPI_TxRx((uint8_t)(arg >> 8));  
    SPI_TxRx((uint8_t)arg);         
    SPI_TxRx(crc);           

    do {
        res = SPI_TxRx(0xFF);
    } while ((res & 0x80) && --n);

    return res;
}

DSTATUS disk_status (BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT; 
    return 0; 
}

DSTATUS disk_initialize (BYTE pdrv) {
    uint8_t n;
    uint32_t timer = 10000;

    if (pdrv != 0) return STA_NOINIT;

    CS_HIGH();
    for (n = 10; n; n--) SPI_TxRx(0xFF); 

    CS_LOW();
    if (SD_SendCommand(0, 0, 0x95) == 1) { 
        while(timer--) {
            if (SD_SendCommand(55, 0, 0x01) <= 1 && SD_SendCommand(41, 0, 0x01) == 0) {
                break; 
            }
        }
    }
    CS_HIGH();
    SPI_TxRx(0xFF);

    if (timer == 0) return STA_NOINIT; 
    return 0; 
}

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || !count) return RES_PARERR;

    CS_LOW();
    if (SD_SendCommand(17, sector, 0x01) == 0) { 
        uint32_t timer = 100000;
        while(SPI_TxRx(0xFF) != 0xFE && timer--); 
        
        if (timer > 0) {
            for (uint32_t i = 0; i < 512; i++) {
                *buff++ = SPI_TxRx(0xFF); 
            }
            SPI_TxRx(0xFF); 
            SPI_TxRx(0xFF); 
            CS_HIGH();
            SPI_TxRx(0xFF);
            return RES_OK;
        }
    }
    CS_HIGH();
    SPI_TxRx(0xFF);
    return RES_ERROR;
}

DRESULT disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || !count) return RES_PARERR;

    CS_LOW();
    if (SD_SendCommand(24, sector, 0x01) == 0) { 
        SPI_TxRx(0xFF); 
        SPI_TxRx(0xFE); 

        for (uint32_t i = 0; i < 512; i++) {
            SPI_TxRx(*buff++); 
        }
        
        SPI_TxRx(0xFF); 
        SPI_TxRx(0xFF); 
        
        if ((SPI_TxRx(0xFF) & 0x1F) == 0x05) { 
            while(SPI_TxRx(0xFF) == 0); 
            CS_HIGH();
            SPI_TxRx(0xFF);
            return RES_OK;
        }
    }
    CS_HIGH();
    SPI_TxRx(0xFF);
    return RES_ERROR;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff) {
    (void)buff; // Silences the unused parameter warning
    if (pdrv != 0) return RES_PARERR;
    if (cmd == CTRL_SYNC) {
        CS_LOW();
        while(SPI_TxRx(0xFF) == 0); 
        CS_HIGH();
        return RES_OK;
    }
    return RES_ERROR;
}

DWORD get_fattime (void) {
    return ((DWORD)(2026 - 1980) << 25) | ((DWORD)3 << 21) | ((DWORD)9 << 16); 
}