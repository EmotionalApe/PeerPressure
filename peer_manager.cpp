#include "peer_manager.h"

#include <iostream>
#include <algorithm>

bool PeerManager::initialize_peers(const std::vector<Peer> &peers, const std::vector<unsigned char> &info_hash) {
    for (const auto &peer : peers) {
        std::cout << "\nConnecting to " << peer.ip << ":" << peer.port << "\n";

        PeerConnection *conn = new PeerConnection(peer.ip, peer.port);

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

    for (auto peer : active_peers) {

        if (peer->has_piece(piece_index)) {
            return peer;
        }
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
        (*it)->close_connection();
        delete *it;
        active_peers.erase(it);
    }
}