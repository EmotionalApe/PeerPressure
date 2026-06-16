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

bool PieceManager::is_piece_complete(uint32_t piece_index) const {
    if (piece_index >= completed_pieces.size()) {
        return false;
    }
    return completed_pieces[piece_index];
}

void PieceManager::mark_piece_complete(uint32_t piece_index) {
    if (piece_index < completed_pieces.size()) {
        completed_pieces[piece_index] = true;
    }
}