#include "download_worker.h"
#include "scheduler.h"
#include "piece_manager.h"
#include "peer_manager.h"
#include "event_logger.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>

DownloadWorker::DownloadWorker(
    uint32_t id,
    std::shared_ptr<PeerConnection> peer_conn,
    PeerManager& peer_mgr,
    Scheduler& scheduler,
    PieceManager& piece_mgr,
    std::atomic<bool>& stop_flag,
    uint32_t piece_length,
    int64_t total_length
) : id(id),
    peer_conn(peer_conn),
    peer_mgr(peer_mgr),
    scheduler(scheduler),
    piece_mgr(piece_mgr),
    stop_flag(stop_flag),
    piece_length(piece_length),
    total_length(total_length),
    start_time(std::chrono::steady_clock::now()) {}

void DownloadWorker::run() {
    std::cout << "Worker " << id << " started.\n";

    while (!stop_flag.load() && scheduler.has_more_pieces()) {
        if (!peer_conn->is_connected()) {
            std::cerr << "Worker " << id << ": Peer connection is not connected. Terminating.\n";
            peer_mgr.remove_peer(peer_conn.get());
            current_piece.store(-1);
            current_block.store(0);
            return;
        }

        // Poll peer connection to keep internal state (chokes, HAVE, etc.) fresh
        while (peer_conn->is_connected() && peer_conn->is_readable(5)) {
            PeerConnection::PeerMessage msg = peer_conn->receive_message();
            if (!msg.valid) {
                std::cerr << "Worker " << id << ": Connection closed by peer.\n";
                EventLogger::instance().log("Worker " + std::to_string(id) + ": Connection closed by peer", "WARNING");
                peer_mgr.remove_peer(peer_conn.get());
                return;
            }
            peer_conn->process_message(msg);
        }

        if (!peer_conn->is_connected()) {
            std::cerr << "Worker " << id << ": Peer disconnected after poll. Terminating.\n";
            peer_mgr.remove_peer(peer_conn.get());
            current_piece.store(-1);
            current_block.store(0);
            return;
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
        EventLogger::instance().log("Worker " + std::to_string(id) + " assigned piece " + std::to_string(piece_index));

        current_piece.store(static_cast<int32_t>(piece_index));
        current_block.store(0);

        if (download_piece(piece_index, current_piece_length)) {
            scheduler.mark_complete(piece_index);
            pieces_downloaded.fetch_add(1);
            total_bytes_downloaded.fetch_add(current_piece_length);
            std::cout << "Worker " << id << ": Verified and saved piece " << piece_index << "\n";
            EventLogger::instance().log("Worker " + std::to_string(id) + ": Verified and saved piece " + std::to_string(piece_index));
        } else {
            failed_downloads.fetch_add(1);
            std::cerr << "Worker " << id << ": Download failed for piece " << piece_index << ". Releasing.\n";
            scheduler.release_piece(piece_index);
            EventLogger::instance().log("Worker " + std::to_string(id) + ": Download failed for piece " + std::to_string(piece_index), "ERROR");
            
            // If the failure was due to socket/disconnect, exit thread
            if (!peer_conn->is_connected()) {
                std::cerr << "Worker " << id << ": Socket disconnected or unreachable. Terminating worker thread.\n";
                peer_mgr.remove_peer(peer_conn.get());
                current_piece.store(-1);
                current_block.store(0);
                return;
            }
        }
        current_piece.store(-1);
        current_block.store(0);
    }
    std::cout << "Worker " << id << " finished.\n";
}

bool DownloadWorker::download_piece(uint32_t piece_index, uint32_t piece_len) {
    scheduler.mark_downloading(piece_index);
    const uint32_t BLOCK_SIZE = 16384;
    std::vector<unsigned char> full_piece;
    uint32_t downloaded = 0;

    while (downloaded < piece_len) {
        if (stop_flag.load() || !peer_conn->is_connected()) {
            return false;
        }

        uint32_t remaining = piece_len - downloaded;
        uint32_t request_size = std::min(BLOCK_SIZE, remaining);

        // request block
        if (!peer_conn->send_request(piece_index, downloaded, request_size)) {
            std::cerr << "Worker " << id << ": Failed to send block request.\n";
            EventLogger::instance().log("Worker " + std::to_string(id) + ": Failed to send block request for piece " + std::to_string(piece_index), "WARNING");
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
            EventLogger::instance().log("Worker " + std::to_string(id) + ": Failed to receive block for piece " + std::to_string(piece_index), "WARNING");
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
        current_block.store(downloaded);
        downloaded_bytes.fetch_add(block_data.size());
    }

    if (piece_mgr.verify_piece(piece_index, full_piece)) {
        return piece_mgr.write_piece(piece_index, full_piece);
    }
    std::cerr << "Worker " << id << ": Hash verification failed for piece " << piece_index << "\n";
    EventLogger::instance().log("Worker " + std::to_string(id) + ": Hash verification failed for piece " + std::to_string(piece_index), "ERROR");
    return false;
}
