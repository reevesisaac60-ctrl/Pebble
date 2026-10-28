#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace pebble::ui {

struct Size {
    float width = 0.0F;
    float height = 0.0F;
};

struct Rect {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

struct Insets {
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
};

enum class Axis { Horizontal, Vertical };
enum class SizeMode { Fixed, Auto, Flex };
enum class Align { Start, Center, End, Stretch };

struct Length {
    SizeMode mode = SizeMode::Auto;
    float value = 0.0F;
    static Length fixed(float value) { return {SizeMode::Fixed, value}; }
    static Length automatic() { return {SizeMode::Auto, 0.0F}; }
    static Length flex(float weight = 1.0F) { return {SizeMode::Flex, weight}; }
};

class Element {
public:
    explicit Element(std::string name = {});
    virtual ~Element() = default;
    Element(const Element&) = delete;
    Element& operator=(const Element&) = delete;

    Element& add(std::unique_ptr<Element> child);
    const std::vector<std::unique_ptr<Element>>& children() const { return m_children; }
    std::vector<std::unique_ptr<Element>>& children() { return m_children; }

    std::string name;
    Length width = Length::automatic();
    Length height = Length::automatic();
    Size minimum{0.0F, 0.0F};
    Size maximum{1.0e7F, 1.0e7F};
    Insets padding{};
    float gap = 0.0F;
    Axis axis = Axis::Vertical;
    Align crossAlign = Align::Stretch;
    Size intrinsic{};
    Rect bounds{};
    bool visible = true;

private:
    std::vector<std::unique_ptr<Element>> m_children;
};

class LayoutEngine {
public:
    // Measures and arranges the tree in logical UI coordinates. DPI conversion
    // belongs to the renderer/window integration, not this layout pass.
    Size measure(const Element& element, Size available) const;
    void arrange(Element& element, Rect bounds) const;

private:
    static float clampDimension(float value, float minimum, float maximum);
    Size measureContent(const Element& element, Size available) const;
    void arrangeChildren(Element& element) const;
};

} // namespace pebble::ui
