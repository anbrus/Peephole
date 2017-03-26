#include "motiondetector.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory.h>

extern "C" {
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

#define DETECT_SENSITIVITY 100
//DETECT_WIDTH*DETECT_HEIGHT/10000

MotionDetector::MotionDetector(int width, int height, AVPixelFormat format):
    m_widthSrc(width),
    m_heightSrc(height),
    m_formatSrc(format)
{
    m_contextScale=sws_getContext(
        width, height, format,
        DETECT_WIDTH, DETECT_HEIGHT, AV_PIX_FMT_GRAY8,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    loadMask("mask.ppm");
}

MotionDetector::~MotionDetector() {
    if(m_contextScale) {
        sws_freeContext(m_contextScale);
        m_contextScale=nullptr;
    }
}

bool MotionDetector::loadMask(const std::string path) {
    std::ifstream s(path, std::ios_base::binary);
    if(!s.good()) {
        std::clog<<"Failed to open mask file"<<std::endl;
        return false;
    }

    std::string format, widthHeight, maxColor;
    int index=0;
    std::string tmp;
    char line[1024];
    while(s.good() && index<3) {
        s.getline(line, 1024);
        tmp=line;
        if(tmp.length()==0) continue;
        if(tmp[0]=='#') continue;

        switch(index) {
        case 0: format=tmp; break;
        case 1: widthHeight=tmp; break;
        case 2: maxColor=tmp; break;
        }
        index++;
    }
    if(!s.good()) {
        std::clog<<"Failed to read mask"<<std::endl;
        return false;
    }
    if(format!="P6") {
        std::clog<<"Invalid mask format"<<std::endl;
        return false;
    }
    tmp=std::to_string(DETECT_WIDTH); tmp+=" "; tmp+=std::to_string(DETECT_HEIGHT);
    if(widthHeight!=tmp) {
        std::clog<<"Invalid mask size"<<std::endl;
        return false;
    }

    uint8_t buf[DETECT_WIDTH*DETECT_HEIGHT];
    s.read(reinterpret_cast<char*>(buf), DETECT_WIDTH*DETECT_HEIGHT);
    if(s.gcount()!=DETECT_WIDTH*DETECT_HEIGHT) {
        std::clog<<"Invalid mask file size"<<std::endl;
        return false;
    }
    for(int dst=0, src=0; dst<DETECT_WIDTH*DETECT_HEIGHT; dst++, src+=3) {
        m_imageMask[dst]=buf[src]+buf[src+1]+buf[src+2]>0;
    }

    return true;
}

uint8_t* MotionDetector::getScaledImage(const uint8_t* frame, size_t length) {
    uint8_t* dataInput[4];
    int linesizesInput[4];
    av_image_fill_linesizes(linesizesInput, m_formatSrc, m_widthSrc);
    av_image_fill_pointers(dataInput, m_formatSrc, m_heightSrc, const_cast<uint8_t*>(frame), linesizesInput);
    int linesizes[1]={ DETECT_WIDTH };
    uint8_t* bufImage[4];
    bufImage[0]=new uint8_t[DETECT_WIDTH*DETECT_HEIGHT];
    if(sws_scale(m_contextScale, dataInput, linesizesInput, 0, m_heightSrc, bufImage, linesizes)!=DETECT_HEIGHT) {
        std::cerr<<"Scale failed"<<std::endl;
        delete[] bufImage[0];
        return nullptr;
    }

    return bufImage[0];
}

void MotionDetector::getDiffImage(const uint8_t* imageSrc, uint8_t* imageDiff) {
    const int level=16;
    for(int n=0; n<DETECT_WIDTH*DETECT_HEIGHT; n++) {
        imageDiff[n]=std::abs(imageSrc[n]-m_imagePrev[n])>level ? (m_imageMask[n] ? 255 : 0) : 0;
    }
}

void MotionDetector::dumpImage(const uint8_t* image, size_t length, int width, int height, const std::string& filepath) {
    std::ofstream outFile(filepath.c_str(), std::ios::binary);
    outFile<<"P5\n";
    outFile<<width <<" "<<height <<" 255\n";
    outFile.write(reinterpret_cast<const char*>(image), length);
}

/* Dilates a 3x3 box */
int MotionDetector::dilate9(uint8_t *img, int width, int height, uint8_t *buffer)
{
    /* - row1, row2 and row3 represent lines in the temporary buffer
     * - window is a sliding window containing max values of the columns
     *   in the 3x3 matrix
     * - widx is an index into the sliding window (this is faster than
     *   doing modulo 3 on i)
     * - blob keeps the current max value
     */
    int y, i, sum = 0, widx;
    uint8_t *row1, *row2, *row3, *rowTemp,*yp;
    unsigned char window[3], blob, latest;

    /* Set up row pointers in the temporary buffer. */
    row1 = buffer;
    row2 = row1 + width;
    row3 = row2 + width;

    /* Init rows 2 and 3. */
    memset(row2, 0, width);
    memcpy(row3, img, width);

    /* Pointer to the current row in img. */
    yp = img;

    for (y = 0; y < height; y++) {
        /* Move down one step; row 1 becomes the previous row 2 and so on. */
        rowTemp = row1;
        row1 = row2;
        row2 = row3;
        row3 = rowTemp;

        /* If we're at the last row, fill with zeros, otherwise copy from img. */
        if (y == height - 1)
            memset(row3, 0, width);
        else
            memcpy(row3, yp + width, width);

        /* Init slots 0 and 1 in the moving window. */
        window[0] = std::max(std::max(row1[0], row2[0]), row3[0]);
        window[1] = std::max(std::max(row1[1], row2[1]), row3[1]);

        /* Init blob to the current max, and set window index. */
        blob = std::max(window[0], window[1]);
        widx = 2;

        /* Iterate over the current row; index i is off by one to eliminate
         * a lot of +1es in the loop.
         */
        for (i = 2; i <= width - 1; i++) {
            /* Get the max value of the next column in the 3x3 matrix. */
            latest = window[widx] = std::max(std::max(row1[i], row2[i]), row3[i]);

            /* If the value is larger than the current max, use it. Otherwise,
             * calculate a new max (because the new value may not be the max.
             */
            if (latest >= blob)
                blob = latest;
            else
                blob = std::max(std::max(window[0], window[1]), window[2]);

            /* Write the max value (blob) to the image. */
            if (blob != 0) {
                *(yp + i - 1) = blob;
                sum++;
            }

            /* Wrap around the window index if necessary. */
            if (++widx == 3)
                widx = 0;
        }

        /* Store zeros in the vertical sides. */
        *yp = *(yp + width - 1) = 0;
        yp += width;
    }

    return sum;
}

/* Erodes a 3x3 box */
int MotionDetector::erode9(uint8_t *img, int width, int height, uint8_t *buffer, uint8_t flag)
{
    int y, i, sum = 0;
    uint8_t *Row1,*Row2,*Row3;
    Row1 = buffer;
    Row2 = Row1 + width;
    Row3 = Row1 + 2*width;
    memset(Row2, flag, width);
    memcpy(Row3, img, width);
    for (y = 0; y < height; y++) {
        memcpy(Row1, Row2, width);
        memcpy(Row2, Row3, width);
        if (y == height - 1)
            memset(Row3, flag, width);
        else
            memcpy(Row3, img+(y + 1) * width, width);

        for (i = width-2; i >= 1; i--) {
            if (Row1[i-1] == 0 ||
                Row1[i]   == 0 ||
                Row1[i+1] == 0 ||
                Row2[i-1] == 0 ||
                Row2[i]   == 0 ||
                Row2[i+1] == 0 ||
                Row3[i-1] == 0 ||
                Row3[i]   == 0 ||
                Row3[i+1] == 0)
                img[y * width + i] = 0;
            else
                sum++;
        }
        img[y * width] = img[y * width + width - 1] = flag;
    }
    return sum;
}


bool MotionDetector::Detect(const uint8_t* frame, size_t length) {
    const uint8_t* image=getScaledImage(frame, length);
    if(!image) return false;

    uint8_t imageFiltered[DETECT_WIDTH*DETECT_HEIGHT];
    std::copy(image, image+DETECT_WIDTH*DETECT_HEIGHT, imageFiltered);

    uint8_t buffer[DETECT_WIDTH*3];
    erode9(imageFiltered, DETECT_WIDTH, DETECT_HEIGHT, buffer, 0);
    dilate9(imageFiltered, DETECT_WIDTH, DETECT_HEIGHT, buffer);

    uint8_t imageDiff[DETECT_WIDTH*DETECT_HEIGHT];
    uint8_t imageObj[DETECT_WIDTH*DETECT_HEIGHT];
    getDiffImage(imageFiltered, imageDiff);
    int countDiff=0;
    for(int n=0; n<DETECT_WIDTH*DETECT_HEIGHT; n++) {
        if(imageDiff[n]) countDiff++;
        imageObj[n]=imageDiff[n] | image[n];
    }
    bool isMotion=!m_isFirstFrame && countDiff>DETECT_SENSITIVITY;
    m_isFirstFrame=false;

    //std::cout<<countDiff<<' '<<isMotion<<std::endl;

    if(isMotion) {
        std::stringstream s;
        s<<"diff_"<<std::setw(5)<<std::setfill('0')<<m_counter<<".ppm";
        dumpImage(imageObj, DETECT_WIDTH*DETECT_HEIGHT, DETECT_WIDTH, DETECT_HEIGHT, s.str());

        m_counter++;
    }

    std::copy(imageFiltered, imageFiltered+DETECT_WIDTH*DETECT_HEIGHT, m_imagePrev);
    delete[] image;

    return isMotion;
}
