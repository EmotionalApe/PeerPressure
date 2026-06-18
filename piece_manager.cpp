#include "piece_manager.h"
#include "sha1.hpp"

#include <sstream>
#include <iomanip>
#include <iostream>
#include "utils.h"


PieceManager::PieceManager(
    const std::string& pieces_blob,
    uint32_t piece_length
)
    : pieces_blob(pieces_blob),
      piece_length(piece_length),
      completed_pieces(pieces_blob.size() / 20, false) {}

void PieceManager::initialize_buffer(int64_t total_length) {
    std::lock_guard<std::mutex> lock(mutex_);
    torrent_data_.assign(total_length, 0);
}

bool PieceManager::verify_piece(
    uint32_t piece_index,
    const std::vector<unsigned char>& piece_data
) {

    // extract expected hash
    std::string expected_hash =
        pieces_blob.substr(piece_index * 20, 20);

    std::string expected_hash_hex =
        utils::bytes_to_hex(expected_hash);

    // hash downloaded data
    SHA1 sha1;

    sha1.update(
        std::string(
            reinterpret_cast<const char*>(piece_data.data()),
            piece_data.size()
        )
    );

    std::string actual_hash_hex =
        sha1.final();

    std::cout << "Expected SHA1: "
              << expected_hash_hex
              << "\n";

    std::cout << "Actual SHA1:   "
              << actual_hash_hex
              << "\n";

    return expected_hash_hex == actual_hash_hex;
}

bool PieceManager::write_piece(
    uint32_t piece_index,
    const std::vector<unsigned char>& piece_data
) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t piece_start = static_cast<uint64_t>(piece_index) * piece_length;
    if (piece_start + piece_data.size() > torrent_data_.size()) {
        std::cerr << "Error: piece data out of bounds for torrent_data buffer\n";
        return false;
    }

    std::copy(piece_data.begin(), piece_data.end(), torrent_data_.begin() + piece_start);
    return true;
}

const std::vector<unsigned char>& PieceManager::get_torrent_data() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return torrent_data_;
}

bool PieceManager::is_piece_complete(uint32_t piece_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (piece_index >= completed_pieces.size()) {
        return false;
    }
    return completed_pieces[piece_index];
}

void PieceManager::mark_piece_complete(uint32_t piece_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (piece_index < completed_pieces.size()) {
        completed_pieces[piece_index] = true;
    }
}