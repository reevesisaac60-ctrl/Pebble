#include "theme.hpp"
#include "glass_surface.hpp"

#include <QApplication>
#include <QColor>
#include <QLabel>
#include <QObject>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtTest/QTest>

namespace {

class BackdropCanvas : public QWidget {
public:
    explicit BackdropCanvas(const QColor &colour)
        : background(colour) {
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter painter(this);
        painter.fillRect(event->rect(), background);
    }

private:
    QColor background;
};

double luminance(const QColor &colour) {
    const auto channel = [](int value) {
        const double part = value / 255.0;

        return part <= 0.03928 ? part / 12.92 : std::pow((part + 0.055) / 1.055, 2.4);
    };

    return 0.2126 * channel(colour.red())
         + 0.7152 * channel(colour.green())
         + 0.0722 * channel(colour.blue());
}

double contrast(const QColor &first, const QColor &second) {
    const double a = luminance(first);
    const double b = luminance(second);
    const double high = qMax(a, b);
    const double low = qMin(a, b);

    return (high + 0.05) / (low + 0.05);
}

double hue(const QColor &colour) {
    const double red = colour.redF();
    const double green = colour.greenF();
    const double blue = colour.blueF();
    const double high = qMax(red, qMax(green, blue));
    const double low = qMin(red, qMin(green, blue));
    const double delta = high - low;

    if (delta <= 0.0) {
        return 0.0;
    }

    double value = 0.0;

    if (high == red) {
        value = 60.0 * std::fmod((green - blue) / delta, 6.0);
    } else if (high == green) {
        value = 60.0 * ((blue - red) / delta + 2.0);
    } else {
        value = 60.0 * ((red - green) / delta + 4.0);
    }

    if (value < 0.0) {
        value += 360.0;
    }

    return value;
}

QList<QPair<QString, QColor>> paletteColours() {
    return {
        {"background", QColor(theme::background)},
        {"panel", QColor(theme::panel)},
        {"surface", QColor(theme::surface)},
        {"completionSurface", QColor(theme::completionSurface)},
        {"overlay", QColor(theme::overlay)},
        {"line", QColor(theme::line)},
        {"lineStrong", QColor(theme::lineStrong)},
        {"chrome", QColor(theme::chrome)},
        {"gutter", QColor(theme::gutter)},
        {"lineNumber", QColor(theme::lineNumber)},
        {"lineNumberActive", QColor(theme::lineNumberActive)},
        {"text", QColor(theme::text)},
        {"textDim", QColor(theme::textDim)},
        {"textFaint", QColor(theme::textFaint)},
        {"accent", QColor(theme::accent)},
        {"selection", QColor(theme::selection)},
        {"currentLine", QColor(theme::currentLine)},
        {"error", QColor(theme::error)},
        {"warning", QColor(theme::warning)},
        {"success", QColor(theme::success)},
        {"info", QColor(theme::info)},
        {"modified", QColor(theme::modified)},
        {"keyword", QColor(theme::keyword)},
        {"string", QColor(theme::string)},
        {"number", QColor(theme::number)},
        {"comment", QColor(theme::comment)},
        {"builtin", QColor(theme::builtin)},
        {"boolean", QColor(theme::boolean)},
        {"function", QColor(theme::function)},
        {"type", QColor(theme::type)},
        {"declaration", QColor(theme::declaration)},
        {"operator", QColor(theme::operatorColour)},
    };
}

QList<QPair<QString, QColor>> chromeColours() {
    return {
        {"background", QColor(theme::background)},
        {"panel", QColor(theme::panel)},
        {"surface", QColor(theme::surface)},
        {"overlay", QColor(theme::overlay)},
        {"line", QColor(theme::line)},
        {"lineStrong", QColor(theme::lineStrong)},
        {"chrome", QColor(theme::chrome)},
        {"gutter", QColor(theme::gutter)},
        {"accent", QColor(theme::accent)},
        {"selection", QColor(theme::selection)},
    };
}

}

class StudioThemeTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    void thePaletteCarriesTheColoursOfTheTheme();
    void theStyleSheetHasNoColourOutsideTheTheme();
    void theStyleSheetCoversEverySurfaceOfTheStudio();
    void theTextIsReadableOnEverySurface();
    void theSecondaryTextIsReadable();
    void theSelectionIsVisibleAndReadable();
    void theSemanticColoursAreApartAndReadable();
    void theColoursOfTheCodeAreApartAndReadable();
    void theInterfaceHoldsNoMint();
    void theSyntaxPaletteUsesTheRequestedColours();
    void theEditorAndPanelsUseGlassSurfaces();
    void theBackdropBlurStrengthChangesTheImage();
    void theGlassSurfaceClipsAndKeepsChildrenSharp();
    void aMissingBackdropUsesAnOpaqueFallback();
    void theGlassBackdropAlignsToTheWindowWhenPanelsMove();
    void brightWallpapersKeepTextAtReadableContrast();

private:
    QString sheet;
};

void StudioThemeTest::initTestCase() {
    sheet = studioStyleSheet();
}

void StudioThemeTest::thePaletteCarriesTheColoursOfTheTheme() {
    const QPalette palette = studioPalette();

    QCOMPARE(palette.color(QPalette::Window), QColor(theme::background));
    QCOMPARE(palette.color(QPalette::WindowText), QColor(theme::text));
    QCOMPARE(palette.color(QPalette::Base), QColor(theme::background));
    QCOMPARE(palette.color(QPalette::AlternateBase), QColor(theme::panel));
    QCOMPARE(palette.color(QPalette::Text), QColor(theme::text));
    QCOMPARE(palette.color(QPalette::Button), QColor(theme::surface));
    QCOMPARE(palette.color(QPalette::ButtonText), QColor(theme::text));
    QCOMPARE(palette.color(QPalette::Highlight), QColor(theme::selection));
    QCOMPARE(palette.color(QPalette::HighlightedText), QColor(theme::text));
    QCOMPARE(palette.color(QPalette::Link), QColor(theme::accent));
    QCOMPARE(palette.color(QPalette::PlaceholderText), QColor(theme::textFaint));

    QVERIFY(
        palette.color(QPalette::Disabled, QPalette::ButtonText)
            != palette.color(QPalette::ButtonText)
    );
}

void StudioThemeTest::theStyleSheetHasNoColourOutsideTheTheme() {
    QVERIFY(!sheet.contains(QLatin1String("@")));

    QSet<QString> allowed;

    for (const auto &entry : paletteColours()) {
        allowed.insert(entry.second.name(QColor::HexRgb));
    }

    const QRegularExpression hex("\\#[0-9a-fA-F]{6}");
    QRegularExpressionMatchIterator matches = hex.globalMatch(sheet);

    while (matches.hasNext()) {
        const QString colour = matches.next().captured(0);

        QVERIFY2(
            allowed.contains(colour.toLower()),
            qPrintable(QStringLiteral("the style sheet carries the colour %1, "
                                      "which is not a colour of the theme").arg(colour))
        );
    }

    const QRegularExpression rgba("rgba\\(([0-9]+),([0-9]+),([0-9]+),");
    matches = rgba.globalMatch(sheet);

    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const QColor colour(
            match.captured(1).toInt(),
            match.captured(2).toInt(),
            match.captured(3).toInt()
        );

        QVERIFY2(
            allowed.contains(colour.name(QColor::HexRgb)),
            qPrintable(QStringLiteral("the style sheet carries the colour %1, "
                                      "which is not a colour of the theme")
                           .arg(colour.name(QColor::HexRgb)))
        );
    }
}

void StudioThemeTest::theStyleSheetCoversEverySurfaceOfTheStudio() {
    const QStringList named = {
        "activitybar",
        "barbutton",
        "blank_page",
        "bottom_panel",
        "clear_output_button",
        "cursor",
        "diagnostics",
        "diagnostics_title",
        "document",
        "editor",
        "encoding",
        "explorer_button",
        "find_input",
        "findbar",
        "hint",
        "link",
        "match_case",
        "muted",
        "output",
        "output_button",
        "panel_title",
        "program_input",
        "project_tree",
        "replace_input",
        "section",
        "state",
        "syntax_colour_button",
        "syntax_preview",
        "syntax_reset_button",
        "tab_close_button",
        "tabs",
        "welcome",
        "welcome_strip",
        "welcome_title",
    };

    for (const QString &name : named) {
        const QString own = QStringLiteral("#%1").arg(name);
        const QString descendant =
            QStringLiteral("QWidget#%1 ").arg(name);

        QVERIFY2(
            sheet.contains(own) || sheet.contains(descendant),
            qPrintable(QStringLiteral("the style sheet has no rule for %1").arg(name))
        );
    }
}

void StudioThemeTest::theTextIsReadableOnEverySurface() {
    const QColor text(theme::text);
    const QList<QColor> surfaces = {
        QColor(theme::background),
        QColor(theme::panel),
        QColor(theme::surface),
        QColor(theme::overlay),
    };

    for (const QColor &surface : surfaces) {
        const double ratio = contrast(text, surface);

        QVERIFY2(
            ratio >= 4.5,
            qPrintable(QStringLiteral("the text on %1 has a ratio of %2, "
                                      "which is under 4.5")
                           .arg(surface.name(QColor::HexRgb)).arg(ratio))
        );
    }
}

void StudioThemeTest::theSecondaryTextIsReadable() {
    const QColor dim(theme::textDim);
    const QList<QColor> surfaces = {
        QColor(theme::background),
        QColor(theme::panel),
        QColor(theme::surface),
    };

    for (const QColor &surface : surfaces) {
        const double ratio = contrast(dim, surface);

        QVERIFY2(
            ratio >= 4.5,
            qPrintable(QStringLiteral("the second text on %1 has a ratio of %2, "
                                      "which is under 4.5")
                           .arg(surface.name(QColor::HexRgb)).arg(ratio))
        );
    }

    QVERIFY(contrast(QColor(theme::text), QColor(theme::textDim)) > 1.5);
    QVERIFY(contrast(QColor(theme::textDim), QColor(theme::textFaint)) > 1.2);
}

void StudioThemeTest::theSelectionIsVisibleAndReadable() {
    const QColor selection(theme::selection);
    const QColor editor(theme::background);

    QVERIFY(contrast(selection, editor) >= 1.3);
    QVERIFY(contrast(QColor(theme::text), selection) >= 4.5);
}

void StudioThemeTest::theSemanticColoursAreApartAndReadable() {
    const QList<QPair<QString, QColor>> semantic = {
        {"error", QColor(theme::error)},
        {"warning", QColor(theme::warning)},
        {"success", QColor(theme::success)},
        {"info", QColor(theme::info)},
    };

    for (int i = 0; i < semantic.size(); ++i) {
        const double ratio = contrast(semantic[i].second, QColor(theme::background));

        QVERIFY2(
            ratio >= 4.5,
            qPrintable(QStringLiteral("the colour of %1 has a ratio of %2, "
                                      "which is under 4.5")
                           .arg(semantic[i].first).arg(ratio))
        );

        for (int j = i + 1; j < semantic.size(); ++j) {
            QVERIFY2(
                semantic[i].second != semantic[j].second,
                qPrintable(QStringLiteral("the colours of %1 and %2 are the same")
                               .arg(semantic[i].first, semantic[j].first))
            );
        }
    }

    for (const auto &entry : semantic) {
        QVERIFY(entry.second != QColor(theme::accent));
        QVERIFY(entry.second != QColor(theme::text));
    }
}

void StudioThemeTest::theColoursOfTheCodeAreApartAndReadable() {
    const QList<QPair<QString, QColor>> colours = {
        {"keyword", QColor(theme::keyword)},
        {"string", QColor(theme::string)},
        {"number", QColor(theme::number)},
        {"comment", QColor(theme::comment)},
        {"function", QColor(theme::function)},
        {"types and declarations", QColor(theme::type)},
        {"operator", QColor(theme::operatorColour)},
    };

    for (int i = 0; i < colours.size(); ++i) {
        const double ratio = contrast(colours[i].second, QColor(theme::background));

        QVERIFY2(
            ratio >= 4.5,
            qPrintable(QStringLiteral("the colour of %1 has a ratio of %2, "
                                      "which is under 4.5")
                           .arg(colours[i].first).arg(ratio))
        );

        for (int j = i + 1; j < colours.size(); ++j) {
            QVERIFY2(
                colours[i].second != colours[j].second,
                qPrintable(QStringLiteral("the colours of %1 and %2 are the same")
                               .arg(colours[i].first, colours[j].first))
            );
        }
    }

    QVERIFY(contrast(QColor(theme::comment), QColor(theme::background))
            < contrast(QColor(theme::keyword), QColor(theme::background)));
}

void StudioThemeTest::theSyntaxPaletteUsesTheRequestedColours() {
    QCOMPARE(QColor(theme::keyword), QColor("#C4B5FD"));
    QCOMPARE(QColor(theme::string), QColor("#F2B8A0"));
    QCOMPARE(QColor(theme::function), QColor("#9CB8FF"));
    QCOMPARE(QColor(theme::builtin), QColor("#9CB8FF"));
    QCOMPARE(QColor(theme::type), QColor("#D6C4FF"));
    QCOMPARE(QColor(theme::declaration), QColor("#D6C4FF"));
    QCOMPARE(QColor(theme::number), QColor("#8BD5E8"));
    QCOMPARE(QColor(theme::boolean), QColor("#8BD5E8"));
    QCOMPARE(QColor(theme::comment), QColor("#9A9AA5"));
    QCOMPARE(QColor(theme::operatorColour), QColor("#D4D4DC"));
    QCOMPARE(QColor(theme::error), QColor("#FF9292"));
}

void StudioThemeTest::theEditorAndPanelsUseGlassSurfaces() {
    QVERIFY(sheet.contains("QMainWindow { background: transparent; }"));
    QVERIFY(sheet.contains(
        "QPlainTextEdit#editor, QPlainTextEdit#editor::viewport { background: transparent;"
    ));
    QVERIFY(sheet.contains("QPlainTextEdit#output { background: transparent;"));
    QVERIFY(sheet.contains("QListWidget#diagnostics { background: transparent;"));
    QVERIFY(sheet.contains("QTreeWidget#project_tree { background: transparent;"));
    QVERIFY(sheet.contains("QWidget#activitybar { background: transparent;"));
    QVERIFY(sheet.contains("QMenu { background: #1c1c22;"));
    QVERIFY(sheet.contains("QStatusBar { background: #0e0e12;"));
}

void StudioThemeTest::theBackdropBlurStrengthChangesTheImage() {
    QImage source(QSize(96, 64), QImage::Format_ARGB32);
    source.fill(Qt::black);

    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width() / 2; ++x) {
            source.setPixelColor(x, y, Qt::white);
        }
    }

    const QImage sharp = makeGlassBackdrop(source, 0, 1.0);
    const QImage blurred = makeGlassBackdrop(source, 30, 1.0);

    QVERIFY(!sharp.isNull());
    QCOMPARE(qRound(sharp.width() / sharp.devicePixelRatio()), source.width());
    const QColor edgeSharp = sharp.pixelColor(sharp.width() / 2, sharp.height() / 2);
    const QColor edgeBlurred = blurred.pixelColor(blurred.width() / 2, blurred.height() / 2);
    QVERIFY(edgeBlurred.red() > edgeSharp.red());
    QVERIFY(edgeBlurred.red() < 230);
}

void StudioThemeTest::theGlassSurfaceClipsAndKeepsChildrenSharp() {
    QImage wallpaper(QSize(100, 80), QImage::Format_ARGB32);
    wallpaper.fill(QColor(130, 90, 50));
    QImage backdrop = makeGlassBackdrop(wallpaper, 45, 1.0);

    BackdropCanvas canvas(QColor(12, 34, 56));
    canvas.resize(100, 80);
    GlassSurface surface(&backdrop, &canvas);
    surface.setGeometry(10, 10, 70, 60);

    auto *label = new QLabel(QStringLiteral("X"), &surface);
    label->setGeometry(8, 8, 32, 32);
    label->setStyleSheet(
        QStringLiteral("color: #f5f5f7; background: transparent; font-size: 22pt;")
    );

    canvas.show();
    QCoreApplication::processEvents();

    QImage rendered(canvas.size(), QImage::Format_ARGB32);
    rendered.fill(Qt::transparent);
    canvas.render(&rendered);

    QCOMPARE(rendered.pixelColor(2, 2), QColor(12, 34, 56));
    QVERIFY(rendered.pixelColor(50, 55) != QColor(theme::panel));
    QCOMPARE(rendered.pixelColor(10, 10), QColor(12, 34, 56));

    bool crispTextPixel = false;
    for (int y = 16; y < 42; ++y) {
        for (int x = 18; x < 48; ++x) {
            const QColor pixel = rendered.pixelColor(x, y);
            if (pixel.red() > 190 && pixel.green() > 190 && pixel.blue() > 190) {
                crispTextPixel = true;
                break;
            }
        }
        if (crispTextPixel) {
            break;
        }
    }
    QVERIFY(crispTextPixel);

    const QColor before = rendered.pixelColor(55, 55);
    wallpaper.fill(QColor(30, 80, 150));
    backdrop = makeGlassBackdrop(wallpaper, 45, 1.0);
    surface.setBackdrop(&backdrop);
    surface.repaint();
    QCoreApplication::processEvents();
    QImage refreshed(surface.size(), QImage::Format_ARGB32);
    refreshed.fill(Qt::transparent);
    surface.render(&refreshed);
    QVERIFY(refreshed.pixelColor(45, 45) != before);

    const QColor defaultOpacity = refreshed.pixelColor(45, 45);
    surface.setAppearance(QColor("#232330"), 92);
    surface.repaint();
    refreshed.fill(Qt::transparent);
    surface.render(&refreshed);
    QVERIFY(refreshed.pixelColor(45, 45) != defaultOpacity);
}

void StudioThemeTest::aMissingBackdropUsesAnOpaqueFallback() {
    BackdropCanvas canvas(QColor(12, 34, 56));
    canvas.resize(70, 60);
    GlassSurface surface(nullptr, &canvas);
    surface.setGeometry(canvas.rect());
    surface.setAppearance(QColor(theme::glassTint), 90);
    canvas.show();
    QCoreApplication::processEvents();

    QImage rendered(surface.size(), QImage::Format_ARGB32);
    rendered.fill(Qt::transparent);
    surface.render(&rendered);
    QCOMPARE(rendered.pixelColor(35, 30).alpha(), 255);
    QVERIFY(rendered.pixelColor(35, 30) != QColor(12, 34, 56));
}

void StudioThemeTest::theGlassBackdropAlignsToTheWindowWhenPanelsMove() {
    QImage wallpaper(QSize(80, 40), QImage::Format_ARGB32);
    wallpaper.fill(QColor("#d64b36"));
    for (int y = 0; y < wallpaper.height(); ++y) {
        for (int x = wallpaper.width() / 2; x < wallpaper.width(); ++x) {
            wallpaper.setPixelColor(x, y, QColor("#397bd2"));
        }
    }
    QImage backdrop = makeGlassBackdrop(wallpaper, 0, 1.0);
    QVERIFY(!backdrop.isNull());
    QVERIFY(backdrop.pixelColor(10, 10).red() > backdrop.pixelColor(10, 10).blue());

    BackdropCanvas canvas(Qt::black);
    canvas.resize(80, 40);
    GlassSurface surface(&backdrop, &canvas);
    surface.setAppearance(QColor(theme::glassTint), 0);
    surface.setGeometry(10, 0, 60, 40);
    canvas.show();
    QCoreApplication::processEvents();

    QImage rendered(canvas.size(), QImage::Format_ARGB32);
    rendered.fill(Qt::transparent);
    canvas.render(&rendered);
    QVERIFY2(rendered.pixelColor(20, 20).red() > rendered.pixelColor(20, 20).blue(),
             qPrintable(rendered.pixelColor(20, 20).name()));
    QVERIFY(rendered.pixelColor(60, 20).blue() > rendered.pixelColor(60, 20).red());

    surface.move(20, 0);
    QCoreApplication::processEvents();
    rendered.fill(Qt::transparent);
    canvas.render(&rendered);
    QVERIFY(rendered.pixelColor(25, 20).red() > rendered.pixelColor(25, 20).blue());
    QVERIFY(rendered.pixelColor(65, 20).blue() > rendered.pixelColor(65, 20).red());
}

void StudioThemeTest::brightWallpapersKeepTextAtReadableContrast() {
    QImage wallpaper(QSize(60, 40), QImage::Format_ARGB32);
    wallpaper.fill(Qt::white);
    const QImage backdrop = makeGlassBackdrop(wallpaper, theme::defaultGlassBlur, 1.0);

    BackdropCanvas canvas(Qt::black);
    canvas.resize(60, 40);
    GlassSurface surface(&backdrop, &canvas);
    surface.setAppearance(QColor(theme::glassTint), theme::defaultGlassTintOpacity);
    surface.setGeometry(canvas.rect());
    canvas.show();
    QCoreApplication::processEvents();

    QImage rendered(surface.size(), QImage::Format_ARGB32);
    surface.render(&rendered);
    const double ratio = contrast(QColor(theme::text), rendered.pixelColor(30, 20));
    QVERIFY(ratio >= 4.5);
}

void StudioThemeTest::theInterfaceHoldsNoMint() {
    for (const auto &entry : chromeColours()) {
        const QColor colour = entry.second;
        const double angle = hue(colour);
        const double saturation = colour.hsvSaturationF();

        const bool green = angle >= 90.0 && angle <= 165.0;

        QVERIFY2(
            !green || saturation <= 0.1,
            qPrintable(QStringLiteral("the colour of %1 is a green of %2")
                           .arg(entry.first).arg(colour.name(QColor::HexRgb)))
        );
    }
}

QTEST_MAIN(StudioThemeTest)
#include "test_studio_theme.moc"
