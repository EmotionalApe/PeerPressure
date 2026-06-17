#pragma once

#include "peer.h"
#include "tracker.h"

#include <vector>
#include <map>

class PeerManager {
private:
    std::vector<PeerConnection*> active_peers;
    std::map<uint32_t, std::vector<PeerConnection*>> availability_map;

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

    const std::vector<PeerConnection*>& get_available_peers() const;

    int score_peer(const PeerConnection* peer) const;

    void update_availability(PeerConnection* peer, uint32_t piece_index);
    void build_availability_map();
    std::vector<PeerConnection*> get_peers_for_piece(uint32_t piece_index) const;
    size_t get_piece_availability(uint32_t piece_index) const;
};