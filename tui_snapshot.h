#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "scheduler.h"
#include "event_logger.h"

struct PeerSnapshot {
    std::string ip;
    uint16_t port;
    bool choking;
    bool interested;
    std::vector<bool> pieces;
    size_t availability_count;
};

struct WorkerSnapshot {
    uint32_t id;
    std::string peer_ip;
    uint16_t peer_port;
    int32_t current_piece; // -1 if idle
    uint32_t current_block;
    double download_speed; // KB/s
    
    // Extended Metrics
    uint32_t pieces_downloaded;
    uint64_t total_bytes_downloaded;
    double average_rate; // KB/s
    uint32_t failed_downloads;
};

struct SwarmStats {
    uint32_t connected_peers;
    uint32_t seeders;
    uint32_t leechers;
    double average_availability;
    uint32_t rarest_piece_availability;
    uint32_t most_common_piece_availability;
};

struct TorrentSnapshot {
    std::string name;
    int64_t total_size;
    uint32_t total_pieces;
    uint32_t piece_length;
    uint32_t completed_pieces;
    double download_rate; // KB/s
    std::vector<Scheduler::PieceState> piece_states;
    std::vector<PeerSnapshot> peers;
    std::vector<WorkerSnapshot> workers;
    
    // Dynamic Swarm and Event fields
    SwarmStats swarm_stats;
    std::vector<ProtocolEvent> events;

    // Completion Status
    bool is_complete = false;
    bool reconstruction_success = false;
    double download_duration_sec = 0.0;
    double avg_download_speed_kbs = 0.0;
    uint32_t connected_peers_used = 0;
    std::string download_location;
};
