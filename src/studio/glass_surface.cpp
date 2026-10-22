#include "glass_surface.hpp"

#include "theme.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include <algorithm>

namespace {

QRgb averagePixel(int red, int green, int blue, int alpha, int count) {
    return qRgba(red / count, green / count, blue / count, alpha / count);
}

QImage boxBlur(const QImage &source, int radius, bool horizontal) {
    const QImage input = source.convertToFormat(QImage::Format_ARGB32);
    QImage output(input.size(), QImage::Format_ARGB32);
    const int major = horizontal ? input.height() : input.width();
    const int minor = horizontal ? input.width() : input.height();
    const int window = radius * 2 + 1;

    for (int line = 0; line < major; ++line) {
        int red = 0;
        int green = 0;
        int blue = 0;
        int alpha = 0;

        for (int offset = -radius; offset <= radius; ++offset) {
            const int position = qBound(0, offset, minor - 1);
            const QRgb pixel = horizontal
                ? input.pixel(position, line)
                : input.pixel(line, position);
            red += qRed(pixel);
            green += qGreen(pixel);
            blue += qBlue(pixel);
            alpha += qAlpha(pixel);
        }

        for (int position = 0; position < minor; ++position) {
            const QRgb blurred = averagePixel(red, green, blue, alpha, window);

            if (horizontal) {
                output.setPixel(position, line, blurred);
            } else {
                output.setPixel(line, position, blurred);
            }

            const int removeAt = qBound(0, position - radius, minor - 1);
            const int addAt = qBound(0, position + radius + 1, minor - 1);
            const QRgb removed = horizontal
                ? input.pixel(removeAt, line)
                : input.pixel(line, removeAt);
            const QRgb added = horizontal
                ? input.pixel(addAt, line)
                : input.pixel(line, addAt);
            red += qRed(added) - qRed(removed);
            green += qGreen(added) - qGreen(removed);
            blue += qBlue(added) - qBlue(removed);
            alpha += qAlpha(added) - qAlpha(removed);
        }
    }

    return output;
}

}

QImage makeGlassBackdrop(const QImage &source, int blurStrength, qreal deviceRatio) {
    if (source.isNull()) {
        return QImage();
    }

    const QSize cacheSize(
        qMax(1, source.width() / 2),
        qMax(1, source.height() / 2)
    );
    QImage softened = source.scaled(
        cacheSize,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation
    ).convertToFormat(QImage::Format_ARGB32);
    const qreal cacheRatio = qMax(0.5, deviceRatio / 2.0);
    const int radius = qMax(0, qRound(qBound(0, blurStrength, 36) * cacheRatio / 2.0));

    if (radius > 0) {
        for (int pass = 0; pass < 3; ++pass) {
            softened = boxBlur(softened, radius, true);
            softened = boxBlur(softened, radius, false);
        }
    }

    softened.setDevicePixelRatio(cacheRatio);
    return softened;
}

GlassSurface::GlassSurface(const QImage *background, QWidget *parent)
    : QWidget(parent), backdrop(background), tint(theme::glassTint),
      opacityPercent(theme::defaultGlassTintOpacity) {
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
}

void GlassSurface::setBackdrop(const QImage *background) {
    backdrop = background;
    update();
}

void GlassSurface::setAppearance(const QColor &newTint, int newOpacityPercent) {
    tint = newTint.isValid() ? newTint : QColor(theme::glassTint);
    opacityPercent = qBound(0, newOpacityPercent, 92);
    update();
}

void GlassSurface::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF panelRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath panelPath;
    panelPath.addRoundedRect(panelRect, 5.0, 5.0);
    painter.setClipPath(panelPath);

    if (backdrop != nullptr && !backdrop->isNull()) {
        const QPoint origin = mapTo(window(), QPoint(0, 0));
        const QRectF windowRect(QPointF(0.0, 0.0), QSizeF(window()->size()));
        const QRectF target = windowRect.translated(-origin.x(), -origin.y());
        painter.drawImage(target, *backdrop, QRectF(backdrop->rect()));
    } else {
        painter.fillRect(rect(), QColor(theme::panel));
    }

    QColor tintLayer = tint;
    tintLayer.setAlphaF(static_cast<float>(opacityPercent) / 100.0F);
    painter.fillPath(panelPath, tintLayer);
    painter.setClipping(false);

    QColor borderColour(theme::glassBorder);
    borderColour.setAlpha(theme::glassBorderAlpha);
    QPen border(borderColour);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(panelPath);

    QColor highlightColour(theme::text);
    highlightColour.setAlpha(theme::glassHighlightAlpha);
    QPen highlight(highlightColour);
    painter.setPen(highlight);
    painter.drawLine(panelRect.topLeft() + QPointF(6.0, 1.0),
                     panelRect.topRight() + QPointF(-6.0, 1.0));
}
