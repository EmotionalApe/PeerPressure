#include "peer_manager.h"

#include <iostream>
#include <algorithm>

bool PeerManager::initialize_peers(const std::vector<Peer> &peers, const std::vector<unsigned char> &info_hash) {
    for (const auto &peer : peers) {
        std::cout << "\nConnecting to " << peer.ip << ":" << peer.port << "\n";

        std::unique_ptr<PeerConnection> conn = std::make_unique<PeerConnection>(peer.ip, peer.port);
        // Do not set peer manager yet to prevent registering in availability_map during initialization

        if (!conn->connect_to_peer()) {
            continue;
        }

        if (!conn->send_handshake(info_hash, "-PC0001-123456789012")) {
            conn->close_connection();
            continue;
        }

        if (!conn->receive_handshake(info_hash)) {
            conn->close_connection();
            continue;
        }

        // process startup messages
        while (conn->is_readable(500)) {
            PeerConnection::PeerMessage msg = conn->receive_message();
            if (!msg.valid) {
                break;
            }
            conn->process_message(msg);
        }

        if (!conn->send_interested()) {
            conn->close_connection();
            continue;
        }

        while (conn->is_choking()) {
            PeerConnection::PeerMessage msg = conn->receive_message();
            if (!msg.valid) {
                break;
            }
            conn->process_message(msg);
        }

        if (conn->is_choking()) {
            std::cerr << "Peer " << peer.ip << " did not unchoke us or disconnected.\n";
            conn->close_connection();
            continue;
        }

        // Peer is fully ready. Set the peer manager and push.
        conn->set_peer_manager(this);
        active_peers.push_back(std::move(conn));

        std::cout << "Peer ready!\n";
    }

    if (!active_peers.empty()) {
        build_availability_map();
        return true;
    }

    return false;
}

PeerConnection* PeerManager::get_peer_for_piece(
    uint32_t piece_index
) {
    auto peers = get_peers_for_piece(piece_index);
    if (!peers.empty()) {
        return peers.front();
    }
    return nullptr;
}

PeerManager::~PeerManager() {
    for (auto& peer : active_peers) {
        peer->close_connection();
    }
}

void PeerManager::remove_peer(PeerConnection* peer) {
    // Clean up from availability_map FIRST (robust cleanup)
    for (auto& pair : availability_map) {
        auto& peers_list = pair.second;
        peers_list.erase(
            std::remove(peers_list.begin(), peers_list.end(), peer),
            peers_list.end()
        );
    }

    auto it = std::find_if(active_peers.begin(), active_peers.end(),
        [peer](const std::unique_ptr<PeerConnection>& p) { return p.get() == peer; });
    if (it != active_peers.end()) {
        (*it)->close_connection();
        active_peers.erase(it);
    }
}

std::vector<PeerConnection*> PeerManager::get_available_peers() const {
    std::vector<PeerConnection*> peers;
    peers.reserve(active_peers.size());
    for (const auto& p : active_peers) {
        peers.push_back(p.get());
    }
    return peers;
}


int PeerManager::score_peer(const PeerConnection* peer) const {
    // Future support for peer scoring. Currently returns a default score.
    return 100;
}

void PeerManager::update_availability(PeerConnection* peer, uint32_t piece_index) {
    auto& peers = availability_map[piece_index];
    if (std::find(peers.begin(), peers.end(), peer) == peers.end()) {
        peers.push_back(peer);
    }
}

void PeerManager::build_availability_map() {
    availability_map.clear();
    for (auto& peer : active_peers) {
        const auto& pieces = peer->get_available_pieces();
        for (size_t i = 0; i < pieces.size(); ++i) {
            if (pieces[i]) {
                update_availability(peer.get(), static_cast<uint32_t>(i));
            }
        }
    }
}


std::vector<PeerConnection*> PeerManager::get_peers_for_piece(uint32_t piece_index) const {
    auto it = availability_map.find(piece_index);
    if (it != availability_map.end()) {
        return it->second;
    }
    return {};
}

size_t PeerManager::get_piece_availability(uint32_t piece_index) const {
    auto it = availability_map.find(piece_index);
    if (it != availability_map.end()) {
        return it->second.size();
    }
    return 0;
}