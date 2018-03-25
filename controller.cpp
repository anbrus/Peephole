#include "controller.h"

#include "camera.h"
#include "cameracapture.h"
#include "camerapreviewport.h"
#include "config.h"

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

Controller* pController=nullptr;

Controller::Controller():
    m_md(WIDTH_SCREEN, HEIGHT_SCREEN, AV_PIX_FMT_RGB565),
    m_captureVideo(m_cam)
{
    pController=this;
}

Controller::~Controller() {
    pController=nullptr;
}

uint8_t* Controller::getScreenImage(const uint8_t* frame, size_t length) {
    uint8_t* dataInput[4];
    int linesizesInput[4];
    av_image_fill_linesizes(linesizesInput, AV_PIX_FMT_YUV420P, WIDTH_PREVIEW);
    av_image_fill_pointers(dataInput, AV_PIX_FMT_YUV420P, ((HEIGHT_PREVIEW+15)/16)*16, const_cast<uint8_t*>(frame), linesizesInput);
    int linesizes[1]={ WIDTH_SCREEN*2 };
    uint8_t* bufImage[4];
    bufImage[0]=new uint8_t[WIDTH_SCREEN*HEIGHT_SCREEN*2];
    if(sws_scale(m_contextScale, dataInput, linesizesInput, 0, HEIGHT_PREVIEW, bufImage, linesizes)!=HEIGHT_SCREEN) {
        std::cerr<<"Scale failed"<<std::endl;
        delete[] bufImage[0];
        return nullptr;
    }

    return bufImage[0];
}

void Controller::onPreviewFrame(const uint8_t* data, size_t length) {
    const uint8_t* imageScreen=getScreenImage(data, length);
    if(!imageScreen) return;

    m_wnd.ShowFrame(imageScreen, WIDTH_SCREEN*HEIGHT_SCREEN*2);
    if(m_counterCameraInit==0) {
        if(m_counterPreviewFrame%5==0 && m_md.Detect(imageScreen, WIDTH_SCREEN*HEIGHT_SCREEN*2)) {
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
    case 1: m_file.WriteVideo(data, length, true);  break;
    case 2: m_file.WriteHeader(data, length);  break;
    }

    std::chrono::steady_clock::duration elapsed=std::chrono::steady_clock::now()-m_timeLastMotion;
    if(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count()>POST_MOTION_SECONDS) {
        if(!m_captureStopInProgress) {
            m_captureStopInProgress=true;
            postRunnable([this] {
                stopCapture();
            });
        }
    }
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
    s<<PATH_VIDEO<<"/motion_"<<timeStr<<".mp4";
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
    std::cout<<"Video stopped"<<std::endl;
    m_captureAudio.Stop();
    std::cout<<"Audio stopped"<<std::endl;
    m_file.WriteTrailer();
    std::cout<<"Trailer writed"<<std::endl;
    m_file.Close();

    m_captureInProgress=false;
    m_captureStopInProgress=false;
    std::cout<<"Stop completed"<<std::endl;
}

void Controller::postRunnable(const std::function<void()>& runnable) {
    std::lock_guard<std::mutex> lck(m_mutexQueue);
    m_queue.push(runnable);
    m_cvQueue.notify_all();
}

void Controller::onSignalTerm(int signal) {
    if(!pController) return;

    pController->m_stop=true;
}

int Controller::Run() {
    signal(SIGTERM, onSignalTerm);
    bcm_host_init();

    //std::this_thread::sleep_for(std::chrono::seconds(2)); //Wait for xserver start

    m_wnd.Show();
    m_contextScale=sws_getContext(
        WIDTH_PREVIEW, HEIGHT_PREVIEW, AV_PIX_FMT_YUV420P,
        WIDTH_SCREEN, HEIGHT_SCREEN, AV_PIX_FMT_RGB565,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    if(!m_cam.Create()) return 1;

    CameraPreviewPort p;
    p.SetPreviewHandler([this](const uint8_t* data, size_t length) { onPreviewFrame(data, length); });
    p.Run(m_cam);

    m_captureVideo.SetCaptureHandler([this](const uint8_t* data, size_t length, int typeFrame) { return onCaptureFrame(data, length, typeFrame); });
    m_captureAudio.SetCaptureHandler([this](const uint8_t* data, size_t length) { onCaptureAudio(data, length); });

    /*while(m_counterCameraInit) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    startCapture();
    std::this_thread::sleep_for(std::chrono::seconds(4));
    stopCapture();
    /*startCapture();
    std::this_thread::sleep_for(std::chrono::seconds(4));
    stopCapture();*/

    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point stop=start+std::chrono::seconds(300);
    while(!m_stop) { //std::chrono::steady_clock::now()<stop) {
        std::unique_lock<std::mutex> lck(m_mutexQueue);
        m_cvQueue.wait_for(lck, std::chrono::seconds(1), [this]{ return !m_queue.empty(); });
        while(!m_queue.empty()) {
            m_queue.front()();
            m_queue.pop();
        }
    }

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
