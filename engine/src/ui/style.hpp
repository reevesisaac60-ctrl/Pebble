#pragma once

#include <cstdint>

namespace pebble::ui {

struct Color {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
};

// Centralized, resolution-independent design tokens. Sizes are logical UI units.
struct Style {
    float spacingXs = 4.0F;
    float spacingSm = 8.0F;
    float spacingMd = 12.0F;
    float spacingLg = 16.0F;
    float controlHeight = 30.0F;
    float titleBarHeight = 34.0F;
    float panelMinimumWidth = 180.0F;
    float cornerRadius = 5.0F;
    Color background{0.075F, 0.075F, 0.085F, 1.0F};
    Color panel{0.105F, 0.105F, 0.12F, 1.0F};
    Color panelRaised{0.14F, 0.14F, 0.16F, 1.0F};
    Color border{0.22F, 0.22F, 0.25F, 1.0F};
    Color text{0.88F, 0.87F, 0.90F, 1.0F};
    Color textMuted{0.56F, 0.55F, 0.60F, 1.0F};
    Color accent{0.68F, 0.61F, 0.86F, 1.0F};
};

struct DisplayMetrics {
    int windowWidth = 0;
    int windowHeight = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    float scaleX() const { return windowWidth > 0 ? static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth) : 1.0F; }
    float scaleY() const { return windowHeight > 0 ? static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight) : 1.0F; }
};

} // namespace pebble::ui
