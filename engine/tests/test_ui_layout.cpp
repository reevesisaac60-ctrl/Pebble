#include "ui/layout.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

using pebble::ui::Align;
using pebble::ui::Axis;
using pebble::ui::Element;
using pebble::ui::Insets;
using pebble::ui::LayoutEngine;
using pebble::ui::Length;
using pebble::ui::Rect;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
bool near(float a, float b, float epsilon = 0.01F) { return std::abs(a - b) <= epsilon; }

void testHorizontalFlexAndGap() {
    Element root("root"); root.axis = Axis::Horizontal; root.gap = 10.0F;
    auto left = std::make_unique<Element>("left"); left->width = Length::fixed(100.0F); left->height = Length::flex();
    auto middle = std::make_unique<Element>("middle"); middle->width = Length::flex(2.0F); middle->height = Length::flex();
    auto right = std::make_unique<Element>("right"); right->width = Length::flex(); right->height = Length::flex();
    Element& l = root.add(std::move(left)); Element& m = root.add(std::move(middle)); Element& r = root.add(std::move(right));
    LayoutEngine layout; layout.arrange(root, {0, 0, 1000, 600});
    require(near(l.bounds.width, 100), "fixed child width preserved");
    require(near(m.bounds.width, 586.6667F), "weighted flex receives two shares of remaining space");
    require(near(r.bounds.width, 293.3333F), "flex receives one share of remaining space");
    require(near(m.bounds.x, 110), "gap applied between first and second child");
    require(near(r.bounds.x, 706.6667F), "second gap applied after weighted flex child");
}

void testNestedPaddingAndMinimums() {
    Element root("root"); root.padding = Insets{10, 20, 30, 40};
    auto child = std::make_unique<Element>("child"); child->width = Length::flex(); child->height = Length::flex();
    child->minimum = {120, 80};
    Element& c = root.add(std::move(child));
    LayoutEngine layout; layout.arrange(root, {5, 7, 500, 400});
    require(near(c.bounds.x, 15) && near(c.bounds.y, 27), "padding offsets child origin");
    require(near(c.bounds.width, 460) && near(c.bounds.height, 340), "flex child fills padded content region");
    layout.arrange(root, {0, 0, 50, 40});
    require(c.bounds.width >= 120 && c.bounds.height >= 80, "minimum constraints preserved for narrow parent");
}

void testVerticalAutoMeasurement() {
    Element root("column"); root.axis = Axis::Vertical; root.gap = 5;
    auto a = std::make_unique<Element>("a"); a->width = Length::fixed(80); a->height = Length::fixed(20);
    auto b = std::make_unique<Element>("b"); b->width = Length::fixed(120); b->height = Length::fixed(30);
    root.add(std::move(a)); root.add(std::move(b));
    LayoutEngine layout;
    const auto size = layout.measure(root, {500, 500});
    require(near(size.width, 120), "auto measurement uses widest child");
    require(near(size.height, 55), "auto measurement sums heights and gap");
}


void testResponsiveWindowSizes() {
    Element root("responsive-root");
    root.axis = Axis::Vertical;
    root.padding = {16.0F, 16.0F, 16.0F, 16.0F};
    root.gap = 12.0F;
    auto top = std::make_unique<Element>("toolbar");
    top->height = Length::fixed(40.0F);
    auto content = std::make_unique<Element>("content");
    content->height = Length::flex();
    Element& toolbar = root.add(std::move(top));
    Element& body = root.add(std::move(content));
    LayoutEngine layout;
    for (const auto& [width, height] : {std::pair{320.0F, 480.0F}, std::pair{800.0F, 600.0F},
                                       std::pair{1920.0F, 1080.0F}}) {
        layout.arrange(root, {0.0F, 0.0F, width, height});
        require(near(toolbar.bounds.width, width - 32.0F), "toolbar adapts to each logical window width");
        require(near(body.bounds.width, width - 32.0F), "content adapts to each logical window width");
        require(body.bounds.height >= 0.0F && body.bounds.y + body.bounds.height <= height,
                "content stays inside available height across window sizes");
    }
}

void testHiddenElementsAndZeroSize() {
    Element root("root"); root.axis = Axis::Horizontal; root.gap = 12;
    auto hidden = std::make_unique<Element>("hidden"); hidden->visible = false; hidden->width = Length::fixed(50); hidden->height = Length::fixed(50);
    auto visible = std::make_unique<Element>("visible"); visible->width = Length::flex(); visible->height = Length::flex();
    Element& h = root.add(std::move(hidden)); Element& v = root.add(std::move(visible));
    LayoutEngine layout; layout.arrange(root, {0, 0, 0, 0});
    require(near(h.bounds.width, 0) && near(h.bounds.height, 0), "hidden child receives empty bounds");
    require(near(v.bounds.width, 0) && near(v.bounds.height, 0), "zero-sized parent does not produce negative dimensions");
}
}

int main() {
    testHorizontalFlexAndGap();
    testNestedPaddingAndMinimums();
    testVerticalAutoMeasurement();
    testResponsiveWindowSizes();
    testHiddenElementsAndZeroSize();
    std::cout << "PebbleUI layout tests passed (5 groups).\n";
    return EXIT_SUCCESS;
}
