#include "ftxui_renderer.h"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/screen/screen.hpp"
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#endif

// Format bytes to human readable string
static std::string format_size(int64_t bytes) {
    const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    int s = 0;
    double count = static_cast<double>(bytes);
    while (count >= 1024 && s < 4) {
        s++;
        count /= 1024;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << count << " " << suffixes[s];
    return oss.str();
}

// Format download rate
static std::string format_speed(double kb_per_sec) {
    if (kb_per_sec >= 1024.0) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << (kb_per_sec / 1024.0) << " MB/s";
        return oss.str();
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << kb_per_sec << " KB/s";
    return oss.str();
}

// Format duration to human readable format (e.g. 1h 12m 30s)
static std::string format_duration(double seconds) {
    int total_secs = static_cast<int>(seconds);
    int hrs = total_secs / 3600;
    int mins = (total_secs % 3600) / 60;
    int secs = total_secs % 60;
    
    std::ostringstream oss;
    if (hrs > 0) {
        oss << hrs << "h " << mins << "m " << secs << "s";
    } else if (mins > 0) {
        oss << mins << "m " << secs << "s";
    } else {
        oss << secs << "s";
    }
    return oss.str();
}

// Draw a text-based vertical block graph of speed history
static ftxui::Element draw_speed_graph(const std::vector<double>& history) {
    using namespace ftxui;
    if (history.empty()) {
        return text(" No historical data ") | dim;
    }

    double max_val = 0.0;
    for (double val : history) {
        if (val > max_val) max_val = val;
    }
    if (max_val < 1.0) max_val = 1.0;

    const int height = 4;
    std::vector<std::string> rows(height, "");

    const char* blocks[] = {" ", " ", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

    for (int r = 0; r < height; ++r) {
        std::string row_str = "";
        for (double val : history) {
            double normalized = val / max_val;
            double pixel_height = normalized * height;
            double row_filled = pixel_height - (height - 1 - r);

            if (row_filled >= 1.0) {
                row_str += "█";
            } else if (row_filled <= 0.0) {
                row_str += " ";
            } else {
                int index = static_cast<int>(row_filled * 8.0);
                if (index < 0) index = 0;
                if (index > 8) index = 8;
                row_str += blocks[index];
            }
        }
        rows[r] = row_str;
    }

    Elements graph_rows;
    for (int r = 0; r < height; ++r) {
        graph_rows.push_back(text(rows[r]) | color(Color::Green));
    }
    
    std::string max_speed_str = "Max: " + format_speed(max_val);
    return vbox({
        vbox(std::move(graph_rows)),
        separator(),
        hbox({ text("0s ago"), filler(), text(max_speed_str) | dim })
    });
}

// Direct-to-console write helper to bypass C++ std::cout redirects
static void tui_write(const std::string& str) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteConsoleA(hOut, str.c_str(), static_cast<DWORD>(str.size()), &written, NULL);
    }
#else
    std::fwrite(str.c_str(), 1, str.size(), stdout);
    std::fflush(stdout);
#endif
}

void FtxuiRenderer::initialize() {
#ifdef _WIN32
    // Enable virtual terminal processing for ANSI escape sequences on Windows
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif

    // Hide cursor and clear screen initially
    tui_write("\033[?25l\033[2J");
}

void FtxuiRenderer::render_frame(const TorrentSnapshot& snapshot, double download_rate) {
    using namespace ftxui;

    // 1. Update rolling history of download rate
    speed_history.push_back(download_rate);
    if (speed_history.size() > max_history) {
        speed_history.erase(speed_history.begin());
    }

    if (snapshot.is_complete) {
        auto banner = borderDouble(
            hbox({
                filler(),
                text("DOWNLOAD COMPLETE ✓") | bold | color(Color::Green),
                filler()
            })
        ) | color(Color::Green) | size(HEIGHT, EQUAL, 3);

        std::vector<std::vector<Element>> stats_table_data = {
            { text("Torrent Name:") | bold | color(Color::Cyan), text(snapshot.name) },
            { text("Torrent Size:") | bold | color(Color::Cyan), text(format_size(snapshot.total_size)) },
            { text("Total Pieces:") | bold | color(Color::Cyan), text(std::to_string(snapshot.total_pieces)) },
            { text("Download Duration:") | bold | color(Color::Cyan), text(format_duration(snapshot.download_duration_sec)) },
            { text("Average Speed:") | bold | color(Color::Cyan), text(format_speed(snapshot.avg_download_speed_kbs)) },
            { text("Connected Peers Used:") | bold | color(Color::Cyan), text(std::to_string(snapshot.connected_peers_used)) },
            { text("Download Location:") | bold | color(Color::Cyan), text(snapshot.download_location) }
        };
        auto stats_table = Table(stats_table_data);
        stats_table.SelectAll().SeparatorVertical(LIGHT);
        stats_table.SelectAll().SeparatorHorizontal(LIGHT);

        auto completion_box = window(
            text(" Session Summary ") | bold | color(Color::Cyan),
            vbox({
                banner,
                separator(),
                stats_table.Render() | hcenter
            })
        ) | size(WIDTH, EQUAL, 90) | hcenter;

        auto document = vbox({
            text("PEER PRESSURE BITTORRENT CLIENT") | bold | color(Color::Magenta) | hcenter,
            separator(),
            filler(),
            completion_box,
            filler(),
            separator(),
            text("Press Ctrl+C to exit.") | dim | hcenter
        });

        auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
        Render(screen, document);

        tui_write(screen.ResetPosition());
        tui_write(screen.ToString());
        return;
    }

    double progress = 0.0;
    if (snapshot.total_pieces > 0) {
        progress = (static_cast<double>(snapshot.completed_pieces) / snapshot.total_pieces) * 100.0;
    }

    std::string progress_str = std::to_string(snapshot.completed_pieces) + " / " + 
                               std::to_string(snapshot.total_pieces) + " pieces (" + 
                               std::to_string(static_cast<int>(progress)) + "%)";

    auto info_box = window(text(" Torrent Information ") | bold | color(Color::Cyan), vbox({
        hbox({ text(" Name:     "), text(snapshot.name) | bold | color(Color::Cyan) }),
        hbox({ text(" Size:     "), text(format_size(snapshot.total_size)) }),
        hbox({ text(" Progress: "), text(progress_str) }),
        hbox({ text("           "), gauge(progress / 100.0) | color(Color::Green) }),
    }));

    auto swarm_box = window(text(" Swarm Health & Statistics ") | bold | color(Color::Cyan), vbox({
        hbox({ text(" Connected Peers: "), text(std::to_string(snapshot.swarm_stats.connected_peers)) | color(Color::Yellow) }),
        hbox({ text(" Seeders/Leechers:"), text(std::to_string(snapshot.swarm_stats.seeders)) | color(Color::Green), text(" / "), text(std::to_string(snapshot.swarm_stats.leechers)) | color(Color::Red) }),
        hbox({ text(" Avg Availability: "), text(std::to_string(snapshot.swarm_stats.average_availability).substr(0, 4)) | color(Color::Cyan) }),
        hbox({ text(" Rarest/Common:    "), text(std::to_string(snapshot.swarm_stats.rarest_piece_availability)) | color(Color::Magenta), text(" / "), text(std::to_string(snapshot.swarm_stats.most_common_piece_availability)) | color(Color::Blue) }),
    }));

    auto speed_graph_box = window(text(" Download Speed History ") | bold | color(Color::Cyan), draw_speed_graph(speed_history));

    // Top row with Torrent Info, Swarm Stats, and Graph side-by-side
    auto top_row = hbox({
        info_box | flex,
        separator(),
        swarm_box | flex,
        separator(),
        speed_graph_box | size(WIDTH, EQUAL, 52)
    });

    // Swarm Peers Table
    std::vector<std::vector<Element>> peer_table_data = {
        { text("Peer IP") | bold, text("Port") | bold, text("Pieces Held") | bold, text("State") | bold }
    };
    for (const auto& p : snapshot.peers) {
        int pieces_held = 0;
        for (bool has : p.pieces) {
            if (has) pieces_held++;
        }

        std::string state_str = "";
        state_str += p.choking ? "Choked" : "Unchoked";
        state_str += p.interested ? " | Interested" : " | Not Interested";

        peer_table_data.push_back({
            text(p.ip),
            text(std::to_string(p.port)),
            text(std::to_string(pieces_held) + " / " + std::to_string(snapshot.total_pieces)),
            text(state_str)
        });
    }
    auto peer_table = Table(peer_table_data);
    peer_table.SelectAll().SeparatorVertical(LIGHT);
    peer_table.SelectRow(0).SeparatorHorizontal(LIGHT);
    peer_table.SelectRow(0).Decorate(bold | color(Color::Cyan));
    auto peer_box = window(text(" Connected Swarm Peers ") | bold | color(Color::Cyan), peer_table.Render());

    // Calculate swarm availability for each piece (Stage 4)
    std::vector<int> piece_avail(snapshot.total_pieces, 0);
    for (const auto& p : snapshot.peers) {
        for (size_t i = 0; i < p.pieces.size() && i < piece_avail.size(); ++i) {
            if (p.pieces[i]) piece_avail[i]++;
        }
    }

    // Swarm Availability Grid with 4-state lifecycle colors
    Elements avail_rows;
    Elements current_avail_row;
    size_t cols = 80;
    for (size_t i = 0; i < snapshot.total_pieces; ++i) {
        Element cell;
        if (snapshot.piece_states[i] == Scheduler::PieceState::COMPLETED) {
            cell = text("█") | color(Color::Green);
        } else if (snapshot.piece_states[i] == Scheduler::PieceState::DOWNLOADING) {
            cell = text("█") | color(Color::Red);
        } else if (snapshot.piece_states[i] == Scheduler::PieceState::RESERVED) {
            cell = text("█") | color(Color::Cyan);
        } else {
            int count = piece_avail[i];
            if (count == 0) {
                cell = text("░") | color(Color::GrayDark);
            } else if (count == 1) {
                cell = text("█") | color(Color::Magenta);
            } else if (count < 4) {
                cell = text("█") | color(Color::Yellow);
            } else {
                cell = text("█") | color(Color::Blue);
            }
        }
        current_avail_row.push_back(cell);

        if (current_avail_row.size() == cols || i == snapshot.total_pieces - 1) {
            avail_rows.push_back(hbox(std::move(current_avail_row)));
            current_avail_row.clear();
        }
    }
    auto swarm_avail_box = window(text(" Swarm Availability & Piece Lifecycle ") | bold | color(Color::Cyan), vbox({
        vbox(std::move(avail_rows)),
        separator(),
        hbox({
            text(" Legend: "),
            text("█ Verified ") | color(Color::Green),
            text("█ Downloading ") | color(Color::Red),
            text("█ Reserved ") | color(Color::Cyan),
            text("░ Unavailable ") | color(Color::GrayDark),
            text("█ Rare (1 peer) ") | color(Color::Magenta),
            text("█ Uncommon (2-3) ") | color(Color::Yellow),
            text("█ Common (4+) ") | color(Color::Blue)
        })
    }));

    // Scheduler Panel Box
    std::vector<int> reserved;
    std::vector<std::pair<int, int>> candidates; // (piece_index, availability)
    for (size_t i = 0; i < snapshot.total_pieces; ++i) {
        if (snapshot.piece_states[i] == Scheduler::PieceState::DOWNLOADING ||
            snapshot.piece_states[i] == Scheduler::PieceState::RESERVED) {
            reserved.push_back(static_cast<int>(i));
        } else if (snapshot.piece_states[i] == Scheduler::PieceState::PENDING) {
            int avail = piece_avail[i];
            if (avail > 0) {
                candidates.push_back({static_cast<int>(i), avail});
            }
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.second < b.second; // Rarest first
    });

    std::string reserved_str = "";
    for (int r : reserved) {
        reserved_str += std::to_string(r) + " ";
    }
    if (reserved_str.empty()) reserved_str = "None";

    std::string candidates_str = "";
    for (size_t i = 0; i < 8 && i < candidates.size(); ++i) {
        candidates_str += std::to_string(candidates[i].first) + "(avail:" + std::to_string(candidates[i].second) + ") ";
    }
    if (candidates_str.empty()) candidates_str = "None";

    auto scheduler_box = window(text(" Rarest-First Scheduler Panel ") | bold | color(Color::Cyan), vbox({
        hbox({ text(" Reserved Pieces:     "), text(reserved_str) | color(Color::Yellow) }),
        hbox({ text(" Next Rarest Pieces:  "), text(candidates_str) | color(Color::Green) })
    }));

    // Worker Status Table
    std::vector<std::vector<Element>> table_data = {
        { text("ID") | bold, text("Assigned Peer") | bold, text("Current Piece (Block)") | bold, text("Done") | bold, text("Fail") | bold, text("Speed") | bold, text("Avg") | bold }
    };
    for (const auto& w : snapshot.workers) {
        std::string peer_str = w.peer_ip + ":" + std::to_string(w.peer_port);
        std::string piece_str = (w.current_piece == -1) ? "Idle" : ("Piece " + std::to_string(w.current_piece) + " (" + std::to_string(w.current_block) + " B)");

        table_data.push_back({
            text(std::to_string(w.id)),
            text(peer_str),
            text(piece_str),
            text(std::to_string(w.pieces_downloaded)),
            text(std::to_string(w.failed_downloads)) | color(w.failed_downloads > 0 ? Color::Red : Color::GrayDark),
            text(format_speed(w.download_speed)) | color(Color::Green),
            text(format_speed(w.average_rate)) | color(Color::Cyan)
        });
    }

    auto worker_table = Table(table_data);
    worker_table.SelectAll().SeparatorVertical(LIGHT);
    worker_table.SelectRow(0).SeparatorHorizontal(LIGHT);
    worker_table.SelectRow(0).Decorate(bold | color(Color::Cyan));
    auto worker_box = window(text(" Download Workers ") | bold | color(Color::Cyan), worker_table.Render());

    // Protocol Event Log Box
    Elements event_elements;
    for (const auto& ev : snapshot.events) {
        Color severity_color = Color::White;
        if (ev.severity == "WARNING") severity_color = Color::Yellow;
        else if (ev.severity == "ERROR") severity_color = Color::Red;
        
        event_elements.push_back(hbox({
            text("[" + ev.timestamp + "] ") | dim,
            text(ev.message) | color(severity_color)
        }));
    }
    if (event_elements.size() > 11) {
        event_elements.erase(event_elements.begin(), event_elements.end() - 11);
    }
    if (event_elements.empty()) {
        event_elements.push_back(text(" No protocol events logged yet. ") | dim);
    }
    auto event_box = window(text(" Protocol Event Log ") | bold | color(Color::Cyan), vbox(std::move(event_elements)));

    // Bottom layout: Event log (left), Peer list (middle), Scheduler & Workers (right)
    auto right_side = vbox({
        scheduler_box,
        separator(),
        worker_box
    });
    
    auto bottom_row = hbox({
        event_box | flex_grow,
        separator(),
        peer_box | flex_grow,
        separator(),
        right_side | flex_grow
    });

    auto document = vbox({
        text("PEER PRESSURE BITTORRENT CLIENT") | bold | color(Color::Magenta) | hcenter,
        separator(),
        top_row,
        separator(),
        swarm_avail_box,
        separator(),
        bottom_row,
        separator(),
        filler(),
        text("Press Ctrl+C to terminate.") | dim | hcenter
    });

    auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
    Render(screen, document);

    // Reset cursor to top-left of screen print area and output new frame
    tui_write(screen.ResetPosition());
    tui_write(screen.ToString());
}

void FtxuiRenderer::shutdown() {
    // Show cursor and clear screen at termination
    tui_write("\033[?25h\033[2J");
}
