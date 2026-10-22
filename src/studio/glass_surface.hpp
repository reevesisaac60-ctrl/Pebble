#pragma once

#include <QImage>
#include <QColor>
#include <QWidget>

class QPaintEvent;

QImage makeGlassBackdrop(const QImage &source, int blurStrength, qreal deviceRatio);

class GlassSurface : public QWidget {
public:
    explicit GlassSurface(const QImage *backdrop, QWidget *parent = nullptr);

    void setBackdrop(const QImage *backdrop);
    void setAppearance(const QColor &tint, int opacityPercent);

    [[nodiscard]] const QImage *backdropImage() const { return backdrop; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    const QImage *backdrop = nullptr;
    QColor tint;
    int opacityPercent = 0;
};
