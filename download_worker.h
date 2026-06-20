#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include "peer.h"

class Scheduler;
class PieceManager;
class PeerManager;

class DownloadWorker {
private:
    uint32_t id;
    std::shared_ptr<PeerConnection> peer_conn;
    PeerManager& peer_mgr;
    Scheduler& scheduler;
    PieceManager& piece_mgr;
    std::atomic<bool>& stop_flag;
    uint32_t piece_length;
    int64_t total_length;

    std::atomic<int32_t> current_piece{-1};
    std::atomic<uint32_t> current_block{0};
    std::atomic<uint64_t> downloaded_bytes{0};

    std::atomic<uint32_t> pieces_downloaded{0};
    std::atomic<uint64_t> total_bytes_downloaded{0};
    std::atomic<uint32_t> failed_downloads{0};
    std::chrono::steady_clock::time_point start_time;

    bool download_piece(uint32_t piece_index, uint32_t piece_len);

public:
    DownloadWorker(
        uint32_t id,
        std::shared_ptr<PeerConnection> peer_conn,
        PeerManager& peer_mgr,
        Scheduler& scheduler,
        PieceManager& piece_mgr,
        std::atomic<bool>& stop_flag,
        uint32_t piece_length,
        int64_t total_length
    );

    void run();

    uint32_t get_id() const { return id; }
    std::shared_ptr<PeerConnection> get_peer_conn() const { return peer_conn; }
    int32_t get_current_piece() const { return current_piece.load(); }
    uint32_t get_current_block() const { return current_block.load(); }
    uint64_t get_and_reset_downloaded_bytes() { return downloaded_bytes.exchange(0); }
    uint64_t get_downloaded_bytes() const { return downloaded_bytes.load(); }
    
    // Performance Getters
    uint32_t get_pieces_downloaded() const { return pieces_downloaded.load(); }
    uint64_t get_total_bytes_downloaded() const { return total_bytes_downloaded.load(); }
    uint32_t get_failed_downloads() const { return failed_downloads.load(); }
    std::chrono::steady_clock::time_point get_start_time() const { return start_time; }
};
