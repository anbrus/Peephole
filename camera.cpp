#include "camera.h"

#include "mmalexception.h"

#include "interface/mmal/util/mmal_default_components.h"

#include <iostream>

Camera::Camera()
{

}

Camera::~Camera() {
    try {
        if(m_camera) {
            MMAL_STATUS_T status=mmal_component_disable(m_camera);
            if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

            mmal_component_destroy(m_camera);
            m_camera=nullptr;
        }
    }catch(const MmalException& e) {
        std::cerr<<e.GetFile()<<':'<<e.GetLine()<<": "<<e.what()<<std::endl;
    }
}

MMAL_COMPONENT_T* const Camera::Get() const {
    return m_camera;
}


bool Camera::Create() {
    bool ret=false;

    try {
        if(m_camera) mmal_component_destroy(m_camera);
        m_camera=nullptr;

        MMAL_STATUS_T status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &m_camera);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        status=mmal_component_enable(m_camera);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        ret=true;
    }catch(const MmalException& e) {
        std::cerr<<e.GetFile()<<':'<<e.GetLine()<<": "<<e.what()<<std::endl;

        if(m_camera) mmal_component_destroy(m_camera);
    }

    return ret;
}
