#pragma once

#include <cstdint>
#include <string>
#include <stddef.h>

extern "C" {
    #include <libavutil/pixfmt.h>
}

#define DETECT_WIDTH 240
#define DETECT_HEIGHT 160

class MotionDetector
{
public:
    MotionDetector(int width, int height, AVPixelFormat format);
    virtual ~MotionDetector();

    bool Detect(const uint8_t* frame, size_t length);

private:
    struct SwsContext* m_contextScale=nullptr;
    AVPixelFormat m_formatSrc;
    int m_widthSrc, m_heightSrc;
    uint8_t m_imagePrev[DETECT_WIDTH*DETECT_HEIGHT]={0};
    bool m_imageMask[DETECT_WIDTH*DETECT_HEIGHT];
    int m_counter=0;
    bool m_isFirstFrame=true;

    uint8_t* getScaledImage(const uint8_t* frame, size_t length);
    void dumpImage(const uint8_t* image, size_t length, int width, int height, const std::string& filepath);
    void getDiffImage(const uint8_t* imageSrc, uint8_t* imageDiff);
    int dilate9(uint8_t *img, int width, int height, uint8_t *buffer);
    int erode9(uint8_t *img, int width, int height, uint8_t *buffer, uint8_t flag);
    bool loadMask(const std::string path);
};

