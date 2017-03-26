#pragma once

#include <exception>
#include <string>

class AvException: public std::exception
{
public:
    AvException(std::string file, int line, int code);
    virtual ~AvException() throw() {}

    virtual const char* what() const noexcept override;

    std::string GetFile() const noexcept;
    int GetLine() const noexcept;

private:
    std::string m_file;
    int m_line;
    int m_code;
    std::string m_message;
};

