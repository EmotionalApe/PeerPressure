#pragma once

#include <cstdint>
#include <atomic>
#include "peer.h"

class Scheduler;
class PieceManager;

class DownloadWorker {
private:
    uint32_t id;
    PeerConnection* peer_conn;
    Scheduler& scheduler;
    PieceManager& piece_mgr;
    std::atomic<bool>& stop_flag;
    uint32_t piece_length;
    int64_t total_length;

    bool download_piece(uint32_t piece_index, uint32_t piece_len);

public:
    DownloadWorker(
        uint32_t id,
        PeerConnection* peer_conn,
        Scheduler& scheduler,
        PieceManager& piece_mgr,
        std::atomic<bool>& stop_flag,
        uint32_t piece_length,
        int64_t total_length
    );

    void run();
};
