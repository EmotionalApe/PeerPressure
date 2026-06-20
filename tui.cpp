#include "tui.h"
#include "ftxui_renderer.h"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/screen/screen.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <map>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <cstdio>
#include <termios.h>
#include <unistd.h>
static int _getch() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif

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

// ==========================================
// TuiManager Implementation
// ==========================================

TuiManager::TuiManager(
    const TorrentSession& session,
    std::unique_ptr<TuiRenderer> r,
    std::atomic<bool>& stop_flag
) : session(session), renderer(std::move(r)), stop_flag(stop_flag) {}

void TuiManager::run() {
    if (!renderer) return;

    renderer->initialize();

    auto last_time = std::chrono::steady_clock::now();
    uint64_t last_downloaded_bytes = 0;
    double current_rate = 0.0;

    std::map<uint32_t, uint64_t> last_worker_bytes;
    std::map<uint32_t, double> worker_speeds;

    while (!stop_flag.load()) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - last_time;

        TorrentSnapshot snap = session.get_snapshot();

        // Calculate download rate
        uint64_t total_downloaded_bytes = 0;
        for (const auto& w : snap.workers) {
            total_downloaded_bytes += w.download_speed * 1024.0; // w.download_speed holds current raw bytes downloaded
        }

        if (elapsed.count() >= 0.5) {
            double diff = 0;
            if (total_downloaded_bytes >= last_downloaded_bytes) {
                diff = static_cast<double>(total_downloaded_bytes - last_downloaded_bytes);
            }
            current_rate = (diff / 1024.0) / elapsed.count(); // KB/s
            last_downloaded_bytes = total_downloaded_bytes;

            // Calculate individual worker speeds
            for (const auto& w : snap.workers) {
                uint64_t current_bytes = w.download_speed * 1024.0;
                uint64_t prev_bytes = 0;
                if (last_worker_bytes.count(w.id)) {
                    prev_bytes = last_worker_bytes[w.id];
                }
                double w_diff = 0.0;
                if (current_bytes >= prev_bytes) {
                    w_diff = static_cast<double>(current_bytes - prev_bytes);
                }
                worker_speeds[w.id] = (w_diff / 1024.0) / elapsed.count(); // KB/s
                last_worker_bytes[w.id] = current_bytes;
            }

            last_time = now;
        }

        // Update the snapshot's worker speeds to the calculated rate before rendering
        for (auto& w : snap.workers) {
            if (worker_speeds.count(w.id)) {
                w.download_speed = worker_speeds[w.id];
            } else {
                w.download_speed = 0.0;
            }
        }

        renderer->render_frame(snap, current_rate);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    renderer->shutdown();
}

// ==========================================
// Legacy Compatibility function
// ==========================================

void run_tui(const TorrentSession& session, std::atomic<bool>& stop_flag) {
    TuiManager manager(session, std::make_unique<FtxuiRenderer>(), stop_flag);
    manager.run();
}

void run_splash_screen() {
    using namespace ftxui;
    
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif

    // Hide cursor and clear screen
    tui_write("\033[?25l\033[2J");

    const std::vector<std::string> loading_stages = {
        "Loading Swarm Engine...",
        "Loading Scheduler...",
        "Initializing Dashboard..."
    };

    const std::vector<std::string> spin = { "[●○○○]", "[○●○○]", "[○○●○]", "[○○○●]" };

    for (int frame = 0; frame < 20; ++frame) {
        std::string stage = loading_stages[0];
        if (frame >= 6 && frame < 12) {
            stage = loading_stages[1];
        } else if (frame >= 12) {
            stage = loading_stages[2];
        }

        std::string spinner = spin[frame % 4];

        auto feature_checklist = window(
            text(" Feature Matrix ") | bold | color(Color::Cyan),
            vbox({
                text(" ✓ Tracker Communication") | color(Color::Green),
                text(" ✓ Peer Wire Protocol") | color(Color::Green),
                text(" ✓ Rarest First Scheduling") | color(Color::Green),
                text(" ✓ Piece Verification") | color(Color::Green),
                text(" ✓ Multi-threaded Downloads") | color(Color::Green),
                text(" ✓ Real-Time Swarm Visualization") | color(Color::Green),
            })
        );

        auto document = vbox({
            text("PEER PRESSURE") | bold | color(Color::Magenta) | hcenter,
            text("Multithreaded BitTorrent Client") | dim | hcenter,
            separator(),
            filler(),
            feature_checklist | hcenter | size(WIDTH, EQUAL, 40),
            filler(),
            separator(),
            hbox({ text(" " + spinner + " "), text(stage) | bold | color(Color::Cyan) }) | hcenter,
            filler(),
        }) | border | size(WIDTH, EQUAL, 80) | size(HEIGHT, EQUAL, 20) | hcenter;

        auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
        Render(screen, document);

        tui_write(screen.ResetPosition());
        tui_write(screen.ToString());

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Restore cursor and clear screen
    tui_write("\033[?25h\033[2J");
}

std::string run_torrent_selection() {
    using namespace ftxui;
    namespace fs = std::filesystem;

    std::vector<fs::path> torrents;
    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.is_regular_file() && entry.path().extension() == ".torrent") {
            torrents.push_back(entry.path());
        }
    }

    if (torrents.empty()) {
        std::cerr << "Error: No .torrent files found in current directory.\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return "";
    }

    int selected_idx = 0;
    bool selected = false;

    // Clear screen and hide cursor
    tui_write("\033[?25l\033[2J");

    while (!selected) {
        Elements menu_items;
        for (size_t i = 0; i < torrents.size(); ++i) {
            std::string prefix = (i == static_cast<size_t>(selected_idx)) ? " > " : "   ";
            std::string file_name = torrents[i].filename().string();
            if (i == static_cast<size_t>(selected_idx)) {
                menu_items.push_back(text(prefix + file_name) | bold | color(Color::Green) | bgcolor(Color::GrayDark));
            } else {
                menu_items.push_back(text(prefix + file_name));
            }
        }

        auto selection_box = window(
            text(" Available Torrents ") | bold | color(Color::Cyan),
            vbox(std::move(menu_items))
        );

        auto document = vbox({
            text("PEER PRESSURE BITTORRENT CLIENT") | bold | color(Color::Magenta) | hcenter,
            separator(),
            text("Use [UP/DOWN] arrows to navigate, press [ENTER] to select:") | dim | hcenter,
            filler(),
            selection_box | hcenter | size(WIDTH, EQUAL, 60),
            filler(),
            separator(),
            text("Press Ctrl+C to terminate.") | dim | hcenter
        }) | border | size(WIDTH, EQUAL, 80) | size(HEIGHT, EQUAL, 20) | hcenter;

        auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
        Render(screen, document);

        tui_write(screen.ResetPosition());
        tui_write(screen.ToString());

        // Read input
        int ch = _getch();
        if (ch == 0 || ch == 224) { // Extended keys
            ch = _getch();
            if (ch == 72) { // Up Arrow
                if (selected_idx > 0) selected_idx--;
            } else if (ch == 80) { // Down Arrow
                if (selected_idx < static_cast<int>(torrents.size() - 1)) selected_idx++;
            }
        } else if (ch == 13) { // Enter key
            selected = true;
        }
    }

    // Restore cursor and clear screen
    tui_write("\033[?25h\033[2J");

    return torrents[selected_idx].string();
}
