#include "tracker.h"
#include "bencode.h"
#include <curl/curl.h>
#include <iostream>
#include <iomanip>
#include <sstream>

// --- Helper Functions (Local to tracker.cpp) ---

static std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        bytes.push_back(static_cast<unsigned char>(std::stoi(byteString, nullptr, 16)));
    }
    return bytes;
}

template <typename T>
static std::string url_encode(const T& data) {
    std::ostringstream oss;
    for (unsigned char byte : data) {
        oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total = size * nmemb;
    output->append((char*)contents, total);
    return total;
}

// --- Tracker Implementation ---

Tracker::Tracker(const std::string& announce_url, const std::string& info_hash, int64_t total_length)
    : announce_url(announce_url), info_hash(info_hash), total_length(total_length) {
    peer_id = "-PC0001-123456789012";
}

std::string Tracker::build_url() {
    std::string encoded_hash = url_encode(hex_to_bytes(info_hash));
    std::string encoded_peer_id = url_encode(peer_id);

    std::string url = announce_url +
        "?info_hash=" + encoded_hash +
        "&peer_id=" + encoded_peer_id +
        "&port=6881" +
        "&uploaded=0" +
        "&downloaded=0" +
        "&left=" + std::to_string(total_length) +
        "&compact=0";
    return url;
}

std::vector<Peer> Tracker::get_peers() {
    std::vector<Peer> peer_list;
    std::string url = build_url();

    CURL* curl = curl_easy_init();
    if (!curl) return peer_list;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "Tracker Request Failed: " << curl_easy_strerror(res) << "\n";
        curl_easy_cleanup(curl);
        return peer_list;
    }

    size_t resp_index = 0;
    BencodeValue tracker_data = parse_any(response, resp_index);
    
    if (tracker_data._dict_val.count("peers")) {
        auto& peers = tracker_data._dict_val["peers"]._list_val;
        for (const auto& peer : peers) {
            auto& p = peer._dict_val;
            peer_list.push_back({p.at("ip")._str_val, static_cast<uint16_t>(p.at("port")._int_val)});
        }
    }

    curl_easy_cleanup(curl);
    return peer_list;
}
