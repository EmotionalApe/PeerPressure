#pragma once

#include "peer.h"
#include "tracker.h"

#include <vector>
#include <map>
#include <memory>
#include <mutex>

class PeerManager {
private:
    std::vector<std::shared_ptr<PeerConnection>> active_peers;
    std::map<uint32_t, std::vector<PeerConnection*>> availability_map;
    mutable std::mutex mutex_;

public:

    ~PeerManager();

    bool initialize_peers(
        const std::vector<Peer>& peers,
        const std::vector<unsigned char>& info_hash
    );

    PeerConnection* get_peer_for_piece(
        uint32_t piece_index
    );

    void remove_peer(
        PeerConnection* peer
    );

    std::vector<std::shared_ptr<PeerConnection>> get_available_peers() const;


    int score_peer(const PeerConnection* peer) const;

    void update_availability(PeerConnection* peer, uint32_t piece_index);
    void build_availability_map();
    std::vector<PeerConnection*> get_peers_for_piece(uint32_t piece_index) const;
    size_t get_piece_availability(uint32_t piece_index) const;
};