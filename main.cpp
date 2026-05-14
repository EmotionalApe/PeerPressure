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
#include <curl/curl.h>

// --- Torrent Logic ---

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
    //piece 0 hash
    std::string expected_hash = pieces_blob.substr(0, 20); 


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
                        
                        PeerConnection::PeerMessage msg = conn.receive_message();
                        if (!msg.valid) {
                            std::cerr << "Failed to receive message\n";
                            continue; 
                        }

                        if (!conn.send_interested()) {
                            std::cerr << "Failed to send interested\n";
                        }

                        bool unchoked = false;
                        while (true) {
                            PeerConnection::PeerMessage msg = conn.receive_message(); 
                            if (!msg.valid) break; 
                            
                            if (msg.id == 1) { // Unchoke
                                std::cout << "Peer " << peer.ip << " unchoked us. Downloading...\n";
                                unchoked = true;
                                break;
                            }
                        }

                        if (unchoked) {
                            
                            std::vector<unsigned char> full_piece; 

                            if (conn.send_request(0, 0, 16384)) {
                                PeerConnection::PeerMessage piece_msg = conn.receive_message(); 
                                
                                if (piece_msg.valid && piece_msg.id == 7) {
                                    if (piece_msg.payload.size() < 8) {
                                        std::cerr << "Invalid piece payload \n"; 
                                    }else {
                                        std::vector<unsigned char> block_data(piece_msg.payload.begin() + 8, piece_msg.payload.end());
                                        full_piece.insert(full_piece.end(), block_data.begin(), block_data.end());
                                        
                                    }
                                }
                            }

                            if (conn.send_request(0, 16384, 16384)) {
                                PeerConnection::PeerMessage second_msg = conn.receive_message(); 
                                
                                if (second_msg.valid && second_msg.id == 7) {
                                    std::vector<unsigned char> block_data(second_msg.payload.begin() + 8, second_msg.payload.end());
                                    full_piece.insert(full_piece.end(), block_data.begin(), block_data.end());
                                } 
                            }

                            SHA1 piece_sha1; 
                            piece_sha1.update(
                                std::string(
                                    reinterpret_cast<char*>(full_piece.data()),
                                    full_piece.size()
                                )
                            );

                            std::string downloaded_hash_hex = piece_sha1.final(); 
                            std::string expected_hash_hex = utils::bytes_to_hex(expected_hash); 

                            if (expected_hash_hex == downloaded_hash_hex) {
                                std::cout << "PIECE VERIFIED SUCCESSFULLY!\n";
                            } else {
                                std::cout << "Piece verification FAILED!\n";
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