#include "download_worker.h"
#include "scheduler.h"
#include "piece_manager.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>

DownloadWorker::DownloadWorker(
    uint32_t id,
    PeerConnection* peer_conn,
    Scheduler& scheduler,
    PieceManager& piece_mgr,
    std::atomic<bool>& stop_flag,
    uint32_t piece_length,
    int64_t total_length
) : id(id),
    peer_conn(peer_conn),
    scheduler(scheduler),
    piece_mgr(piece_mgr),
    stop_flag(stop_flag),
    piece_length(piece_length),
    total_length(total_length) {}

void DownloadWorker::run() {
    std::cout << "Worker " << id << " started.\n";

    while (!stop_flag.load() && scheduler.has_more_pieces()) {
        // Poll peer connection to keep internal state (chokes, HAVE, etc.) fresh
        while (peer_conn->is_readable(5)) {
            PeerConnection::PeerMessage msg = peer_conn->receive_message();
            if (!msg.valid) {
                std::cerr << "Worker " << id << ": Connection closed by peer.\n";
                return;
            }
            peer_conn->process_message(msg);
        }

        if (peer_conn->is_choking()) {
            // Idle/Wait if peer is choking us
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::optional<uint32_t> piece_idx_opt = scheduler.acquire_next_piece(*peer_conn);
        if (!piece_idx_opt.has_value()) {
            // No pieces available that this peer has, sleep a bit and retry
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        uint32_t piece_index = *piece_idx_opt;
        uint32_t current_piece_length = piece_length;
        uint64_t piece_start = static_cast<uint64_t>(piece_index) * piece_length;
        uint64_t remaining = total_length - piece_start;

        if (remaining < piece_length) {
            current_piece_length = static_cast<uint32_t>(remaining);
        }

        std::cout << "Worker " << id << ": Downloading piece " << piece_index << "\n";

        if (download_piece(piece_index, current_piece_length)) {
            scheduler.mark_complete(piece_index);
            std::cout << "Worker " << id << ": Verified and saved piece " << piece_index << "\n";
        } else {
            std::cerr << "Worker " << id << ": Download failed for piece " << piece_index << ". Releasing.\n";
            scheduler.release_piece(piece_index);
            
            // If the failure was due to socket/disconnect, exit thread
            if (!peer_conn->is_readable(0)) {
                std::cerr << "Worker " << id << ": Socket disconnected or unreachable. Terminating worker thread.\n";
                return;
            }
        }
    }
    std::cout << "Worker " << id << " finished.\n";
}

bool DownloadWorker::download_piece(uint32_t piece_index, uint32_t piece_len) {
    const uint32_t BLOCK_SIZE = 16384;
    std::vector<unsigned char> full_piece;
    uint32_t downloaded = 0;

    while (downloaded < piece_len) {
        if (stop_flag.load()) {
            return false;
        }

        uint32_t remaining = piece_len - downloaded;
        uint32_t request_size = std::min(BLOCK_SIZE, remaining);

        // request block
        if (!peer_conn->send_request(piece_index, downloaded, request_size)) {
            std::cerr << "Worker " << id << ": Failed to send block request.\n";
            return false;
        }

        // receive piece message
        PeerConnection::PeerMessage msg;
        while (true) {
            msg = peer_conn->receive_message();
            if (!msg.valid) {
                break;
            }
            peer_conn->process_message(msg);
            if (msg.id == 7) {
                break;
            }
        }

        if (!msg.valid || msg.id != 7) {
            std::cerr << "Worker " << id << ": Failed to receive piece block.\n";
            return false;
        }

        // validate payload
        if (msg.payload.size() < 8) {
            std::cerr << "Worker " << id << ": Invalid block payload size.\n";
            return false;
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
    }

    if (piece_mgr.verify_piece(piece_index, full_piece)) {
        return piece_mgr.write_piece(piece_index, full_piece);
    }
    return false;
}
