#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <iomanip>
#include "bencode.h"
#include "sha1.hpp"
#include "tracker.h"
#include "peer.h"
#include "utils.h"
#include "piece_manager.h"
#include <curl/curl.h>
#include "file_manager.h"

// --- Torrent Logic ---

std::vector<unsigned char> download_piece(
    PeerConnection& conn,
    uint32_t piece_index,
    uint32_t piece_length
) {

    const uint32_t BLOCK_SIZE = 16384;

    std::vector<unsigned char> full_piece;

    uint32_t downloaded = 0;

    while (downloaded < piece_length) {

        uint32_t remaining =
            piece_length - downloaded;

        uint32_t request_size =
            std::min(BLOCK_SIZE, remaining);

        // request block
        if (!conn.send_request(
            piece_index,
            downloaded,
            request_size
        )) {

            std::cerr << "Failed to request block\n";
            return {};
        }

        // receive piece message
        PeerConnection::PeerMessage msg;
        while (true) {
            msg = conn.receive_message();
            conn.process_message(msg);
            break;
        }

        if (!msg.valid || msg.id != 7) {

            std::cerr << "Failed to receive piece block\n";
            return {};
        }

        // validate payload
        if (msg.payload.size() < 8) {

            std::cerr << "Invalid piece payload\n";
            return {};
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

        std::cout << "Downloaded "
                  << downloaded
                  << " / "
                  << piece_length
                  << " bytes\n";
    }

    return full_piece;
}

std::vector<unsigned char> download_piece_from_peers(
    const std::vector<Peer>& peers,
    uint32_t piece_index,
    uint32_t piece_length,
    const std::vector<unsigned char>& raw_info_hash,

    PieceManager& piece_manager
) {

    for (const auto& peer : peers) {

        std::cout << "\nTrying peer " << peer.ip << ":" << peer.port << "\n";

        PeerConnection conn(peer.ip, peer.port);

        if (!conn.connect_to_peer()) {
            continue;
        }

        if (!conn.send_handshake(raw_info_hash, "-PC0001-123456789012")) {
            conn.close_connection();
            continue;
        }

        if (!conn.receive_handshake(raw_info_hash)) {
            conn.close_connection();
            continue;
        }

        // process initial messages
        while (conn.is_readable(500)) {
            PeerConnection::PeerMessage msg = conn.receive_message();
            if (!msg.valid) break;
            conn.process_message(msg);
        }

        // peer lacks piece
        if (!conn.has_piece(piece_index)) {

            std::cout << "Peer lacks piece " << piece_index << "\n";

            conn.close_connection();
            continue;
        }

        if (!conn.send_interested()) {

            conn.close_connection();
            continue;
        }

        // wait for unchoke
        while (conn.is_choking()) {

            PeerConnection::PeerMessage msg = conn.receive_message();

            conn.process_message(msg);
        }

        // download piece
        auto piece_data = download_piece(conn, piece_index, piece_length);
        conn.close_connection();

        if (piece_data.empty()) {
            continue;
        }

        // verify piece
        if (!piece_manager.verify_piece(piece_index, piece_data)) {
            std::cout << "Verification failed\n";
            continue;
        }

        std::cout << "Successfully downloaded piece " << piece_index << "\n";
        return piece_data;
    }

    return {};
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    // 1. Load Torrent File
    std::ifstream file("test2.torrent", std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open test.torrent\n";
        return 1;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string data = buffer.str();

    size_t index = 0;
    BencodeValue root = parse_any(data, index);
    auto& dict = root._dict_val;

    // 2. Extract Basic Info
    if (dict.find("announce") == dict.end() || dict.find("info") == dict.end()) {
        std::cerr << "Error: Invalid torrent file structure\n";
        return 1;
    }

    std::string announce = dict["announce"]._str_val;
    BencodeValue info = dict["info"];
    auto& info_dict = info._dict_val;

    //pieces blob for hash checking
    std::string pieces_blob = info_dict["pieces"]._str_val; 
    uint32_t piece_length = 0;
    if (info_dict.count("piece length")) {
        piece_length = static_cast<uint32_t>(info_dict["piece length"]._int_val);
    }
    uint32_t total_pieces =
    pieces_blob.size() / 20;

    std::cout << "Total Pieces: "
            << total_pieces
            << "\n";
    PieceManager piece_manager(pieces_blob, piece_length);


    // 3. Calculate Total Length
    int64_t total_length = 0;
    if (info_dict.count("length")) {
        total_length = info_dict["length"]._int_val;
    } else if (info_dict.count("files")) {
        for (const auto& f : info_dict["files"]._list_val) {
            total_length += f._dict_val.at("length")._int_val;
        }
    }

    // 4. Generate Info Hash
    std::string encoded_info = bencode(info);
    SHA1 sha1;
    sha1.update(encoded_info);
    std::string hash_hex = sha1.final();
    
    std::cout << "Announce URL: " << announce << "\n";
    std::cout << "Info Hash:    " << hash_hex << "\n";
    std::cout << "Total Size:   " << total_length << " bytes\n\n";

    // 5. Request Peers from Tracker
    Tracker tracker(announce, hash_hex, total_length);
    std::vector<Peer> peers = tracker.get_peers();

    std::vector<unsigned char> torrent_data;

    // 6. Display Peers
    if (!peers.empty()) {
        std::cout << "Found " << peers.size() << " peers:\n";
        for (const auto& peer : peers) {
            std::cout << "  - " << peer.ip << ":" << peer.port << "\n";
        }

        // 7. Peer Handshake (Attempt with peers until one succeeds)
        std::vector<unsigned char> raw_info_hash = tracker.get_raw_info_hash();
        bool handshake_success = false;

        for (const auto& peer : peers) {
            std::cout << "\nAttempting handshake with " << peer.ip << ":" << peer.port << "...\n";
            
            PeerConnection conn(peer.ip, peer.port);

            if (conn.connect_to_peer()) {
                if (conn.send_handshake(raw_info_hash, "-PC0001-123456789012")) {
                    if (conn.receive_handshake(raw_info_hash)) {
                        std::cout << "Handshake verified with peer " << peer.ip << "!\n";
                        handshake_success = true;
                        
                        if (!conn.send_interested()) {
                            std::cerr << "Failed to send interested\n";
                        }

                        while (conn.is_choking()) {
                            PeerConnection::PeerMessage msg = conn.receive_message(); 
                            if (!msg.valid) break; 
                            
                            conn.process_message(msg);
                        }
                        bool unchoked = !conn.is_choking();
                        if (unchoked) {
                            std::cout << "Peer " << peer.ip << " unchoked us. Downloading...\n";
                        }

                        std::cout << "Peer has piece 0? "
                                  << conn.has_piece(0)
                                  << "\n";

                        std::cout << "Peer has piece 588? "
                                  << conn.has_piece(588)
                                  << "\n";

                        if (unchoked) {
                            for (uint32_t piece = 0; piece < total_pieces; piece++) {
                                uint32_t current_piece_length = piece_length;
                                uint64_t piece_start = static_cast<uint64_t>(piece) * piece_length;

                                uint64_t remaining = total_length - piece_start;

                                if (remaining < piece_length) {
                                    current_piece_length = static_cast<uint32_t>(remaining);
                                }
                                if (!conn.has_piece(piece)) {
                                    std::cerr << "Peer does not have piece "
                                              << piece
                                              << "\n";
                                    continue;
                                }

                                std::vector<unsigned char> piece_data =
                                    download_piece_from_peers(
                                        peers,
                                        piece,
                                        current_piece_length,
                                        raw_info_hash,
                                        piece_manager
                                    );
                                
                                if (piece_data.empty()) {
                                    std::cerr << "Failed downloading piece "
                                                << piece
                                                << "\n";
                                    break;
                                }
        
                                if (!piece_manager.verify_piece(piece, piece_data)) {
                                    std::cerr << "Piece verification failed: "
                                                << piece
                                                << "\n";
        
                                    break;
                                }
        
                                torrent_data.insert(
                                    torrent_data.end(),
                                    piece_data.begin(),
                                    piece_data.end()
                                );
        
                                std::cout << "Verified piece "
                                            << piece
                                            << " / "
                                            << total_pieces
                                            << "\n";
                            }

                            if (!torrent_data.empty()) {
                                std::ofstream out(
                                    "torrent_data.bin",
                                    std::ios::binary
                                );

                                out.write(
                                    reinterpret_cast<const char*>(
                                        torrent_data.data()
                                    ),
                                    torrent_data.size()
                                );
                                out.close();

                                std::cout << "Saved torrent_data.bin\n";
                                if (FileManager::reconstruct_files(
                                    info,
                                    torrent_data
                                )) {

                                    std::cout << "Torrent reconstruction complete!\n";
                                }
                                else {

                                    std::cout << "Torrent reconstruction failed!\n";
                                }
                            }
                        }
                        
                        conn.close_connection();
                        break;
                    }
                }
                conn.close_connection();
            }
        }

        if (!handshake_success) {
            std::cout << "\nFailed to handshake with any available peers.\n";
        }
    } else {
        std::cout << "No peers found or tracker request failed.\n";
    }

    curl_global_cleanup();
    return 0;
}