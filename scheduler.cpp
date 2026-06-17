#include "scheduler.h"
#include "peer.h"
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
    for (auto state : piece_states) {
        if (state != PieceState::COMPLETED) {
            return true;
        }
    }
    return false;
}

std::vector<unsigned char> Scheduler::download_next_piece(uint32_t& out_piece_index) {
    // 1. Poll active peers for any incoming messages (e.g. HAVE, bitfields, chokes)
    std::vector<PeerConnection*> peers_to_remove;
    for (auto peer : peer_mgr.get_available_peers()) {
        while (peer->is_readable(5)) { // 5ms timeout to prevent long blocking
            PeerConnection::PeerMessage msg = peer->receive_message();
            if (!msg.valid) {
                peers_to_remove.push_back(peer);
                break;
            }
            peer->process_message(msg);
        }
    }
    for (auto peer : peers_to_remove) {
        std::cerr << "Scheduler: Peer disconnected during message polling. Removing peer.\n";
        peer_mgr.remove_peer(peer);
    }

    if (peer_mgr.get_available_peers().empty()) {
        std::cerr << "Scheduler: No available peers left for download.\n";
        return {};
    }

    // 2. Select the rarest available pending piece
    int target_piece = -1;
    size_t min_availability = std::numeric_limits<size_t>::max();

    for (uint32_t i = 0; i < total_pieces; ++i) {
        if (piece_states[i] == PieceState::PENDING) {
            size_t availability = peer_mgr.get_piece_availability(i);
            if (availability > 0 && availability < min_availability) {
                min_availability = availability;
                target_piece = i;
            }
        }
    }

    if (target_piece == -1) {
        std::cout << "Scheduler: No pending pieces with availability > 0.\n";
        return {};
    }

    std::cout << "Scheduler: Selected rarest piece " << target_piece 
              << " with availability " << min_availability << "\n";

    out_piece_index = static_cast<uint32_t>(target_piece);
    uint32_t current_piece_length = piece_length;
    uint64_t piece_start = static_cast<uint64_t>(target_piece) * piece_length;
    uint64_t remaining = total_length - piece_start;

    if (remaining < piece_length) {
        current_piece_length = static_cast<uint32_t>(remaining);
    }

    piece_states[target_piece] = PieceState::DOWNLOADING;
    std::vector<unsigned char> piece_data;
    bool success = false;

    while (true) {
        PeerConnection* peer = peer_mgr.get_peer_for_piece(target_piece);
        if (!peer) {
            break;
        }

        std::cout << "Scheduler: Selected peer for piece " << target_piece << "\n";

        piece_data = download_piece_from_peer(*peer, target_piece, current_piece_length);
        if (!piece_data.empty()) {
            if (piece_mgr.verify_piece(target_piece, piece_data)) {
                success = true;
                break;
            } else {
                std::cerr << "Piece verification failed. Removing peer and trying another...\n";
            }
        } else {
            std::cerr << "Download failed from peer. Removing peer and trying another...\n";
        }

        peer_mgr.remove_peer(peer);
    }

    if (success) {
        piece_states[target_piece] = PieceState::COMPLETED;
        piece_mgr.mark_piece_complete(target_piece);
        return piece_data;
    } else {
        piece_states[target_piece] = PieceState::PENDING;
        return {};
    }
}

std::vector<unsigned char> Scheduler::download_piece_from_peer(
    PeerConnection& conn,
    uint32_t piece_index,
    uint32_t piece_len
) {
    const uint32_t BLOCK_SIZE = 16384;
    std::vector<unsigned char> full_piece;
    uint32_t downloaded = 0;

    while (downloaded < piece_len) {
        uint32_t remaining = piece_len - downloaded;
        uint32_t request_size = std::min(BLOCK_SIZE, remaining);

        // request block
        if (!conn.send_request(piece_index, downloaded, request_size)) {
            std::cerr << "Failed to request block\n";
            return {};
        }

        // receive piece message
        PeerConnection::PeerMessage msg;
        while (true) {
            msg = conn.receive_message();
            if (!msg.valid) {
                break;
            }
            conn.process_message(msg);
            if (msg.id == 7) {
                break;
            }
        }

        if (!msg.valid || msg.id != 7) {
            std::cerr << "Failed to receive piece block\n";
            return {};
        }

        // validate payload
        if (msg.payload.size() < 8) {
            std::cerr << "Invalid piece payload\n";
            return {};
        }

        // extract block data
        std::vector<unsigned char> block_data(
            msg.payload.begin() + 8,
            msg.payload.end()
        );

        full_piece.insert(
            full_piece.end(),
            block_data.begin(),
            block_data.end()
        );

        downloaded += block_data.size();

        std::cout << "Downloaded " << downloaded << " / " << piece_len << " bytes\n";
    }

    return full_piece;
}

void Scheduler::handle_have_update(PeerConnection* peer, uint32_t piece_index) {
    peer_mgr.update_availability(peer, piece_index);
}