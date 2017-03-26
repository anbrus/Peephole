#pragma once

#include "rpi.h"

#include <cstdint>
#include <cstdlib>

class Microphone
{
public:
    Microphone();
    virtual ~Microphone();

    bool Start();
    void Stop();
    size_t Read(int16_t* data, size_t count);

private:
    bcm2835_peripheral peripheralPcm={ PCM_BASE };
    bcm2835_peripheral peripheralClk={ CLK_BASE };
    bcm2835_peripheral peripheralDma={ DMA_BASE };
    bcm2835_peripheral peripheralDmaBuffer;
    bcm2835_peripheral peripheralDmaCb;
    static const size_t m_countDmaBuf=44100*4;
    unsigned m_posRead=0;
    int16_t m_lastOut=0, m_lastInput=0;

    int16_t fir_basic(int16_t input, int ntaps, const float h[], int16_t z[]);
    void decim(int factor_M, int H_size, const float *const p_H,
               int16_t *const p_Z, int num_inp, const int16_t *p_inp, int16_t *p_out,
               int *p_num_out);
    void removeDc(int16_t* input, size_t sizeInput, int16_t* output);
};

