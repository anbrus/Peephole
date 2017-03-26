#pragma once

#include "interface/mmal/mmal.h"

class Camera
{
public:
    Camera();
    virtual ~Camera();

    MMAL_COMPONENT_T* const Get() const;
    bool Create();

private:
    MMAL_COMPONENT_T* m_camera = nullptr;

};

