#include "theme.hpp"

#include "assets.hpp"

#include <QApplication>
#include <QFont>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QStyleFactory>

#include <functional>

namespace {

QPixmap drawIcon(
    const QColor &colour,
    const std::function<void(QPainter &, const QColor &)> &paint
) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    paint(painter, colour);

    return pixmap;
}

QIcon statefulIcon(
    const QColor &off,
    const QColor &on,
    const std::function<void(QPainter &, const QColor &)> &paint
) {
    QIcon icon;

    icon.addPixmap(drawIcon(off, paint), QIcon::Normal, QIcon::Off);
    icon.addPixmap(drawIcon(on, paint), QIcon::Normal, QIcon::On);
    icon.addPixmap(drawIcon(on, paint), QIcon::Active, QIcon::Off);
    icon.addPixmap(drawIcon(on, paint), QIcon::Active, QIcon::On);

    return icon;
}

void paintExplorer(QPainter &painter, const QColor &colour) {
    painter.setPen(QPen(colour, 1.3));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(6, 3, 12, 18));

    for (const int y : {9, 13, 17}) {
        painter.drawLine(QPointF(9, y), QPointF(15, y));
    }
}

void paintOutput(QPainter &painter, const QColor &colour) {
    painter.setPen(QPen(colour, 1.3));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(3, 5, 18, 14));
    painter.drawLine(QPointF(3, 14), QPointF(21, 14));
}

void paintPebble(QPainter &painter, const QColor &colour) {
    painter.setPen(QPen(colour, 1.4));
    painter.setBrush(QColor(theme::surface));

    QPainterPath stone;
    stone.moveTo(12, 3);
    stone.cubicTo(18, 3, 21, 8, 20, 13);
    stone.cubicTo(19, 18, 15, 21, 11, 21);
    stone.cubicTo(6, 21, 3, 17, 4, 12);
    stone.cubicTo(5, 7, 7, 3, 12, 3);
    stone.closeSubpath();
    painter.drawPath(stone);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(theme::accent));
    painter.drawEllipse(QPointF(9, 9), 2.6, 1.8);
}

void paintClose(QPainter &painter, const QColor &colour) {
    painter.setPen(QPen(colour, 2.2, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);

    painter.drawLine(QPointF(6, 6), QPointF(18, 18));
    painter.drawLine(QPointF(18, 6), QPointF(6, 18));
}

void paintDirtyDot(QPainter &painter, const QColor &colour) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(colour);
    painter.drawEllipse(QPointF(12, 12), 3.6, 3.6);
}

QString rgbParts(const char *hex) {
    const QColor colour = QColor(QLatin1String(hex));

    return QStringLiteral("%1,%2,%3")
        .arg(colour.red())
        .arg(colour.green())
        .arg(colour.blue());
}

void putColour(QString &sheet, const char *name, const char *colour) {
    sheet.replace(
        QLatin1String(name),
        QLatin1String(colour)
    );
}

}

namespace icons {

QIcon explorer() {
    return statefulIcon(
        QColor(theme::textDim),
        QColor(theme::text),
        paintExplorer
    );
}

QIcon output() {
    return statefulIcon(
        QColor(theme::textDim),
        QColor(theme::text),
        paintOutput
    );
}

QIcon close() {
    QIcon icon;

    icon.addPixmap(drawIcon(QColor(theme::textFaint), paintClose), QIcon::Normal, QIcon::Off);
    icon.addPixmap(drawIcon(QColor(theme::text), paintClose), QIcon::Active, QIcon::Off);

    return icon;
}

QIcon dirtyDot() {
    QIcon icon;

    icon.addPixmap(drawIcon(QColor(theme::textFaint), paintDirtyDot), QIcon::Normal, QIcon::Off);
    icon.addPixmap(drawIcon(QColor(theme::text), paintDirtyDot), QIcon::Active, QIcon::Off);

    return icon;
}

QPixmap pebble() {
    return drawIcon(QColor(theme::accent), paintPebble);
}

}

QPalette studioPalette() {
    QPalette palette;

    palette.setColor(QPalette::Window, QColor(theme::background));
    palette.setColor(QPalette::WindowText, QColor(theme::text));
    palette.setColor(QPalette::Base, QColor(theme::background));
    palette.setColor(QPalette::AlternateBase, QColor(theme::panel));
    palette.setColor(QPalette::Text, QColor(theme::text));
    palette.setColor(QPalette::Button, QColor(theme::surface));
    palette.setColor(QPalette::ButtonText, QColor(theme::text));
    palette.setColor(QPalette::BrightText, QColor(theme::accent));
    palette.setColor(QPalette::Highlight, QColor(theme::selection));
    palette.setColor(QPalette::HighlightedText, QColor(theme::text));
    palette.setColor(QPalette::ToolTipBase, QColor(theme::surface));
    palette.setColor(QPalette::ToolTipText, QColor(theme::text));
    palette.setColor(QPalette::PlaceholderText, QColor(theme::textFaint));
    palette.setColor(QPalette::Link, QColor(theme::accent));
    palette.setColor(QPalette::Light, QColor(theme::overlay));
    palette.setColor(QPalette::Midlight, QColor(theme::line));
    palette.setColor(QPalette::Dark, QColor(theme::line));
    palette.setColor(QPalette::Mid, QColor(theme::lineStrong));
    palette.setColor(QPalette::Shadow, QColor(theme::background));

    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(theme::textFaint));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(theme::textFaint));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(theme::textFaint));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(theme::textFaint));

    return palette;
}

QString studioStyleSheet() {
    QString sheet = QStringLiteral(R"CSS(
/* The frame of the window and the text of the widgets. */
/* The window paints the user's wallpaper underneath the workspace. The
   GlassSurface panels composite a blurred, darkened copy beneath their crisp
   child widgets. */
QMainWindow { background: transparent; }
QWidget { color: @text@; font-family: "JetBrains Mono", "Cascadia Mono", "DejaVu Sans Mono", monospace; }
QLabel { background: transparent; }

/* The menu bar. */
QMenuBar { background: @chrome@; border-bottom: 1px solid @line@;
           padding: 3px 6px; spacing: 3px; }
QMenuBar::item { background: transparent; color: @textDim@; border-radius: 4px;
                 padding: 5px 9px; }
QMenuBar::item:selected, QMenuBar::item:pressed { background: rgba(@accentRgb@, 38);
                                                  color: @text@; }
QMenuBar::item:disabled { color: @textFaint@; }

/* The menus. They are the same everywhere, in the menu bar and in the
   context menu of the project tree. */
QMenu { background: @surface@; border: 1px solid @lineStrong@; padding: 5px;
        border-radius: 6px; }
QMenu::item { color: @text@; border-radius: 3px; padding: 7px 30px 7px 12px; }
QMenu::item:selected { background: rgba(@accentRgb@, 48); color: @text@; }
QMenu::item:disabled { color: @textFaint@; }
QMenu::separator { background: @line@; height: 1px; margin: 5px 8px; }
QMenu::icon { padding-left: 6px; }

/* The status bar. */
QStatusBar { background: @chrome@; color: @textDim@; border-top: 1px solid @line@;
            padding: 0 3px; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: @textDim@; padding: 2px 9px; }
QStatusBar QLabel#document, QStatusBar QLabel#cursor,
QStatusBar QLabel#encoding { color: @textDim@; }
QStatusBar QLabel#state { color: @textDim@; }
QStatusBar QLabel#state[busy="true"] { color: @info@; }
QStatusBar QLabel#state[modified="true"] { color: @modified@; }

/* The tabs of the documents. The tab bar paints the marker of the tab
   that is active, so the tab itself only carries the space for it. */
QTabWidget, QTabWidget#tabs { background: transparent; }
QTabWidget::pane { background: transparent; border: none; }
QTabBar { background: transparent; border: none;
          qproperty-drawBase: 0; }
QTabBar::tab { background: transparent; color: @textDim@; border: none;
               border-bottom: 2px solid transparent;
               padding: 9px 8px 9px 14px; min-width: 72px; max-width: 220px; }
QTabBar::tab:hover:!selected { background: rgba(@accentRgb@, 22); color: @text@; }
QTabBar::tab:selected { background: rgba(@accentRgb@, 20); color: @text@; }
QToolButton#tab_close_button { background: transparent; border: none;
                                padding: 0; border-radius: 4px; }
QToolButton#tab_close_button:hover { background: rgba(@accentRgb@, 48); }

/* The surfaces that hold text. */
QPlainTextEdit, QTextEdit, QLineEdit { background: @background@; color: @text@;
                                       border: 1px solid @line@;
                                       selection-background-color: @selection@;
                                       selection-color: @text@; }
QLineEdit { border-radius: 4px; padding: 4px 7px; }
QLineEdit:focus { border-color: @accent@; }
QPlainTextEdit:focus, QTextEdit:focus { border-color: @lineStrong@; }

/* The tree of the project and the list of the diagnostics. */
QTreeWidget, QListWidget { background: @panel@; color: @text@;
                            border: 1px solid @line@; outline: none; }
QTreeWidget::item, QListWidget::item { padding: 3px 2px; }
QTreeWidget::item:hover, QListWidget::item:hover { background: @overlay@; }
QTreeWidget::item:selected, QListWidget::item:selected { background: @selection@;
                                                           color: @text@; }
QHeaderView { background: transparent; border: none; }
QHeaderView::section { background: transparent; color: @textDim@;
                        border: none; border-bottom: 1px solid @line@;
                        font-size: 8.5pt; padding: 8px 12px; text-align: left; }
QWidget#bottom_panel { background: transparent; border: none; }
QWidget#explorer_glass, QWidget#editor_glass { background: transparent; }
QLabel#panel_title { background: transparent; color: @textDim@;
                     font-size: 8.5pt; padding: 2px 6px; }

/* The output pane and the diagnostics are panels, not code. */
QPlainTextEdit#output { background: transparent; border: none; }
QPlainTextEdit#editor, QPlainTextEdit#editor::viewport { background: transparent; border: none; }
QPlainTextEdit#editor:focus { border-color: @accent@; }
QTreeWidget#project_tree { background: transparent; color: @text@;
                           border: none; outline: 0; padding: 5px 0;
                           show-decoration-selected: 1; }
QTreeWidget#project_tree::viewport { background: transparent; }
QTreeWidget#project_tree::item { padding: 3px 5px; min-height: 24px; border: none; }
QTreeWidget#project_tree::item:hover { background: rgba(@accentRgb@, 30); }
QTreeWidget#project_tree::item:selected { background: rgba(@selectionRgb@, 205);
                                          color: @text@;
                                          border-left: 2px solid @accent@; }
QTreeWidget#project_tree::item:selected:!active { background: rgba(@selectionRgb@, 145); }
QListWidget#diagnostics { background: transparent; border: none; }
QListWidget#diagnostics::item { padding: 3px 6px; border-radius: 3px; }
QListWidget#diagnostics::item:hover { background: rgba(@accentRgb@, 22); }
QListWidget#diagnostics::item:selected { background: rgba(@selectionRgb@, 190); }
QListWidget#diagnostics::viewport { background: transparent; }

/* The buttons. */
QPushButton { background: @surface@; color: @text@; border: 1px solid @lineStrong@;
              border-radius: 4px; padding: 4px 12px; }
QPushButton:hover { background: @overlay@; border-color: @accent@; }
QPushButton:pressed { background: @panel@; }
QPushButton:disabled { color: @textFaint@; border-color: @line@; }
QPushButton:focus { border-color: @accent@; }

/* The button that empties the output pane. */
QPushButton#clear_output_button { background: transparent; color: @textDim@;
                                  border: none; padding: 2px 8px; }
QPushButton#clear_output_button:hover { background: @surface@; color: @text@; }

/* The activity bar. The panel that is on the screen is marked. */
QWidget#activitybar { background: transparent; border: none; }
QToolButton { background: transparent; border: none; }
QToolButton#explorer_button, QToolButton#output_button {
    border-left: 2px solid transparent; }
QToolButton#explorer_button:hover, QToolButton#output_button:hover {
    background: rgba(@accentRgb@, 22); }
QToolButton#explorer_button:checked, QToolButton#output_button:checked {
    background: rgba(@accentRgb@, 22); border-left: 2px solid @accent@; }

/* The find and replace panel. */
QWidget#findbar { background: transparent;
                  border-bottom: 1px solid @line@; }
QWidget#findbar QLineEdit, QLineEdit#find_input, QLineEdit#replace_input {
    background: @background@; color: @text@; border: 1px solid @line@; }
QWidget#findbar QLineEdit:focus, QLineEdit#find_input:focus,
QLineEdit#replace_input:focus { border-color: @accent@; }
QWidget#findbar QPushButton, QPushButton#barbutton {
    background: @surface@; color: @text@; border: 1px solid @lineStrong@;
    border-radius: 5px; padding: 3px 10px; }
QWidget#findbar QToolButton, QToolButton#match_case {
    background: transparent; color: @textDim@; border: 1px solid transparent;
    border-radius: 5px; padding: 5px 9px; }
QWidget#findbar QToolButton:hover, QToolButton#match_case:hover {
    background: rgba(@accentRgb@, 22); color: @text@; }
QWidget#findbar QToolButton:checked, QToolButton#match_case:checked { color: @text@; }
QWidget#findbar QPushButton:hover, QPushButton#barbutton:hover {
    background: @overlay@; border-color: @accent@; color: @text@; }
QWidget#findbar QPushButton:disabled, QPushButton#barbutton:disabled {
    color: @textFaint@; }
QWidget#findbar QLabel { color: @textDim@; }

/* The line that sends an answer to the running program. */
QLineEdit#program_input { background: @background@; color: @text@;
                          border: 1px solid @lineStrong@; border-radius: 5px;
                          padding: 4px 8px; }
QLineEdit#program_input:focus { border-color: @accent@; }
QLineEdit#program_input:disabled { color: @textFaint@; background: @panel@;
                                    border-color: @line@; }

/* The strip with the welcome tab, which looks like the tab bar of the documents. */
QWidget#welcome_strip { background: transparent; }
QWidget#welcome_strip QTabBar { background: transparent; }

/* The page that stands in when the welcome tab was closed. */
QWidget#blank_page { background: transparent; }

/* The welcome page. It stands where the documents stand while none is open. */
QWidget#welcome { background: transparent; }
QLabel#welcome_title { font-size: 15pt; font-weight: 600; color: @text@; }
QLabel#section { font-size: 8pt; font-weight: 600; color: @accent@;
                 padding-top: 2px; }
QLabel#muted { color: @textDim@; }
QLabel#hint { font-size: 8pt; color: @textFaint@; }
QPushButton#link { background: transparent; border: 1px solid transparent;
                   text-align: left; padding: 5px 9px; color: @text@; }
QPushButton#link:hover { background: rgba(@accentRgb@, 22);
                         border-color: @line@; color: @accent@; }
QPushButton#link:focus { border-color: @accent@; background: rgba(@accentRgb@, 22); }

/* The syntax colour dialog. */
QPushButton#syntax_colour_button { border-radius: 3px; padding: 5px 10px;
                                   color: @background@; }
QPushButton#syntax_reset_button { background: transparent; border: 1px solid @line@;
                                  color: @textDim@; }
QPushButton#syntax_reset_button:hover { background: @surface@; color: @text@;
                                        border-color: @accent@; }
QPlainTextEdit#syntax_preview { background: @background@; border: 1px solid @line@; }

/* The checkbox of a panel. */
QCheckBox { color: @text@; spacing: 7px; }
QCheckBox::indicator { background: @background@; border: 1px solid @lineStrong@;
                       border-radius: 3px; height: 14px; width: 14px; }
QCheckBox::indicator:hover { border-color: @accent@; }
QCheckBox::indicator:checked { background: @accent@; border-color: @accent@; }

/* The scrollbars. */
QScrollBar:vertical { background: transparent; width: 11px; margin: 0; }
QScrollBar::handle:vertical { background: rgba(@accentRgb@, 70); border-radius: 3px;
                              min-height: 28px; margin: 2px 2px 2px 3px; }
QScrollBar::handle:vertical:hover { background: rgba(@accentRgb@, 130); }
QScrollBar:horizontal { background: transparent; height: 11px; margin: 0; }
QScrollBar::handle:horizontal { background: rgba(@accentRgb@, 70); border-radius: 3px;
                                min-width: 28px; margin: 3px 2px 2px 2px; }
QScrollBar::handle:horizontal:hover { background: rgba(@accentRgb@, 130); }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* The handles between the panels. */
QSplitter::handle { background: transparent; }
QSplitter::handle:horizontal { width: 5px; }
QSplitter::handle:vertical { height: 5px; }
QSplitter::handle:hover { background: rgba(@accentRgb@, 90); }

/* The titles of the sections. */
QLabel#diagnostics_title { color: @textDim@; font-size: 8.5pt; padding: 2px 6px; }

/* The hints. */
QToolTip { background: @completionSurface@; color: @text@; border: 1px solid @lineStrong@;
           padding: 5px 8px; }
QFrame#intellisense_popup, QFrame#parameter_hint {
    background: @completionSurface@; border: 1px solid @lineStrong@;
    border-radius: 5px; }
QFrame#parameter_hint QLabel { background: transparent; color: @text@;
                               padding: 6px 10px; }
QListWidget#intellisense_list { background: transparent; border: none;
                                outline: none; padding: 4px; }
QListWidget#intellisense_list::item { color: @text@; padding: 4px 8px;
                                      border-radius: 3px; }
QListWidget#intellisense_list::item:hover { background: rgba(@accentRgb@, 26); }
QListWidget#intellisense_list::item:selected { background: rgba(@accentRgb@, 54);
                                               color: @text@; }

/* The dialogs. */
QDialog, QMessageBox { background: @background@; }
QDialog QLabel { color: @text@; }
)CSS");

    putColour(sheet, "@text@", theme::text);
    putColour(sheet, "@textDim@", theme::textDim);
    putColour(sheet, "@textFaint@", theme::textFaint);
    putColour(sheet, "@accent@", theme::accent);
    putColour(sheet, "@background@", theme::background);
    putColour(sheet, "@panel@", theme::panel);
    putColour(sheet, "@surface@", theme::surface);
    putColour(sheet, "@completionSurface@", theme::completionSurface);
    putColour(sheet, "@overlay@", theme::overlay);
    putColour(sheet, "@line@", theme::line);
    putColour(sheet, "@lineStrong@", theme::lineStrong);
    putColour(sheet, "@selection@", theme::selection);
    putColour(sheet, "@error@", theme::error);
    putColour(sheet, "@warning@", theme::warning);
    putColour(sheet, "@success@", theme::success);
    putColour(sheet, "@info@", theme::info);
    putColour(sheet, "@modified@", theme::modified);

    putColour(sheet, "@chrome@", theme::chrome);
    sheet.replace(
        QLatin1String("@accentRgb@"),
        rgbParts(theme::accent)
    );
    sheet.replace(
        QLatin1String("@selectionRgb@"),
        rgbParts(theme::selection)
    );

    return sheet;
}

QFont studioCodeFont(int pointSize) {
    QFont font;

    font.setFamilies({
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("Cascadia Mono"),
        QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("monospace")
    });
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(pointSize);

    return font;
}

QFont studioApplicationFont() {
    QFont font = studioCodeFont(10);

    font.setPointSizeF(9.5);

    return font;
}

QString studioDesktopFileName() {
    return QStringLiteral("pebble-studio");
}

void applyStudioTheme(QApplication &application) {
    QGuiApplication::setDesktopFileName(studioDesktopFileName());

    application.setStyle(QStyleFactory::create("Fusion"));
    application.setPalette(studioPalette());
    application.setFont(studioApplicationFont());

    const QIcon icon = studio::icon();

    application.setWindowIcon(icon.isNull() ? QIcon(icons::pebble()) : icon);
    application.setStyleSheet(studioStyleSheet());
}
