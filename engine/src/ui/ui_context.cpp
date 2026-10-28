#include "ui/ui_context.hpp"

#include <algorithm>
#include <utility>

namespace pebble::ui {
namespace {
bool contains(const Rect& r, float x, float y) {
    return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}
}

Button::Button(std::string text, std::function<void()> callback)
    : Element("Button"), label(std::move(text)), onClick(std::move(callback)) {
    height = Length::fixed(30.0F);
    minimum = {48.0F, 30.0F};
    padding = {10.0F, 5.0F, 10.0F, 5.0F};
}
Label::Label(std::string value) : Element("Label"), text(std::move(value)) {
    height = Length::automatic();
    intrinsic = {0.0F, 20.0F};
}
CheckBox::CheckBox(std::string text, bool initial)
    : Element("CheckBox"), label(std::move(text)), checked(initial) {
    height = Length::fixed(28.0F);
    minimum = {80.0F, 28.0F};
    padding = {6.0F, 4.0F, 6.0F, 4.0F};
}
TextField::TextField(std::string initial) : Element("TextField"), value(std::move(initial)) {
    height = Length::fixed(32.0F);
    minimum = {100.0F, 32.0F};
    padding = {8.0F, 5.0F, 8.0F, 5.0F};
}

Context::Context(const Style& style) : m_style(style) {}
void Context::beginFrame(DisplayMetrics metrics) {
    m_metrics = metrics;
    m_frameOpen = true;
    m_hovered = nullptr;
}
void Context::endFrame() { m_frameOpen = false; }
void Context::requestFocus(Element* element) { m_focused = element; }
void Context::clearFocus() { m_focused = nullptr; }

Element* Context::hitTest(Element& root, float x, float y) const {
    if (!root.visible || !contains(root.bounds, x, y)) return nullptr;
    for (auto it = root.children().rbegin(); it != root.children().rend(); ++it) {
        if (*it) if (Element* hit = hitTest(**it, x, y)) return hit;
    }
    return &root;
}
void Context::handlePointer(const PointerEvent& event) {
    if (!m_frameOpen || !m_root) return;
    Element* hit = hitTest(*m_root, event.x, event.y);
    if (event.type == PointerEventType::Move) {
        m_hovered = hit;
        return;
    }
    if (event.type == PointerEventType::Down && event.button == 1) {
        m_pressed = hit;
        if (auto* field = dynamic_cast<TextField*>(hit); field && field->enabled) m_focused = field;
        else if (dynamic_cast<Button*>(hit) || dynamic_cast<CheckBox*>(hit)) m_focused = hit;
        else m_focused = nullptr;
        return;
    }
    if (event.type == PointerEventType::Up && event.button == 1) {
        Element* pressed = m_pressed;
        m_pressed = nullptr;
        if (!pressed || pressed != hit) return;
        if (auto* button = dynamic_cast<Button*>(pressed); button && button->enabled && button->onClick) button->onClick();
        else if (auto* check = dynamic_cast<CheckBox*>(pressed); check && check->enabled) check->checked = !check->checked;
    }
}
void Context::handleText(std::string_view utf8) {
    auto* field = dynamic_cast<TextField*>(m_focused);
    if (!field || !field->enabled || field->value.size() >= field->maximumLength) return;
    const std::size_t count = std::min(utf8.size(), field->maximumLength - field->value.size());
    field->value.append(utf8.substr(0, count));
}
void Context::handleBackspace() {
    auto* field = dynamic_cast<TextField*>(m_focused);
    if (field && field->enabled && !field->value.empty()) {
        // Remove one UTF-8 code point, not merely one byte.
        field->value.pop_back();
        while (!field->value.empty() && (static_cast<unsigned char>(field->value.back()) & 0xC0U) == 0x80U)
            field->value.pop_back();
    }
}
void Context::collectFocusable(Element& element, std::vector<Element*>& out) const {
    if (!element.visible) return;
    if (dynamic_cast<Button*>(&element) || dynamic_cast<CheckBox*>(&element) || dynamic_cast<TextField*>(&element))
        out.push_back(&element);
    for (auto& child : element.children()) if (child) collectFocusable(*child, out);
}
void Context::handleTab(bool reverse) {
    if (!m_root) return;
    std::vector<Element*> focusable;
    collectFocusable(*m_root, focusable);
    if (focusable.empty()) { m_focused = nullptr; return; }
    const auto it = std::find(focusable.begin(), focusable.end(), m_focused);
    if (it == focusable.end()) { m_focused = reverse ? focusable.back() : focusable.front(); return; }
    const auto index = static_cast<std::size_t>(std::distance(focusable.begin(), it));
    m_focused = reverse ? focusable[(index + focusable.size() - 1) % focusable.size()]
                         : focusable[(index + 1) % focusable.size()];
}
void Context::drawTree(Element& element, Renderer& renderer) {
    if (!element.visible || element.bounds.width <= 0.0F || element.bounds.height <= 0.0F) return;
    if (auto* button = dynamic_cast<Button*>(&element)) {
        const bool hovered = m_hovered == button;
        const bool pressed = m_pressed == button;
        const Color fill = !button->enabled ? m_style.panel : pressed ? m_style.accent : hovered ? m_style.panelRaised : m_style.panel;
        renderer.fillRect(button->bounds, fill, m_style.cornerRadius);
        renderer.strokeRect(button->bounds, m_style.border, 1.0F, m_style.cornerRadius);
        const Size textSize = renderer.measureText(button->label, 13.0F);
        renderer.text(button->bounds.x + (button->bounds.width - textSize.width) * 0.5F,
                      button->bounds.y + (button->bounds.height - textSize.height) * 0.5F,
                      button->label, m_style.text, 13.0F);
    } else if (auto* label = dynamic_cast<Label*>(&element)) {
        renderer.text(label->bounds.x, label->bounds.y, label->text, m_style.text, 13.0F);
    } else if (auto* check = dynamic_cast<CheckBox*>(&element)) {
        const Rect box{check->bounds.x, check->bounds.y + (check->bounds.height - 16.0F) * 0.5F, 16.0F, 16.0F};
        renderer.fillRect(box, check->checked ? m_style.accent : m_style.panelRaised, 3.0F);
        renderer.strokeRect(box, m_style.border, 1.0F, 3.0F);
        renderer.text(box.x + 23.0F, check->bounds.y + 5.0F, check->label, m_style.text, 13.0F);
    } else if (auto* field = dynamic_cast<TextField*>(&element)) {
        renderer.fillRect(field->bounds, m_style.panel, m_style.cornerRadius);
        renderer.strokeRect(field->bounds, m_focused == field ? m_style.accent : m_style.border, 1.0F, m_style.cornerRadius);
        renderer.text(field->bounds.x + 8.0F, field->bounds.y + 8.0F,
                      field->value, field->value.empty() ? m_style.textMuted : m_style.text, 13.0F);
    }
    for (auto& child : element.children()) if (child) drawTree(*child, renderer);
}
void Context::render(Element& root, Renderer& renderer) {
    if (!m_frameOpen) return;
    m_root = &root;
    m_layout.arrange(root, {0.0F, 0.0F, static_cast<float>(m_metrics.windowWidth), static_cast<float>(m_metrics.windowHeight)});
    renderer.beginFrame(m_metrics);
    drawTree(root, renderer);
    renderer.endFrame();
}

} // namespace pebble::ui
