#include "torrent_session.h"
#include "file_manager.h"
#include <iostream>
#include <fstream>
#include <vector>

TorrentSession::TorrentSession(
    Tracker& tracker,
    PeerManager& peer_mgr,
    PieceManager& piece_mgr,
    Scheduler& scheduler,
    const BencodeValue& torrent_info,
    int64_t total_length
) : tracker(tracker),
    peer_mgr(peer_mgr),
    piece_mgr(piece_mgr),
    scheduler(scheduler),
    torrent_info(torrent_info),
    total_length(total_length) {}

bool TorrentSession::start_session() {
    if (total_length <= 0) {
        std::cerr << "Error: Invalid total length: " << total_length << "\n";
        return false;
    }

    std::vector<unsigned char> torrent_data(total_length, 0);
    uint32_t piece_index = 0;

    std::cout << "Starting Torrent Session download...\n";

    uint32_t piece_length = 0;
    auto& info_dict = torrent_info._dict_val;
    if (info_dict.count("piece length")) {
        piece_length = static_cast<uint32_t>(info_dict.at("piece length")._int_val);
    } else {
        std::cerr << "Error: piece length not found in torrent info\n";
        return false;
    }

    while (scheduler.has_more_pieces()) {
        std::vector<unsigned char> piece_data = scheduler.download_next_piece(piece_index);
        if (piece_data.empty()) {
            std::cerr << "Failed downloading piece " << piece_index << " from all available peers\n";
            return false;
        }

        uint64_t piece_start = static_cast<uint64_t>(piece_index) * piece_length;
        if (piece_start + piece_data.size() > torrent_data.size()) {
            std::cerr << "Error: piece data out of bounds for torrent_data buffer\n";
            return false;
        }

        std::copy(piece_data.begin(), piece_data.end(), torrent_data.begin() + piece_start);
        std::cout << "Verified piece " << piece_index << "\n";
    }

    if (!torrent_data.empty()) {
        std::ofstream out("torrent_data.bin", std::ios::binary);
        if (!out) {
            std::cerr << "Error: Could not open torrent_data.bin for writing\n";
            return false;
        }
        out.write(reinterpret_cast<const char*>(torrent_data.data()), torrent_data.size());
        out.close();

        std::cout << "Saved torrent_data.bin\n";
        if (FileManager::reconstruct_files(torrent_info, torrent_data)) {
            std::cout << "Torrent reconstruction complete!\n";
            return true;
        } else {
            std::cerr << "Torrent reconstruction failed!\n";
            return false;
        }
    }

    return false;
}
