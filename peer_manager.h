#pragma once

#include "peer.h"
#include "tracker.h"

#include <vector>

class PeerManager {
private:

    std::vector<PeerConnection*> active_peers;

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
};