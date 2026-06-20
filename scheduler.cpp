#include "scheduler.h"
#include "peer.h"
#include "event_logger.h"
#include <iostream>
#include <algorithm>
#include <limits>

Scheduler::Scheduler(
    PeerManager& peer_mgr,
    PieceManager& piece_mgr,
    uint32_t total_pieces,
    uint32_t piece_length,
    int64_t total_length
) : peer_mgr(peer_mgr),
    piece_mgr(piece_mgr),
    total_pieces(total_pieces),
    piece_length(piece_length),
    total_length(total_length),
    piece_states(total_pieces, PieceState::PENDING) {}

bool Scheduler::has_more_pieces() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto state : piece_states) {
        if (state != PieceState::COMPLETED) {
            return true;
        }
    }
    return false;
}

std::optional<uint32_t> Scheduler::acquire_next_piece(const PeerConnection& peer) {
    std::lock_guard<std::mutex> lock(mutex_);
    int target_piece = -1;
    size_t min_availability = std::numeric_limits<size_t>::max();

    for (uint32_t i = 0; i < total_pieces; ++i) {
        if (piece_states[i] == PieceState::PENDING) {
            if (peer.has_piece(i)) {
                size_t availability = peer_mgr.get_piece_availability(i);
                if (availability > 0 && availability < min_availability) {
                    min_availability = availability;
                    target_piece = i;
                }
            }
        }
    }

    if (target_piece != -1) {
        piece_states[target_piece] = PieceState::RESERVED;
        EventLogger::instance().log("Piece " + std::to_string(target_piece) + " reserved (Rarest First)");
        return static_cast<uint32_t>(target_piece);
    }
    return std::nullopt;
}

void Scheduler::release_piece(uint32_t piece_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (piece_index < total_pieces) {
        piece_states[piece_index] = PieceState::PENDING;
        EventLogger::instance().log("Piece " + std::to_string(piece_index) + " released back to pool", "WARNING");
    }
}

void Scheduler::mark_downloading(uint32_t piece_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (piece_index < total_pieces && piece_states[piece_index] == PieceState::RESERVED) {
        piece_states[piece_index] = PieceState::DOWNLOADING;
        EventLogger::instance().log("Piece " + std::to_string(piece_index) + " status changed to DOWNLOADING");
    }
}

void Scheduler::mark_complete(uint32_t piece_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (piece_index < total_pieces) {
        piece_states[piece_index] = PieceState::COMPLETED;
        piece_mgr.mark_piece_complete(piece_index);
        EventLogger::instance().log("Piece " + std::to_string(piece_index) + " marked complete");
    }
}

std::vector<Scheduler::PieceState> Scheduler::get_piece_states() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return piece_states;
}

void Scheduler::handle_have_update(PeerConnection* peer, uint32_t piece_index) {
    peer_mgr.update_availability(peer, piece_index);
}