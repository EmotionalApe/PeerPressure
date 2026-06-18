#include "torrent_session.h"
#include "file_manager.h"
#include "download_worker.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <atomic>
#include <thread>

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

    piece_mgr.initialize_buffer(total_length);
    std::cout << "Starting Torrent Session download...\n";

    uint32_t piece_length = 0;
    auto& info_dict = torrent_info._dict_val;
    if (info_dict.count("piece length")) {
        piece_length = static_cast<uint32_t>(info_dict.at("piece length")._int_val);
    } else {
        std::cerr << "Error: piece length not found in torrent info\n";
        return false;
    }

    std::atomic<bool> stop_flag(false);
    auto available_peers = peer_mgr.get_available_peers();
    if (available_peers.empty()) {
        std::cerr << "No available peers to download.\n";
        return false;
    }

    std::vector<std::unique_ptr<DownloadWorker>> workers;
    std::vector<std::thread> threads;

    std::cout << "Spawning " << available_peers.size() << " concurrent download workers...\n";

    for (size_t i = 0; i < available_peers.size(); ++i) {
        workers.push_back(std::make_unique<DownloadWorker>(
            static_cast<uint32_t>(i),
            available_peers[i],
            scheduler,
            piece_mgr,
            stop_flag,
            piece_length,
            total_length
        ));
    }

    for (auto& worker : workers) {
        threads.push_back(std::thread(&DownloadWorker::run, worker.get()));
    }

    // Wait for all worker threads to complete
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (scheduler.has_more_pieces()) {
        std::cerr << "Error: Download incomplete. Some pieces were not downloaded.\n";
        return false;
    }


    const auto& torrent_data = piece_mgr.get_torrent_data();
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
