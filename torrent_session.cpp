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
    std::vector<unsigned char> torrent_data;
    uint32_t piece_index = 0;

    std::cout << "Starting Torrent Session download...\n";

    while (scheduler.has_more_pieces()) {
        std::vector<unsigned char> piece_data = scheduler.download_next_piece(piece_index);
        if (piece_data.empty()) {
            std::cerr << "Failed downloading piece " << piece_index << " from all available peers\n";
            return false;
        }

        torrent_data.insert(
            torrent_data.end(),
            piece_data.begin(),
            piece_data.end()
        );
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
