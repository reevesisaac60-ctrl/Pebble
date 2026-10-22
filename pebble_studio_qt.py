#!/usr/bin/env python3

import codecs
import re
import sys
from pathlib import Path

from PySide6.QtCore import (
    QEasingCurve,
    QProcess,
    QRect,
    QSettings,
    QSize,
    Qt,
    QTimer,
    QVariantAnimation,
)
from PySide6.QtGui import (
    QAction,
    QBrush,
    QColor,
    QFont,
    QIcon,
    QKeySequence,
    QPainter,
    QPalette,
    QPen,
    QPixmap,
    QSyntaxHighlighter,
    QTextBlockFormat,
    QTextCharFormat,
    QTextCursor,
    QTextDocument,
    QTextFormat,
    QTextOption,
)
from PySide6.QtWidgets import (
    QAbstractItemView,
    QApplication,
    QColorDialog,
    QDialog,
    QDialogButtonBox,
    QDockWidget,
    QFileDialog,
    QFormLayout,
    QHBoxLayout,
    QHeaderView,
    QInputDialog,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QSizePolicy,
    QTabBar,
    QTabWidget,
    QTextEdit,
    QToolBar,
    QToolButton,
    QTreeView,
    QVBoxLayout,
    QWidget,
)

try:
    from PySide6.QtWidgets import QFileIconProvider, QFileSystemModel
except ImportError:
    from PySide6.QtGui import QFileSystemModel
    from PySide6.QtWidgets import QFileIconProvider

APP_DIR = Path(__file__).resolve().parent
ROOT = Path.home() / "Documents" / "Pebble" if getattr(sys, "frozen", False) else APP_DIR
DEFAULT_BACKGROUND = str(APP_DIR / "pebble_wallpaper.svg")

TEXT = "#d8dedb"
BRIGHT = "#f0f5f1"
DIM = "#9aa7a0"
FAINT = "#647169"
LINE = "#303a34"
LINE_STRONG = "#414e46"
SOLID = "#202522"
HOVER = "rgba(166,215,180,18)"
SELECT = "rgba(166,215,180,36)"
SELECTION_BG = "#405348"

KEYWORD = "#b7c5ef"
STRING = "#a9d6b5"
NUMBER = "#e2c28e"
COMMENT = "#78877d"
BUILTIN = "#8dcfc0"
BOOLEAN = "#91c6bb"
FUNCTION = "#d7dfaa"
OPERATOR = "#b9c9bd"

SYNTAX_COLOUR_DEFAULTS = {
    "keyword": KEYWORD,
    "string": STRING,
    "number": NUMBER,
    "comment": COMMENT,
    "builtin": BUILTIN,
    "boolean": BOOLEAN,
    "function": FUNCTION,
    "operator": OPERATOR,
}
SYNTAX_COLOUR_LABELS = {
    "keyword": "Keywords",
    "string": "Strings",
    "number": "Numbers",
    "comment": "Comments",
    "builtin": "Built-ins (say, ask)",
    "boolean": "Booleans (true, false)",
    "function": "Function calls",
    "operator": "Operators and punctuation",
}

LEVEL_COLOURS = {"normal": TEXT, "info": DIM, "warning": "#bfa974", "error": "#c58b85"}
LEVEL_PREFIX = {"warning": "warning: ", "error": "error: "}

MONO = '"JetBrains Mono", "Cascadia Mono", "DejaVu Sans Mono", monospace'
KEYWORDS = {
    "rock", "start", "let", "say", "ask", "if", "otherwise",
    "repeat", "fun", "give", "true", "false", "and", "or", "not",
}

TOKEN_RE = re.compile(
    r"(?P<comment>#[^\n]*)"
    r"|(?P<string>\"(?:\\.|[^\"\\])*\"?|'(?:\\.|[^'\\])*'?)"
    r"|(?P<number>\b\d+(?:\.\d+)?\b)"
    r"|(?P<word>[A-Za-z_]\w*)"
    r"|(?P<operator>==|!=|<=|>=|=>|->|[+\-*/%=<>{}()\[\],.:])"
)

STYLESHEET = """QMainWindow { background: transparent; }
QWidget { color: %(text)s; }
QLabel { background: transparent; }
QDialog { background: %(solid)s; }
QMessageBox QLabel { min-width: 260px; }
QToolTip {
    background: #252d28; color: %(bright)s;
    border: 1px solid %(line_strong)s; padding: 5px 8px;
}

QMenuBar {
    background: rgba(30,36,32,%(chrome)d);
    border-bottom: 1px solid %(line)s;
    padding: 3px 6px; spacing: 3px;
}
QMenuBar::item {
    background: transparent; color: %(dim)s;
    padding: 5px 9px; border-radius: 4px;
}
QMenuBar::item:selected, QMenuBar::item:pressed {
    background: %(hover)s; color: %(bright)s;
}
QMenuBar::item:disabled { color: %(faint)s; }
QLabel#wordmark {
    color: %(bright)s; font-family: %(mono)s;
    font-size: 9pt; font-weight: 600; padding: 0 12px;
}
QLabel#docinfo { color: %(dim)s; padding: 0 12px; }
QMenu {
    background: #252d28; border: 1px solid %(line_strong)s;
    padding: 5px; border-radius: 6px;
}
QMenu::item {
    padding: 7px 30px 7px 12px; color: %(text)s; border-radius: 3px;
}
QMenu::item:selected { background: %(select)s; color: %(bright)s; }
QMenu::item:disabled { color: %(faint)s; }
QMenu::separator { height: 1px; background: %(line)s; margin: 5px 8px; }

QToolBar {
    background: rgba(30,36,32,%(chrome)d);
    border: none; spacing: 4px; padding: 4px 8px;
}
QToolBar::separator {
    background: %(line_strong)s; width: 1px; margin: 6px 7px;
}
QToolBar#activity {
    background: rgba(27,33,29,%(chrome)d);
    border-right: 1px solid %(line)s; padding: 7px 0; spacing: 3px;
}
QToolButton {
    background: transparent; color: %(dim)s; border: 1px solid transparent;
    border-radius: 5px; padding: 5px 9px;
}
QToolButton:hover { background: %(hover)s; color: %(bright)s; }
QToolButton:pressed { background: %(select)s; }
QToolButton:checked { color: %(bright)s; }
QToolButton:disabled { color: %(faint)s; }
QToolBar QToolButton:focus {
    border-color: %(mint)s; background: %(hover)s;
}
QToolBar#activity QToolButton {
    padding: 9px 10px; border-radius: 0;
    border-left: 2px solid transparent;
}
QToolBar#activity QToolButton:checked {
    border-left: 2px solid %(mint)s; background: %(hover)s;
}
QToolButton#tabclose {
    padding: 0; color: %(faint)s; border-radius: 4px;
}
QToolButton#tabclose:hover {
    background: %(select)s; color: %(bright)s;
}

QPushButton {
    background: #2b342e; color: %(text)s;
    border: 1px solid %(line_strong)s; border-radius: 5px;
    padding: 6px 14px;
}
QPushButton:hover {
    background: #344238; border-color: %(mint)s; color: %(bright)s;
}
QPushButton:pressed { background: #3b5142; }
QPushButton:focus { border-color: %(mint)s; }
QPushButton:disabled {
    color: %(faint)s; background: #242a26; border-color: %(line)s;
}
QPushButton#link {
    background: transparent; border: 1px solid transparent;
    text-align: left; padding: 5px 9px; color: %(bright)s;
}
QPushButton#link:hover {
    background: %(hover)s; border-color: %(line)s; color: %(mint)s;
}
QPushButton#link:focus { border-color: %(mint)s; background: %(hover)s; }
QPushButton#barbutton { padding: 3px 10px; }

QInputDialog QLineEdit {
    background: #1c211e; border: 1px solid %(line_strong)s;
    border-radius: 5px; padding: 6px 8px;
    selection-background-color: %(selection)s;
}
QInputDialog QLineEdit:focus { border-color: %(mint)s; }
QInputDialog QListView {
    background: #1c211e; border: 1px solid %(line_strong)s; outline: 0;
}
QInputDialog QListView::item { padding: 5px 7px; }
QInputDialog QListView::item:selected {
    background: %(select)s; color: %(bright)s;
}
QDialog QPlainTextEdit {
    background: #1c211e; border: 1px solid %(line_strong)s;
}

QWidget#findbar {
    background: rgba(29,35,31,%(chrome)d);
    border-bottom: 1px solid %(line)s;
}
QLineEdit#find_input, QLineEdit#replace_input, QLineEdit#program_input {
    background: #1c211e; border: 1px solid %(line_strong)s;
    border-radius: 5px; padding: 4px 8px;
    selection-background-color: %(selection)s;
}
QLineEdit#find_input:focus, QLineEdit#replace_input:focus,
QLineEdit#program_input:focus { border-color: %(mint)s; }
QLineEdit#program_input:disabled {
    color: %(faint)s; background: #191d1a; border-color: %(line)s;
}

QTabWidget::pane { border: none; background: transparent; }
QTabBar {
    background: rgba(29,35,31,%(chrome)d);
    border: none; qproperty-drawBase: 0;
}
QTabBar::tab {
    background: transparent; color: %(dim)s;
    padding: 9px 8px 9px 14px; border: none;
    border-bottom: 2px solid transparent;
    min-width: 72px; max-width: 220px;
}
QTabBar::tab:hover:!selected {
    background: %(hover)s; color: %(text)s;
}
QTabBar::tab:selected {
    background: rgba(32,39,34,%(editor)d);
    color: %(bright)s; border-bottom: 2px solid %(mint)s;
}

QMainWindow::separator { background: transparent; width: 5px; height: 5px; }
QMainWindow::separator:hover { background: rgba(166,215,180,65); }
QDockWidget { titlebar-close-icon: none; titlebar-normal-icon: none; }
QDockWidget::title {
    background: rgba(29,35,31,%(chrome)d); color: %(dim)s;
    font-size: 8.5pt; padding: 8px 12px;
    border-bottom: 1px solid %(line)s; text-align: left;
}

QTreeView#explorer_tree {
    background: rgba(31,37,33,%(panel)d); color: %(text)s;
    border: none; outline: 0; padding: 5px 0; show-decoration-selected: 1;
    selection-background-color: %(selection)s;
}
QTreeView#explorer_tree::item {
    padding: 3px 5px; min-height: 24px; border: none;
}
QTreeView#explorer_tree::item:hover { background: %(hover)s; }
QTreeView#explorer_tree::item:selected {
    background: %(select)s; color: %(bright)s;
    border-left: 2px solid %(mint)s;
}
QTreeView#explorer_tree::item:selected:!active { background: %(hover)s; }

QPlainTextEdit#editor {
    background: rgba(31,37,33,%(editor)d); color: %(text)s;
    border: none; selection-background-color: %(selection)s;
    selection-color: #ffffff;
}
QTextEdit#output {
    background: rgba(29,35,31,%(panel)d); color: %(text)s;
    border: none; selection-background-color: %(selection)s;
    selection-color: #ffffff;
}
QWidget#welcome { background: rgba(31,37,33,%(editor)d); }
QLabel#welcome_title { font-size: 18pt; font-weight: 600; color: %(bright)s; }
QLabel#section {
    font-size: 8pt; font-weight: 600; color: %(mint)s;
    padding-top: 2px;
}
QLabel#muted { color: %(dim)s; }
QLabel#hint { font-family: %(mono)s; font-size: 8pt; color: %(faint)s; }

QStatusBar {
    background: rgba(29,35,31,%(chrome)d);
    border-top: 1px solid %(line)s;
}
QStatusBar::item { border: none; }
QStatusBar QLabel { color: %(dim)s; padding: 2px 9px; }
QStatusBar QLabel#state { color: %(mint)s; }
QStatusBar QLabel#state[modified="true"] { color: #e2c28e; }

QScrollBar:vertical {
    background: transparent; width: 10px; margin: 2px; border: none;
}
QScrollBar:horizontal {
    background: transparent; height: 10px; margin: 2px; border: none;
}
QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
    background: #465249; border-radius: 4px;
    min-height: 28px; min-width: 28px; margin: 1px;
}
QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {
    background: %(mint)s;
}
QScrollBar::add-line, QScrollBar::sub-line {
    width: 0; height: 0; border: none;
}
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
"""


def code_font(size):
    font = QFont()
    font.setFamilies(["JetBrains Mono", "Cascadia Mono", "DejaVu Sans Mono", "monospace"])
    font.setStyleHint(QFont.StyleHint.Monospace)
    font.setPointSizeF(size)
    return font


def dark_palette():
    palette = QPalette()
    roles = {
        QPalette.ColorRole.Window: SOLID,
        QPalette.ColorRole.WindowText: TEXT,
        QPalette.ColorRole.Base: "#050505",
        QPalette.ColorRole.AlternateBase: "#0b0b0b",
        QPalette.ColorRole.Text: TEXT,
        QPalette.ColorRole.Button: "#141414",
        QPalette.ColorRole.ButtonText: TEXT,
        QPalette.ColorRole.Highlight: SELECTION_BG,
        QPalette.ColorRole.HighlightedText: "#ffffff",
        QPalette.ColorRole.ToolTipBase: SOLID,
        QPalette.ColorRole.ToolTipText: TEXT,
        QPalette.ColorRole.PlaceholderText: FAINT,
    }
    for role, colour in roles.items():
        palette.setColor(role, QColor(colour))
    for role in (QPalette.ColorRole.Text, QPalette.ColorRole.WindowText,
                 QPalette.ColorRole.ButtonText):
        palette.setColor(QPalette.ColorGroup.Disabled, role, QColor(FAINT))
    return palette


def line_icon(kind):
    icon = QIcon()
    states = [
        (QIcon.Mode.Normal, QIcon.State.Off, DIM),
        (QIcon.Mode.Normal, QIcon.State.On, BRIGHT),
        (QIcon.Mode.Active, QIcon.State.Off, BRIGHT),
        (QIcon.Mode.Active, QIcon.State.On, BRIGHT),
    ]
    for mode, state, colour in states:
        pix = QPixmap(48, 48)
        pix.fill(Qt.GlobalColor.transparent)
        p = QPainter(pix)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        pen = QPen(QColor(colour))
        pen.setWidthF(2.6)
        p.setPen(pen)
        if kind == "files":
            p.drawRect(12, 6, 24, 36)
            for y in (18, 26, 34):
                p.drawLine(18, y, 30, y)
        else:
            p.drawRect(6, 10, 36, 28)
            p.drawLine(6, 28, 42, 28)
        p.end()
        icon.addPixmap(pix, mode, state)
    return icon


class PlainIconProvider(QFileIconProvider):
    def icon(self, *_args):
        return QIcon()


class ProjectModel(QFileSystemModel):
    def data(self, index, role=Qt.ItemDataRole.DisplayRole):
        if index.isValid():
            if role == Qt.ItemDataRole.ForegroundRole and self.isDir(index):
                return QBrush(QColor(DIM))
            if role == Qt.ItemDataRole.ToolTipRole:
                return self.filePath(index)
        return super().data(index, role)


class PebbleHighlighter(QSyntaxHighlighter):
    def __init__(self, document, colours=None):
        super().__init__(document)
        self.colours = dict(SYNTAX_COLOUR_DEFAULTS)
        if colours:
            for key in self.colours:
                candidate = QColor(colours.get(key, self.colours[key]))
                if candidate.isValid():
                    self.colours[key] = candidate.name()
        self.formats = {}
        self._build_rules()

    def _build_rules(self):
        self.formats = {}
        for key, colour in self.colours.items():
            fmt = QTextCharFormat()
            fmt.setForeground(QColor(colour))
            if key in ("builtin", "boolean", "keyword"):
                fmt.setFontWeight(QFont.Weight.Bold)
            if key == "comment":
                fmt.setFontItalic(True)
            self.formats[key] = fmt

    def set_colours(self, colours):
        for key in SYNTAX_COLOUR_DEFAULTS:
            candidate = QColor(colours.get(key, self.colours[key]))
            if candidate.isValid():
                self.colours[key] = candidate.name()
        self._build_rules()
        self.rehighlight()

    def highlightBlock(self, text):
        for match in TOKEN_RE.finditer(text):
            kind = match.lastgroup
            if kind == "word":
                word = match.group()
                if word in ("say", "ask"):
                    kind = "builtin"
                elif word in ("true", "false"):
                    kind = "boolean"
                elif word in KEYWORDS:
                    kind = "keyword"
                elif text[match.end():].lstrip().startswith("("):
                    kind = "function"
                else:
                    continue
            self.setFormat(
                match.start(),
                match.end() - match.start(),
                self.formats[kind],
            )


class SyntaxColoursDialog(QDialog):
    def __init__(self, colours, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Syntax Colours")
        self.setMinimumWidth(470)
        self.colours = dict(SYNTAX_COLOUR_DEFAULTS)
        self.colours.update(colours)
        self.colour_buttons = {}

        layout = QVBoxLayout(self)
        form = QFormLayout()

        for key, label in SYNTAX_COLOUR_LABELS.items():
            button = QPushButton(self.colours[key])
            button.setMinimumWidth(105)
            button.clicked.connect(
                lambda checked=False, colour_key=key:
                    self.choose_colour(colour_key)
            )
            self.colour_buttons[key] = button
            form.addRow(label, button)

        layout.addLayout(form)

        layout.addWidget(QLabel("Preview"))
        self.preview = QPlainTextEdit()
        self.preview.setReadOnly(True)
        self.preview.setMinimumHeight(145)
        self.preview.setPlainText(
            'rock start()\n'
            '    let message = "Hello, Pebble!"\n'
            '    let count = 42\n'
            '    say message\n'
            '    if true\n'
            '        ask "Continue?"\n'
        )
        self.preview.setFont(QFont("monospace", 10))
        self.preview_highlighter = PebbleHighlighter(
            self.preview.document(), self.colours
        )
        layout.addWidget(self.preview)

        reset_button = QPushButton("Reset to Defaults")
        reset_button.clicked.connect(self.reset_defaults)
        layout.addWidget(reset_button)

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok
            | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def choose_colour(self, key):
        selected = QColorDialog.getColor(
            QColor(self.colours[key]), self, SYNTAX_COLOUR_LABELS[key]
        )
        if selected.isValid():
            self.colours[key] = selected.name()
            self.refresh_colour(key)

    def refresh_colour(self, key):
        button = self.colour_buttons[key]
        colour = self.colours[key]
        button.setText(colour)
        button.setStyleSheet(
            "QPushButton {"
            f"background-color: {colour};"
            "color: #202522;"
            "padding: 5px 10px;"
            "border-radius: 3px;"
            "}"
        )
        self.preview_highlighter.set_colours(self.colours)

    def reset_defaults(self):
        self.colours = dict(SYNTAX_COLOUR_DEFAULTS)
        for key in self.colour_buttons:
            self.refresh_colour(key)


class LineNumberArea(QWidget):
    def __init__(self, editor):
        super().__init__(editor)
        self.editor = editor

    def sizeHint(self):
        return QSize(self.editor.gutter_width, 0)

    def paintEvent(self, event):
        self.editor.paint_line_numbers(event)


class CodeEditor(QPlainTextEdit):
    INDENT = "    "

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("editor")
        self.setFrameShape(QPlainTextEdit.Shape.NoFrame)
        self.setCursorWidth(2)
        self.gutter_width = 42
        self.line_number_area = LineNumberArea(self)
        self.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)

        text_option = self.document().defaultTextOption()
        text_option.setFlags(
            text_option.flags() | QTextOption.Flag.ShowTabsAndSpaces
        )
        self.document().setDefaultTextOption(text_option)

        self.blockCountChanged.connect(self.update_gutter_width)
        self.updateRequest.connect(self.update_gutter)
        self._line_highlight_alpha = 10
        self._line_animation = QVariantAnimation(self)
        self._line_animation.setDuration(170)
        self._line_animation.setEasingCurve(QEasingCurve.Type.OutCubic)
        self._line_animation.valueChanged.connect(self._set_line_alpha)
        self.cursorPositionChanged.connect(self._animate_current_line)
        self.update_gutter_width()
        self.highlight_current_line()

    def update_gutter_width(self, _count=0):
        digits = max(2, len(str(max(1, self.blockCount()))))
        self.gutter_width = 24 + self.fontMetrics().horizontalAdvance("9") * digits
        self.setViewportMargins(self.gutter_width, 0, 0, 0)
        self.line_number_area.setGeometry(0, 0, self.gutter_width, self.height())

    def update_gutter(self, rect, dy):
        if dy:
            self.line_number_area.scroll(0, dy)
        else:
            self.line_number_area.update(0, rect.y(), self.gutter_width, rect.height())
        if rect.contains(self.viewport().rect()):
            self.update_gutter_width()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        rect = self.contentsRect()
        self.line_number_area.setGeometry(
            QRect(rect.left(), rect.top(), self.gutter_width, rect.height())
        )

    def paint_line_numbers(self, event):
        painter = QPainter(self.line_number_area)
        painter.fillRect(event.rect(), QColor("#252d28"))

        current = self.textCursor().block()
        block = self.firstVisibleBlock()
        number = block.blockNumber()
        top = round(
            self.blockBoundingGeometry(block).translated(self.contentOffset()).top()
        )

        painter.setFont(self.font())
        while block.isValid() and top <= event.rect().bottom():
            height = round(self.blockBoundingRect(block).height())
            bottom = top + height
            if block.isVisible() and bottom >= event.rect().top():
                painter.setPen(QColor(BRIGHT if block == current else '#777777'))
                line = block.layout().lineAt(0) if block.layout() else None
                y, h = (top + round(line.y()), round(line.height())) if line and line.isValid() else (top, height)
                painter.drawText(
                    0, y, self.gutter_width - 12, h,
                    Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter,
                    str(number + 1),
                )
            block = block.next()
            top = bottom
            number += 1
        painter.end()

    def _animate_current_line(self):
        self._line_animation.stop()
        self._line_animation.setDuration(140)
        self._line_animation.setStartValue(10)
        self._line_animation.setKeyValueAt(0.45, 13)
        self._line_animation.setEndValue(10)
        self._line_animation.start()

    def _set_line_alpha(self, value):
        self._line_highlight_alpha = int(value)
        self.highlight_current_line()

    def highlight_current_line(self):
        selections = []
        if not self.isReadOnly():
            extra = QTextEdit.ExtraSelection()
            extra.format.setBackground(
                QColor(166, 215, 180, self._line_highlight_alpha)
            )
            extra.format.setProperty(QTextFormat.Property.FullWidthSelection, True)
            extra.cursor = self.textCursor()
            extra.cursor.clearSelection()
            selections.append(extra)
        self.setExtraSelections(selections)
        self.line_number_area.update()

    def keyPressEvent(self, event):
        key, mods = event.key(), event.modifiers()
        if not self.isReadOnly():
            ctrl = bool(mods & Qt.KeyboardModifier.ControlModifier)
            alt = bool(mods & Qt.KeyboardModifier.AltModifier)
            if ctrl and key == Qt.Key.Key_Slash:
                self.toggle_comment()
                return
            if not ctrl and not alt:
                if key in (Qt.Key.Key_Return, Qt.Key.Key_Enter):
                    self._newline_keep_indent()
                    return
                if key == Qt.Key.Key_Tab:
                    self._tab()
                    return
                if key == Qt.Key.Key_Backtab:
                    self.outdent()
                    return
        super().keyPressEvent(event)

    def _newline_keep_indent(self):
        cursor = self.textCursor()
        block = cursor.block()
        before = block.text()[: cursor.selectionStart() - block.position()]
        indent = before[: len(before) - len(before.lstrip(" \t"))]
        cursor.beginEditBlock()
        cursor.removeSelectedText()
        cursor.insertBlock()
        cursor.insertText(indent)
        cursor.endEditBlock()
        self.setTextCursor(cursor)
        self.ensureCursorVisible()

    def _selected_blocks(self):
        cursor = self.textCursor()
        doc = self.document()
        start, end = cursor.selectionStart(), cursor.selectionEnd()
        first, last = doc.findBlock(start), doc.findBlock(end)
        if (
            end > start
            and end == last.position()
            and last.blockNumber() > first.blockNumber()
        ):
            last = last.previous()
        blocks = []
        block = first
        while block.isValid():
            blocks.append(block)
            if block.blockNumber() == last.blockNumber():
                break
            block = block.next()
        return blocks

    def _tab(self):
        cursor = self.textCursor()
        doc = self.document()
        if (
            doc.findBlock(cursor.selectionStart()).blockNumber()
            != doc.findBlock(cursor.selectionEnd()).blockNumber()
        ):
            self.indent()
            return
        column = cursor.selectionStart() - doc.findBlock(cursor.selectionStart()).position()
        cursor.insertText(" " * (4 - column % 4))

    def indent(self):
        edit = self.textCursor()
        edit.beginEditBlock()
        for block in self._selected_blocks():
            if block.text().strip():
                QTextCursor(block).insertText(self.INDENT)
        edit.endEditBlock()

    def outdent(self):
        edit = self.textCursor()
        edit.beginEditBlock()
        for block in self._selected_blocks():
            text = block.text()
            count = 0
            while count < 4 and count < len(text) and text[count] == " ":
                count += 1
            if count == 0 and text.startswith("\t"):
                count = 1
            if count:
                cursor = QTextCursor(block)
                cursor.movePosition(
                    QTextCursor.MoveOperation.Right,
                    QTextCursor.MoveMode.KeepAnchor,
                    count,
                )
                cursor.removeSelectedText()
        edit.endEditBlock()

    def toggle_comment(self):
        lines = [b for b in self._selected_blocks() if b.text().strip()]
        if not lines:
            return
        uncomment = all(b.text().lstrip().startswith("#") for b in lines)
        edit = self.textCursor()
        edit.beginEditBlock()
        for block in lines:
            text = block.text()
            offset = len(text) - len(text.lstrip())
            cursor = QTextCursor(self.document())
            cursor.setPosition(block.position() + offset)
            if uncomment:
                count = 2 if text.lstrip().startswith("# ") else 1
                cursor.setPosition(
                    block.position() + offset + count,
                    QTextCursor.MoveMode.KeepAnchor,
                )
                cursor.removeSelectedText()
            else:
                cursor.insertText("# ")
        edit.endEditBlock()


class FindBar(QWidget):
    def __init__(self, editor):
        super().__init__()
        self.editor = editor
        self.setObjectName("findbar")
        self.setAttribute(Qt.WidgetAttribute.WA_StyledBackground, True)
        self.hide()

        self.find_input = QLineEdit()
        self.find_input.setObjectName("find_input")
        self.find_input.setPlaceholderText("find")
        self.replace_input = QLineEdit()
        self.replace_input.setObjectName("replace_input")
        self.replace_input.setPlaceholderText("replace")
        self.case_button = QToolButton()
        self.case_button.setText("Aa")
        self.case_button.setCheckable(True)
        self.case_button.setToolTip("Match Case")
        self.message = QLabel("")
        self.message.setObjectName("hint")

        def button(text, tip, slot):
            widget = QPushButton(text)
            widget.setObjectName("barbutton")
            widget.setToolTip(tip)
            widget.clicked.connect(slot)
            return widget

        top = QHBoxLayout()
        top.addWidget(self.find_input, 1)
        top.addWidget(self.case_button)
        top.addWidget(button("↑", "Previous (Shift+F3)", lambda _checked=False: self.find(True)))
        top.addWidget(button("↓", "Next (F3)", lambda _checked=False: self.find(False)))
        top.addWidget(self.message)
        top.addWidget(button("×", "Close (Esc)", lambda _checked=False: self.close_bar()))

        self.replace_row = QWidget()
        bottom = QHBoxLayout(self.replace_row)
        bottom.setContentsMargins(0, 0, 0, 0)
        bottom.addWidget(self.replace_input, 1)
        bottom.addWidget(
            button("Replace", "Replace current match", lambda _checked=False: self.replace_one())
        )
        bottom.addWidget(
            button("Replace All", "Replace every match", lambda _checked=False: self.replace_all())
        )

        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 6, 8, 6)
        layout.setSpacing(6)
        layout.addLayout(top)
        layout.addWidget(self.replace_row)

        self.find_input.returnPressed.connect(lambda: self.find(False))
        self.replace_input.returnPressed.connect(self.replace_one)

    def open_bar(self, with_replace=False):
        selected = self.editor.textCursor().selectedText()
        if selected and "\u2029" not in selected:
            self.find_input.setText(selected)
        self.replace_row.setVisible(with_replace)
        self.show()
        if with_replace and self.find_input.text():
            self.replace_input.setFocus()
        else:
            self.find_input.setFocus()
            self.find_input.selectAll()

    def close_bar(self):
        self.hide()
        self.editor.setFocus()

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_Escape:
            self.close_bar()
        else:
            super().keyPressEvent(event)

    def _flags(self):
        flags = QTextDocument.FindFlag(0)
        if self.case_button.isChecked():
            flags |= QTextDocument.FindFlag.FindCaseSensitively
        return flags

    def find(self, backward=False):
        query = self.find_input.text()
        if not query:
            self.message.setText("")
            return False
        flags = self._flags()
        if backward:
            flags |= QTextDocument.FindFlag.FindBackward
        doc = self.editor.document()
        found = doc.find(query, self.editor.textCursor(), flags)
        wrapped = found.isNull()
        if wrapped:
            start = QTextCursor(doc)
            if backward:
                start.movePosition(QTextCursor.MoveOperation.End)
            found = doc.find(query, start, flags)
        if found.isNull():
            self.message.setText("no matches")
            return False
        self.message.setText("wrapped" if wrapped else "")
        self.editor.setTextCursor(found)
        self.editor.ensureCursorVisible()
        return True

    def replace_one(self):
        query = self.find_input.text()
        cursor = self.editor.textCursor()
        selected = cursor.selectedText()
        if self.case_button.isChecked():
            same = selected == query
        else:
            same = selected.lower() == query.lower()
        if query and same:
            cursor.insertText(self.replace_input.text())
            self.editor.setTextCursor(cursor)
        self.find(False)

    def replace_all(self):
        query = self.find_input.text()
        if not query:
            return
        doc = self.editor.document()
        flags = self._flags()
        count = 0
        edit = QTextCursor(doc)
        edit.beginEditBlock()
        cursor = QTextCursor(doc)
        while True:
            found = doc.find(query, cursor, flags)
            if found.isNull():
                break
            found.insertText(self.replace_input.text())
            cursor = found
            count += 1
        edit.endEditBlock()
        self.message.setText(f"replaced {count}")


class TabButton(QToolButton):
    def __init__(self):
        super().__init__()
        self.setObjectName("tabclose")
        self.setToolTip("Close Tab")
        self.setAutoRaise(True)
        self.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.setFixedSize(18, 18)
        self._dirty = False
        self._hover = False
        self._refresh()

    def set_dirty(self, dirty):
        self._dirty = dirty
        self._refresh()

    def _refresh(self):
        self.setText("●" if self._dirty and not self._hover else "×")

    def enterEvent(self, event):
        self._hover = True
        self._refresh()
        super().enterEvent(event)

    def leaveEvent(self, event):
        self._hover = False
        self._refresh()
        super().leaveEvent(event)


class EditorTab(QWidget):
    LINE_HEIGHT = 128

    def __init__(self, path=None, source="", syntax_colours=None):
        super().__init__()
        self.path = Path(path).resolve() if path else None
        self.dirty = False
        self.close_button = None

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self.editor = CodeEditor()
        self.editor.setFont(code_font(11))
        self.editor.setTabStopDistance(self.editor.fontMetrics().horizontalAdvance(" ") * 4)
        self.editor.document().setDocumentMargin(12)
        self.editor.update_gutter_width()
        self.highlighter = PebbleHighlighter(
            self.editor.document(), syntax_colours
        )
        self.editor.setPlainText(source)
        self.apply_line_height()
        self.editor.document().clearUndoRedoStacks()
        self.editor.document().setModified(False)
        self.editor.document().modificationChanged.connect(self.set_dirty)
        self.find_bar = FindBar(self.editor)
        layout.addWidget(self.find_bar)
        layout.addWidget(self.editor)

    def apply_line_height(self):
        cursor = QTextCursor(self.editor.document())
        cursor.select(QTextCursor.SelectionType.Document)
        fmt = QTextBlockFormat()
        fmt.setLineHeight(
            float(self.LINE_HEIGHT),
            QTextBlockFormat.LineHeightTypes.ProportionalHeight.value,
        )
        cursor.mergeBlockFormat(fmt)

    def set_dirty(self, dirty):
        self.dirty = dirty

    @property
    def display_name(self):
        return self.path.name if self.path else "Untitled.peb"


class AnimatedTabBar(QTabBar):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._indicator_rect = QRect()
        self._indicator_animation = QVariantAnimation(self)
        self._indicator_animation.setDuration(180)
        self._indicator_animation.setEasingCurve(
            QEasingCurve.Type.OutCubic
        )
        self._indicator_animation.valueChanged.connect(
            self._set_indicator_rect
        )
        self.currentChanged.connect(self._animate_indicator)

    def _set_indicator_rect(self, rect):
        self._indicator_rect = rect
        self.update()

    def _animate_indicator(self, index):
        if index < 0:
            return

        target = self.tabRect(index)
        if not target.isValid():
            return

        start = self._indicator_rect
        if not start.isValid():
            self._indicator_rect = target
            self.update()
            return

        self._indicator_animation.stop()
        self._indicator_animation.setStartValue(start)
        self._indicator_animation.setEndValue(target)

        if start == target:
            self._indicator_rect = target
            self.update()
            return

        self._indicator_animation.start()

    def resizeEvent(self, event):
        super().resizeEvent(event)

        index = self.currentIndex()
        if index < 0:
            return

        target = self.tabRect(index)
        if not target.isValid():
            return

        self._indicator_animation.stop()
        self._indicator_rect = target
        self.update()

    def paintEvent(self, event):
        super().paintEvent(event)

        index = self.currentIndex()
        if index < 0:
            return

        rect = self._indicator_rect
        if not rect.isValid():
            rect = self.tabRect(index)
        if not rect.isValid():
            return

        indicator = QRect(
            rect.left() + 10,
            rect.y() + rect.height() - 3,
            max(0, rect.width() - 20),
            2,
        )

        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QColor("#a6d7b4"))
        painter.drawRoundedRect(indicator, 1, 1)
        painter.end()


from pebble_studio_common import (
    build_pebble_run_command,
    list_example_paths,
    resolve_examples_dir,
)


class PebbleStudio(QMainWindow):
    def __init__(self):
        super().__init__()

        self.appearance_settings = QSettings("Pebble", "Pebble Studio")
        self.syntax_colours = dict(SYNTAX_COLOUR_DEFAULTS)
        for key, default in SYNTAX_COLOUR_DEFAULTS.items():
            stored = self.appearance_settings.value(
                f"syntax/{key}", default, type=str
            )
            candidate = QColor(stored)
            self.syntax_colours[key] = (
                candidate.name() if candidate.isValid() else default
            )
        self.background_path = self.appearance_settings.value(
            "background_path", DEFAULT_BACKGROUND, type=str
        )
        self.background_dim = max(0, min(245, self.appearance_settings.value(
            "background_dim", 220, type=int
        )))
        self._wallpaper = QPixmap()
        self._scaled = None
        self._scaled_key = None

        self.setWindowTitle("Pebble Studio")

        self.icon_path = APP_DIR / "assets" / "icons" / "pebble.png"
        if self.icon_path.is_file():
            self.setWindowIcon(QIcon(str(self.icon_path)))

        self.resize(1280, 820)
        self.setDockNestingEnabled(True)
        self.documents = []

        self.create_actions()
        self.create_explorer()
        self.create_output()

        self.process = QProcess(self)
        self.process.readyReadStandardOutput.connect(self.read_program_stdout)
        self.process.readyReadStandardError.connect(self.read_program_stderr)
        self.process.finished.connect(self.program_finished)
        self.process.errorOccurred.connect(self.program_error)
        self.process.stateChanged.connect(self.update_run_actions)
        self._decoders = {}
        self._kill_timer = QTimer(self)
        self._kill_timer.setSingleShot(True)
        self._kill_timer.timeout.connect(self.force_stop_program)
        self.create_menu()
        self.create_activity_bar()
        self.create_editor_area()
        self.create_status()

        self.apply_theme()
        self.load_wallpaper()
        self.show_welcome()
        self.flash(f"workspace: {ROOT}")

    def surface_alpha(self, floor):
        remaining = max(0.01, 1 - self.background_dim / 255)
        needed = 1 - 0.14 / remaining
        return int(255 * max(floor, needed))

    def apply_theme(self):
        self.setStyleSheet(STYLESHEET % {
            "text": TEXT, "bright": BRIGHT, "dim": DIM, "faint": FAINT,
            "line": LINE, "line_strong": LINE_STRONG, "solid": SOLID,
            "hover": HOVER, "select": SELECT, "selection": SELECTION_BG,
            "mono": MONO, "mint": "#a6d7b4",
            "chrome": self.surface_alpha(0.82),
            "panel": self.surface_alpha(0.55),
            "editor": self.surface_alpha(0.90),
        })

    def choose_syntax_colours(self):
        dialog = SyntaxColoursDialog(self.syntax_colours, self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        self.syntax_colours = dict(dialog.colours)
        for key, colour in self.syntax_colours.items():
            self.appearance_settings.setValue(f"syntax/{key}", colour)

        for index in range(self.tabs.count()):
            widget = self.tabs.widget(index)
            if isinstance(widget, EditorTab):
                widget.highlighter.set_colours(self.syntax_colours)

        self.flash("syntax colours updated")

    def load_wallpaper(self):
        pix = QPixmap(self.background_path) if self.background_path else QPixmap()
        if pix.isNull() and self.background_path and self.background_path != DEFAULT_BACKGROUND:
            self.flash("background image could not be loaded. using default.")
            pix = QPixmap(DEFAULT_BACKGROUND)
        self._wallpaper = pix
        self._scaled = None
        self.update()

    def scaled_wallpaper(self):
        if self._wallpaper.isNull():
            return None
        dpr = self.devicePixelRatioF()
        key = (self.width(), self.height(), dpr)
        if self._scaled_key != key:
            scaled = self._wallpaper.scaled(
                QSize(round(self.width() * dpr), round(self.height() * dpr)),
                Qt.AspectRatioMode.KeepAspectRatioByExpanding,
                Qt.TransformationMode.SmoothTransformation,
            )
            scaled.setDevicePixelRatio(dpr)
            self._scaled, self._scaled_key = scaled, key
        return self._scaled

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.fillRect(event.rect(), QColor("#000000"))
        pix = self.scaled_wallpaper()
        if pix is not None:
            dpr = pix.devicePixelRatio()
            x = (self.width() - round(pix.width() / dpr)) // 2
            y = (self.height() - round(pix.height() / dpr)) // 2
            painter.drawPixmap(x, y, pix)
        painter.fillRect(event.rect(), QColor(0, 0, 0, self.background_dim))
        painter.end()

    def set_background(self, path):
        self.background_path = path
        self.appearance_settings.setValue("background_path", path)
        self.load_wallpaper()

    def choose_background(self):
        start = Path(self.background_path).parent if self.background_path else ROOT
        filename, _ = QFileDialog.getOpenFileName(
            self, "Choose Background Image", str(start),
            "Images (*.png *.jpg *.jpeg *.webp *.bmp *.svg)",
        )
        if not filename:
            return
        if QPixmap(filename).isNull():
            self.notice("Invalid Image", "Pebble Studio could not load that image.",
                        QMessageBox.Icon.Warning)
            return
        self.set_background(filename)

    def set_background_darkness(self):
        dialog = self.make_input("Background Darkness", "Dark Overlay (Higher Means Darker):")
        dialog.setInputMode(QInputDialog.InputMode.IntInput)
        dialog.setIntRange(0, 245)
        dialog.setIntStep(5)
        dialog.setIntValue(self.background_dim)
        if dialog.exec():
            self.background_dim = dialog.intValue()
            self.appearance_settings.setValue("background_dim", self.background_dim)
            self.apply_theme()
            self.update()

    def notice(self, title, text, icon=QMessageBox.Icon.Information):
        box = QMessageBox(icon, title, text, parent=self)
        box.addButton("OK", QMessageBox.ButtonRole.AcceptRole)
        box.exec()

    def make_input(self, title, label):
        dialog = QInputDialog(self)
        dialog.setWindowTitle(title)
        dialog.setLabelText(label)
        dialog.setOkButtonText("OK")
        dialog.setCancelButtonText("Cancel")
        return dialog

    def flash(self, text, ms=5000):
        self.status_message.setText(text)
        self._flash_timer.start(ms)

    def log(self, text, level="normal"):
        fmt = QTextCharFormat()
        fmt.setForeground(QColor(LEVEL_COLOURS[level]))
        cursor = self.output.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        if cursor.positionInBlock() > 0:
            cursor.insertBlock()
        cursor.insertText(LEVEL_PREFIX.get(level, "") + text + "\n", fmt)
        self.output.setTextCursor(cursor)
        self.output.ensureCursorVisible()

    def create_actions(self):
        definitions = [
            ("new", "New File", "Ctrl+N", self.new_file),
            ("open", "Open File…", "Ctrl+O", self.open_file),
            ("Save", "Save", "Ctrl+S", self.save_file),
            ("save_as", "Save As…", "Ctrl+Shift+S", self.save_as),
            ("close_tab", "Close Tab", "Ctrl+W", self.close_tab),
            ("run", "Run File", "F5", self.run_current_file),
            ("stop", "Stop", "Shift+F5", self.stop_program),
            ("Find", "Find", "Ctrl+F", self.find_text),
            ("replace", "Replace", "Ctrl+H", self.replace_text),
            ("find_next", "Find Next", "F3", lambda _checked=False: self.find_step(False)),
            ("find_prev", "Find Previous", "Shift+F3", lambda _checked=False: self.find_step(True)),
            ("next_tab", "Next Tab", "Ctrl+PgDown", lambda _checked=False: self.step_tab(1)),
            ("prev_tab", "Previous Tab", "Ctrl+PgUp", lambda _checked=False: self.step_tab(-1)),
        ]
        self.actions = {}
        for key, label, shortcut, callback in definitions:
            action = QAction(label, self)
            action.setShortcut(QKeySequence(shortcut))
            action.setToolTip(f"{label} ({shortcut})")
            action.triggered.connect(callback)
            self.addAction(action)
            self.actions[key] = action
        self.actions["run"].setToolTip("Run File (F5)")
        self.actions["stop"].setToolTip("Stop (Shift+F5)")
        self.actions["stop"].setEnabled(False)

    def create_activity_bar(self):
        bar = QToolBar("Activity", self)
        bar.setObjectName("activity")
        bar.setMovable(False)
        bar.setFloatable(False)
        bar.setOrientation(Qt.Orientation.Vertical)
        bar.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonIconOnly)
        bar.setIconSize(QSize(22, 22))
        bar.setFixedWidth(44)

        for dock, kind, tip in (
            (self.explorer_dock, "files", "Explorer"),
            (self.output_dock, "Output", "Output"),
        ):
            action = dock.toggleViewAction()
            action.setIcon(line_icon(kind))
            action.setToolTip(tip)
            bar.addAction(action)

        self.activity_bar = bar
        self.addToolBar(Qt.ToolBarArea.LeftToolBarArea, bar)

    def create_menu(self):
        menubar = self.menuBar()

        file_menu = menubar.addMenu("&File")
        for key in ("new", "open", "Save", "save_as"):
            file_menu.addAction(self.actions[key])
        file_menu.addSeparator()
        file_menu.addAction(self.actions["close_tab"])
        file_menu.addSeparator()
        file_menu.addAction("Exit", self.close)

        edit_menu = menubar.addMenu("&Edit")
        edit_menu.addAction("Undo", lambda: self.current_editor_call("undo"))
        edit_menu.addAction("Redo", lambda: self.current_editor_call("redo"))
        edit_menu.addSeparator()
        edit_menu.addAction("Select All", lambda: self.current_editor_call("selectAll"))
        edit_menu.addAction(self.actions["Find"])
        for key in ("replace", "find_next", "find_prev"):
            edit_menu.addAction(self.actions[key])

        run_menu = menubar.addMenu("&Run")
        run_menu.addAction(self.actions["run"])
        run_menu.addAction(self.actions["stop"])

        view_menu = menubar.addMenu("&View")
        view_menu.addAction(self.explorer_dock.toggleViewAction())
        view_menu.addAction(self.output_dock.toggleViewAction())
        view_menu.addAction("Clear Output", self.output.clear)
        view_menu.addSeparator()
        appearance = view_menu.addMenu("Appearance")
        appearance.addAction("Syntax Colours…", self.choose_syntax_colours)
        appearance.addSeparator()
        appearance.addAction("Choose Background Image…", self.choose_background)
        appearance.addAction("Background Darkness…", self.set_background_darkness)
        appearance.addSeparator()
        appearance.addAction("Remove Background Image", lambda: self.set_background(""))
        appearance.addAction(
            "Restore Default Background", lambda: self.set_background(DEFAULT_BACKGROUND)
        )

        help_menu = menubar.addMenu("&Help")
        help_menu.addAction("Pebble Language Ref", self.show_reference)
        help_menu.addAction("About", self.about)

        menubar.addSeparator()
        menubar.addAction(self.actions["run"])
        menubar.addAction(self.actions["stop"])

    def create_toolbar(self):
        toolbar = self.addToolBar("Main")
        toolbar.setObjectName("main_toolbar")
        toolbar.setMovable(False)
        wordmark = QLabel("Pebble Studio")
        wordmark.setObjectName("wordmark")
        toolbar.addWidget(wordmark)
        toolbar.addSeparator()
        for key in ("new", "open", "Save"):
            toolbar.addAction(self.actions[key])
        spacer = QWidget()
        spacer.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        toolbar.addWidget(spacer)
        self.doc_info = QLabel("")
        self.doc_info.setObjectName("docinfo")
        toolbar.addWidget(self.doc_info)
        toolbar.addSeparator()
        toolbar.addAction(self.actions["run"])
        toolbar.addAction(self.actions["stop"])

    def create_explorer(self):
        self.explorer_dock = QDockWidget("Explorer", self)
        self.explorer_dock.setObjectName("explorer_dock")
        self.explorer_dock.setMinimumWidth(170)

        self.project_model = ProjectModel(self)
        self.project_model.setIconProvider(PlainIconProvider())
        self.project_model.setRootPath(str(ROOT))
        self.project_model.setNameFilters(["*.peb"])
        self.project_model.setNameFilterDisables(False)

        tree = QTreeView()
        tree.setObjectName("explorer_tree")
        tree.setModel(self.project_model)
        tree.setRootIndex(self.project_model.index(str(ROOT)))
        tree.setHeaderHidden(True)
        tree.setIndentation(14)
        tree.setUniformRowHeights(True)
        tree.setTextElideMode(Qt.TextElideMode.ElideMiddle)
        tree.setHorizontalScrollMode(QAbstractItemView.ScrollMode.ScrollPerPixel)
        tree.header().setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        for column in range(1, 4):
            tree.hideColumn(column)
        tree.doubleClicked.connect(self.open_tree_item)
        tree.activated.connect(self.open_tree_item)
        self.project_tree = tree
        self.explorer_dock.setWidget(tree)
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, self.explorer_dock)
        self.resizeDocks([self.explorer_dock], [230], Qt.Orientation.Horizontal)

    def create_output(self):
        self.output_dock = QDockWidget("Output", self)
        self.output_dock.setObjectName("output_dock")
        self.output_dock.setMinimumHeight(100)

        console = QWidget(self.output_dock)
        console_layout = QVBoxLayout(console)
        console_layout.setContentsMargins(6, 6, 6, 6)
        console_layout.setSpacing(6)

        self.output = QTextEdit(console)
        self.output.setObjectName("output")
        self.output.setFrameShape(QTextEdit.Shape.NoFrame)
        self.output.setReadOnly(True)
        self.output.setFont(code_font(10))
        self.output.document().setDocumentMargin(12)
        self.output.document().setMaximumBlockCount(20000)

        self.program_input = QLineEdit(console)
        self.program_input.setObjectName("program_input")
        self.program_input.setPlaceholderText(
            "Program input — type an answer and press Enter"
        )
        self.program_input.setFont(code_font(10))
        self.program_input.setEnabled(False)
        self.program_input.returnPressed.connect(self.send_program_input)

        console_layout.addWidget(self.output, 1)
        console_layout.addWidget(self.program_input)
        self.output_dock.setWidget(console)

        self.addDockWidget(
            Qt.DockWidgetArea.BottomDockWidgetArea,
            self.output_dock,
        )
        self.resizeDocks(
            [self.output_dock],
            [180],
            Qt.Orientation.Vertical,
        )

    def create_editor_area(self):
        self.tabs = QTabWidget()
        self.tabs.setTabBar(AnimatedTabBar())
        self.tabs.setDocumentMode(True)
        self.tabs.setMovable(True)
        self.tabs.setElideMode(Qt.TextElideMode.ElideRight)
        self.tabs.setMinimumSize(320, 200)
        self.tabs.currentChanged.connect(self.update_status)
        self.setCentralWidget(self.tabs)

    def create_status(self):
        self.status_message = QLabel("")
        self.status_message.setSizePolicy(
            QSizePolicy.Policy.Ignored,
            QSizePolicy.Policy.Preferred,
        )

        self.doc_info = QLabel("")
        self.doc_info.setObjectName("docinfo")

        self.status_cursor = QLabel("")
        self.status_encoding = QLabel("UTF-8")
        self.status_state = QLabel("Ready")
        self.status_state.setObjectName("state")

        bar = self.statusBar()
        bar.setSizeGripEnabled(False)
        bar.addWidget(self.status_message, 1)
        bar.addPermanentWidget(self.doc_info)

        for label in (
            self.status_cursor,
            self.status_encoding,
            self.status_state,
        ):
            bar.addPermanentWidget(label)

        self._flash_timer = QTimer(self)
        self._flash_timer.setSingleShot(True)
        self._flash_timer.timeout.connect(
            lambda: self.status_message.setText("")
        )

    def current_document(self):
        widget = self.tabs.currentWidget()
        return widget if isinstance(widget, EditorTab) else None

    def current_editor_call(self, method):
        doc = self.current_document()
        if doc:
            getattr(doc.editor, method)()

    def add_tab(self, widget, title):
        index = self.tabs.addTab(widget, title)
        button = TabButton()
        button.clicked.connect(
            lambda _checked=False, w=widget: self.close_tab(self.tabs.indexOf(w))
        )
        self.tabs.tabBar().setTabButton(index, QTabBar.ButtonPosition.RightSide, button)
        return button

    def refresh_tab_label(self, doc):
        index = self.tabs.indexOf(doc)
        if index >= 0:
            self.tabs.setTabText(index, doc.display_name)
            self.tabs.setTabToolTip(index, str(doc.path) if doc.path else doc.display_name)
        if doc.close_button is not None:
            doc.close_button.set_dirty(doc.dirty)
        self.update_status()

    def add_document(self, path=None, source=""):
        doc = EditorTab(path, source, self.syntax_colours)
        doc.editor.cursorPositionChanged.connect(self.update_status)
        doc.editor.document().modificationChanged.connect(
            lambda _dirty, d=doc: self.refresh_tab_label(d)
        )
        self.documents.append(doc)
        doc.close_button = self.add_tab(doc, doc.display_name)
        self.tabs.setCurrentWidget(doc)
        self.refresh_tab_label(doc)
        doc.editor.setFocus()
        return doc

    def get_recent_files(self):
        stored = self.appearance_settings.value("recent_files", [], type=list)
        recent = []
        seen = set()

        for item in stored:
            path = Path(str(item)).expanduser()

            try:
                path = path.resolve()
            except OSError:
                continue

            key = str(path)

            if key in seen or not path.is_file():
                continue

            seen.add(key)
            recent.append(path)

            if len(recent) == 8:
                break

        self.appearance_settings.setValue(
            "recent_files", [str(path) for path in recent]
        )
        return recent

    def record_recent_file(self, path):
        path = Path(path).resolve()

        recent = [
            item for item in self.get_recent_files()
            if item != path
        ]
        recent.insert(0, path)

        self.appearance_settings.setValue(
            "recent_files", [str(item) for item in recent[:8]]
        )

    def show_welcome(self):
        page = QWidget()
        page.setObjectName("welcome")
        page.setProperty("is_welcome", True)
        page.setAttribute(Qt.WidgetAttribute.WA_StyledBackground, True)

        outer = QVBoxLayout(page)
        outer.setContentsMargins(64, 0, 40, 0)
        outer.setSpacing(0)
        outer.addStretch(3)

        brand = QHBoxLayout()
        brand.setSpacing(12)

        if self.icon_path.is_file():
            icon_label = QLabel()
            pixmap = QPixmap(str(self.icon_path))

            if not pixmap.isNull():
                icon_label.setPixmap(
                    pixmap.scaled(
                        32, 32,
                        Qt.AspectRatioMode.KeepAspectRatio,
                        Qt.TransformationMode.SmoothTransformation,
                    )
                )
                icon_label.setFixedSize(32, 32)
                brand.addWidget(icon_label)

        title = QLabel("Pebble Studio")
        title.setObjectName("welcome_title")
        brand.addWidget(title)
        brand.addStretch(1)
        outer.addLayout(brand)

        subtitle = QLabel("A programming environment for Pebble.")
        subtitle.setObjectName("muted")
        outer.addWidget(subtitle)
        outer.addSpacing(28)

        section = QLabel("Get Started")
        section.setObjectName("section")
        outer.addWidget(section)
        outer.addSpacing(6)

        for label, shortcut, callback in (
            ("New File", "Ctrl+N", self.new_file),
            ("Open File…", "Ctrl+O", self.open_file),
            ("Open Example…", "", self.open_example),
        ):
            row = QHBoxLayout()
            button = QPushButton(label)
            button.setObjectName("link")
            button.setFixedWidth(160)
            button.clicked.connect(callback)

            hint = QLabel(shortcut)
            hint.setObjectName("hint")

            row.addWidget(button)
            row.addWidget(hint)
            row.addStretch(1)
            outer.addLayout(row)

        outer.addSpacing(26)

        recent_heading = QLabel("Recent Files")
        recent_heading.setObjectName("section")
        outer.addWidget(recent_heading)
        outer.addSpacing(6)

        recent = self.get_recent_files()

        if recent:
            for path in recent:
                row = QHBoxLayout()

                button = QPushButton(path.name)
                button.setObjectName("link")
                button.setToolTip(str(path))
                button.clicked.connect(
                    lambda _checked=False, p=path: self.open_path(p)
                )

                location = QLabel(str(path.parent))
                location.setObjectName("hint")
                location.setSizePolicy(
                    QSizePolicy.Policy.Expanding,
                    QSizePolicy.Policy.Preferred,
                )

                row.addWidget(button)
                row.addWidget(location, 1)
                outer.addLayout(row)
        else:
            empty = QLabel("No Recent Files Yet")
            empty.setObjectName("muted")
            outer.addWidget(empty)

        outer.addStretch(4)
        self.add_tab(page, "welcome")

    def ensure_welcome(self):
        if self.tabs.count() == 0:
            self.show_welcome()

    def remove_welcome(self):
        for index in range(self.tabs.count()):
            widget = self.tabs.widget(index)
            if widget.property("is_welcome"):
                self.tabs.removeTab(index)
                widget.deleteLater()
                return

    def new_file(self):
        self.remove_welcome()
        self.add_document()
        self.flash("New File")

    def open_file(self):
        filename, _ = QFileDialog.getOpenFileName(
            self, "Open File", str(ROOT),
            "Pebble programs (*.peb);;All files (*)",
        )
        if filename:
            self.open_path(Path(filename))

    def open_path(self, path):
        path = path.resolve()
        for doc in self.documents:
            if doc.path == path:
                self.tabs.setCurrentWidget(doc)
                return
        try:
            source = path.read_text(encoding="UTF-8")
        except (OSError, UnicodeError) as exc:
            self.log(f"could not open {path.name}: {exc}", "error")
            self.notice("Open Failed", str(exc), QMessageBox.Icon.Critical)
            return

        self.remove_welcome()
        self.add_document(path, source)
        self.record_recent_file(path)
        self.flash(f"opened {path.name}")

    def open_tree_item(self, index):
        path = Path(self.project_model.filePath(index))
        if path.is_file() and path.suffix.lower() == ".peb":
            self.open_path(path)

    def save_file(self):
        doc = self.current_document()
        if doc is None:
            return
        if doc.path is None:
            self.save_as()
            return
        self.write_document(doc, doc.path)

    def save_as(self):
        doc = self.current_document()
        if doc is None:
            return
        default = doc.path if doc.path else ROOT / "Untitled.peb"
        filename, _ = QFileDialog.getSaveFileName(
            self, "Save As", str(default),
            "Pebble programs (*.peb);;All files (*)",
        )
        if filename:
            path = Path(filename)
            if not path.suffix:
                path = path.with_suffix(".peb")
            self.write_document(doc, path)

    def write_document(self, doc, path):
        path = path.resolve()
        for other in self.documents:
            if other is not doc and other.path == path:
                self.notice(
                    "File Already Open",
                    f"{path.name} is already open in another tab.",
                    QMessageBox.Icon.Warning,
                )
                self.tabs.setCurrentWidget(other)
                return

        try:
            path.write_text(doc.editor.toPlainText(), encoding="UTF-8")
        except OSError as exc:
            self.log(f"could not save {path.name}: {exc}", "error")
            self.notice("Save Failed", str(exc), QMessageBox.Icon.Critical)
            return

        doc.path = path
        doc.editor.document().setModified(False)
        doc.dirty = False
        self.refresh_tab_label(doc)
        self.project_model.setRootPath(str(ROOT))
        self.record_recent_file(path)
        self.flash(f"saved {path.name}")

    def confirm_close(self, doc):
        if not doc.dirty:
            return True
        box = QMessageBox(
            QMessageBox.Icon.Question, "Unsaved Changes",
            f"Save changes to {doc.display_name}?", parent=self,
        )
        save = box.addButton("Save", QMessageBox.ButtonRole.AcceptRole)
        box.addButton("Discard", QMessageBox.ButtonRole.DestructiveRole)
        cancel = box.addButton("Cancel", QMessageBox.ButtonRole.RejectRole)
        box.setDefaultButton(save)
        box.setEscapeButton(cancel)
        box.exec()
        clicked = box.clickedButton()
        if clicked is cancel:
            return False
        if clicked is save:
            self.tabs.setCurrentWidget(doc)
            self.save_file()
            return not doc.dirty
        return True

    def close_tab(self, index=None):
        if isinstance(index, bool) or index is None:
            index = self.tabs.currentIndex()
        if index < 0:
            return
        doc = self.tabs.widget(index)
        if isinstance(doc, EditorTab) and not self.confirm_close(doc):
            return
        self.tabs.removeTab(index)
        if isinstance(doc, EditorTab) and doc in self.documents:
            self.documents.remove(doc)
        doc.deleteLater()
        self.ensure_welcome()
        self.update_status()

    def find_text(self):
        doc = self.current_document()
        if doc:
            doc.find_bar.open_bar(False)

    def replace_text(self):
        doc = self.current_document()
        if doc:
            doc.find_bar.open_bar(True)

    def find_step(self, backward):
        doc = self.current_document()
        if doc and not doc.find_bar.find(backward):
            self.flash("no matches")

    def step_tab(self, step):
        if self.tabs.count():
            self.tabs.setCurrentIndex(
                (self.tabs.currentIndex() + step) % self.tabs.count()
            )

    def open_example(self):
        examples = resolve_examples_dir(ROOT, APP_DIR)

        paths = list_example_paths(examples)
        if not paths:
            self.notice(
                "No Examples",
                f"No Pebble examples were found in:\n{examples}",
            )
            return

        dialog = self.make_input("Open Example", "Choose an Example:")
        dialog.setOption(QInputDialog.InputDialogOption.UseListViewForComboBoxItems)
        dialog.setComboBoxItems([path.name for path in paths])
        dialog.setComboBoxEditable(False)

        if dialog.exec() and dialog.textValue():
            self.open_path(examples / dialog.textValue())

    def is_running(self):
        return self.process.state() != QProcess.ProcessState.NotRunning

    def update_status(self, *_args):
        doc = self.current_document()
        if doc is None:
            self.doc_info.setText("")
            self.status_cursor.setText("")
            self.status_encoding.hide()
            self.set_state(
                "Running" if self.is_running() else "Ready",
                False,
            )
            self.setWindowTitle("Pebble Studio")
            return
        cursor = doc.editor.textCursor()
        self.doc_info.setText(doc.display_name + ("  ●" if doc.dirty else ""))
        self.status_cursor.setText(
            f"line {cursor.blockNumber() + 1}, column {cursor.positionInBlock() + 1}"
        )
        self.status_encoding.show()
        if self.is_running():
            self.set_state("Running", False)
        else:
            self.set_state("Modified" if doc.dirty else "Saved", doc.dirty)
        self.setWindowTitle(
            f"{doc.display_name}{' *' if doc.dirty else ''} — Pebble Studio"
        )

    def set_state(self, text, modified):
        self.status_state.setText(text)
        self.status_state.setProperty("modified", modified)
        self.status_state.style().unpolish(self.status_state)
        self.status_state.style().polish(self.status_state)

    def show_reference(self):
        self.notice(
            "Pebble Language Reference",
            "rock start { ... }\nEntry point for a program.\n\n"
            "let score = 10\nDeclare a variable.\n\n"
            'say "Hello"\nPrint a value.\n\n'
            'ask "Your name: "\nRequest input.\n\n'
            "if condition { ... } otherwise { ... }\nConditional execution.\n\n"
            "repeat 5 { ... }\nRepeat a block.\n\n"
            "fun greet(name) { ... give value }\nDefine a function.",
        )

    def about(self):
        self.notice(
            "About Pebble Studio",
            "Heya!\nPebble Studio Is A Small Programming Enviroment Built For The Coding Language Pebble!\n\n"
            "(Built with Python and PySide6)",
        )

    def update_run_actions(self, *_args):
        running = self.is_running()
        self.actions["run"].setEnabled(not running)
        self.actions["stop"].setEnabled(running)
        self.program_input.setEnabled(running)
        if running:
            self.program_input.setPlaceholderText(
                "Type your answer and press Enter"
            )
        else:
            self.program_input.setPlaceholderText(
                "Program input — available while a program is running"
            )
        self.update_status()

    def run_current_file(self):
        if self.process.state() != QProcess.ProcessState.NotRunning:
            self.log("A Pebble program is already running.", "warning")
            return

        doc = self.current_document()
        if doc is None:
            self.notice("Run Pebble", "Open a .peb file before running it.")
            return

        if doc.path is None:
            self.save_as()
            doc = self.current_document()
            if doc is None or doc.path is None:
                return

        if doc.dirty:
            self.save_file()
            if doc.dirty:
                self.log("Run cancelled because the file was not saved.", "warning")
                return

        if doc.path.suffix.lower() != ".peb":
            self.notice("Run Pebble", "Pebble Studio runs .peb files.")
            return

        program, arguments, working_directory, error_message = build_pebble_run_command(
            doc.path, APP_DIR, getattr(sys, "frozen", False), sys.executable
        )
        if error_message is not None:
            self.log(error_message, "error")
            self.output_dock.show()
            return

        self._decoders = {}
        self.output.clear()
        self.output_dock.show()
        self.log(f"Running {doc.path.name}…")
        self.process.setWorkingDirectory(working_directory)
        self.process.start(program, arguments)
        self.update_run_actions()

    def append_program_output(self, data, level="normal"):
        decoder = self._decoders.setdefault(
            level, codecs.getincrementaldecoder("utf-8")(errors="replace")
        )
        text = decoder.decode(bytes(data)).replace("\r\n", "\n").replace("\r", "\n")
        if not text:
            return

        fmt = QTextCharFormat()
        fmt.setForeground(QColor(LEVEL_COLOURS[level]))
        cursor = self.output.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        cursor.insertText(text, fmt)
        self.output.setTextCursor(cursor)
        self.output.ensureCursorVisible()

    def send_program_input(self):
        if self.process.state() != QProcess.ProcessState.Running:
            return

        answer = self.program_input.text()
        written = self.process.write((answer + "\n").encode("utf-8"))

        if written < 0:
            self.log(
                f"could not send input: {self.process.errorString()}",
                "error",
            )
            return

        prompt_pending = self.output.document().lastBlock().length() > 1
        echo = answer if prompt_pending else f"> {answer}"
        self.append_program_output(echo.encode("utf-8") + b"\n", "info")
        self.program_input.clear()

    def read_program_stdout(self):
        self.append_program_output(self.process.readAllStandardOutput())

    def read_program_stderr(self):
        self.append_program_output(self.process.readAllStandardError(), "error")

    def program_finished(self, exit_code, exit_status):
        self._kill_timer.stop()
        self.read_program_stdout()
        self.read_program_stderr()
        self.update_run_actions()

        if exit_status == QProcess.ExitStatus.CrashExit:
            self.log("The Pebble program crashed.", "error")
        elif exit_code == 0:
            self.log("Program finished successfully.")
        else:
            self.log(f"Program exited with code {exit_code}.", "error")

    def program_error(self, error):
        if error == QProcess.ProcessError.FailedToStart:
            self.log(
                f"Could not start the Pebble program: {self.process.errorString()}",
                "error",
            )
        self.update_run_actions()

    def stop_program(self):
        if self.process.state() == QProcess.ProcessState.NotRunning:
            return
        if self._kill_timer.isActive():
            return

        self.log("Stopping Pebble program…", "warning")
        self.process.terminate()
        self._kill_timer.start(1500)

    def force_stop_program(self):
        if self.process.state() != QProcess.ProcessState.NotRunning:
            self.process.kill()

    def closeEvent(self, event):
        for doc in list(self.documents):
            if not self.confirm_close(doc):
                event.ignore()
                return
        self._kill_timer.stop()
        if self.process.state() != QProcess.ProcessState.NotRunning:
            self.process.terminate()
            if not self.process.waitForFinished(1000):
                self.process.kill()
                self.process.waitForFinished(1000)
        event.accept()


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    app = QApplication(sys.argv)
    app.setApplicationName("Pebble Studio")
    app.setStyle("Fusion")
    app.setPalette(dark_palette())
    font = QFont()
    font.setFamilies([
        "JetBrains Mono",
        "Cascadia Mono",
        "DejaVu Sans Mono",
        "monospace",
    ])
    font.setPointSizeF(9.5)
    app.setFont(font)
    window = PebbleStudio()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()