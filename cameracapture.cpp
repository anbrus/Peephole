#include "cameracapture.h"

#include "mmalexception.h"

#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util_params.h"

#include <iostream>
#include <thread>

#define MMAL_CAMERA_VIDEO_PORT 1

#define CAPTURE_FRAME_RATE_NUM 5
#define CAPTURE_FRAME_RATE_DEN 1

#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080

#define CLIP(X) ( (X) > 255 ? 255 : (X) < 0 ? 0 : X)

// RGB -> YUV
#define RGB2Y(R, G, B) CLIP(( (  66 * (R) + 129 * (G) +  25 * (B) + 128) >> 8) +  16)
#define RGB2U(R, G, B) CLIP(( ( -38 * (R) -  74 * (G) + 112 * (B) + 128) >> 8) + 128)
#define RGB2V(R, G, B) CLIP(( ( 112 * (R) -  94 * (G) -  18 * (B) + 128) >> 8) + 128)

CameraCapture::CameraCapture(const Camera& camera):
    m_camera(camera)
{

}

CameraCapture::~CameraCapture() {
    if(m_poolEncoder) {
        mmal_pool_destroy(m_poolEncoder);
        m_poolEncoder=nullptr;
    }
}

void CameraCapture::SetCaptureHandler(std::function<void (const uint8_t*, size_t, int)> handler) {
    m_handler=handler;
}


bool CameraCapture::createEncoder() {
    bool ret=false;

    try {
        MMAL_STATUS_T status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &m_encoder);

        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        MMAL_PORT_T *encoder_input = m_encoder->input[0];
        MMAL_PORT_T *encoder_output = m_encoder->output[0];

        mmal_format_copy(encoder_output->format, encoder_input->format);

        encoder_output->format->encoding = MMAL_ENCODING_H264;
        //encoder_output->format->encoding_variant=MMAL_ENCODING_VARIANT_H264_RAW;

        encoder_output->format->bitrate = 2000000;

        encoder_output->buffer_size = encoder_output->buffer_size_recommended;

        encoder_output->buffer_num = encoder_output->buffer_num_recommended;

        encoder_output->format->es->video.frame_rate.num = 0;
        encoder_output->format->es->video.frame_rate.den = 1;

        status = mmal_port_format_commit(encoder_output);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        if (0)
        {
            MMAL_PARAMETER_VIDEO_RATECONTROL_T param = {{ MMAL_PARAMETER_RATECONTROL, sizeof(param)}, MMAL_VIDEO_RATECONTROL_DEFAULT};
            status = mmal_port_parameter_set(encoder_output, &param.hdr);
            if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);
        }

        MMAL_PARAMETER_VIDEO_PROFILE_T  param;
        param.hdr.id = MMAL_PARAMETER_PROFILE;
        param.hdr.size = sizeof(param);

        param.profile[0].profile = MMAL_VIDEO_PROFILE_H264_BASELINE;
        param.profile[0].level=MMAL_VIDEO_LEVEL_H264_42;

        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        status = mmal_component_enable(m_encoder);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        m_poolEncoder = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);
        if (!m_poolEncoder) throw std::string("Failed to create buffer header pool for encoder output port");

        ret=true;
    }catch(const MmalException& e) {
        std::cerr<<e.GetFile()<<':'<<e.GetLine()<<": "<<e.what()<<std::endl;

        if(m_encoder) {
            mmal_component_destroy(m_encoder);
            m_encoder=nullptr;
        }
    }catch(const std::string& e) {
        std::cerr<<e<<std::endl;

        if(m_encoder) {
            mmal_component_destroy(m_encoder);
            m_encoder=nullptr;
        }
    }

    return ret;
}


void CameraCapture::onCaptureData(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    CameraCapture* capture=reinterpret_cast<CameraCapture*>(buffer->user_data);

    std::string fourCC("no code");
    if(buffer->cmd) fourCC=std::string(reinterpret_cast<char*>(&buffer->cmd), 4);
    //std::cout<<"capture "<<fourCC<<" flags: "<<buffer->flags<<" len: "<<buffer->length<<" pts: "<<buffer->pts<<"dts: "<<buffer->dts<<std::endl;

    if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) {
        mmal_buffer_header_mem_lock(buffer);
        if(buffer->data[4]==0x27) { //SPS
            capture->m_bufferExtra.push_back(1);
            capture->m_bufferExtra.push_back(buffer->data[5]);
            capture->m_bufferExtra.push_back(buffer->data[6]);
            capture->m_bufferExtra.push_back(buffer->data[7]);
            capture->m_bufferExtra.push_back(0xFC | 3);
            capture->m_bufferExtra.push_back(0xE0 | 1);

            capture->m_bufferExtra.push_back(0);
            capture->m_bufferExtra.push_back(buffer->length-4);
            for(size_t n=4; n<buffer->length; n++)
                capture->m_bufferExtra.push_back(buffer->data[n]);
        }else if(buffer->data[4]==0x28) { //PPS
            capture->m_bufferExtra.push_back(1);

            capture->m_bufferExtra.push_back(0);
            capture->m_bufferExtra.push_back(buffer->length-4);
            for(size_t n=4; n<buffer->length; n++)
                capture->m_bufferExtra.push_back(buffer->data[n]);
        }
        if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) {
            capture->m_handler(capture->m_bufferExtra.data(), capture->m_bufferExtra.size(), 2);
        }
        mmal_buffer_header_mem_unlock(buffer);
    }else
        if(buffer->cmd==0 && capture->m_handler && buffer->length>0) {
            mmal_buffer_header_mem_lock(buffer);
            capture->m_bufferFrame.insert(capture->m_bufferFrame.end(), buffer->data, buffer->data+buffer->length);
            mmal_buffer_header_mem_unlock(buffer);

            if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) {
                //std::cout<<"Send "<<capture->m_bufferFrame.size()<<std::endl;
                uint32_t len=capture->m_bufferFrame.size()-4;
                capture->m_bufferFrame[3]=len&0xFF; len>>=8;
                capture->m_bufferFrame[2]=len&0xFF; len>>=8;
                capture->m_bufferFrame[1]=len&0xFF; len>>=8;
                capture->m_bufferFrame[0]=len&0xFF; len>>=8;
                capture->m_handler(capture->m_bufferFrame.data(), capture->m_bufferFrame.size(), buffer->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME ? 1 : 0);
                capture->m_bufferFrame.clear();
            }
        }

    mmal_buffer_header_release(buffer);

    if(port->is_enabled) {
        MMAL_BUFFER_HEADER_T *bufferNew = mmal_queue_get(capture->m_poolEncoder->queue);
        if (!bufferNew) {
            std::cerr<<"Unable to get a required buffer from pool queue"<<std::endl;
        }else {
            if (mmal_port_send_buffer(port, bufferNew)!= MMAL_SUCCESS)
                std::cerr<<"Unable to send a buffer to encoder output port"<<std::endl;
        }
    }

    capture->annotateUpdate();
}

void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    std::string fourCC("no code");
    if(buffer->cmd) fourCC=std::string(reinterpret_cast<char*>(&buffer->cmd), 4);
    //std::cout<<"control "<<fourCC<<std::endl;

    mmal_buffer_header_release(buffer);

    if (mmal_port_send_buffer(port, buffer)!= MMAL_SUCCESS)
        std::cerr<<"Unable to send a buffer to encoder output port"<<std::endl;
}


bool CameraCapture::Start() {
    bool ret=false;
    m_bufferFrame.clear();
    m_bufferExtra.clear();

    try {
        if(m_poolEncoder) {
            mmal_pool_destroy(m_poolEncoder);
            m_poolEncoder=nullptr;
        }

        createEncoder();

        MMAL_PORT_T* portCameraVideo=m_camera.Get()->output[MMAL_CAMERA_VIDEO_PORT];
        MMAL_PORT_T* portEncoderInput=m_encoder->input[0];
        MMAL_PORT_T* portEncoderOutput=m_encoder->output[0];
        MMAL_ES_FORMAT_T *format = portCameraVideo->format;

        format->encoding=MMAL_ENCODING_OPAQUE;
        format->encoding_variant = MMAL_ENCODING_I420;
        format->es->video.width = VCOS_ALIGN_UP(FRAME_WIDTH, 32);
        format->es->video.height = VCOS_ALIGN_UP(FRAME_HEIGHT, 16);
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width = FRAME_WIDTH;
        format->es->video.crop.height = FRAME_HEIGHT;
        format->es->video.frame_rate.num = CAPTURE_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = CAPTURE_FRAME_RATE_DEN;

        MMAL_STATUS_T status = mmal_port_format_commit(portCameraVideo);
        if(status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        portCameraVideo->buffer_num = portCameraVideo->buffer_num_recommended;
        portCameraVideo->buffer_size = portCameraVideo->buffer_size_recommended;

        status=mmal_connection_create(&m_connection, portCameraVideo, portEncoderInput, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        status=mmal_connection_enable(m_connection);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        status=mmal_port_enable(portEncoderOutput, onCaptureData);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        status=mmal_port_parameter_set_boolean(portCameraVideo, MMAL_PARAMETER_CAPTURE, 1);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        int num = mmal_queue_length(m_poolEncoder->queue);
        for (int q=0; q<num; q++) {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(m_poolEncoder->queue);

            if (!buffer) throw std::string("Unable to get a required buffer from pool queue");
            buffer->user_data=this;

            if (mmal_port_send_buffer(portEncoderOutput, buffer)!= MMAL_SUCCESS) throw std::string("Unable to send a buffer to encoder output port");
        }

        annotateUpdate();

        ret=true;
    }catch(const MmalException& e) {
        std::cerr<<e.GetFile()<<':'<<e.GetLine()<<": "<<e.what()<<std::endl;
    }catch(const std::string& e) {
        std::cerr<<e<<std::endl;
    }

    return ret;
}

void CameraCapture::annotateUpdate() {
    MMAL_PARAMETER_CAMERA_ANNOTATE_V3_T annotate={{MMAL_PARAMETER_ANNOTATE, sizeof(MMAL_PARAMETER_CAMERA_ANNOTATE_V3_T)}};
    annotate.enable=1;
    //annotate.custom_text_colour = MMAL_TRUE;
    //annotate.custom_text_Y = 149; //RGB2Y(0, 1, 0);
    //annotate.custom_text_U = 43;//RGB2U(0, 1, 0);
    //annotate.custom_text_V = 21;
    std::time_t now=std::time(nullptr);
    std::tm tm = *std::localtime(&now);
    std::strftime(annotate.text, MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3-1, "%d.%m.%Y %H:%M:%S", &tm);
    annotate.text_size=20;
    MMAL_STATUS_T status=mmal_port_parameter_set(m_camera.Get()->control, &annotate.hdr);
    if (status != MMAL_SUCCESS) {
        MmalException e(__FILE__, __LINE__, status);
        std::cerr<<e.what()<<std::endl;
    }
}

void CameraCapture::Stop() {
    try {
        MMAL_PORT_T* portCapture=m_camera.Get()->output[MMAL_CAMERA_VIDEO_PORT];
        MMAL_STATUS_T status=mmal_port_parameter_set_boolean(portCapture, MMAL_PARAMETER_CAPTURE, 0);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        //status = mmal_port_disable(camera.Get()->control);
        //if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        MMAL_PORT_T* portEncoderOutput=m_encoder->output[0];
        status=mmal_port_disable(portEncoderOutput);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        status=mmal_connection_disable(m_connection);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);

        status=mmal_connection_destroy(m_connection);
        if (status != MMAL_SUCCESS) throw MmalException(__FILE__, __LINE__, status);
        m_connection=nullptr;

        mmal_component_destroy(m_encoder);
        m_encoder=nullptr;

        if(m_poolEncoder) {
            mmal_pool_destroy(m_poolEncoder);
            m_poolEncoder=nullptr;
        }
    }catch(const MmalException& e) {
        std::cerr<<e.GetFile()<<':'<<e.GetLine()<<": "<<e.what()<<std::endl;
    }catch(const std::string& e) {
        std::cerr<<e<<std::endl;
    }
}
