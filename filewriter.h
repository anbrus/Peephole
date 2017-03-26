#pragma once

extern "C" {
#include <libavformat/avformat.h>
}

#include <string>
#include <mutex>

class FileWriter
{
public:
    FileWriter();
    virtual ~FileWriter();

    bool Create(const std::string pathFile);
    bool WriteHeader(const uint8_t* data, int length);
    bool WriteVideo(const uint8_t* data, int length, bool isKeyFrame);
    bool WriteAudio(const uint8_t* data, int length);
    bool WriteTrailer();
    void Close();

private:
    AVFormatContext* m_context=nullptr;
    std::mutex m_mutexWrite;
    int64_t m_ptsVideo=0;
    int64_t m_ptsAudio=0;
};
