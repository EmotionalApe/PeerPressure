#pragma once
#include "tui_renderer.h"
#include <vector>
#include <cstddef>

class FtxuiRenderer : public TuiRenderer {
private:
    std::vector<double> speed_history;
    const size_t max_history = 50;

public:
    void initialize() override;
    void render_frame(const TorrentSnapshot& snapshot, double download_rate) override;
    void shutdown() override;
};
