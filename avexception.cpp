#include "avexception.h"

extern "C" {
#include <libavutil/error.h>
}

AvException::AvException(std::string file, int line, int code):
    m_file(file),
    m_line(line),
    m_code(code)
{
    char buf[1024];
    av_strerror(code, buf, 1023);
    m_message=buf;
}

const char* AvException::what() const noexcept {
    return m_message.c_str();
}

std::string AvException::GetFile() const noexcept {
    return m_file;
}

int AvException::GetLine() const noexcept {
    return m_line;
}
