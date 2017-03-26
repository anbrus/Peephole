#include "mmalexception.h"

#include "interface/mmal/util/mmal_util.h"

MmalException::MmalException(std::string file, int line, MMAL_STATUS_T code):
    m_file(file),
    m_line(line),
    m_code(code)
{
    m_message=mmal_status_to_string(code);
}

const char* MmalException::what() const noexcept {
    return m_message.c_str();
}

std::string MmalException::GetFile() const noexcept {
    return m_file;
}

int MmalException::GetLine() const noexcept {
    return m_line;
}
