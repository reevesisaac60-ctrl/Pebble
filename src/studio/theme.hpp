#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QPixmap>
#include <QString>

class QApplication;

namespace theme {

constexpr const char *background = "#0b0b0e";
constexpr const char *panel = "#121216";
constexpr const char *surface = "#1c1c22";
constexpr const char *completionSurface = "#17171d";
constexpr const char *overlay = "#24242b";
constexpr const char *line = "#303038";
constexpr const char *lineStrong = "#3e3e48";
constexpr const char *glassTint = "#111118";
constexpr const char *glassBorder = "#5a5a69";
constexpr int defaultGlassTintOpacity = 86;
constexpr int defaultGlassBlur = 22;
constexpr int glassBorderAlpha = 170;
constexpr int glassHighlightAlpha = 18;

constexpr const char *chrome = "#0e0e12";

constexpr const char *gutter = "#101014";
constexpr const char *lineNumber = "#6a6a76";
constexpr const char *lineNumberActive = "#a1a1aa";

constexpr const char *text = "#f5f5f7";
constexpr const char *textDim = "#92929d";
constexpr const char *textFaint = "#6b6b76";

constexpr const char *accent = "#a1a1aa";

constexpr const char *selection = "#33334a";
constexpr const char *currentLine = "#a1a1aa";

constexpr const char *error = "#ff9292";
constexpr const char *warning = "#d9a441";
constexpr const char *success = "#63b58e";
constexpr const char *info = "#7fb3d6";

constexpr const char *modified = "#d9a441";

constexpr const char *keyword = "#c4b5fd";
constexpr const char *string = "#f2b8a0";
constexpr const char *number = "#8bd5e8";
constexpr const char *comment = "#9a9aa5";
constexpr const char *builtin = "#9cb8ff";
constexpr const char *boolean = "#8bd5e8";
constexpr const char *function = "#9cb8ff";
constexpr const char *type = "#d6c4ff";
constexpr const char *declaration = "#d6c4ff";
constexpr const char *operatorColour = "#d4d4dc";
constexpr const char *ghostText = "#898695";

constexpr const char *wallpaper = "#000000";

}

namespace icons {

QIcon explorer();

QIcon output();

QIcon close();

QIcon dirtyDot();

QPixmap pebble();

}

QPalette studioPalette();

constexpr int defaultBackgroundDarkness = 220;

QString studioStyleSheet();

QFont studioCodeFont(int pointSize);

QFont studioUiFont(int pointSize);

QFont studioApplicationFont();

void applyStudioTheme(QApplication &application);

QString studioDesktopFileName();
