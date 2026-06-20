#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <iomanip>
#include "bencode.h"
#include "sha1.hpp"
#include "tracker.h"
#include "peer.h"
#include "utils.h"
#include "piece_manager.h"
#include <curl/curl.h>
#include "file_manager.h"
#include "peer_manager.h"
#include "scheduler.h"
#include "torrent_session.h"
#include "tui.h"

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Run Splash Screen and Torrent Selection
    run_splash_screen();
    std::string torrent_path = run_torrent_selection();
    if (torrent_path.empty()) {
        std::cerr << "Error: No torrent file selected or found.\n";
        curl_global_cleanup();
        return 1;
    }
    
    // 1. Load Torrent File
    std::ifstream file(torrent_path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open torrent file: " << torrent_path << "\n";
        curl_global_cleanup();
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

    // pieces blob for hash checking
    std::string pieces_blob = info_dict["pieces"]._str_val; 
    uint32_t piece_length = 0;
    if (info_dict.count("piece length")) {
        piece_length = static_cast<uint32_t>(info_dict["piece length"]._int_val);
    }
    uint32_t total_pieces = pieces_blob.size() / 20;

    std::cout << "Total Pieces: " << total_pieces << "\n";
    PieceManager piece_manager(pieces_blob, piece_length);

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

    if (!peers.empty()) {
        std::cout << "Found " << peers.size() << " peers:\n";
        for (const auto& peer : peers) {
            std::cout << "  - " << peer.ip << ":" << peer.port << "\n";
        }

        // 6. Peer Initialization and Downloading
        std::vector<unsigned char> raw_info_hash = tracker.get_raw_info_hash();
        
        PeerManager peer_manager;
        if (!peer_manager.initialize_peers(peers, raw_info_hash)) {
            std::cerr << "No usable peers\n";
            return 1;
        }

        // 7. Create Scheduler and start download session
        Scheduler scheduler(peer_manager, piece_manager, total_pieces, piece_length, total_length);
        TorrentSession session(tracker, peer_manager, piece_manager, scheduler, info, total_length);

        if (session.start_session()) {
            std::cout << "Download session completed successfully!\n";
        } else {
            std::cerr << "Download session failed!\n";
        }
    } else {
        std::cout << "No peers found or tracker request failed.\n";
    }

    curl_global_cleanup();
    return 0;
}