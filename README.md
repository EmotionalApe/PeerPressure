# PeerPressure 🚀

A lightweight C++ BitTorrent client implementation focusing on the core protocols: Bencoding, Tracker communication, and Peer discovery.

## 📁 Project Structure

| File | Description |
| :--- | :--- |
| `main.cpp` | The entry point. Handles file loading, coordination between components, and output. |
| `bencode.h` / `bencode.cpp` | Implements **Bencoding** (parsing and encoding). Used for reading `.torrent` files and tracker responses. |
| `tracker.h` / `tracker.cpp` | Handles communication with the **BitTorrent Tracker** via HTTP GET requests. |
| `sha1.hpp` | A header-only implementation of the **SHA-1** hashing algorithm, used to generate the `info_hash`. |
| `test.torrent` | Sample torrent file used for testing. |

---

## ⚙️ Application Flow

The application follows these steps to find peers for a given torrent:

1.  **Load Torrent**: Reads the binary content of `test.torrent`.
2.  **Parse Bencode**: Decodes the torrent file into a structured dictionary.
3.  **Calculate Info Hash**: 
    *   Extracts the `info` dictionary from the torrent.
    *   Re-encodes it into Bencode format.
    *   Calculates the SHA-1 hash of the encoded string.
4.  **Tracker Request**:
    *   Constructs a URL using the `announce` address from the torrent.
    *   Appends required parameters: `info_hash`, `peer_id`, `port`, `uploaded`, `downloaded`, `left`, and `compact`.
    *   Performs an HTTP GET request using `libcurl`.
5.  **Parse Peers**: Decodes the tracker's response and extracts a list of IP addresses and ports of available peers.

---

## 🚀 How to Run

### Prerequisites

You need a C++ compiler (like `g++`) and the `libcurl` development library installed on your system.

**On Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential libcurl4-openssl-dev
```

### Compilation

Compile the project by linking all necessary source files and the curl library:

```bash
g++ main.cpp bencode.cpp tracker.cpp -lcurl -o peer_pressure
```

### Execution

Ensure you have a valid `test.torrent` file in the same directory as the executable, then run:

```bash
./peer_pressure
```

---

## 🛠️ Built With

*   **C++** - Core logic.
*   **libcurl** - Network communication.
*   **SHA-1** - public domain implementation by Steve Reid & others.
