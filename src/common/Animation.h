#pragma once
#include <algorithm>
#include <cmath>
namespace suite {
// Semi-implicit integration with bounded substeps remains stable after a stalled UI frame.
struct Spring {
    double value = 0, velocity = 0;
    void advance(double target, double seconds, double frequency = 15);
    bool settled(double target) const;
};
struct Rect { int left, top, right, bottom; };
inline bool atEdge(int x, int y, Rect monitor, bool right, int threshold) {
    threshold = std::clamp(threshold, 1, 32);
    return y >= monitor.top && y < monitor.bottom &&
        (right ? x >= monitor.right - threshold && x < monitor.right
               : x >= monitor.left && x < monitor.left + threshold);
}
inline double stagger(double progress, int index) {
    return std::clamp((progress - index * 0.09) / 0.64, 0.0, 1.0);
}
}
