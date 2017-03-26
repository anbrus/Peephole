#pragma once

#include <functional>
#include <mutex>
#include <condition_variable>

#include "interface/mmal/mmal.h"

class Camera;

class CameraPreviewPort
{
public:
    CameraPreviewPort();
    virtual ~CameraPreviewPort();

    void SetPreviewHandler(std::function<void (const uint8_t*, size_t)> handler);
    bool Run(const Camera& camera);
    void Stop(const Camera& camera);

private:
    std::function<void (const uint8_t*, size_t)> m_handler;
    MMAL_POOL_T *m_pool=nullptr;

    static void onPreviewData(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
};

