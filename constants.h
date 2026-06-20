#pragma once

// Shared protocol constants used across tracker, peer manager, and peer connection.
// The peer ID format follows the Azureus-style convention: -CC####-<random12>.
namespace bt {
    constexpr const char* PEER_ID          = "-PC0001-123456789012";
    constexpr const char* PROTOCOL_STRING  = "BitTorrent protocol";
}
