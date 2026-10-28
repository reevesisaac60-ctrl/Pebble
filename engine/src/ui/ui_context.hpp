#pragma once

#include "ui/layout.hpp"
#include "ui/renderer.hpp"
#include "ui/style.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace pebble::ui {

enum class PointerEventType { Move, Down, Up, Scroll };
struct PointerEvent {
    PointerEventType type = PointerEventType::Move;
    float x = 0.0F;
    float y = 0.0F;
    float scrollX = 0.0F;
    float scrollY = 0.0F;
    int button = 1;
};

// Frame-scoped interaction state shared by all PebbleUI widgets.
class Context {
public:
    explicit Context(const Style& style = {});
    void beginFrame(DisplayMetrics metrics);
    void endFrame();
    void handlePointer(const PointerEvent& event);
    void handleText(std::string_view utf8);
    void handleBackspace();
    void handleTab(bool reverse = false);
    void render(Element& root, Renderer& renderer);

    const DisplayMetrics& metrics() const { return m_metrics; }
    const Style& style() const { return m_style; }
    Element* hovered() const { return m_hovered; }
    Element* focused() const { return m_focused; }
    void requestFocus(Element* element);
    void clearFocus();

private:
    DisplayMetrics m_metrics{};
    Style m_style{};
    LayoutEngine m_layout{};
    Element* m_hovered = nullptr;
    Element* m_focused = nullptr;
    Element* m_pressed = nullptr;
    bool m_frameOpen = false;
    Element* m_root = nullptr;

    Element* hitTest(Element& root, float x, float y) const;
    void drawTree(Element& element, Renderer& renderer);
    void collectFocusable(Element& element, std::vector<Element*>& out) const;
};

class Button : public Element {
public:
    explicit Button(std::string label, std::function<void()> onClick = {});
    std::string label;
    std::function<void()> onClick;
    bool enabled = true;
};

class Label : public Element {
public:
    explicit Label(std::string text = {});
    std::string text;
};

class CheckBox : public Element {
public:
    explicit CheckBox(std::string label = {}, bool checked = false);
    std::string label;
    bool checked = false;
    bool enabled = true;
};

class TextField : public Element {
public:
    explicit TextField(std::string value = {});
    std::string value;
    std::size_t maximumLength = 4096;
    bool enabled = true;
};

} // namespace pebble::ui
