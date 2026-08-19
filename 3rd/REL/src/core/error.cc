#include "error.h"

#include <sstream>

namespace rel {

std::string Error::to_string() const
{
    const char* label = "error";
    switch (kind)
    {
        case ErrorKind::Lexical: label = "lexical error";  break;
        case ErrorKind::Syntax:  label = "syntax error";   break;
        case ErrorKind::RunTime: label = "runtime error";  break;
    }

    std::ostringstream oss;
    oss << label << ": line " << line << ", column " << column
        << ": " << message;
    return oss.str();
}

} // namespace rel
