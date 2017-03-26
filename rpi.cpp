#include "rpi.h"
#include "mailbox.h"

#include <iostream>

#define MEM_FLAG_DIRECT           (1 << 2)
#define MEM_FLAG_COHERENT         (2 << 2)
#define MEM_FLAG_L1_NONALLOCATING (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT)

#define BUS_TO_PHYS(x) ((x)&~0xC0000000)

struct bcm2835_peripheral gpio = {GPIO_BASE};
struct bcm2835_peripheral bsc0 = {BSC0_BASE};

static int mbox_fd = -1;
static int mem_fd=-1;
// Exposes the physical address defined in the passed structure using mmap on /dev/mem
int map_peripheral(struct bcm2835_peripheral *p)
{
   // Open /dev/mem
    if(mem_fd<0) {
        mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
        if(mem_fd<0) {
            printf("Failed to open /dev/mem, try checking permissions.\n");
            return -1;
        }
    }

   p->map = mmap(
      NULL,
      BLOCK_SIZE,
      PROT_READ|PROT_WRITE,
      MAP_SHARED,
      mem_fd,  // File descriptor to physical memory virtual file '/dev/mem'
      p->addr_p      // Address in physical map that we want this memory block to expose
   );

   if (p->map == MAP_FAILED) {
        perror("mmap");
        return -1;
   }

   //p->addr = (volatile unsigned int *)p->map;

   return 0;
}

void unmap_peripheral(struct bcm2835_peripheral *p) {

    munmap(const_cast<void*>(p->map), BLOCK_SIZE);
    //close(p->mem_fd);
}

void dump_bsc_status() {

    unsigned int s = BSC0_S;

    printf("BSC0_S: ERR=%d  RXF=%d  TXE=%d  RXD=%d  TXD=%d  RXR=%d  TXW=%d  DONE=%d  TA=%d\n",
        (s & BSC_S_ERR) != 0,
        (s & BSC_S_RXF) != 0,
        (s & BSC_S_TXE) != 0,
        (s & BSC_S_RXD) != 0,
        (s & BSC_S_TXD) != 0,
        (s & BSC_S_RXR) != 0,
        (s & BSC_S_TXW) != 0,
        (s & BSC_S_DONE) != 0,
        (s & BSC_S_TA) != 0 );
}

// Function to wait for the I2C transaction to complete
void wait_i2c_done() {
        //Wait till done, let's use a timeout just in case
        int timeout = 50;
        while((!((BSC0_S) & BSC_S_DONE)) && --timeout) {
            usleep(1000);
        }
        if(timeout == 0)
            printf("wait_i2c_done() timeout. Something went wrong.\n");
}

void i2c_init()
{
    INP_GPIO(0);
    SET_GPIO_ALT(0, 0);
    INP_GPIO(1);
    SET_GPIO_ALT(1, 0);
}

// Priority

int SetProgramPriority(int priorityLevel)
{
    struct sched_param sched;

    memset (&sched, 0, sizeof(sched));

    if (priorityLevel > sched_get_priority_max (SCHED_RR))
        priorityLevel = sched_get_priority_max (SCHED_RR);

    sched.sched_priority = priorityLevel;

    return sched_setscheduler (0, SCHED_RR, &sched);
}

bool alloc_phys_mem(size_t size, struct bcm2835_peripheral& p) {
    if (mbox_fd < 0) {
        mbox_fd = mbox_open();
        if(mbox_fd < 0) {
            printf("Failed to open /dev/vcio, try checking permissions.\n");
            return false;
        }
    }

    size = size % PAGE_SIZE == 0 ? size : (size + PAGE_SIZE) & ~(PAGE_SIZE - 1);
    p.size=size;
    p.handle = mem_alloc(mbox_fd, size, PAGE_SIZE, MEM_FLAG_L1_NONALLOCATING);
    p.addr_p = mem_lock(mbox_fd, p.handle);
    p.map = mapmem(BUS_TO_PHYS(p.addr_p), size);
    //fprintf(stderr, "Alloc: %6d bytes;  %p (bus=0x%08x, phys=0x%08x)\n", (int)size, result.mem, result.bus_addr, BUS_TO_PHYS(result.bus_addr));
    //assert(result.bus_addr);  // otherwise: couldn't allocate contiguous block.
    memset(const_cast<void*>(p.map), 0x00, size);

    return true;
}

bool alloc_dma_cb(int count, struct bcm2835_peripheral& p) {
    return alloc_phys_mem(sizeof(bcm2835_dma_cb)*count, p);
}

void free_phys_mem(struct bcm2835_peripheral& p) {
    if (!p.map) return;
    if(mbox_fd < 0) {
        std::clog<<"/dev/vcio is not opened"<<std::endl;
        return;
    }
      //assert(mbox_fd >= 0);  // someone should've initialized that on allocate.
    unmapmem(const_cast<void*>(p.map), p.size);
    mem_unlock(mbox_fd, p.handle);
    mem_free(mbox_fd, p.handle);
    p.map = nullptr;
}
