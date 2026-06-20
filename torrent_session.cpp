#include "torrent_session.h"
#include "file_manager.h"
#include "download_worker.h"
#include "tui.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <atomic>
#include <thread>
#include <filesystem>
#include <limits>

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
    total_length(total_length) {
    start_time_ = std::chrono::steady_clock::now();
}

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

    std::vector<std::thread> threads;

    std::cout << "Spawning " << available_peers.size() << " concurrent download workers...\n";

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        workers.clear();
        for (size_t i = 0; i < available_peers.size(); ++i) {
            workers.push_back(std::make_unique<DownloadWorker>(
                static_cast<uint32_t>(i),
                available_peers[i],
                peer_mgr,
                scheduler,
                piece_mgr,
                stop_flag,
                piece_length,
                total_length
            ));
        }
    }

    // Redirect stdout/stderr to log file to keep TUI clean
    std::ofstream log_file("session.log");
    std::streambuf* old_cout = nullptr;
    std::streambuf* old_cerr = nullptr;
    if (log_file.is_open()) {
        old_cout = std::cout.rdbuf(log_file.rdbuf());
        old_cerr = std::cerr.rdbuf(log_file.rdbuf());
    }

    std::atomic<bool> tui_stop_flag(false);
    std::thread tui_thread(run_tui, std::ref(*this), std::ref(tui_stop_flag));

    for (auto& worker : workers) {
        threads.push_back(std::thread(&DownloadWorker::run, worker.get()));
    }

    // Wait for all worker threads to complete
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Now check if download completed successfully
    bool download_success = !scheduler.has_more_pieces();
    bool recon_success = false;

    if (download_success) {
        // Calculate duration and avg speed
        auto end_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> duration = end_time - start_time_;
        download_duration_sec_ = duration.count();
        if (download_duration_sec_ < 0.1) {
            download_duration_sec_ = 0.1; // Avoid division by zero
        }
        avg_download_speed_kbs_ = (static_cast<double>(total_length) / 1024.0) / download_duration_sec_;

        // Connected peers used: check workers that downloaded at least 1 byte/piece
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            connected_peers_used_ = 0;
            for (const auto& w : workers) {
                if (w->get_total_bytes_downloaded() > 0) {
                    connected_peers_used_++;
                }
            }
        }

        // Get download location
        std::string root_name = "Unknown Torrent";
        if (torrent_info._dict_val.count("name")) {
            root_name = torrent_info._dict_val.at("name")._str_val;
        }
        download_location_ = std::filesystem::absolute(root_name).string();

        // Save data and reconstruct
        const auto& torrent_data = piece_mgr.get_torrent_data();
        if (!torrent_data.empty()) {
            std::ofstream out("torrent_data.bin", std::ios::binary);
            if (out) {
                out.write(reinterpret_cast<const char*>(torrent_data.data()), torrent_data.size());
                out.close();
                recon_success = FileManager::reconstruct_files(torrent_info, torrent_data);
            }
        }
        
        reconstruction_success_ = recon_success;
        if (recon_success) {
            is_complete_ = true;
        }
    }

    if (is_complete_) {
        // Keep TUI running until Ctrl+C (which terminates process). We just join.
        if (tui_thread.joinable()) {
            tui_thread.join();
        }
    } else {
        // Signal and join TUI thread
        tui_stop_flag.store(true);
        if (tui_thread.joinable()) {
            tui_thread.join();
        }
    }

    // Restore stdout/stderr redirect
    if (old_cout) std::cout.rdbuf(old_cout);
    if (old_cerr) std::cerr.rdbuf(old_cerr);

    if (!is_complete_) {
        std::cerr << "Error: Download incomplete or reconstruction failed.\n";
        return false;
    }

    return true;
}

TorrentSnapshot TorrentSession::get_snapshot() const {
    TorrentSnapshot snap;
    
    // Torrent name
    snap.name = "Unknown Torrent";
    if (torrent_info._dict_val.count("name")) {
        snap.name = torrent_info._dict_val.at("name")._str_val;
    }
    
    snap.total_size = total_length;
    snap.total_pieces = scheduler.get_piece_states().size();
    
    uint32_t piece_length = 0;
    if (torrent_info._dict_val.count("piece length")) {
        piece_length = static_cast<uint32_t>(torrent_info._dict_val.at("piece length")._int_val);
    }
    snap.piece_length = piece_length;
    
    // Count completed pieces
    snap.piece_states = scheduler.get_piece_states();
    snap.completed_pieces = 0;
    for (auto state : snap.piece_states) {
        if (state == Scheduler::PieceState::COMPLETED) {
            snap.completed_pieces++;
        }
    }
    
    // Connected peers
    auto available_peers = peer_mgr.get_available_peers();
    for (const auto& peer : available_peers) {
        PeerSnapshot p_snap;
        p_snap.ip = peer->get_ip();
        p_snap.port = peer->get_port();
        p_snap.choking = peer->is_choking();
        p_snap.interested = peer->is_interested();
        p_snap.pieces = peer->get_available_pieces();
        p_snap.availability_count = peer_mgr.get_piece_availability(0); // general count is filled dynamically
        snap.peers.push_back(p_snap);
    }
    
    // Calculate Swarm Stats
    snap.swarm_stats.connected_peers = snap.peers.size();
    snap.swarm_stats.seeders = 0;
    snap.swarm_stats.leechers = 0;
    
    std::vector<uint32_t> piece_avail(snap.total_pieces, 0);
    for (const auto& p : snap.peers) {
        bool is_seeder = true;
        for (size_t i = 0; i < p.pieces.size(); ++i) {
            if (p.pieces[i]) {
                if (i < piece_avail.size()) {
                    piece_avail[i]++;
                }
            } else {
                is_seeder = false;
            }
        }
        if (p.pieces.size() >= snap.total_pieces && is_seeder) {
            snap.swarm_stats.seeders++;
        } else {
            snap.swarm_stats.leechers++;
        }
    }
    
    uint32_t total_avail = 0;
    uint32_t min_avail = snap.peers.empty() ? 0 : std::numeric_limits<uint32_t>::max();
    uint32_t max_avail = 0;
    for (uint32_t avail : piece_avail) {
        total_avail += avail;
        if (avail < min_avail) min_avail = avail;
        if (avail > max_avail) max_avail = avail;
    }
    
    snap.swarm_stats.average_availability = snap.total_pieces > 0 ? 
        static_cast<double>(total_avail) / snap.total_pieces : 0.0;
    snap.swarm_stats.rarest_piece_availability = (min_avail == std::numeric_limits<uint32_t>::max()) ? 0 : min_avail;
    snap.swarm_stats.most_common_piece_availability = max_avail;
    
    // Workers
    std::lock_guard<std::mutex> lock(session_mutex_);
    snap.download_rate = 0.0;
    for (const auto& worker : workers) {
        WorkerSnapshot w_snap;
        w_snap.id = worker->get_id();
        auto pc = worker->get_peer_conn();
        if (pc) {
            w_snap.peer_ip = pc->get_ip();
            w_snap.peer_port = pc->get_port();
        } else {
            w_snap.peer_ip = "N/A";
            w_snap.peer_port = 0;
        }
        w_snap.current_piece = worker->get_current_piece();
        w_snap.current_block = worker->get_current_block();
        w_snap.download_speed = static_cast<double>(worker->get_downloaded_bytes()) / 1024.0; // raw bytes
        
        // Populate extended metrics
        w_snap.pieces_downloaded = worker->get_pieces_downloaded();
        w_snap.total_bytes_downloaded = worker->get_total_bytes_downloaded();
        w_snap.failed_downloads = worker->get_failed_downloads();
        
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - worker->get_start_time()
        ).count();
        if (elapsed > 0) {
            w_snap.average_rate = static_cast<double>(w_snap.total_bytes_downloaded) / 1024.0 / elapsed; // KB/s
        } else {
            w_snap.average_rate = 0.0;
        }
        
        snap.workers.push_back(w_snap);
    }
    
    snap.is_complete = is_complete_;
    snap.reconstruction_success = reconstruction_success_;
    snap.download_duration_sec = download_duration_sec_;
    snap.avg_download_speed_kbs = avg_download_speed_kbs_;
    snap.connected_peers_used = connected_peers_used_;
    snap.download_location = download_location_;
    
    // Populate events
    snap.events = EventLogger::instance().get_events();
    
    return snap;
}
