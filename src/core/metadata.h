#pragma once
#include <string>

namespace metadata {

inline const std::string AUTHOR = "voltsparx";
inline const std::string CONTACT = "voltsparx@gmail.com";

inline std::string by_line() {
    return "By: " + AUTHOR;
}

inline std::string contact_line() {
    return "Contact: " + CONTACT;
}

} // namespace metadata
