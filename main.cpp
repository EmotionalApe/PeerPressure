#include <iostream>
#include <fstream>
#include <sstream>
#include "bencode.h"

int main() {
    std::ifstream file("test.torrent", std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string torrent_data = buffer.str();

    size_t index = 0;
    try{
        BencodeValue val = parse_any(torrent_data, index);
        std::cout << "Torrent parsed successfully\n\n";
        print_bencode(val);
        std::cout << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Error parsing torrent: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}