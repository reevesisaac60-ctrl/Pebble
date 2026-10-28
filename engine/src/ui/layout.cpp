#include "ui/layout.hpp"

#include <cmath>
#include <numeric>
#include <utility>

namespace pebble::ui {
namespace {
float nonNegative(float value) { return std::isfinite(value) ? std::max(0.0F, value) : 0.0F; }
float mainSize(Size size, Axis axis) { return axis == Axis::Horizontal ? size.width : size.height; }
float crossSize(Size size, Axis axis) { return axis == Axis::Horizontal ? size.height : size.width; }
float crossMin(Size size, Axis axis) { return crossSize(size, axis); }
}

Element::Element(std::string elementName) : name(std::move(elementName)) {}

Element& Element::add(std::unique_ptr<Element> child) {
    if (child) {
        m_children.push_back(std::move(child));
        return *m_children.back();
    }
    return *this;
}

float LayoutEngine::clampDimension(float value, float minimum, float maximum) {
    const float safeMinimum = nonNegative(minimum);
    const float safeMaximum = std::max(safeMinimum, nonNegative(maximum));
    return std::clamp(nonNegative(value), safeMinimum, safeMaximum);
}

Size LayoutEngine::measure(const Element& element, Size available) const {
    if (!element.visible) return {};
    Size content = measureContent(element, available);
    const float horizontalPadding = nonNegative(element.padding.left) + nonNegative(element.padding.right);
    const float verticalPadding = nonNegative(element.padding.top) + nonNegative(element.padding.bottom);
    float width = element.width.mode == SizeMode::Fixed ? element.width.value : content.width + horizontalPadding;
    float height = element.height.mode == SizeMode::Fixed ? element.height.value : content.height + verticalPadding;
    width = clampDimension(width, element.minimum.width, std::min(element.maximum.width, available.width));
    height = clampDimension(height, element.minimum.height, std::min(element.maximum.height, available.height));
    return {width, height};
}

Size LayoutEngine::measureContent(const Element& element, Size available) const {
    float main = 0.0F;
    float cross = 0.0F;
    std::size_t visibleCount = 0;
    for (const auto& child : element.children()) {
        if (!child || !child->visible) continue;
        const Size childSize = measure(*child, available);
        main += mainSize(childSize, element.axis);
        cross = std::max(cross, crossSize(childSize, element.axis));
        ++visibleCount;
    }
    if (visibleCount > 1) main += nonNegative(element.gap) * static_cast<float>(visibleCount - 1);
    const float intrinsicMain = mainSize(element.intrinsic, element.axis);
    const float intrinsicCross = crossSize(element.intrinsic, element.axis);
    main = std::max(main, intrinsicMain);
    cross = std::max(cross, intrinsicCross);
    return element.axis == Axis::Horizontal ? Size{main, cross} : Size{cross, main};
}

void LayoutEngine::arrange(Element& element, Rect bounds) const {
    if (!element.visible) { element.bounds = {}; return; }
    element.bounds = {bounds.x, bounds.y,
        clampDimension(bounds.width, element.minimum.width, element.maximum.width),
        clampDimension(bounds.height, element.minimum.height, element.maximum.height)};
    arrangeChildren(element);
}

void LayoutEngine::arrangeChildren(Element& element) const {
    const float left = element.bounds.x + nonNegative(element.padding.left);
    const float top = element.bounds.y + nonNegative(element.padding.top);
    const float innerWidth = std::max(0.0F, element.bounds.width - nonNegative(element.padding.left) - nonNegative(element.padding.right));
    const float innerHeight = std::max(0.0F, element.bounds.height - nonNegative(element.padding.top) - nonNegative(element.padding.bottom));
    const float availableMain = element.axis == Axis::Horizontal ? innerWidth : innerHeight;
    const float availableCross = element.axis == Axis::Horizontal ? innerHeight : innerWidth;
    const float gap = nonNegative(element.gap);

    float fixedAndAuto = 0.0F;
    float flexWeight = 0.0F;
    std::size_t visibleCount = 0;
    for (const auto& child : element.children()) {
        if (!child || !child->visible) continue;
        ++visibleCount;
        const Length mainLength = element.axis == Axis::Horizontal ? child->width : child->height;
        if (mainLength.mode == SizeMode::Flex) flexWeight += std::max(0.0F, mainLength.value);
        else fixedAndAuto += mainSize(measure(*child, {innerWidth, innerHeight}), element.axis);
    }
    const float gaps = visibleCount > 1 ? gap * static_cast<float>(visibleCount - 1) : 0.0F;
    const float flexAvailable = std::max(0.0F, availableMain - fixedAndAuto - gaps);
    float cursor = element.axis == Axis::Horizontal ? left : top;

    for (const auto& childPtr : element.children()) {
        if (!childPtr) continue;
        Element& child = *childPtr;
        if (!child.visible) { child.bounds = {}; continue; }
        const Length mainLength = element.axis == Axis::Horizontal ? child.width : child.height;
        const Length crossLength = element.axis == Axis::Horizontal ? child.height : child.width;
        const Size measured = measure(child, {innerWidth, innerHeight});
        const float weight = mainLength.mode == SizeMode::Flex ? std::max(0.0F, mainLength.value) : 0.0F;
        const float childMain = mainLength.mode == SizeMode::Flex
            ? (flexWeight > 0.0F ? flexAvailable * weight / flexWeight : 0.0F)
            : mainSize(measured, element.axis);
        float childCross = crossLength.mode == SizeMode::Fixed ? crossLength.value : crossSize(measured, element.axis);
        if (element.crossAlign == Align::Stretch && crossLength.mode != SizeMode::Fixed) childCross = availableCross;
        childCross = clampDimension(childCross,
            crossMin(child.minimum, element.axis),
            std::min(crossMin(child.maximum, element.axis), availableCross));
        const float crossOffset = element.crossAlign == Align::Center ? (availableCross - childCross) * 0.5F
            : element.crossAlign == Align::End ? availableCross - childCross : 0.0F;
        Rect childBounds;
        if (element.axis == Axis::Horizontal) childBounds = {cursor, top + crossOffset, childMain, childCross};
        else childBounds = {left + crossOffset, cursor, childCross, childMain};
        arrange(child, childBounds);
        cursor += childMain + gap;
    }
}

} // namespace pebble::ui
