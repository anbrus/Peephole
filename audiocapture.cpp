#include "audiocapture.h"

//#include "alsaexception.h"
#include "avexception.h"

#include <libavutil/channel_layout.h>
#include <iostream>
#include <fstream>

//#define FRAME_RATE 5

AudioCapture::AudioCapture()
{
    //snd_pcm_hw_params_t *paramsHw=nullptr;
    //try {
        avcodec_register_all();
        av_log_set_level(AV_LOG_DEBUG);
        AVCodec* encoder=avcodec_find_encoder(AV_CODEC_ID_MP3);
        m_contextEncoder=avcodec_alloc_context3(encoder);
        m_contextEncoder->bit_rate=64000;
        m_contextEncoder->sample_fmt = AV_SAMPLE_FMT_S16;
        m_contextEncoder->sample_rate=44100;
        m_contextEncoder->channels=1;
        m_contextEncoder->channel_layout=AV_CH_LAYOUT_MONO;
        int err=avcodec_open2(m_contextEncoder, encoder, NULL);
        if(err<0) {
            AvException e(__FILE__, __LINE__, err);
            std::clog<<e.GetFile()<<"("<<e.GetLine()<<"): "<<e.what()<<std::endl;
        }

        /*err=snd_pcm_open(&m_pcm, "hw:1,0", SND_PCM_STREAM_CAPTURE, 0);
        if(err<0) throw AlsaException(__FILE__, __LINE__, err);

        err=snd_pcm_hw_params_malloc(&paramsHw);
        if(err<0) throw AlsaException(__FILE__, __LINE__, err);

        err=snd_pcm_hw_params_any(m_pcm, paramsHw);
        if(err<0) throw AlsaException(__FILE__, __LINE__, err);

        err=snd_pcm_hw_params_set_access(m_pcm, paramsHw, SND_PCM_ACCESS_RW_INTERLEAVED);
        if(err<0) throw AlsaException(__FILE__, __LINE__, err);
        err=snd_pcm_hw_params_set_format(m_pcm, paramsHw, SND_PCM_FORMAT_S16_LE);
        if(err<0) throw AlsaException(__FILE__, __LINE__, err);
        err=snd_pcm_hw_params_set_rate_near(m_pcm, paramsHw, &m_rate, 0);
        if(err<0) throw AlsaException(__FILE__, __LINE__, err);
        err=snd_pcm_hw_params_set_channels(m_pcm, paramsHw, 1);
        if(err<0) throw AlsaException(__FILE__, __LINE__, err);
        snd_pcm_uframes_t countPeriods=32;
        snd_pcm_uframes_t sizePeriod = m_contextEncoder->frame_size;
        err=snd_pcm_hw_params_set_periods(m_pcm, paramsHw, countPeriods, 0);
        if(err<0) throw AlsaException(__FILE__, __LINE__, err);
        snd_pcm_uframes_t size = (sizePeriod * countPeriods) >> 2;
        err=snd_pcm_hw_params_set_buffer_size_near(m_pcm, paramsHw, &size);
        if(err<0) throw AlsaException(__FILE__, __LINE__, err);
        err=snd_pcm_hw_params(m_pcm, paramsHw);
        if(err<0) throw AlsaException(__FILE__, __LINE__, err);*/
    //}catch(const AlsaException& e) {
    //    std::clog<<e.GetFile()<<"("<<e.GetLine()<<"): "<<e.what()<<std::endl;
    //}
    //if(paramsHw) snd_pcm_hw_params_free(paramsHw);
}

AudioCapture::~AudioCapture()
{
//    if(m_pcm) {
//        snd_pcm_close(m_pcm);
//        m_pcm=nullptr;
//    }
    if(m_contextEncoder) {
        avcodec_free_context(&m_contextEncoder);
        m_contextEncoder=nullptr;
    }
}

void AudioCapture::SetCaptureHandler(std::function<void(const uint8_t* data, size_t length)> handler) {
    m_handler=handler;
}

bool AudioCapture::Start() {
    Stop();

    m_stop=false;
    m_thread=std::thread([this]{
        AVFrame* frame=av_frame_alloc();
        frame->nb_samples=m_contextEncoder->frame_size;
        frame->format=m_contextEncoder->sample_fmt;
        frame->channel_layout=m_contextEncoder->channel_layout;
        int err=av_frame_get_buffer(frame, 0);
        if(err<0) {
            AvException e(__FILE__, __LINE__, err);
            std::clog<<e.GetFile()<<"("<<e.GetLine()<<"): "<<e.what()<<std::endl;

            av_frame_free(&frame);
            return;
        }
        err=av_frame_make_writable(frame);
        if(err<0) {
            AvException e(__FILE__, __LINE__, err);
            std::clog<<e.GetFile()<<"("<<e.GetLine()<<"): "<<e.what()<<std::endl;

            av_frame_free(&frame);
            return;
        }

        //std::ofstream out("1.mp3", std::ios_base::binary);

        m_mic.Start();
        uint8_t buf[65536];
        while(!m_stop) {
            int readedSamples=m_mic.Read(reinterpret_cast<int16_t*>(buf), m_contextEncoder->frame_size); //snd_pcm_readi(m_pcm, buf, m_contextEncoder->frame_size);
            //out.write(reinterpret_cast<char*>(buf), readedSamples*2);
            /*for(int n=0; n<m_contextEncoder->frame_size; n++) {
                reinterpret_cast<uint16_t*>(buf)[n]=n%100;
            }*/
            //std::cout<<readedSamples<<std::endl;
            if(readedSamples<=0) {
                //AlsaException e(__FILE__, __LINE__, readedSamples);
                //std::clog<<e.GetFile()<<"("<<e.GetLine()<<"): "<<e.what()<<std::endl;
                break;
            }

            memcpy(frame->buf[0]->data, buf, m_contextEncoder->frame_size*2);
            frame->buf[0]->size=m_contextEncoder->frame_size*2;
            AVPacket pkt;
            av_init_packet(&pkt);
            pkt.data=nullptr;
            pkt.size=0;

            int isPacket=0;
            int err=avcodec_encode_audio2(m_contextEncoder, &pkt, frame, &isPacket);
            if(err<0) {
                AvException e(__FILE__, __LINE__, err);
                std::clog<<e.GetFile()<<"("<<e.GetLine()<<"): "<<e.what()<<std::endl;
                break;
            }

            //std::cout<<"Audio: "<<pkt.size<<std::endl;
            if(isPacket) {
                if(m_handler) m_handler(reinterpret_cast<uint8_t*>(pkt.data), pkt.size);
                //out.write(reinterpret_cast<char*>(pkt.data), pkt.size);
                av_packet_unref(&pkt);
            }
        }
        av_frame_free(&frame);
        m_mic.Stop();
    });
}

void AudioCapture::Stop() {
    m_stop=true;
    if(m_thread.joinable()) m_thread.join();
}
