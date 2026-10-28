#pragma once

#include "ui/layout.hpp"
#include "ui/style.hpp"

#include <cstdint>
#include <string_view>

namespace pebble::ui {

// Backend-neutral draw contract. The OpenGL implementation will own batching,
// GPU resources, font atlas lifetime and restoration of engine GL state.
// Coordinates and clipping rectangles are expressed in logical UI units.
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual void beginFrame(const DisplayMetrics& metrics) = 0;
    virtual void endFrame() = 0;
    virtual void pushClip(Rect rect) = 0;
    virtual void popClip() = 0;
    virtual void fillRect(Rect rect, Color color, float radius = 0.0F) = 0;
    virtual void strokeRect(Rect rect, Color color, float thickness = 1.0F, float radius = 0.0F) = 0;
    virtual void line(float x1, float y1, float x2, float y2, Color color, float thickness = 1.0F) = 0;
    virtual Size measureText(std::string_view text, float logicalFontSize) const = 0;
    virtual void text(float x, float y, std::string_view text, Color color, float logicalFontSize) = 0;
    virtual void texture(std::uint32_t textureId, Rect rect, Color tint = {1, 1, 1, 1}) = 0;
};

} // namespace pebble::ui
