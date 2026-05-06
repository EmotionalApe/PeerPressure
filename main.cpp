#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <iomanip>
#include "bencode.h"
#include "sha1.hpp"
#include "tracker.h"

// --- Torrent Logic ---

int main() {
    // 1. Load Torrent File
    std::ifstream file("test.torrent", std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open test.torrent\n";
        return 1;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string data = buffer.str();

    size_t index = 0;
    BencodeValue root = parse_any(data, index);
    auto& dict = root._dict_val;

    // 2. Extract Basic Info
    if (dict.find("announce") == dict.end() || dict.find("info") == dict.end()) {
        std::cerr << "Error: Invalid torrent file structure\n";
        return 1;
    }

    std::string announce = dict["announce"]._str_val;
    BencodeValue info = dict["info"];
    auto& info_dict = info._dict_val;

    // 3. Calculate Total Length
    int64_t total_length = 0;
    if (info_dict.count("length")) {
        total_length = info_dict["length"]._int_val;
    } else if (info_dict.count("files")) {
        for (const auto& f : info_dict["files"]._list_val) {
            total_length += f._dict_val.at("length")._int_val;
        }
    }

    // 4. Generate Info Hash
    std::string encoded_info = bencode(info);
    SHA1 sha1;
    sha1.update(encoded_info);
    std::string hash_hex = sha1.final();
    
    std::cout << "Announce URL: " << announce << "\n";
    std::cout << "Info Hash:    " << hash_hex << "\n";
    std::cout << "Total Size:   " << total_length << " bytes\n\n";

    // 5. Request Peers from Tracker
    Tracker tracker(announce, hash_hex, total_length);
    std::vector<Peer> peers = tracker.get_peers();

    // 6. Display Peers
    if (!peers.empty()) {
        std::cout << "Found " << peers.size() << " peers:\n";
        for (const auto& peer : peers) {
            std::cout << "  - " << peer.ip << ":" << peer.port << "\n";
        }
    } else {
        std::cout << "No peers found or tracker request failed.\n";
    }

    return 0;
}