#pragma once

#include "tracker.h"
#include "peer_manager.h"
#include "piece_manager.h"
#include "scheduler.h"
#include "bencode.h"
#include "tui_snapshot.h"
#include "download_worker.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>

#include <chrono>

class TorrentSession {
private:
    Tracker& tracker;
    PeerManager& peer_mgr;
    PieceManager& piece_mgr;
    Scheduler& scheduler;
    BencodeValue torrent_info;
    int64_t total_length;

    std::vector<std::unique_ptr<DownloadWorker>> workers;
    mutable std::mutex session_mutex_;

    // Completion Tracking
    std::chrono::steady_clock::time_point start_time_;
    bool is_complete_ = false;
    bool reconstruction_success_ = false;
    double download_duration_sec_ = 0.0;
    double avg_download_speed_kbs_ = 0.0;
    uint32_t connected_peers_used_ = 0;
    std::string download_location_;

public:
    TorrentSession(
        Tracker& tracker,
        PeerManager& peer_mgr,
        PieceManager& piece_mgr,
        Scheduler& scheduler,
        const BencodeValue& torrent_info,
        int64_t total_length
    );

    bool start_session();
    TorrentSnapshot get_snapshot() const;
};
