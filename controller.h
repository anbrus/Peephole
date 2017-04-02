#pragma once

#include "windowpreview.h"
#include "camera.h"
#include "cameracapture.h"
#include "filewriter.h"
#include "motiondetector.h"
#include "audiocapture.h"

#include <chrono>
#include <queue>
#include <functional>
#include <condition_variable>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

#define POST_MOTION_SECONDS 5

class Controller
{
public:
    Controller();
    virtual ~Controller();

    int Run();

private:
    WindowPreview m_wnd;
    Camera m_cam;
    CameraCapture m_captureVideo;
    FileWriter m_file;
    MotionDetector m_md;
    AudioCapture m_captureAudio;
    struct SwsContext* m_contextScale=nullptr;
    int m_counterCameraInit=20;
    int m_counterPreviewFrame=0;
    bool m_captureInProgress=false;
    std::chrono::steady_clock::time_point m_timeLastMotion;
    std::queue<std::function<void()>> m_queue;
    std::condition_variable m_cvQueue;
    std::mutex m_mutexQueue;
    bool m_stop=false;

    uint8_t* getScreenImage(const uint8_t* frame, size_t length);
    void onPreviewFrame(const uint8_t* data, size_t length);
    void onCaptureFrame(const uint8_t* data, size_t length, int typeFrame);
    void onCaptureAudio(const uint8_t* data, size_t length);
    void updateMotionTime();
    void startCapture();
    void stopCapture();
    void postRunnable(const std::function<void()>& runnable);
    static void onSignalTerm(int signal);
};

