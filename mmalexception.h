#pragma once

#include "interface/mmal/mmal.h"
#include <exception>
#include <string>

class MmalException: public std::exception
{
public:
    MmalException(std::string file, int line, MMAL_STATUS_T code);
    virtual ~MmalException() throw() {}

    virtual const char* what() const noexcept override;

    std::string GetFile() const noexcept;
    int GetLine() const noexcept;

private:
    std::string m_file;
    int m_line;
    MMAL_STATUS_T m_code;
    std::string m_message;
};

