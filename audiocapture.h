#pragma once

#include "microphone.h"

#include <functional>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
}

class AudioCapture
{
public:
    AudioCapture();
    virtual ~AudioCapture();

    void SetCaptureHandler(std::function<void(const uint8_t* data, size_t length)> handler);
    bool Start();
    void Stop();

private:
    //snd_pcm_t* m_pcm=nullptr;
    Microphone m_mic;
    AVCodecContext* m_contextEncoder=nullptr;
    unsigned int m_rate=44100;
    std::function<void(const uint8_t* data, size_t length)> m_handler;
    std::thread m_thread;
    bool m_stop=false;
};
