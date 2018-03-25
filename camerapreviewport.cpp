#include "camerapreviewport.h"

#include "camera.h"
#include "mmalexception.h"
#include "config.h"

#include "interface/mmal/util/mmal_util.h"

#include <iostream>

#define MMAL_CAMERA_PREVIEW_PORT 0

#define PREVIEW_FRAME_RATE_NUM 5
#define PREVIEW_FRAME_RATE_DEN 1

CameraPreviewPort::CameraPreviewPort()
{

}

CameraPreviewPort::~CameraPreviewPort() {
    if(m_pool) {
        mmal_pool_destroy(m_pool);
        m_pool=nullptr;
    }
}

void CameraPreviewPort::SetPreviewHandler(std::function<void (const uint8_t*, size_t)> handler) {
    m_handler=handler;
}

void CameraPreviewPort::onPreviewData(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    CameraPreviewPort* preview=reinterpret_cast<CameraPreviewPort*>(buffer->user_data);
    std::string fourCC("no code");
    if(buffer->cmd) fourCC=std::string(reinterpret_cast<char*>(&buffer->cmd), 4);
    //std::cout<<"preview "<<fourCC<<" len: "<<buffer->length<<std::endl;

    if(buffer->cmd==0 && preview->m_handler && buffer->length>0) {
        mmal_buffer_header_mem_lock(buffer);
        preview->m_handler(buffer->data, buffer->length);
        mmal_buffer_header_mem_unlock(buffer);
    }

    mmal_buffer_header_release(buffer);

    if(port->is_enabled) {
        MMAL_BUFFER_HEADER_T *bufferNew = mmal_queue_get(preview->m_pool->queue);
        if (!bufferNew) {
            std::cerr<<"Unable to get a required buffer from pool queue"<<std::endl;
        }else {
            if (mmal_port_send_buffer(port, bufferNew)!= MMAL_SUCCESS)
                std::cerr<<"Unable to send a buffer to encoder output port"<<std::endl;
        }
    }
}


bool CameraPreviewPort::Run(const Camera& camera) {
    bool ret=false;

    try {
        if(m_pool) {
            mmal_pool_destroy(m_pool);
            m_pool=nullptr;
        }

        MMAL_PORT_T* portPreview=camera.Get()->output[MMAL_CAMERA_PREVIEW_PORT];
        MMAL_ES_FORMAT_T *format = portPreview->format;

        format->encoding = MMAL_ENCODING_I420;
        format->es->video.width = VCOS_ALIGN_UP(WIDTH_PREVIEW, 32);
        format->es->video.height = VCOS_ALIGN_UP(HEIGHT_PREVIEW, 16);
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width = WIDTH_PREVIEW;
        format->es->video.crop.height = HEIGHT_PREVIEW;
        format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;

        MMAL_STATUS_T status = mmal_port_format_commit(portPreview);
        if(status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        portPreview->buffer_num = portPreview->buffer_num_recommended;
        portPreview->buffer_size = portPreview->buffer_size_recommended;

        m_pool = mmal_port_pool_create(portPreview, portPreview->buffer_num, portPreview->buffer_size);
        if (!m_pool) throw std::string("Create pool failed");


        status=mmal_port_enable(portPreview, onPreviewData);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        int num = mmal_queue_length(m_pool->queue);
        for (int q=0; q<num; q++) {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(m_pool->queue);

            if (!buffer) throw std::string("Unable to get a required buffer from pool queue");
            buffer->user_data=this;

            if (mmal_port_send_buffer(portPreview, buffer)!= MMAL_SUCCESS) throw std::string("Unable to send a buffer to encoder output port");
        }

        ret=true;
    }catch(const MmalException& e) {
        std::cerr<<e.GetFile()<<':'<<e.GetLine()<<": "<<e.what()<<std::endl;
    }catch(const std::string& e) {
        std::cerr<<e<<std::endl;
    }

    return ret;
}

void CameraPreviewPort::Stop(const Camera& camera) {
    try {
        std::cout<<"Terminating preview"<<std::endl;

        MMAL_PORT_T* portPreview=camera.Get()->output[MMAL_CAMERA_PREVIEW_PORT];

        MMAL_STATUS_T status=mmal_port_disable(portPreview);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        std::cout<<"Preview terminated"<<std::endl;
    }catch(const MmalException& e) {
        std::cerr<<e.GetFile()<<':'<<e.GetLine()<<": "<<e.what()<<std::endl;
    }catch(const std::string& e) {
        std::cerr<<e<<std::endl;
    }
}
