#include "filewriter.h"

#include "avexception.h"

#include <iostream>

#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>

FileWriter::FileWriter()
{
    av_register_all();
}

FileWriter::~FileWriter() {
    avformat_free_context(m_context);
}

bool FileWriter::Create(const std::string pathFile) {
    std::lock_guard<std::mutex> lck(m_mutexWrite);
    m_ptsVideo=0;
    m_ptsAudio=0;
    m_context=avformat_alloc_context();
    m_context->oformat=av_guess_format(nullptr, pathFile.c_str(), nullptr);
    strcpy(m_context->filename, pathFile.c_str());
    m_context->flags|=0;
    m_context->debug=1;
    if(avio_open(&m_context->pb, pathFile.c_str(), AVIO_FLAG_WRITE)<0) {
        std::cerr<<"Could not open "<<pathFile<<std::endl;
        return false;
    }

    AVCodec* codecVideo=avcodec_find_encoder(AV_CODEC_ID_H264);
    AVStream* m_streamVideo=avformat_new_stream(m_context, codecVideo);
    m_streamVideo->time_base=(AVRational){ 1, 5 };
    //m_streamVideo->codec=avcodec_alloc_context3(codecVideo);
    m_streamVideo->codec->flags|=CODEC_FLAG_GLOBAL_HEADER;
    m_streamVideo->codec->codec_id=AV_CODEC_ID_H264;
    m_streamVideo->codec->codec_type=AVMEDIA_TYPE_VIDEO;
    m_streamVideo->codec->bit_rate = 2000000;
    m_streamVideo->codec->width=1920;
    m_streamVideo->codec->height=1080;
    m_streamVideo->codec->time_base=m_streamVideo->time_base;
    m_streamVideo->codec->gop_size=0;
    m_streamVideo->codec->pix_fmt=AV_PIX_FMT_YUV420P;

    AVCodec* codecAudio=avcodec_find_encoder(AV_CODEC_ID_MP3);
    AVStream* m_streamAudio=avformat_new_stream(m_context, codecAudio);
    m_streamAudio->time_base=(AVRational){ 1, 44100 };
    //m_streamAudio->codec=avcodec_alloc_context3(codecAudio);
    m_streamAudio->codec->flags|=CODEC_FLAG_GLOBAL_HEADER;
    m_streamAudio->codec->codec_id=AV_CODEC_ID_MP3;
    m_streamAudio->codec->codec_type=AVMEDIA_TYPE_AUDIO;
    m_streamAudio->codec->bit_rate = 64000;
    m_streamAudio->codec->sample_rate=44100;
    m_streamAudio->codec->channel_layout=AV_CH_LAYOUT_MONO;
    m_streamAudio->codec->channels=1;
    m_streamAudio->codec->sample_fmt=AVSampleFormat::AV_SAMPLE_FMT_S16;
    m_streamAudio->codec->time_base=m_streamAudio->time_base;
    //m_streamAudio->codec->gop_size=0;

    return true;
}

bool FileWriter::WriteHeader(const uint8_t* data, int length) {
    std::lock_guard<std::mutex> lck(m_mutexWrite);
    m_context->streams[0]->codec->extradata_size = length;
    m_context->streams[0]->codec->extradata = (uint8_t*)av_malloc(m_context->streams[0]->codec->extradata_size);
    memcpy(m_context->streams[0]->codec->extradata, data, length);

    int res=avformat_write_header(m_context, nullptr);
    if(res<0) {
        AvException e(__FILE__, __LINE__, res);
        std::clog<<e.GetFile()<<"("<<e.GetLine()<<"): "<<e.what()<<std::endl;
    }

    std::cout<<"Header writed"<<std::endl;
}

bool FileWriter::WriteVideo(const uint8_t* data, int length, bool isKeyFrame) {
    std::lock_guard<std::mutex> lck(m_mutexWrite);

    av_log_set_level(AV_LOG_DEBUG);
    AVPacket packet={ 0 };
    av_init_packet(&packet);
    packet.data=const_cast<uint8_t*>(data);
    packet.size=length;
    packet.stream_index=0;
    packet.flags=isKeyFrame ? AV_PKT_FLAG_KEY : 0;
    packet.pts=m_ptsVideo; //av_rescale_q(m_ptsVideo, m_context->streams[0]->codec->time_base, m_context->streams[0]->time_base);
    packet.dts=m_ptsVideo;
    packet.duration=1;
    //std::cout<<"Video: "<<packet.pts<<std::endl;
    int res=av_interleaved_write_frame(m_context, &packet);
    if(res<0) {
        AvException e(__FILE__, __LINE__, res);
        std::clog<<e.GetFile()<<"("<<e.GetLine()<<"): "<<e.what()<<std::endl;
    }
    m_ptsVideo++;

    //flush
    //res=av_interleaved_write_frame(m_context, nullptr);
    //if(res<0) {
    //    AvException e(__FILE__, __LINE__, res);
    //    std::clog<<e.GetFile()<<"("<<e.GetLine()<<"): "<<e.what()<<std::endl;
    //}
}

bool FileWriter::WriteAudio(const uint8_t* data, int length) {
    std::lock_guard<std::mutex> lck(m_mutexWrite);

    AVPacket packet={ 0 };
    av_init_packet(&packet);
    packet.data=const_cast<uint8_t*>(data);
    packet.size=length;
    packet.stream_index=1;
    packet.pts=m_ptsAudio; //av_rescale_q(m_ptsAudio, m_context->streams[1]->codec->time_base, m_context->streams[1]->time_base);
    packet.dts=m_ptsAudio;
    packet.duration=1152;
    //std::cout<<"Audio: "<<packet.pts<<std::endl;
    int res=av_interleaved_write_frame(m_context, &packet);
    if(res<0) {
        AvException e(__FILE__, __LINE__, res);
        std::clog<<e.GetFile()<<"("<<e.GetLine()<<"): "<<e.what()<<std::endl;
    }
    m_ptsAudio+=1152;
}

bool FileWriter::WriteTrailer() {
    av_write_trailer(m_context);
}

void FileWriter::Close() {
    //avcodec_free_context(&m_context->streams[0]->codec);
    avformat_free_context(m_context);
    m_context=nullptr;
}
