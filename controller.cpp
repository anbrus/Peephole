#include "controller.h"

#include "camera.h"
#include "cameracapture.h"
#include "camerapreviewport.h"

#include "bcm_host.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>

extern "C" {
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

Controller::Controller():
    m_md(SCREEN_WIDTH, SCREEN_HEIGHT, AV_PIX_FMT_RGB565),
    m_captureVideo(m_cam)
{

}

Controller::~Controller() {

}

uint8_t* Controller::getScreenImage(const uint8_t* frame, size_t length) {
    uint8_t* dataInput[4];
    int linesizesInput[4];
    av_image_fill_linesizes(linesizesInput, AV_PIX_FMT_YUV420P, 1920);
    av_image_fill_pointers(dataInput, AV_PIX_FMT_YUV420P, 1088, const_cast<uint8_t*>(frame), linesizesInput);
    int linesizes[1]={ SCREEN_WIDTH*2 };
    uint8_t* bufImage[4];
    bufImage[0]=new uint8_t[SCREEN_WIDTH*SCREEN_HEIGHT*2];
    if(sws_scale(m_contextScale, dataInput, linesizesInput, 0, 1080, bufImage, linesizes)!=SCREEN_HEIGHT) {
        std::cerr<<"Scale failed"<<std::endl;
        delete[] bufImage[0];
        return nullptr;
    }

    return bufImage[0];
}

void Controller::onPreviewFrame(const uint8_t* data, size_t length) {
    const uint8_t* imageScreen=getScreenImage(data, length);
    if(!imageScreen) return;

    m_wnd.ShowFrame(imageScreen, SCREEN_WIDTH*SCREEN_HEIGHT*2);
    if(m_counterCameraInit==0) {
        if(m_counterPreviewFrame%5==0 && m_md.Detect(imageScreen, SCREEN_WIDTH*SCREEN_HEIGHT*2)) {
            if(m_captureInProgress) updateMotionTime();
            else postRunnable([this] { startCapture(); });
        }
    }else m_counterCameraInit--;

    m_counterPreviewFrame++;

    delete[] imageScreen;
}

void Controller::onCaptureFrame(const uint8_t* data, size_t length, int typeFrame) {
    switch(typeFrame) {
    case 0: m_file.WriteVideo(data, length, false); break;
    case 1: m_file.WriteVideo(data, length, true); break;
    case 2: m_file.WriteHeader(data, length); break;
    }

    std::chrono::steady_clock::duration elapsed=std::chrono::steady_clock::now()-m_timeLastMotion;
    if(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count()>POST_MOTION_SECONDS)
        postRunnable([this] { stopCapture(); });
}

void Controller::onCaptureAudio(const uint8_t* data, size_t length) {
    m_file.WriteAudio(data, length);
}

void Controller::updateMotionTime() {
    m_timeLastMotion=std::chrono::steady_clock::now();
}

void Controller::startCapture() {
    std::cout<<"Start capture"<<std::endl;

    m_timeLastMotion=std::chrono::steady_clock::now();

    std::stringstream s;
    std::time_t now=std::time(nullptr);
    std::tm tm = *std::localtime(&now);
    char timeStr[64];
    std::strftime(timeStr, 64, "%Y%m%d_%H%M%S", &tm);
    s<<"motion_"<<timeStr<<".mp4";
    m_file.Create(s.str());
    //m_file.WriteHeader();

    m_captureVideo.Start();
    m_captureAudio.Start();
    m_captureInProgress=true;
}

void Controller::stopCapture() {
    if(!m_captureInProgress) return;

    std::cout<<"Stop capture"<<std::endl;

    m_captureVideo.Stop();
    m_captureAudio.Stop();
    m_file.WriteTrailer();
    m_file.Close();

    m_captureInProgress=false;
}

void Controller::postRunnable(const std::function<void()>& runnable) {
    std::lock_guard<std::mutex> lck(m_mutexQueue);
    m_queue.push(runnable);
    m_cvQueue.notify_all();
}

int Controller::Run() {
    bcm_host_init();

    m_wnd.Show();
    m_contextScale=sws_getContext(
        1920, 1080, AV_PIX_FMT_YUV420P,
        SCREEN_WIDTH, SCREEN_HEIGHT, AV_PIX_FMT_RGB565,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    if(!m_cam.Create()) return 1;

    CameraPreviewPort p;
    p.SetPreviewHandler([this](const uint8_t* data, size_t length) { onPreviewFrame(data, length); });
    p.Run(m_cam);

    m_captureVideo.SetCaptureHandler([this](const uint8_t* data, size_t length, int typeFrame) { onCaptureFrame(data, length, typeFrame); });
    m_captureAudio.SetCaptureHandler([this](const uint8_t* data, size_t length) { onCaptureAudio(data, length); });

    while(m_counterCameraInit) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    /*startCapture();
    std::this_thread::sleep_for(std::chrono::seconds(8));
    stopCapture();*/
    /*for(int n=0; n<50; n++) {
        startCapture();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        stopCapture();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }*/

    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point stop=start+std::chrono::hours(10);
    while(std::chrono::steady_clock::now()<stop) {
        std::unique_lock<std::mutex> lck(m_mutexQueue);
        m_cvQueue.wait_for(lck, std::chrono::seconds(1), [this]{ return !m_queue.empty(); });
        while(!m_queue.empty()) {
            m_queue.front()();
            m_queue.pop();
        }
    }

    //std::this_thread::sleep_for(std::chrono::seconds(600));

    p.Stop(m_cam);

    stopCapture();

    m_wnd.Close();

    if(m_contextScale) {
        sws_freeContext(m_contextScale);
        m_contextScale=nullptr;
    }

    bcm_host_deinit();

    return 0;
}
