#include "ui/ui_context.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace pebble::ui;

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
class NullRenderer final : public Renderer {
public:
    int beginCount = 0;
    int endCount = 0;
    void beginFrame(const DisplayMetrics&) override { ++beginCount; }
    void endFrame() override { ++endCount; }
    void pushClip(Rect) override {}
    void popClip() override {}
    void fillRect(Rect, Color, float) override {}
    void strokeRect(Rect, Color, float, float) override {}
    void line(float, float, float, float, Color, float) override {}
    Size measureText(std::string_view text, float size) const override { return {static_cast<float>(text.size()) * size * 0.5F, size}; }
    void text(float, float, std::string_view, Color, float) override {}
    void texture(std::uint32_t, Rect, Color) override {}
};
void testButtonAndCheckboxInteraction() {
    Element root("root"); root.axis = Axis::Vertical; root.gap = 4;
    int clicks = 0;
    auto button = std::make_unique<Button>("Run", [&] { ++clicks; });
    Button& b = static_cast<Button&>(root.add(std::move(button)));
    auto check = std::make_unique<CheckBox>("Enable", false);
    CheckBox& c = static_cast<CheckBox&>(root.add(std::move(check)));
    Context context; NullRenderer renderer;
    context.beginFrame({400, 200, 400, 200}); context.render(root, renderer);
    context.handlePointer({PointerEventType::Down, b.bounds.x + 2, b.bounds.y + 2});
    context.handlePointer({PointerEventType::Up, b.bounds.x + 2, b.bounds.y + 2});
    require(clicks == 1, "button click callback fired once");
    context.handlePointer({PointerEventType::Down, c.bounds.x + 2, c.bounds.y + 2});
    context.handlePointer({PointerEventType::Up, c.bounds.x + 2, c.bounds.y + 2});
    require(c.checked, "checkbox toggles on click");
    require(renderer.beginCount == 1 && renderer.endCount == 1, "renderer frame balanced");
    context.endFrame();
}
void testTextInputAndFocusTraversal() {
    Element root("root"); root.axis = Axis::Vertical;
    auto field = std::make_unique<TextField>("Peb");
    TextField& f = static_cast<TextField&>(root.add(std::move(field)));
    auto button = std::make_unique<Button>("OK");
    Button& b = static_cast<Button&>(root.add(std::move(button)));
    Context context; NullRenderer renderer;
    context.beginFrame({320, 180, 640, 360}); context.render(root, renderer);
    context.requestFocus(&f); context.handleText("ble");
    require(f.value == "Pebble", "text input appends to focused field");
    context.handleBackspace();
    require(f.value == "Pebbl", "backspace removes one ASCII character");
    context.handleTab();
    require(context.focused() == &b, "tab moves focus to next interactive widget");
    context.handleTab(true);
    require(context.focused() == &f, "reverse tab moves focus backwards");
    context.endFrame();
}
}
int main() {
    testButtonAndCheckboxInteraction();
    testTextInputAndFocusTraversal();
    std::cout << "PebbleUI context tests passed (2 groups).\n";
    return EXIT_SUCCESS;
}
