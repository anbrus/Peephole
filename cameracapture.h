#pragma once

#include "camera.h"

#include <functional>
#include <vector>

#include "interface/mmal/util/mmal_connection.h"

class CameraCapture
{
public:
    CameraCapture(const Camera& camera);
    virtual ~CameraCapture();

    bool Start();
    void Stop();
    void SetCaptureHandler(std::function<void (const uint8_t*, size_t, int)> handler);

private:
    std::function<void (const uint8_t*, size_t, int)> m_handler;
    MMAL_POOL_T *m_poolEncoder=nullptr;
    MMAL_COMPONENT_T *m_encoder=nullptr;
    MMAL_CONNECTION_T *m_connection=nullptr;
    std::vector<uint8_t> m_bufferFrame;
    std::vector<uint8_t> m_bufferExtra;
    const Camera& m_camera;

    static void onCaptureData(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

    bool createEncoder();
    void annotateUpdate();
};

