#pragma once

#include "tracker.h"
#include "peer_manager.h"
#include "piece_manager.h"
#include "scheduler.h"
#include "bencode.h"
#include <string>

class TorrentSession {
private:
    Tracker& tracker;
    PeerManager& peer_mgr;
    PieceManager& piece_mgr;
    Scheduler& scheduler;
    BencodeValue torrent_info;
    int64_t total_length;

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
};
