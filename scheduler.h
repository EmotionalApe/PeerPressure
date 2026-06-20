#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <mutex>
#include <optional>
#include "peer_manager.h"
#include "piece_manager.h"

class Scheduler {
public:
    enum class PieceState {
        PENDING,
        RESERVED,
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
    mutable std::mutex mutex_;


public:
    Scheduler(
        PeerManager& peer_mgr,
        PieceManager& piece_mgr,
        uint32_t total_pieces,
        uint32_t piece_length,
        int64_t total_length
    );

    std::optional<uint32_t> acquire_next_piece(const PeerConnection& peer);
    void release_piece(uint32_t piece_index);
    void mark_downloading(uint32_t piece_index);
    void mark_complete(uint32_t piece_index);

    bool has_more_pieces() const;
    std::vector<PieceState> get_piece_states() const;

    // Hooks for future extensions
    void handle_have_update(PeerConnection* peer, uint32_t piece_index);
};