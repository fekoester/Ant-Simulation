// Software rendering of the colony into an RGB buffer (frame dumps and the viewer's map).
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "colony.hpp"

namespace ants {

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> rgb;
    void put(int x, int y, std::array<uint8_t, 3> c) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        uint8_t *q = &rgb[((size_t)y * w + x) * 3];
        q[0] = c[0], q[1] = c[1], q[2] = c[2];
    }
};

inline std::array<uint8_t, 3> stateColor(State s) {
    switch (s) {
        case State::Search: return {245, 245, 245};
        case State::ToFood: return {255, 220, 0};
        case State::ToNest: return {255, 90, 20};
    }
    return {255, 0, 255};
}

// Pheromone on a log scale (it spans many orders of magnitude because of base^distance), food piles in red,
// the nest in black, ants coloured by state.
inline Image render(const Colony &c, int scale, bool showAnts = true) {
    Image img;
    img.w = c.W * scale, img.h = c.H * scale;
    img.rgb.assign((size_t)img.w * img.h * 3, 0);
    double maxLog = 0;
    for (const auto &t : c.tiles) maxLog = std::max(maxLog, std::log10(1.0 + t.pheromone));
    for (int y = 0; y < c.H; y++)
        for (int x = 0; x < c.W; x++) {
            const Tile &t = c.at(x, y);
            std::array<uint8_t, 3> col{58, 96, 46};  // grass
            if (t.pheromone > 0 && maxLog > 0) {
                const double v = std::log10(1.0 + t.pheromone) / maxLog;  // 0..1
                col = {(uint8_t)(58 + v * (170 - 58)), (uint8_t)(96 - v * 70), (uint8_t)(46 + v * (200 - 46))};
            }
            if (t.food > 0) {
                const double f = std::min(1.0, t.food / (double)c.p.foodMax);
                col = {(uint8_t)(150 + 100 * f), 30, 30};
            }
            if (std::abs(x - c.nestX) <= 1 && std::abs(y - c.nestY) <= 1) col = {20, 15, 10};
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++) img.put(x * scale + dx, y * scale + dy, col);
        }
    if (!showAnts) return img;
    const int r = std::max(1, scale / 3);
    for (const auto &a : c.ants) {
        const int cx = a.x * scale + scale / 2, cy = a.y * scale + scale / 2;
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
                if (dx * dx + dy * dy <= r * r) img.put(cx + dx, cy + dy, stateColor(a.state));
    }
    return img;
}

inline void writePpm(const std::string &path, const Image &img) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write " + path);
    f << "P6\n" << img.w << ' ' << img.h << "\n255\n";
    f.write((const char *)img.rgb.data(), (std::streamsize)img.rgb.size());
}

}  // namespace ants
