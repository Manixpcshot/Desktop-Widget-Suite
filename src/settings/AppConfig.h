#pragma once
namespace suite {
struct AppConfig {
    bool right = true;
    int threshold = 3;
    int speed = 15;
    int opacity = 240;
    // Physical desktop coordinates, including negative multi-monitor origins.
    int anchorX = 0, anchorY = 0;
    bool hasAnchor = false;
};
}
