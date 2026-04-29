#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <iomanip>
#include <curl/curl.h>
#include "bencode.h"
#include "sha1.hpp"

// --- Helper Functions ---

std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        bytes.push_back(static_cast<unsigned char>(std::stoi(byteString, nullptr, 16)));
    }
    return bytes;
}

template <typename T>
std::string url_encode(const T& data) {
    std::ostringstream oss;
    for (unsigned char byte : data) {
        oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total = size * nmemb;
    output->append((char*)contents, total);
    return total;
}

// --- Torrent Logic ---

int main() {
    // 1. Load Torrent File
    std::ifstream file("test2.torrent", std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open test2.torrent\n";
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
    
    std::string encoded_hash = url_encode(hex_to_bytes(hash_hex));
    std::string peer_id = "-PC0001-123456789012";
    std::string encoded_peer_id = url_encode(peer_id);

    // 5. Construct Tracker URL
    std::string url = announce +
        "?info_hash=" + encoded_hash +
        "&peer_id=" + encoded_peer_id +
        "&port=6881" +
        "&uploaded=0" +
        "&downloaded=0" +
        "&left=" + std::to_string(total_length) +
        "&compact=0";

    std::cout << "Announce URL: " << announce << "\n";
    std::cout << "Info Hash:    " << hash_hex << "\n";
    std::cout << "Total Size:   " << total_length << " bytes\n";
    std::cout << "Tracker URL:  " << url << "\n\n";

    // 6. Request Peers from Tracker
    CURL* curl = curl_easy_init();
    if (!curl) return 1;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "Tracker Request Failed: " << curl_easy_strerror(res) << "\n";
        curl_easy_cleanup(curl);
        return 1;
    }

    // 7. Parse and Display Peers
    size_t resp_index = 0;
    BencodeValue tracker_data = parse_any(response, resp_index);
    
    if (tracker_data._dict_val.count("peers")) {
        auto& peers = tracker_data._dict_val["peers"]._list_val;
        std::cout << "Found " << peers.size() << " peers:\n";
        for (const auto& peer : peers) {
            auto& p = peer._dict_val;
            std::cout << "  - " << p.at("ip")._str_val << ":" << p.at("port")._int_val << "\n";
        }
    } else {
        std::cout << "No peers found in tracker response.\n";
    }

    curl_easy_cleanup(curl);
    return 0;
}