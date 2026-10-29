#include "Animation.h"
namespace suite {
void Spring::advance(double target, double seconds, double frequency) {
    if (!std::isfinite(seconds) || seconds <= 0) return;
    seconds = std::min(seconds, 0.1);
    frequency = std::clamp(frequency, 5.0, 30.0);
    while (seconds > 0) {
        const double dt = std::min(seconds, 1.0 / 240.0);
        velocity += (frequency * frequency * (target - value) - 1.5 * frequency * velocity) * dt;
        value += velocity * dt;
        seconds -= dt;
    }
    if (settled(target)) { value = target; velocity = 0; }
}
bool Spring::settled(double target) const {
    return std::abs(value - target) < 0.0005 && std::abs(velocity) < 0.001;
}
}
