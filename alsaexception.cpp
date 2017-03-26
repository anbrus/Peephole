#include "alsaexception.h"

#include <alsa/asoundlib.h>

AlsaException::AlsaException(std::string file, int line, int code):
    m_file(file),
    m_line(line),
    m_code(code)
{
    //m_message=snd_strerror(code);
}

const char* AlsaException::what() const noexcept {
    return m_message.c_str();
}

std::string AlsaException::GetFile() const noexcept {
    return m_file;
}

int AlsaException::GetLine() const noexcept {
    return m_line;
}
