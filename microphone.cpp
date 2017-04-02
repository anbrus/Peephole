#include "microphone.h"

#include <cmath>
#include <iostream>
#include <thread>

int16_t zDecim1[10]={0};
int16_t zDecim2[10]={0};
int16_t zFilter[10]={0};
const float hDecim[]={ -0.0024*2, -0.0323*2, 0*2, 0.2823*2, 0.5000*2, 0.2823*2, 0*2, -0.0323*2, -0.0024*2 }; //{ -1.0/16, 0, 9.0/16, 1, 9.0/16, 0, -1.0/16 };
const float hFilter[]={ -0.0078608,  -0.0081485,   0.0763059,   0.2957418,   0.4293018,   0.2957418,  0.0763059,  -0.0081485,  -0.0078608 };

#define CS_A_STBY 1<<25
#define CS_A_SYNC 1<<24
#define CS_A_RXERR 1<<16
#define CS_A_DMAEN 1<<9
#define CS_A_RXCLR 1<<4
#define CS_A_RXON 1<<1
#define CS_A_EN 1

Microphone::Microphone()
{
    if(map_peripheral(&gpio) == -1)
    {
      std::clog<<"Failed to map the physical GPIO registers into the virtual memory space."<<std::endl;
      return;
    }

    volatile uint32_t* gpiofsel1=&reinterpret_cast<volatile uint32_t*>(gpio.map)[1];
    volatile uint32_t* gpiofsel2=&reinterpret_cast<volatile uint32_t*>(gpio.map)[2];
    *gpiofsel1=*gpiofsel1 & 0b11000000111111111111111111111111 | 0b00100100000000000000000000000000;
    *gpiofsel2=*gpiofsel2 & 0b11111111111111111111111111000000 | 0b00000000000000000000000000100100;

    if(map_peripheral(&peripheralPcm)==-1) {
        std::clog<<"Failed to map the pcm registers into the virtual memory space."<<std::endl;
        return;
    }
    if(map_peripheral(&peripheralClk)==-1) {
        std::clog<<"Failed to map the clk registers into the virtual memory space."<<std::endl;
        return;
    }
    if(map_peripheral(&peripheralDma)==-1) {
        std::clog<<"Failed to map the dma registers into the virtual memory space."<<std::endl;
        return;
    }
    if(!alloc_phys_mem(m_countDmaBuf*4, peripheralDmaBuffer)) {
        std::clog<<"Failed to alloc dma memory buffer."<<std::endl;
        return;
    }
    alloc_dma_cb(1, peripheralDmaCb);
    volatile bcm2835_dma_cb* dmaCb=reinterpret_cast<volatile bcm2835_dma_cb*>(peripheralDmaCb.map);
    dmaCb[0].ti=0b00000000000000110000010000010000;
    dmaCb[0].source_ad=0x7E203004;
    dmaCb[0].dest_ad=peripheralDmaBuffer.addr_p;
    dmaCb[0].txfr_len=m_countDmaBuf*4;
    dmaCb[0].nextconbk=peripheralDmaCb.addr_p;

    volatile bcm2835_pcm* pcm=reinterpret_cast<volatile bcm2835_pcm*>(peripheralPcm.map);
    volatile bcm2835_clk* clk=reinterpret_cast<volatile bcm2835_clk*>(peripheralClk.map);
    uint64_t freq=4*16*44100;
    uint64_t osc=500000000; //PLLD
    uint32_t divi=(osc<<12)/freq;
    clk->ctl=0x5a000216;
    clk->div=0x5a000000|divi;
}

Microphone::~Microphone() {
    Stop();

    if(peripheralDma.map) unmap_peripheral(&peripheralDma);
    if(peripheralClk.map) unmap_peripheral(&peripheralClk);
    if(peripheralPcm.map) unmap_peripheral(&peripheralPcm);
    if(peripheralDmaBuffer.map) free_phys_mem(peripheralDmaBuffer);

    peripheralPcm={ PCM_BASE };
    peripheralClk={ CLK_BASE };
    peripheralDma={ DMA_BASE };
    peripheralDmaBuffer={0};
    peripheralDmaCb={0};
}

bool Microphone::Start() {
    std::cout<<"Start recording"<<std::endl;

    if(!peripheralDma.map || !peripheralPcm.map) return false;

    volatile bcm2835_dma_channel* dma=reinterpret_cast<volatile bcm2835_dma_channel*>(peripheralDma.map);
    dma[3].CS=0b10000000000000000000000000000000;
    dma[3].CONBLK_AD=peripheralDmaCb.addr_p;
    dma[3].CS=0b00000000000000000000000000000001;

    volatile bcm2835_pcm* pcm=reinterpret_cast<volatile bcm2835_pcm*>(peripheralPcm.map);
    pcm->cs_a=  CS_A_SYNC | CS_A_STBY | CS_A_RXERR | CS_A_RXCLR | CS_A_EN; //0b00000010000000010000000000010001; //EN=1 STBY=1 RXCLR=1 RXERR=1
    pcm->mode_a=0b00000111000000000111110000010000;
    //pcm->dreq_a=
    pcm->rxc_a= 0b01000000000010000100000100001000;
    while(pcm->cs_a & (CS_A_SYNC)) ;
    pcm->cs_a=  CS_A_STBY | CS_A_RXON | CS_A_EN | CS_A_DMAEN | 0<<7; //0b00000010000000000000001010000011;
}

void Microphone::Stop() {
    if(!peripheralDma.map || !peripheralPcm.map) return;

    volatile bcm2835_dma_channel* dma=reinterpret_cast<volatile bcm2835_dma_channel*>(peripheralDma.map);
    dma[3].CS=0b10000000000000000000000000000000;
    volatile bcm2835_pcm* pcm=reinterpret_cast<volatile bcm2835_pcm*>(peripheralPcm.map);
    pcm->cs_a=  0b00000000000000000000000000000001;
    std::cout<<"Recording stopped"<<std::endl;
}

size_t Microphone::Read(int16_t* data, size_t count) {
    if(!peripheralDma.map) return 0;

    volatile bcm2835_dma_channel* dma=reinterpret_cast<volatile bcm2835_dma_channel*>(peripheralDma.map);

    if(count*4>m_countDmaBuf/2) {
        std::clog<<"Count too big"<<std::endl;
        return 0;
    }

    int countReady=0;
    for(;;) {
        int pos=(dma[3].DEST_AD-peripheralDmaBuffer.addr_p)/4;
        countReady=pos-m_posRead;
        //std::cout<<countReady<<std::endl;
        if(countReady<0) countReady=m_countDmaBuf+countReady;
        if(countReady>=count*4) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    //std::cout<<countReady<<std::endl;
    countReady=count*4;

    std::unique_ptr<int16_t[]> buf1(new int16_t[countReady]);
    std::unique_ptr<int16_t[]> buf2(new int16_t[countReady]);
    int pos=m_posRead;
    for(int n=0; n<countReady; n++) {
        buf1[n]=(reinterpret_cast<volatile uint32_t*>(peripheralDmaBuffer.map)[pos]&0xffff)-32767;
        pos++;
        if(pos>=m_countDmaBuf) pos=0;
    }
    m_posRead=pos;

    int sizeOutput=0;
    removeDc(buf1.get(), countReady, buf2.get());
    decim(2, 9, hDecim, zDecim1, countReady, buf2.get(), buf1.get(), &sizeOutput);
    decim(2, 9, hDecim, zDecim2, sizeOutput, buf1.get(), buf2.get(), &sizeOutput);
    for(int n=0; n<sizeOutput; n++) {
        data[n]=fir_basic(buf2[n], 9, hFilter, zFilter);
    }

    return sizeOutput;
}

int16_t Microphone::fir_basic(int16_t input, int ntaps, const float h[], int16_t z[])
{
    /* store input at the beginning of the delay line */
    z[0] = input;

    /* calc FIR */
    float accum = 0;
    for (int ii = 0; ii < ntaps; ii++) {
        accum += h[ii] * z[ii];
    }

    /* shift delay line */
    for (int ii = ntaps - 2; ii >= 0; ii--) {
        z[ii + 1] = z[ii];
    }

    return std::round(accum);
}

void Microphone::decim(int factor_M, int H_size, const float *const p_H,
           int16_t *const p_Z, int num_inp, const int16_t *p_inp, int16_t *p_out,
           int *p_num_out)
{
    int tap, num_out;

    /* this implementation assuems num_inp is a multiple of factor_M */
    //assert(num_inp % factor_M == 0);

    num_out = 0;
    while (num_inp >= factor_M) {
        /* shift Z delay line up to make room for next samples */
        for (tap = H_size - 1; tap >= factor_M; tap--) {
            p_Z[tap] = p_Z[tap - factor_M];
        }

        /* copy next samples from input buffer to bottom of Z delay line */
        for (tap = factor_M - 1; tap >= 0; tap--) {
            p_Z[tap] = *p_inp++;
        }
        num_inp -= factor_M;

        /* calculate FIR sum */
        float sum = 0;
        for (tap = 0; tap < H_size; tap++) {
            sum += p_H[tap] * p_Z[tap];
        }
        *p_out++ = std::round(sum);     /* store sum and point to next output */
        num_out++;
    }

    *p_num_out = num_out;   /* pass number of outputs back to caller */
}

void Microphone::removeDc(int16_t* input, size_t sizeInput, int16_t* output) {
    static const float a=1.59e-3/(1.59e-3+1.0/(44100*4));
    output[0]=a*(m_lastOut+input[0]-m_lastInput);
    for(int n=1; n<sizeInput; n++) {
        output[n]=a*(output[n-1] + input[n] - input[n-1]);
    }
    m_lastOut=output[sizeInput-1];
    m_lastInput=input[sizeInput-1];
}

