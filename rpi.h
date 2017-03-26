#pragma once

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>

#include <sched.h>		// To set the priority on linux

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdint.h>

// Define which Raspberry Pi board are you using. Take care to have defined only one at time.
//#define RPI
//#define RPI2

#ifdef RPI
#define BCM2708_PERI_BASE       0x20000000
#define GPIO_BASE               (BCM2708_PERI_BASE + 0x200000)	// GPIO controller
#define BSC0_BASE 		(BCM2708_PERI_BASE + 0x205000)	// I2C controller
#endif

#ifdef RPI2
#define BCM2708_PERI_BASE       0x3F000000
#define GPIO_BASE               (BCM2708_PERI_BASE + 0x00200000)	// GPIO controller. Maybe wrong. Need to be tested.
#define BSC0_BASE 		(BCM2708_PERI_BASE + 0x804000)	// I2C controller
#define PCM_BASE (BCM2708_PERI_BASE + 0x00203000) //0x7E203000
#define CLK_BASE (BCM2708_PERI_BASE + 0x00101000) //0x7E101098
#define DMA_BASE (BCM2708_PERI_BASE + 0x00007000) //0x7E007300
#endif

#define PAGE_SIZE 		(4*1024)
#define BLOCK_SIZE 		(4*1024)

// IO Acces
struct bcm2835_peripheral {
    unsigned long addr_p;
    //int mem_fd;
    volatile void *map;
    //volatile unsigned int *addr;
    unsigned int handle;
    size_t size;
};

extern struct bcm2835_peripheral gpio; 	// They have to be found somewhere, but can't be in the header
extern struct bcm2835_peripheral bsc0;	// so use extern!!

struct bcm2835_pcm {
    uint32_t cs_a;
    uint32_t fifo_a;
    uint32_t mode_a;
    uint32_t rxc_a;
    uint32_t txc_a;
    uint32_t dreq_a;
    uint32_t inten_a;
    uint32_t intstc_a;
    uint32_t gray;
};

struct bcm2835_clk {
    uint8_t reserved[0x98];
    uint32_t ctl;
    uint32_t div;
    //uint32_t ctl1;
    //uint32_t div1;
    //uint32_t ctl2;
    //uint32_t div2;
};

struct bcm2835_dma_channel {
    uint32_t CS;
    uint32_t CONBLK_AD;
    uint32_t TI;
    uint32_t SOURCE_AD;
    uint32_t DEST_AD;
    uint32_t TXFR_LEN;
    uint32_t STRIDE;
    uint32_t NEXTCONBK;
    uint32_t DEBUG;
    uint32_t reserved[55];
};

struct bcm2835_dma_cb {
    uint32_t ti;
    uint32_t source_ad;
    uint32_t dest_ad;
    uint32_t txfr_len;
    uint32_t stride;
    uint32_t nextconbk;
};

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) 	*(reinterpret_cast<volatile uint32_t*>(gpio.map) + ((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) 	*(reinterpret_cast<volatile uint32_t*>(gpio.map) + ((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(reinterpret_cast<volatile uint32_t*>(gpio.map) + (((g)/10))) |= (((a)<=3?(a) + 4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET 	*(reinterpret_cast<volatile uint32_t*>(gpio.map) + 7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR 	*(reinterpret_cast<volatile uint32_t*>(gpio.map) + 10) // clears bits which are 1 ignores bits which are 0

#define GPIO_READ(g) 	*(gpio.addr + 13) &= (1<<(g))

// I2C macros
#define BSC0_C        	*(reinterpret_cast<volatile uint32_t*>(bsc0.map) + 0x00)
#define BSC0_S        	*(reinterpret_cast<volatile uint32_t*>(bsc0.map) + 0x01)
#define BSC0_DLEN    	*(reinterpret_cast<volatile uint32_t*>(bsc0.map) + 0x02)
#define BSC0_A        	*(reinterpret_cast<volatile uint32_t*>(bsc0.map) + 0x03)
#define BSC0_FIFO    	*(reinterpret_cast<volatile uint32_t*>(bsc0.map) + 0x04)

#define BSC_C_I2CEN    	(1 << 15)
#define BSC_C_INTR    	(1 << 10)
#define BSC_C_INTT    	(1 << 9)
#define BSC_C_INTD    	(1 << 8)
#define BSC_C_ST    	(1 << 7)
#define BSC_C_CLEAR    	(1 << 4)
#define BSC_C_READ    	1

#define START_READ    	BSC_C_I2CEN|BSC_C_ST|BSC_C_CLEAR|BSC_C_READ
#define START_WRITE   	BSC_C_I2CEN|BSC_C_ST

#define BSC_S_CLKT	(1 << 9)
#define BSC_S_ERR    	(1 << 8)
#define BSC_S_RXF    	(1 << 7)
#define BSC_S_TXE    	(1 << 6)
#define BSC_S_RXD    	(1 << 5)
#define BSC_S_TXD    	(1 << 4)
#define BSC_S_RXR    	(1 << 3)
#define BSC_S_TXW    	(1 << 2)
#define BSC_S_DONE   	(1 << 1)
#define BSC_S_TA    	1

#define CLEAR_STATUS    BSC_S_CLKT|BSC_S_ERR|BSC_S_DONE


// Function prototypes
int map_peripheral(struct bcm2835_peripheral *p);
void unmap_peripheral(struct bcm2835_peripheral *p);

// I2C
void dump_bsc_status();
void wait_i2c_done();
void i2c_init();
// Priority
int SetProgramPriority(int priorityLevel);
bool alloc_dma_cb(int count, struct bcm2835_peripheral& p);
void free_phys_mem(struct bcm2835_peripheral& p);
bool alloc_phys_mem(size_t size, struct bcm2835_peripheral& p);
