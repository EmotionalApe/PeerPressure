#include "peer_manager.h"
#include "event_logger.h"
#include "constants.h"

#include <iostream>
#include <algorithm>
#include <chrono>

bool PeerManager::initialize_peers(const std::vector<Peer> &peers, const std::vector<unsigned char> &info_hash) {
    for (const auto &peer : peers) {
        std::shared_ptr<PeerConnection> conn = std::make_shared<PeerConnection>(peer.ip, peer.port);
        // Do not set peer manager yet to prevent registering in availability_map during initialization

        if (!conn->connect_to_peer()) {
            continue;
        }

        if (!conn->send_handshake(info_hash, bt::PEER_ID)) {
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

        // Wait for an unchoke message with a wall-clock deadline (30 s).
        // Without a deadline, a peer sending infinite keep-alives would stall here forever.
        auto unchoke_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (conn->is_choking()) {
            if (std::chrono::steady_clock::now() > unchoke_deadline) {
                std::cerr << "Peer " << peer.ip << " did not unchoke within 30 s. Skipping.\n";
                EventLogger::instance().log("Peer " + peer.ip + " unchoke timed out", "WARNING");
                break;
            }
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
        {
            std::lock_guard<std::mutex> lock(mutex_);
            active_peers.push_back(std::move(conn));
        }

        std::cout << "Peer ready!\n";
        EventLogger::instance().log("Peer " + peer.ip + ":" + std::to_string(peer.port) + " initialized and added to swarm");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_peers.empty()) {
        availability_map.clear();
        for (auto& peer : active_peers) {
            const auto& pieces = peer->get_available_pieces();
            for (size_t i = 0; i < pieces.size(); ++i) {
                if (pieces[i]) {
                    auto& peers_list = availability_map[static_cast<uint32_t>(i)];
                    if (std::find(peers_list.begin(), peers_list.end(), peer.get()) == peers_list.end()) {
                        peers_list.push_back(peer.get());
                    }
                }
            }
        }
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
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& peer : active_peers) {
        peer->close_connection();
    }
}

void PeerManager::remove_peer(PeerConnection* peer) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Clean up from availability_map FIRST (robust cleanup)
    for (auto& pair : availability_map) {
        auto& peers_list = pair.second;
        peers_list.erase(
            std::remove(peers_list.begin(), peers_list.end(), peer),
            peers_list.end()
        );
    }

    auto it = std::find_if(active_peers.begin(), active_peers.end(),
        [peer](const std::shared_ptr<PeerConnection>& p) { return p.get() == peer; });
    if (it != active_peers.end()) {
        std::string ip_str = (*it)->get_ip();
        (*it)->close_connection();
        active_peers.erase(it);
        EventLogger::instance().log("Peer " + ip_str + " removed from active swarm list", "WARNING");
    }
}

std::vector<std::shared_ptr<PeerConnection>> PeerManager::get_available_peers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<PeerConnection>> peers;
    peers.reserve(active_peers.size());
    for (const auto& p : active_peers) {
        peers.push_back(p);
    }
    return peers;
}


int PeerManager::score_peer(const PeerConnection* /*peer*/) const {
    // Future support for peer scoring. Currently returns a default score.
    return 100;
}

void PeerManager::update_availability(PeerConnection* peer, uint32_t piece_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& peers = availability_map[piece_index];
    if (std::find(peers.begin(), peers.end(), peer) == peers.end()) {
        peers.push_back(peer);
    }
}

void PeerManager::build_availability_map() {
    std::lock_guard<std::mutex> lock(mutex_);
    availability_map.clear();
    for (auto& peer : active_peers) {
        const auto& pieces = peer->get_available_pieces();
        for (size_t i = 0; i < pieces.size(); ++i) {
            if (pieces[i]) {
                auto& peers_list = availability_map[static_cast<uint32_t>(i)];
                if (std::find(peers_list.begin(), peers_list.end(), peer.get()) == peers_list.end()) {
                    peers_list.push_back(peer.get());
                }
            }
        }
    }
}


std::vector<PeerConnection*> PeerManager::get_peers_for_piece(uint32_t piece_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = availability_map.find(piece_index);
    if (it != availability_map.end()) {
        return it->second;
    }
    return {};
}

size_t PeerManager::get_piece_availability(uint32_t piece_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = availability_map.find(piece_index);
    if (it != availability_map.end()) {
        return it->second.size();
    }
    return 0;
}