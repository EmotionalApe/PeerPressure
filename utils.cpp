#include "utils.h"
#include <sstream>
#include <iomanip>
#include <cctype>

namespace utils {

std::string bytes_to_hex(const std::vector<unsigned char>& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char b : bytes) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::string bytes_to_hex(const std::string& bytes) {
    std::vector<unsigned char> v(bytes.begin(), bytes.end());
    return bytes_to_hex(v);
}

std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        bytes.push_back(static_cast<unsigned char>(std::stoi(byteString, nullptr, 16)));
    }
    return bytes;
}

std::string url_encode(const std::vector<unsigned char>& data) {
    std::ostringstream oss;
    for (unsigned char byte : data) {
        if (isalnum(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '~') {
            oss << (char)byte;
        } else {
            oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
    }
    return oss.str();
}

std::string url_encode(const std::string& data) {
    std::vector<unsigned char> v(data.begin(), data.end());
    return url_encode(v);
}

uint32_t read_uint32_be(const unsigned char* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8)  |
           (static_cast<uint32_t>(data[3]));
}

}
