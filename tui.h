#pragma once
#include "tui_renderer.h"
#include "torrent_session.h"
#include <atomic>
#include <memory>

class TuiManager {
private:
    const TorrentSession& session;
    std::unique_ptr<TuiRenderer> renderer;
    std::atomic<bool>& stop_flag;

public:
    TuiManager(
        const TorrentSession& session,
        std::unique_ptr<TuiRenderer> renderer,
        std::atomic<bool>& stop_flag
    );

    void run();
};

// Legacy entrypoint function for backward compatibility
void run_tui(const TorrentSession& session, std::atomic<bool>& stop_flag);

void run_splash_screen();
std::string run_torrent_selection();
