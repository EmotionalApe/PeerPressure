#pragma once
#include "tui_snapshot.h"

class TuiRenderer {
public:
    virtual ~TuiRenderer() = default;

    // Set up console settings (e.g. hide cursor, raw mode)
    virtual void initialize() = 0;

    // Render a single screen frame based on the latest snapshot data
    virtual void render_frame(const TorrentSnapshot& snapshot, double download_rate) = 0;

    // Restore terminal configuration (e.g. show cursor)
    virtual void shutdown() = 0;
};
