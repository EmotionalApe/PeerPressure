#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "peer_manager.h"
#include "piece_manager.h"

class Scheduler {
public:
    enum class PieceState {
        PENDING,
        DOWNLOADING,
        COMPLETED
    };

private:
    PeerManager& peer_mgr;
    PieceManager& piece_mgr;
    uint32_t total_pieces;
    uint32_t piece_length;
    int64_t total_length;

    std::vector<PieceState> piece_states;

    // Internal helper to download a single piece from a specific peer
    std::vector<unsigned char> download_piece_from_peer(
        PeerConnection& conn,
        uint32_t piece_index,
        uint32_t piece_len
    );

public:
    Scheduler(
        PeerManager& peer_mgr,
        PieceManager& piece_mgr,
        uint32_t total_pieces,
        uint32_t piece_length,
        int64_t total_length
    );

    // Decides next piece to download, finds peer, downloads/verifies it
    std::vector<unsigned char> download_next_piece(uint32_t& out_piece_index);

    bool has_more_pieces() const;

    // Hooks for future extensions
    void handle_have_update(PeerConnection* peer, uint32_t piece_index);
};