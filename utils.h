#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace utils {

std::string bytes_to_hex(const std::vector<unsigned char>& bytes);
std::string bytes_to_hex(const std::string& bytes);
std::vector<unsigned char> hex_to_bytes(const std::string& hex);
std::string url_encode(const std::vector<unsigned char>& data);
std::string url_encode(const std::string& data);
uint32_t read_uint32_be(const unsigned char* data);

}
