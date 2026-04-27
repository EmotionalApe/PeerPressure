#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include "bencode.h"
#include "sha1.hpp"
#include <iomanip> 

std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> bytes;

    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::stoi(byteString, nullptr, 16));
        bytes.push_back(byte);
    }

    return bytes;
}

std::string url_encode(const std::vector<unsigned char>& data) {
    std::ostringstream oss;

    for (unsigned char byte : data) {
        oss << '%'
            << std::uppercase
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<int>(byte);
    }

    return oss.str();
}


int main() {
    std::ifstream file("test.torrent", std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file\n";
        return 1;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string data = buffer.str();

    size_t index = 0;
    BencodeValue root = parse_any(data, index);

    // Extract info dictionary
    auto& dict = root._dict_val;

    if (dict.find("info") == dict.end()) {
        std::cerr << "No info dictionary found\n";
        return 1;
    }

    BencodeValue info = dict["info"];

    // Re-encode info
    std::string encoded_info = bencode(info);

    // Compute hash
    SHA1 sha1;
    sha1.update(encoded_info);
    std::string hash_hex = sha1.final();

    std::cout << "Info hash (hex): " << hash_hex << "\n";

    std::vector<unsigned char> raw_bytes = hex_to_bytes(hash_hex);
    std::cout << "Raw bytes count: " << raw_bytes.size() << " (Expected: 20)\n";
    std::string encoded_hash = url_encode(raw_bytes);
    std::cout << "Encoded hash: " << encoded_hash << "\n";
    
    return 0;
}