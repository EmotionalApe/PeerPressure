#include "peer_manager.h"

#include <iostream>
#include <algorithm>

bool PeerManager::initialize_peers(const std::vector<Peer> &peers, const std::vector<unsigned char> &info_hash) {
    for (const auto &peer : peers) {
        std::cout << "\nConnecting to " << peer.ip << ":" << peer.port << "\n";

        PeerConnection *conn = new PeerConnection(peer.ip, peer.port);
        conn->set_peer_manager(this);

        if (!conn->connect_to_peer()) {
            delete conn;
            continue;
        }

        if (!conn->send_handshake(info_hash, "-PC0001-123456789012")) {
            conn->close_connection();
            delete conn;
            continue;
        }

        if (!conn->receive_handshake(info_hash)) {
            conn->close_connection();
            delete conn;
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
            delete conn;
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
            delete conn;
            continue;
        }

        active_peers.push_back(conn);

        std::cout << "Peer ready!\n";
    }

    return !active_peers.empty();
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
    for (auto peer : active_peers) {
        peer->close_connection();
        delete peer;
    }
}

void PeerManager::remove_peer(PeerConnection* peer) {
    auto it = std::find(active_peers.begin(), active_peers.end(), peer);
    if (it != active_peers.end()) {
        // Clean up from availability_map
        for (auto& pair : availability_map) {
            auto& peers_list = pair.second;
            peers_list.erase(
                std::remove(peers_list.begin(), peers_list.end(), peer),
                peers_list.end()
            );
        }
        (*it)->close_connection();
        delete *it;
        active_peers.erase(it);
    }
}

const std::vector<PeerConnection*>& PeerManager::get_available_peers() const {
    return active_peers;
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
    for (auto peer : active_peers) {
        const auto& pieces = peer->get_available_pieces();
        for (size_t i = 0; i < pieces.size(); ++i) {
            if (pieces[i]) {
                update_availability(peer, static_cast<uint32_t>(i));
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