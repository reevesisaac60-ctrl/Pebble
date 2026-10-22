#include "studio.hpp"
#include "assets.hpp"
#include "glass_surface.hpp"
#include "theme.hpp"

#include "pebble_manifest/project.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QEasingCurve>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QFont>
#include <QFormLayout>
#include <QHelpEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScreen>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSet>
#include <QSettings>
#include <QSlider>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextStream>
#include <QTimer>
#include <QToolTip>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

const QString &untitledName() {
    static const QString name =QLatin1String("Untitled.peb");
    return name;
}

const QString &sourceSuffix() {
    static const QString suffix = QLatin1String("peb");
    return suffix;
}

const QString &pebbleFileFilter() {
    static const QString filter = QStringLiteral(
        "Pebble programs (*.%1);;All files (*)"
    ).arg(sourceSuffix());
    return filter;
}

constexpr int kIndentWidth = 4;

constexpr int kRecentFileLimit = 8;

QString leadingWhitespace(const QString &text) {
    int index = 0;

    while (index < text.size() &&
           (text.at(index) == QLatin1Char(' ') || text.at(index) == QLatin1Char('\t'))) {
        ++index;
    }

    return text.left(index);
}

bool isUsableExecutable(const QString &path) {
    const QFileInfo info(path);

    return info.exists() && info.isFile() && info.isExecutable();
}

QString studioDirectory(const QString &studioBinary) {
    if (studioBinary.isEmpty()) {
        return QString();
    }

    return QFileInfo(studioBinary).absolutePath();
}

QString findInSearchPath(const QString &name, const QString &searchPath) {
    if (searchPath.isEmpty() || name.isEmpty()) {
        return QString();
    }

    const QChar separator = QChar::fromLatin1(
        #ifdef Q_OS_WIN
        ';'
        #else
        ':'
        #endif
    );

    const QStringList directories = searchPath.split(
        separator,
        Qt::SkipEmptyParts
    );

    for (const QString &directory : directories) {
        const QString candidate = directory + "/" + name;

        if (isUsableExecutable(candidate)) {
            return candidate;
        }
    }

    return QString();
}

}

ExecutableResolution resolvePebbleExecutable(
    const QString &studioBinary,
    const QString &envPebble,
    const QString &searchPath
) {
    const QString adjacent = studioDirectory(studioBinary) + "/pebble";

    if (isUsableExecutable(adjacent)) {
        return {true, adjacent, QString()};
    }

    if (!envPebble.isEmpty() && isUsableExecutable(envPebble)) {
        return {true, envPebble, QString()};
    }

    const QString found = findInSearchPath("pebble", searchPath);

    if (!found.isEmpty()) {
        return {true, found, QString()};
    }

    QString reason = "Could not find the Pebble CLI executable. Looked for "
                     "'pebble' next to the Studio";

    if (!envPebble.isEmpty()) {
        reason += ", in the PEBBLE environment variable ('" + envPebble + "')";
    }

    reason += " and in PATH.";

    return {false, QString(), reason};
}

bool parseDiagnosticLine(const QString &line, DiagnosticEntry &entry) {
    static const QRegularExpression pattern(
        "^(.*?):([0-9]+):([0-9]+): (error|warning): (.+)$"
    );

    QString text = line;

    if (text.endsWith('\r')) {
        text.chop(1);
    }

    const auto match = pattern.match(text);

    if (!match.hasMatch()) {
        return false;
    }

    bool lineOk = false;
    const int lineNumber = match.captured(2).toInt(&lineOk);

    bool columnOk = false;
    const int columnNumber = match.captured(3).toInt(&columnOk);

    if (!lineOk || !columnOk || lineNumber < 1 || columnNumber < 1) {
        return false;
    }

    const QString path = match.captured(1).trimmed();

    if (path.isEmpty()) {
        return false;
    }

    entry.path = path;
    entry.line = lineNumber;
    entry.column = columnNumber;
    entry.severity = match.captured(4);
    entry.message = match.captured(5);

    return true;
}

namespace {

QString outputPrefix(OutputLevel level) {
    switch (level) {
    case OutputLevel::Info:
        return QStringLiteral("info: ");
    case OutputLevel::Warning:
        return QStringLiteral("warning: ");
    case OutputLevel::Error:
        return QStringLiteral("error: ");
    case OutputLevel::Normal:
        break;
    }

    return QString();
}

OutputLevel outputLevelOfLine(const QString &line) {
    DiagnosticEntry entry;

    if (parseDiagnosticLine(line, entry)) {
        return OutputLevel::Normal;
    }

    return OutputLevel::Error;
}

QPixmap readWallpaperPixmap(const QString &path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize imageSize = reader.size();
    constexpr int maxWallpaperDimension = 2560;

    if (imageSize.isValid() &&
        (imageSize.width() > maxWallpaperDimension ||
         imageSize.height() > maxWallpaperDimension)) {
        reader.setScaledSize(imageSize.scaled(
            QSize(maxWallpaperDimension, maxWallpaperDimension),
            Qt::KeepAspectRatio
        ));
    }

    return QPixmap::fromImage(reader.read());
}

QPixmap readWallpaperThumbnail(const QString &path, const QSize &size) {
    if (path.isEmpty() || size.isEmpty()) {
        return QPixmap();
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize imageSize = reader.size();

    if (imageSize.isValid() && !imageSize.isEmpty()) {
        reader.setScaledSize(imageSize.scaled(size, Qt::KeepAspectRatio));
    }

    const QImage image = reader.read();

    if (image.isNull()) {
        return QPixmap();
    }

    return QPixmap::fromImage(image);
}

void feedLines(
    QString &buffer,
    const QString &text,
    const std::function<void(const QString &)> &onLine
) {
    buffer += text;

    qsizetype start = 0;

    while (true) {
        const qsizetype index = buffer.indexOf(QLatin1Char('\n'), start);

        if (index < 0) {
            break;
        }

        onLine(buffer.mid(start, index - start));
        start = index + 1;
    }

    buffer.remove(0, start);
}

}

namespace {

struct Token {
    int start = 0;
    int length = 0;
    SyntaxCategory category = SyntaxCategory::Plain;
};

bool isKeyword(const QString &word) {
    static const QSet<QString> keywords = {
        QStringLiteral("fn"),
        QStringLiteral("let"),
        QStringLiteral("var"),
        QStringLiteral("import"),
        QStringLiteral("package"),
        QStringLiteral("export"),
        QStringLiteral("if"),
        QStringLiteral("else"),
        QStringLiteral("while"),
        QStringLiteral("return")
    };

    return keywords.contains(word);
}

bool isLiteral(const QString &word) {
    return word == QLatin1String("true") ||
        word == QLatin1String("false") ||
        word == QLatin1String("null");
}

bool isType(const QString &word) {
    static const QSet<QString> types = {
        QStringLiteral("Int"),
        QStringLiteral("Float"),
        QStringLiteral("Bool"),
        QStringLiteral("String"),
        QStringLiteral("Void")
    };

    return types.contains(word);
}

bool isWordStart(QChar character) {
    return character.isLetter() || character == QLatin1Char('_');
}

bool isWordPart(QChar character) {
    return character.isLetterOrNumber() || character == QLatin1Char('_');
}

bool isOperatorCharacter(QChar character) {
    static const QString characters = QStringLiteral("+-*/%=<>!&|(){}[],.:;");

    return characters.contains(character);
}

QList<Token> scanLine(const QString &text) {
    QList<Token> tokens;

    const int length = static_cast<int>(text.size());
    int index = 0;
    bool declarationFollows = false;

    while (index < length) {
        const QChar character = text.at(index);
        const int start = index;

        if (character == QLatin1Char('/') &&
            index + 1 < length &&
            text.at(index + 1) == QLatin1Char('/')) {
            tokens.append(
                Token{start, length - start, SyntaxCategory::Comment}
            );
            break;
        }

        if (character == QLatin1Char('"')) {
            int end = index + 1;

            while (end < length) {
                if (text.at(end) == QLatin1Char('\\')) {
                    end += 2;
                    continue;
                }

                if (text.at(end) == QLatin1Char('"')) {
                    ++end;
                    break;
                }

                ++end;
            }

            tokens.append(Token{start, end - start, SyntaxCategory::String});
            index = end;
            continue;
        }

        if (character.isDigit()) {
            int end = index;

            while (end < length && text.at(end).isDigit()) {
                ++end;
            }

            if (end + 1 < length &&
                text.at(end) == QLatin1Char('.') &&
                text.at(end + 1).isDigit()) {
                ++end;

                while (end < length && text.at(end).isDigit()) {
                    ++end;
                }
            }

            tokens.append(Token{start, end - start, SyntaxCategory::Number});
            index = end;
            continue;
        }

        if (isWordStart(character)) {
            int end = index;

            while (end < length && isWordPart(text.at(end))) {
                ++end;
            }

            const QString word = text.mid(index, end - index);
            const QString rest = text.mid(end);
            SyntaxCategory category = SyntaxCategory::Plain;

            if (isType(word)) {
                category = SyntaxCategory::Type;
            } else if (isLiteral(word)) {
                category = SyntaxCategory::Literal;
            } else if (word == QLatin1String("print")) {
                category = SyntaxCategory::Builtin;
            } else if (isKeyword(word)) {
                category = SyntaxCategory::Keyword;
                declarationFollows = word == QLatin1String("let") ||
                    word == QLatin1String("var");
                if (word != QLatin1String("let") &&
                    word != QLatin1String("var")) {
                    declarationFollows = false;
                }
            } else if (declarationFollows) {
                category = SyntaxCategory::Declaration;
                declarationFollows = false;
            } else if (rest.trimmed().startsWith(QLatin1Char('('))) {
                category = SyntaxCategory::Function;
            }

            tokens.append(Token{start, end - index, category});
            index = end;
            continue;
        }

        if (isOperatorCharacter(character)) {
            tokens.append(Token{start, 1, SyntaxCategory::Operator});
        }

        ++index;
    }

    return tokens;
}

}

SyntaxCategory syntaxCategoryAt(const QString &line, int offset) {
    for (const Token &token : scanLine(line)) {
        if (offset >= token.start && offset < token.start + token.length) {
            return token.category;
        }
    }

    return SyntaxCategory::Plain;
}

QStringList syntaxColourKeys() {
    return {
        QStringLiteral("keyword"),
        QStringLiteral("string"),
        QStringLiteral("number"),
        QStringLiteral("comment"),
        QStringLiteral("builtin"),
        QStringLiteral("boolean"),
        QStringLiteral("function"),
        QStringLiteral("type"),
        QStringLiteral("declaration"),
        QStringLiteral("operator")
    };
}

QString syntaxColourLabel(const QString &key) {
    if (key == QLatin1String("keyword")) {
        return QStringLiteral("Keywords");
    }

    if (key == QLatin1String("string")) {
        return QStringLiteral("Strings");
    }

    if (key == QLatin1String("number")) {
        return QStringLiteral("Numbers");
    }

    if (key == QLatin1String("comment")) {
        return QStringLiteral("Comments");
    }

    if (key == QLatin1String("builtin")) {
        return QStringLiteral("Built-ins (print)");
    }

    if (key == QLatin1String("boolean")) {
        return QStringLiteral("Booleans (true, false)");
    }

    if (key == QLatin1String("function")) {
        return QStringLiteral("Function calls");
    }

    if (key == QLatin1String("type")) {
        return QStringLiteral("Types");
    }

    if (key == QLatin1String("declaration")) {
        return QStringLiteral("Declarations");
    }

    if (key == QLatin1String("operator")) {
        return QStringLiteral("Operators and punctuation");
    }

    return key;
}

SyntaxColourMap defaultSyntaxColours() {
    SyntaxColourMap colours;

    colours.insert(QStringLiteral("keyword"), QString::fromLatin1(theme::keyword));
    colours.insert(QStringLiteral("string"), QString::fromLatin1(theme::string));
    colours.insert(QStringLiteral("number"), QString::fromLatin1(theme::number));
    colours.insert(QStringLiteral("comment"), QString::fromLatin1(theme::comment));
    colours.insert(QStringLiteral("builtin"), QString::fromLatin1(theme::builtin));
    colours.insert(QStringLiteral("boolean"), QString::fromLatin1(theme::boolean));
    colours.insert(QStringLiteral("function"), QString::fromLatin1(theme::function));
    colours.insert(QStringLiteral("type"), QString::fromLatin1(theme::type));
    colours.insert(
        QStringLiteral("declaration"),
        QString::fromLatin1(theme::declaration)
    );
    colours.insert(
        QStringLiteral("operator"),
        QString::fromLatin1(theme::operatorColour)
    );

    return colours;
}

SyntaxColourMap normalisedSyntaxColours(const SyntaxColourMap &colours) {
    SyntaxColourMap result = defaultSyntaxColours();

    for (const QString &key : syntaxColourKeys()) {
        const QColor candidate(colours.value(key));

        if (candidate.isValid()) {
            result.insert(key, candidate.name());
        }
    }

    return result;
}

PebbleHighlighter::PebbleHighlighter(
    QTextDocument *document,
    const SyntaxColourMap &colours
)
    : QSyntaxHighlighter(document),
      currentColours(normalisedSyntaxColours(colours)) {
}

void PebbleHighlighter::setColours(const SyntaxColourMap &colours) {
    currentColours = normalisedSyntaxColours(colours);
    rehighlight();
}

void PebbleHighlighter::highlightBlock(const QString &text) {
    for (const Token &token : scanLine(text)) {
        if (token.category == SyntaxCategory::Plain) {
            continue;
        }

        QTextCharFormat format;

        switch (token.category) {
        case SyntaxCategory::Keyword:
            format.setForeground(QColor(currentColours.value("keyword")));
            format.setFontWeight(QFont::Bold);
            break;
        case SyntaxCategory::Literal:
            format.setForeground(QColor(currentColours.value("boolean")));
            break;
        case SyntaxCategory::Builtin:
            format.setForeground(QColor(currentColours.value("builtin")));
            break;
        case SyntaxCategory::Function:
            format.setForeground(QColor(currentColours.value("function")));
            break;
        case SyntaxCategory::Type:
            format.setForeground(QColor(currentColours.value("type")));
            break;
        case SyntaxCategory::Declaration:
            format.setForeground(QColor(currentColours.value("declaration")));
            break;
        case SyntaxCategory::String:
            format.setForeground(QColor(currentColours.value("string")));
            break;
        case SyntaxCategory::Number:
            format.setForeground(QColor(currentColours.value("number")));
            break;
        case SyntaxCategory::Comment:
            format.setForeground(QColor(currentColours.value("comment")));
            format.setFontItalic(true);
            break;
        case SyntaxCategory::Operator:
            format.setForeground(QColor(currentColours.value("operator")));
            break;
        case SyntaxCategory::Plain:
            break;
        }

        setFormat(token.start, token.length, format);
    }
}

namespace {

const char *syntaxPreviewSample() {
    return "fn main() {\n"
           "    let message = \"Hello, Pebble!\";\n"
           "    var count = 42;\n"
           "    print(message, count);\n"
           "    if true {\n"
           "        print(count + 1);\n"
           "    }\n"
           "}\n";
}

}

SyntaxColoursDialog::SyntaxColoursDialog(
    const SyntaxColourMap &colours,
    QWidget *parent
)
    : QDialog(parent), currentColours(normalisedSyntaxColours(colours)) {
    setWindowTitle(QStringLiteral("Syntax Colours"));
    setObjectName(QStringLiteral("syntax_colours_dialog"));
    setMinimumWidth(470);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    for (const QString &key : syntaxColourKeys()) {
        auto *button = new QPushButton(currentColours.value(key), this);
        button->setObjectName(QStringLiteral("syntax_colour_button"));
        button->setMinimumWidth(105);

        connect(button, &QPushButton::clicked, this, [this, key]() {
            chooseColour(key);
        });

        buttons.insert(key, button);
        form->addRow(syntaxColourLabel(key), button);
    }

    layout->addLayout(form);
    layout->addWidget(new QLabel(QStringLiteral("Preview"), this));

    previewWidget = new QPlainTextEdit(this);
    previewWidget->setObjectName(QStringLiteral("syntax_preview"));
    previewWidget->setReadOnly(true);
    previewWidget->setMinimumHeight(170);
    previewWidget->setFont(studioCodeFont(10));
    previewWidget->setPlainText(QString::fromLatin1(syntaxPreviewSample()));
    previewHighlighter = new PebbleHighlighter(
        previewWidget->document(),
        currentColours
    );
    layout->addWidget(previewWidget);

    resetButtonWidget = new QPushButton(QStringLiteral("Reset to Defaults"), this);
    resetButtonWidget->setObjectName(QStringLiteral("syntax_reset_button"));
    connect(resetButtonWidget, &QPushButton::clicked, this, [this]() {
        resetDefaults();
    });
    layout->addWidget(resetButtonWidget);

    auto *box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this
    );
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(box);

    for (const QString &key : syntaxColourKeys()) {
        refreshColour(key);
    }
}

QPushButton *SyntaxColoursDialog::colourButton(const QString &key) const {
    return buttons.value(key, nullptr);
}

void SyntaxColoursDialog::setColour(const QString &key, const QString &colour) {
    const QColor candidate(colour);

    if (!buttons.contains(key) || !candidate.isValid()) {
        return;
    }

    currentColours.insert(key, candidate.name());
    refreshColour(key);
}

void SyntaxColoursDialog::resetDefaults() {
    currentColours = defaultSyntaxColours();

    for (const QString &key : syntaxColourKeys()) {
        refreshColour(key);
    }
}

void SyntaxColoursDialog::chooseColour(const QString &key) {
    const QColor selected = QColorDialog::getColor(
        QColor(currentColours.value(key)),
        this,
        syntaxColourLabel(key)
    );

    if (selected.isValid()) {
        setColour(key, selected.name());
    }
}

void SyntaxColoursDialog::refreshColour(const QString &key) {
    QPushButton *button = buttons.value(key, nullptr);

    if (button == nullptr) {
        return;
    }

    const QString colour = currentColours.value(key);

    const QColor swatch(colour);
    const bool light = swatch.lightness() > 128;

    button->setText(colour);
    button->setStyleSheet(
        QStringLiteral(
            "QPushButton { background-color: %1; color: %2;"
            " border: 1px solid %3; border-radius: 3px;"
            " padding: 5px 10px; }"
        ).arg(colour,
              light ? QLatin1String(theme::background)
                    : QLatin1String(theme::text),
              QLatin1String(theme::lineStrong))
    );

    previewHighlighter->setColours(currentColours);
}

QString WallpaperPickerDialog::displayNameFor(const QString &path) {
    const QFileInfo info(path);
    QString base = info.completeBaseName();

    if (base.isEmpty()) {
        base = info.fileName();
    }

    base.replace('_', ' ');
    base.replace('-', ' ');

    QStringList words;
    QString word;

    for (const QChar character : base) {
        if (character.isSpace()) {
            if (!word.isEmpty()) {
                words << word;
                word.clear();
            }

            continue;
        }

        if (!word.isEmpty()
            && word.at(word.size() - 1).isLetter() != character.isLetter()) {
            words << word;
            word.clear();
        }

        word += character;
    }

    if (!word.isEmpty()) {
        words << word;
    }

    QStringList readable;

    for (const QString &part : words) {
        readable << part.at(0).toUpper() + part.mid(1);
    }

    if (readable.isEmpty()) {
        return info.fileName();
    }

    return readable.join(' ');
}

WallpaperPickerDialog::WallpaperPickerDialog(
    const QStringList &wallpapers,
    const QString &currentPath,
    const QString &defaultPath,
    QWidget *parent
) : QDialog(parent) {
    setObjectName("wallpaper_picker_dialog");
    setWindowTitle("Choose a Wallpaper");
    setMinimumSize(600, 430);

    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(
        "Pick the wallpaper of the Studio. The panels keep their glass over it.",
        this
    );
    intro->setObjectName("text_dim");
    layout->addWidget(intro);

    auto *body = new QHBoxLayout;

    list = new QListWidget(this);
    list->setObjectName("wallpaper_list");
    list->setViewMode(QListView::IconMode);
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setWrapping(true);
    list->setWordWrap(true);
    list->setSpacing(6);
    list->setIconSize(QSize(168, 95));
    list->setGridSize(QSize(184, 128));
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setUniformItemSizes(false);
    body->addWidget(list, 1);

    auto *side = new QVBoxLayout;

    preview = new QLabel(this);
    preview->setObjectName("wallpaper_preview");
    preview->setFixedSize(240, 135);
    preview->setAlignment(Qt::AlignCenter);
    preview->setFrameShape(QFrame::StyledPanel);
    side->addWidget(preview);

    nameLabelWidget = new QLabel(this);
    nameLabelWidget->setObjectName("wallpaper_name");
    nameLabelWidget->setWordWrap(true);
    nameLabelWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
    side->addWidget(nameLabelWidget);

    side->addStretch(1);
    body->addLayout(side);
    layout->addLayout(body, 1);

    const QSize thumbnailSize(168, 95);

    const auto addEntry = [this, thumbnailSize](
                              const QString &path,
                              const QString &label
                          ) {
        auto *item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        const QPixmap thumbnail = readWallpaperThumbnail(path, thumbnailSize);

        if (!thumbnail.isNull()) {
            item->setIcon(QIcon(thumbnail));
        }

        return item;
    };

    const QString fallback = defaultPath.isEmpty() ? currentPath : defaultPath;
    QStringList offered;

    if (!fallback.isEmpty()) {
        offered << fallback;
        addEntry(fallback, QStringLiteral("Studio Default"));
    }

    for (const QString &path : wallpapers) {
        if (offered.contains(path)) {
            continue;
        }

        offered << path;
        addEntry(path, displayNameFor(path));
    }

    if (!currentPath.isEmpty() && !offered.contains(currentPath)) {
        offered << currentPath;
        addEntry(currentPath, displayNameFor(currentPath));
    }

    QListWidgetItem *chosen = nullptr;

    for (int index = 0; index < list->count(); ++index) {
        QListWidgetItem *item = list->item(index);

        if (item->data(Qt::UserRole).toString() == currentPath) {
            chosen = item;
            break;
        }
    }

    if (chosen == nullptr && list->count() > 0) {
        chosen = list->item(0);
    }

    if (chosen != nullptr) {
        list->setCurrentItem(chosen);
        refreshPreview(chosen);
        selected = chosen->data(Qt::UserRole).toString();
    }

    connect(list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *item) { refreshPreview(item); });
    connect(list, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item) {
                refreshPreview(item);
                accept();
            });

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this
    );
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Apply"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        const QListWidgetItem *item = list->currentItem();

        if (item != nullptr) {
            selected = item->data(Qt::UserRole).toString();
        }

        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QStringList WallpaperPickerDialog::offeredPaths() const {
    QStringList paths;

    for (int index = 0; index < list->count(); ++index) {
        paths << list->item(index)->data(Qt::UserRole).toString();
    }

    return paths;
}

QString WallpaperPickerDialog::displayedName(const QString &path) const {
    for (int index = 0; index < list->count(); ++index) {
        const QListWidgetItem *item = list->item(index);

        if (item->data(Qt::UserRole).toString() == path) {
            return item->text();
        }
    }

    return QString();
}

void WallpaperPickerDialog::refreshPreview(QListWidgetItem *item) {
    if (item == nullptr) {
        preview->clear();
        nameLabelWidget->clear();
        return;
    }

    const QString path = item->data(Qt::UserRole).toString();
    selected = path;
    const QPixmap image = readWallpaperThumbnail(path, QSize(240, 135));

    if (image.isNull()) {
        preview->setPixmap(QPixmap());
    } else {
        preview->setPixmap(image.scaled(
            preview->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        ));
    }

    nameLabelWidget->setText(
        QStringLiteral("%1\n%2").arg(item->text(), QFileInfo(path).fileName())
    );
}

OutputHighlighter::OutputHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document) {
}
void OutputHighlighter::highlightBlock(const QString &text) {
    QTextCharFormat format;
    format.setForeground(QColor(theme::text));

    if (text.startsWith(QLatin1String("error: "))) {
        format.setForeground(QColor(theme::error));
    } else if (text.startsWith(QLatin1String("warning: "))) {
        format.setForeground(QColor(theme::warning));
    } else if (text.startsWith(QLatin1String("info: "))) {
        format.setForeground(QColor(theme::textDim));
    } else {
        DiagnosticEntry entry;

        if (parseDiagnosticLine(text, entry)) {
            format.setForeground(
                QColor(entry.severity == QLatin1String("warning")
                          ? theme::warning
                          : theme::error)
            );
        }
    }

    setFormat(0, static_cast<int>(text.length()), format);
}

LineNumberArea::LineNumberArea(CodeEditor *codeEditor)
    : QWidget(codeEditor), editor(codeEditor) {
}

QSize LineNumberArea::sizeHint() const {
    return QSize(editor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event) {
    QPainter painter(this);

    QTextBlock block = editor->firstVisibleBlock();
    int number = block.blockNumber();

    int top = static_cast<int>(
        editor->blockBoundingGeometry(block).translated(editor->contentOffset()).top()
    );
    int bottom = top + static_cast<int>(editor->blockBoundingRect(block).height());

    const int currentLine = editor->currentLineNumber();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.setPen(
                QColor(number == currentLine ? theme::lineNumberActive : theme::lineNumber)
            );
            int textTop = top;
            int textHeight = editor->fontMetrics().height();

            if (block.layout() != nullptr && block.layout()->lineCount() > 0) {
                const QTextLine line = block.layout()->lineAt(0);

                if (line.isValid()) {
                    textTop = top + qRound(line.y());
                    textHeight = qRound(line.height());
                }
            }

            painter.drawText(
                0,
                textTop,
                width() - 12,
                textHeight,
                Qt::AlignRight | Qt::AlignVCenter,
                QString::number(number + 1)
            );
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(editor->blockBoundingRect(block).height());
        ++number;
    }
}

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent) {
    setObjectName("editor");
    lineNumberArea = new LineNumberArea(this);

    connect(
        this,
        &QPlainTextEdit::blockCountChanged,
        this,
        [this]() {
            updateLineNumberAreaWidth();
            viewport()->update();
            lineNumberArea->update();
        }
    );

    connect(
        this,
        &QPlainTextEdit::updateRequest,
        this,
        [this](const QRect &, int) {
            lineNumberArea->update();
            viewport()->update();
            positionCompletionPopup();
        }
    );

    connect(
        this,
        &QPlainTextEdit::cursorPositionChanged,
        this,
        [this]() {
            highlightCurrentLine();
            suggestionDismissed = false;
            refreshCompletions();
            refreshParameterHint();
            hideHoverDocumentation();
        }
    );

    setFont(studioCodeFont(11));
    setFrameStyle(QFrame::NoFrame);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);
    viewport()->setAutoFillBackground(false);
    viewport()->setBackgroundRole(QPalette::NoRole);
    viewport()->setAttribute(Qt::WA_NoSystemBackground);
    viewport()->setAttribute(Qt::WA_TranslucentBackground);
    QPalette editorPalette = palette();
    editorPalette.setColor(QPalette::Base, QColor(0, 0, 0, 0));
    editorPalette.setColor(QPalette::Window, QColor(0, 0, 0, 0));
    setPalette(editorPalette);
    viewport()->setPalette(editorPalette);
    lineNumberArea->setAttribute(Qt::WA_NoSystemBackground);
    lineNumberArea->setAutoFillBackground(false);
    setCursorWidth(2);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * kIndentWidth);
    document()->setDocumentMargin(12);

    QTextOption option = document()->defaultTextOption();
    option.setFlags(option.flags() | QTextOption::ShowTabsAndSpaces);
    document()->setDefaultTextOption(option);

    updateLineNumberAreaWidth();
    highlightCurrentLine();

    intelliSenseTimer = new QTimer(this);
    intelliSenseTimer->setSingleShot(true);
    intelliSenseTimer->setInterval(100);
    connect(intelliSenseTimer, &QTimer::timeout, this, &CodeEditor::rebuildIntelliSense);
    connect(document(), &QTextDocument::contentsChanged, this, [this]() {
        intelliSenseDirty = true;
        suggestionDismissed = false;
        ghostSuffix.clear();
        hideCompletions();
        if (parameterHint != nullptr) parameterHint->hide();
        viewport()->update();
        intelliSenseTimer->start();
    });

    intelliSenseWatcher = new QFileSystemWatcher(this);
    const auto moduleChanged = [this](const QString &) {
        intelliSense.invalidateModuleCache();
        intelliSenseDirty = true;
        suggestionDismissed = false;
        ghostSuffix.clear();
        hideCompletions();
        intelliSenseTimer->start();
    };
    connect(intelliSenseWatcher, &QFileSystemWatcher::fileChanged,
            this, moduleChanged);
    connect(intelliSenseWatcher, &QFileSystemWatcher::directoryChanged,
            this, moduleChanged);

    hoverTimer = new QTimer(this);
    hoverTimer->setSingleShot(true);
    hoverTimer->setInterval(500);

    completionPopup = new QFrame(this, Qt::ToolTip | Qt::FramelessWindowHint);
    completionPopup->setObjectName("intellisense_popup");
    completionPopup->setAttribute(Qt::WA_ShowWithoutActivating);
    completionPopup->setFocusPolicy(Qt::NoFocus);
    auto *popupLayout = new QVBoxLayout(completionPopup);
    popupLayout->setContentsMargins(2, 2, 2, 2);
    completionList = new QListWidget(completionPopup);
    completionList->setObjectName("intellisense_list");
    completionList->setFocusPolicy(Qt::NoFocus);
    completionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    completionList->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    popupLayout->addWidget(completionList);
    connect(completionList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        acceptCompletion(completionList->row(item));
    });
    parameterHint = new QFrame(this, Qt::ToolTip | Qt::FramelessWindowHint);
    parameterHint->setObjectName("parameter_hint");
    parameterHint->setAttribute(Qt::WA_ShowWithoutActivating);
    parameterHint->setStyleSheet(QStringLiteral(
        "QFrame#parameter_hint { background: #17171D; border: 1px solid #494653; "
        "border-radius: 5px; } QLabel { color: #D8D8E0; padding: 7px 10px; }"
    ));
    auto *hintLayout = new QVBoxLayout(parameterHint);
    hintLayout->setContentsMargins(0, 0, 0, 0);
    parameterHintLabel = new QLabel(parameterHint);
    parameterHintLabel->setTextFormat(Qt::RichText);
    hintLayout->addWidget(parameterHintLabel);
    viewport()->setMouseTracking(true);
    viewport()->installEventFilter(this);
    connect(hoverTimer, &QTimer::timeout, this, &CodeEditor::showHoverDocumentation);
}

void CodeEditor::setIntelliSenseSourcePath(const QString &path) {
    intelliSenseSourcePath = path;
    intelliSenseDirty = true;
    if (intelliSenseTimer != nullptr) intelliSenseTimer->stop();
    rebuildIntelliSense();
}

void CodeEditor::setIntelliSenseProjectPackages(
    const QStringList &searchPaths,
    const QStringList &packageNames
) {
    intelliSense.setProjectPackages(searchPaths, packageNames);
    intelliSenseDirty = true;
    if (intelliSenseTimer != nullptr) intelliSenseTimer->start();
}

QStringList CodeEditor::completionNames() const {
    QStringList names;
    for (const IntelliSenseCompletion &item : completionItems) names.append(item.name);
    return names;
}

bool CodeEditor::completionPopupVisible() const {
    return completionPopup != nullptr && completionPopup->isVisible();
}

int CodeEditor::completionPopupCurrentRow() const {
    return completionList != nullptr ? completionList->currentRow() : -1;
}

QString CodeEditor::parameterHintText() const {
    return parameterHintLabel != nullptr ? parameterHintLabel->text() : QString();
}

int CodeEditor::activeParameterHintIndex() const {
    if (parameterHintLabel == nullptr) return -1;
    return parameterHintLabel->property("activeParameter").toInt();
}

void CodeEditor::requestCompletions() {
    if (intelliSenseDirty) rebuildIntelliSense();
    suggestionDismissed = false;
    refreshCompletions(true);
}

void CodeEditor::rebuildIntelliSense() {
    intelliSense.rebuild(toPlainText(), intelliSenseSourcePath);
    intelliSenseDirty = false;
    refreshIntelliSenseWatchPaths();
    refreshCompletions();
    refreshParameterHint();
}

void CodeEditor::refreshIntelliSenseWatchPaths() {
    if (intelliSenseWatcher == nullptr) return;
    QStringList files = intelliSense.moduleSourceFiles();
    QStringList directories = intelliSense.moduleSourceDirectories();
    files.sort(Qt::CaseSensitive);
    directories.sort(Qt::CaseSensitive);
    QStringList watchedFiles = intelliSenseWatcher->files();
    QStringList watchedDirectories = intelliSenseWatcher->directories();
    watchedFiles.sort(Qt::CaseSensitive);
    watchedDirectories.sort(Qt::CaseSensitive);
    if (files == watchedFiles && directories == watchedDirectories) return;
    if (!watchedFiles.isEmpty()) intelliSenseWatcher->removePaths(watchedFiles);
    if (!watchedDirectories.isEmpty()) intelliSenseWatcher->removePaths(watchedDirectories);
    if (!files.isEmpty()) intelliSenseWatcher->addPaths(files);
    if (!directories.isEmpty()) intelliSenseWatcher->addPaths(directories);
}

void CodeEditor::refreshCompletions(bool explicitRequest) {
    if (intelliSenseDirty || suggestionDismissed || textCursor().hasSelection()) {
        hideCompletions();
        return;
    }

    const int position = textCursor().position();
    const QString prefix = intelliSense.prefixAt(position);
    if (!explicitRequest && prefix.isEmpty() &&
        !intelliSense.hasContextWithoutPrefix(position)) {
        hideCompletions();
        return;
    }

    completionItems = intelliSense.completions(position, explicitRequest);
    if (completionItems.isEmpty()) {
        hideCompletions();
        return;
    }

    const int ghostIndex = completionPopupVisible() && completionList->currentRow() >= 0
        ? completionList->currentRow() : 0;
    const IntelliSenseCompletion &best = completionItems.at(
        qBound(0, ghostIndex, completionItems.size() - 1)
    );
    ghostSuffix = best.name.startsWith(prefix)
        ? best.name.mid(prefix.size()) : QString();

    const bool showList = explicitRequest || completionItems.size() > 1;
    if (!showList) {
        completionPopup->hide();
    } else {
        const int oldRow = completionList->currentRow();
        completionList->clear();
        for (const IntelliSenseCompletion &item : completionItems) {
            QString display = item.name + QStringLiteral("    ·  ") + item.kind;
            if (!item.signature.isEmpty()) display += QStringLiteral("    ") + item.signature;
            else if (!item.detail.isEmpty()) display += QStringLiteral("    ") + item.detail;
            auto *entry = new QListWidgetItem(display, completionList);
            entry->setData(Qt::UserRole, item.name);
            entry->setToolTip(item.signature.isEmpty() ? item.detail : item.signature);
        }
        completionList->setCurrentRow(qBound(
            0, oldRow < 0 ? 0 : oldRow, static_cast<int>(completionItems.size()) - 1
        ));
        completionList->setFixedWidth(490);
        completionList->setFixedHeight(
            qMin(8, static_cast<int>(completionItems.size())) * 30 + 10
        );
        completionPopup->show();
        positionCompletionPopup();
        completionPopup->raise();
    }
    viewport()->update();
}

void CodeEditor::refreshParameterHint() {
    if (parameterHint == nullptr || parameterHintLabel == nullptr || intelliSenseDirty ||
        textCursor().hasSelection()) {
        if (parameterHint != nullptr) parameterHint->hide();
        return;
    }
    const IntelliSenseSignature help = intelliSense.signatureHelpAt(textCursor().position());
    if (!help.isValid()) {
        parameterHint->hide();
        return;
    }
    const qsizetype open = help.signature.indexOf(QLatin1Char('('));
    const qsizetype close = help.signature.indexOf(QLatin1Char(')'), open + 1);
    QString rendered = help.signature.toHtmlEscaped();
    if (open >= 0 && close > open && help.activeParameter < help.parameters.size()) {
        qsizetype parameterStart = open + 1;
        for (int index = 0; index < help.activeParameter; ++index) {
            const qsizetype comma = rendered.indexOf(QLatin1Char(','), parameterStart);
            if (comma < 0 || comma >= close) break;
            parameterStart = comma + 1;
            while (parameterStart < close && rendered.at(parameterStart).isSpace()) {
                ++parameterStart;
            }
        }
        const QString escaped = help.parameters.at(help.activeParameter).toHtmlEscaped();
        if (rendered.mid(parameterStart, escaped.size()) != escaped) parameterStart = -1;
        if (parameterStart >= 0 && parameterStart < close) {
            rendered.insert(parameterStart + escaped.size(), QStringLiteral("</b>"));
            rendered.insert(parameterStart, QStringLiteral(
                "<b style='color:#C4B5FD'>"));
        }
    }
    parameterHintLabel->setProperty("activeParameter", help.activeParameter);
    parameterHintLabel->setText(rendered);
    parameterHint->adjustSize();
    const QPoint caret = viewport()->mapToGlobal(cursorRect().bottomLeft());
    QScreen *screen = QGuiApplication::screenAt(caret);
    if (screen == nullptr) screen = QGuiApplication::primaryScreen();
    if (screen != nullptr) {
        const QRect available = screen->availableGeometry();
        const QSize size = parameterHint->sizeHint();
        int x = qBound(available.left(), caret.x(), available.right() - size.width() + 1);
        int y = caret.y() + 4;
        if (y + size.height() > available.bottom()) y = caret.y() - size.height() - 4;
        y = qBound(available.top(), y, available.bottom() - size.height() + 1);
        parameterHint->move(x, y);
    }
    parameterHint->show();
    parameterHint->raise();
}

void CodeEditor::hideCompletions() {
    ghostSuffix.clear();
    completionItems.clear();
    if (completionPopup != nullptr) completionPopup->hide();
    if (viewport() != nullptr) viewport()->update();
}

void CodeEditor::acceptCompletion(int row) {
    const int selected = row >= 0 ? row :
        (completionPopupVisible() ? completionList->currentRow() : 0);
    if (selected < 0 || selected >= completionItems.size()) return;

    const QString prefix = intelliSense.prefixAt(textCursor().position());
    const QString name = completionItems.at(selected).name;
    if (!name.startsWith(prefix) || name == prefix) return;

    QTextCursor cursor = textCursor();
    cursor.insertText(name.mid(prefix.size()));
    setTextCursor(cursor);
    suggestionDismissed = false;
    hideCompletions();
}

void CodeEditor::moveCompletionSelection(int delta) {
    if (!completionPopupVisible() || completionItems.isEmpty()) return;
    const int count = static_cast<int>(completionItems.size());
    const int next = (completionList->currentRow() + delta + count) % count;
    completionList->setCurrentRow(next);
    const QString prefix = intelliSense.prefixAt(textCursor().position());
    ghostSuffix = completionItems.at(next).name.mid(prefix.size());
    viewport()->update();
}

void CodeEditor::positionCompletionPopup() {
    if (completionPopup == nullptr || completionList == nullptr ||
        !completionPopup->isVisible()) return;
    const QPoint caret = viewport()->mapToGlobal(cursorRect().bottomLeft());
    QScreen *screen = QGuiApplication::screenAt(caret);
    if (screen == nullptr) screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) return;

    const QRect available = screen->availableGeometry();
    const QSize size = completionPopup->sizeHint().expandedTo(QSize(490, 70));
    int x = caret.x();
    int y = caret.y() + 3;
    if (y + size.height() > available.bottom()) y = caret.y() - size.height() - 3;
    x = qBound(available.left(), x, available.right() - size.width() + 1);
    y = qBound(available.top(), y, available.bottom() - size.height() + 1);
    completionPopup->move(x, y);
}

void CodeEditor::paintEvent(QPaintEvent *event) {
    QPlainTextEdit::paintEvent(event);
    if (ghostSuffix.isEmpty() || !hasFocus() || textCursor().hasSelection()) return;

    const QRect caret = cursorRect();
    QPainter painter(viewport());
    painter.setClipRect(viewport()->rect());
    painter.setPen(QColor(theme::ghostText));
    painter.setFont(font());
    painter.drawText(caret.left(), caret.top() + fontMetrics().ascent(), ghostSuffix);
}

bool CodeEditor::eventFilter(QObject *watched, QEvent *event) {
    if (watched == viewport()) {
        if (event->type() == QEvent::MouseMove) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            QTextCursor cursor = cursorForPosition(mouse->position().toPoint());
            hoverPosition = cursor.position();
            hoverTimer->start();
        } else if (event->type() == QEvent::Leave) {
            hideHoverDocumentation();
        }
    }
    return QPlainTextEdit::eventFilter(watched, event);
}

void CodeEditor::showHoverDocumentation() {
    if (intelliSenseDirty) rebuildIntelliSense();
    const IntelliSenseHover hover = intelliSense.hoverAt(hoverPosition);
    if (!hover.isValid()) {
        QToolTip::hideText();
        return;
    }

    QTextCursor cursor(document());
    cursor.setPosition(qBound(0, hoverPosition, static_cast<int>(document()->characterCount()) - 1));
    const QPoint anchor = viewport()->mapToGlobal(cursorRect(cursor).bottomLeft());
    QToolTip::showText(anchor, hover.title + QLatin1Char('\n') + hover.detail, viewport());
}

void CodeEditor::hideHoverDocumentation() {
    if (hoverTimer != nullptr) hoverTimer->stop();
    hoverPosition = -1;
    QToolTip::hideText();
}

void CodeEditor::focusInEvent(QFocusEvent *event) {
    QPlainTextEdit::focusInEvent(event);
    suggestionDismissed = false;
    if (intelliSenseDirty) rebuildIntelliSense();
    else refreshCompletions();
}

void CodeEditor::focusOutEvent(QFocusEvent *event) {
    QPlainTextEdit::focusOutEvent(event);
    hideCompletions();
    hideHoverDocumentation();
}

void CodeEditor::applyLineHeight() {
    QTextCursor cursor(document());
    cursor.select(QTextCursor::Document);

    QTextBlockFormat format;
    format.setLineHeight(128.0, QTextBlockFormat::ProportionalHeight);
    cursor.mergeBlockFormat(format);
}

void CodeEditor::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);

    updateLineNumberAreaGeometry();
    positionCompletionPopup();
}

void CodeEditor::moveEvent(QMoveEvent *event) {
    QPlainTextEdit::moveEvent(event);
    positionCompletionPopup();
}

void CodeEditor::updateLineNumberAreaGeometry() {
    const QRect contents = contentsRect();
    const int width = lineNumberAreaWidth();

    lineNumberArea->setGeometry(
        contents.left(),
        contents.top(),
        width,
        contents.height()
    );
}

int CodeEditor::lineNumberAreaWidth() const {
    int digits = 1;
    int maximum = qMax(1, blockCount());

    while (maximum >= 10) {
        maximum /= 10;
        ++digits;
    }

    digits = qMax(2, digits);

    return 24 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

QTextBlock CodeEditor::firstVisibleBlock() const {
    return QPlainTextEdit::firstVisibleBlock();
}

QRectF CodeEditor::blockBoundingGeometry(const QTextBlock &block) const {
    return QPlainTextEdit::blockBoundingGeometry(block);
}

QRectF CodeEditor::blockBoundingRect(const QTextBlock &block) const {
    return QPlainTextEdit::blockBoundingRect(block);
}

QPointF CodeEditor::contentOffset() const {
    return QPlainTextEdit::contentOffset();
}

int CodeEditor::currentLineNumber() const {
    return textCursor().blockNumber();
}

bool CodeEditor::isCurrentLine(const QTextBlock &block) const {
    return block.isValid() && block.blockNumber() == currentLineNumber();
}

void CodeEditor::updateLineNumberAreaWidth() {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);

    updateLineNumberAreaGeometry();
}

void CodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> selections;

    if (!isReadOnly()) {
        QColor tint(theme::currentLine);
        tint.setAlpha(lineTint);

        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(tint);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }

    setExtraSelections(selections);
    lineNumberArea->update();
}

int CodeEditor::indentWidth() {
    return kIndentWidth;
}

QString CodeEditor::indentText() {
    return QString(indentWidth(), QLatin1Char(' '));
}

void CodeEditor::keyPressEvent(QKeyEvent *event) {
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    const bool control = modifiers.testFlag(Qt::ControlModifier);
    const bool shift = modifiers.testFlag(Qt::ShiftModifier);
    const bool alt = modifiers.testFlag(Qt::AltModifier);

    if (event->key() == Qt::Key_F12 && modifiers == Qt::NoModifier) {
        if (intelliSenseDirty) rebuildIntelliSense();
        const IntelliSenseDefinition target = definitionAtCursor();
        if (target.isValid()) {
            emit goToDefinitionRequested(target.path, target.line, target.column);
        }
        event->accept();
        return;
    }
    if (control && alt && event->key() == Qt::Key_Left) {
        emit returnFromDefinitionRequested();
        event->accept();
        return;
    }

    if (control && !alt && event->key() == Qt::Key_Space) {
        requestCompletions();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape &&
        (completionPopupVisible() || !ghostSuffix.isEmpty())) {
        suggestionDismissed = true;
        hideCompletions();
        event->accept();
        return;
    }
    if (completionPopupVisible()) {
        if (event->key() == Qt::Key_Up) {
            moveCompletionSelection(-1);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Down) {
            moveCompletionSelection(1);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
            acceptCompletion();
            event->accept();
            return;
        }
    }

    if (!isReadOnly()) {
        if (control && !alt && event->key() == Qt::Key_Slash) {
            toggleComment();
            return;
        }

        if (!control && !alt) {
            if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
                newlineKeepIndent();
                return;
            }

            if (event->key() == Qt::Key_Backtab ||
                (event->key() == Qt::Key_Tab && shift)) {
                outdent();
                return;
            }

            if (event->key() == Qt::Key_Tab) {
                if (!completionItems.isEmpty()) {
                    const int selected = completionPopupVisible()
                        ? completionList->currentRow() : 0;
                    const QString prefix = intelliSense.prefixAt(textCursor().position());
                    if (selected >= 0 && selected < completionItems.size() &&
                        completionItems.at(selected).name != prefix) {
                        acceptCompletion(selected);
                        event->accept();
                        return;
                    }
                }
                insertIndentation();
                return;
            }
        }
    }

    if (!isReadOnly() && !control && !alt && event->text() == QStringLiteral("}")) {
        QTextCursor cursor = textCursor();
        const QTextBlock block = cursor.block();
        const QString line = block.text();
        if (line.trimmed().isEmpty() &&
            cursor.position() == block.position() + line.length()) {
            const QString leading = leadingWhitespace(line);
            const int remove = qMin(indentWidth(), static_cast<int>(leading.length()));

            if (remove > 0) {
                cursor.beginEditBlock();
                cursor.movePosition(QTextCursor::StartOfBlock);
                cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, remove);
                cursor.removeSelectedText();
                cursor.endEditBlock();
                setTextCursor(cursor);
            }
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}

void CodeEditor::contextMenuEvent(QContextMenuEvent *event) {
    if (intelliSenseDirty) rebuildIntelliSense();
    QTextCursor targetCursor = cursorForPosition(event->pos());
    const IntelliSenseDefinition target = intelliSense.definitionAt(targetCursor.position());
    QMenu *menu = createStandardContextMenu();
    menu->addSeparator();
    QAction *definitionAction = menu->addAction(QStringLiteral("Go to Definition"));
    definitionAction->setEnabled(target.isValid());
    connect(definitionAction, &QAction::triggered, this, [this, target]() {
        emit goToDefinitionRequested(target.path, target.line, target.column);
    });
    menu->exec(event->globalPos());
    delete menu;
}

void CodeEditor::insertIndentation() {
    const QTextCursor cursor = textCursor();
    const QTextDocument *doc = this->document();

    const int startBlock = doc->findBlock(cursor.selectionStart()).blockNumber();
    const int endBlock = doc->findBlock(cursor.selectionEnd()).blockNumber();

    if (startBlock != endBlock) {
        indent();
        return;
    }

    const int column = cursor.positionInBlock();
    const int spaces = indentWidth() - (column % indentWidth());

    QTextCursor edit = cursor;
    edit.insertText(QString(spaces, QLatin1Char(' ')));
    setTextCursor(edit);
}

void CodeEditor::indent() {
    QList<BlockEdit> edits;

    for (const QTextBlock &block : selectedBlocks()) {
        if (block.text().trimmed().isEmpty()) {
            continue;
        }

        edits.append(
            BlockEdit{block.blockNumber(), 0, indentWidth(), true, indentText()}
        );
    }

    if (edits.isEmpty()) {
        return;
    }

    applyBlockEdits(edits);
}

void CodeEditor::outdent() {
    QList<BlockEdit> edits;

    for (const QTextBlock &block : selectedBlocks()) {
        const QString text = block.text();
        const int size = static_cast<int>(text.size());
        int count = 0;

        while (count < indentWidth() &&
               count < size &&
               text.at(count) == QLatin1Char(' ')) {
            ++count;
        }

        if (count == 0 && text.startsWith(QLatin1Char('\t'))) {
            count = 1;
        }

        if (count == 0) {
            continue;
        }

        edits.append(
            BlockEdit{block.blockNumber(), 0, count, false, QString()}
        );
    }

    if (edits.isEmpty()) {
        return;
    }

    applyBlockEdits(edits);
}

void CodeEditor::toggleComment() {
    QList<QTextBlock> lines;

    for (const QTextBlock &block : selectedBlocks()) {
        if (!block.text().trimmed().isEmpty()) {
            lines.append(block);
        }
    }

    if (lines.isEmpty()) {
        return;
    }

    const QString marker = QStringLiteral("//");

    bool uncomment = true;

    for (const QTextBlock &block : lines) {
        if (!block.text().trimmed().startsWith(marker)) {
            uncomment = false;
            break;
        }
    }

    QList<BlockEdit> edits;

    for (const QTextBlock &block : lines) {
        const QString text = block.text();
        const int offset = static_cast<int>(leadingWhitespace(text).size());

        if (uncomment) {
            const int length = text.mid(offset).startsWith(marker + " ") ? 3 : 2;
            edits.append(
                BlockEdit{block.blockNumber(), offset, length, false, QString()}
            );
        } else {
            edits.append(
                BlockEdit{
                    block.blockNumber(),
                    offset,
                    3,
                    true,
                    marker + " "
                }
            );
        }
    }

    applyBlockEdits(edits);
}

void CodeEditor::newlineKeepIndent() {
    QTextCursor cursor = textCursor();
    const QTextBlock block = cursor.block();

    const QString before = block.text().left(
        cursor.selectionStart() - block.position()
    );
    QString indent = leadingWhitespace(before);

    const QString trimmedBefore = before.trimmed();
    if (trimmedBefore.endsWith(QLatin1Char('{'))) {
        indent += indentText();
    }

    cursor.beginEditBlock();
    cursor.removeSelectedText();
    cursor.insertBlock();
    cursor.insertText(indent);
    cursor.endEditBlock();

    setTextCursor(cursor);
    ensureCursorVisible();
}

QList<QTextBlock> CodeEditor::selectedBlocks() const {
    QList<QTextBlock> blocks;

    const QTextCursor cursor = textCursor();
    const int start = cursor.selectionStart();
    const int end = cursor.selectionEnd();

    QTextBlock first = document()->findBlock(start);
    QTextBlock last = document()->findBlock(end);

    if (end > start &&
        end == last.position() &&
        last.blockNumber() > first.blockNumber()) {
        last = last.previous();
    }

    for (QTextBlock block = first; block.isValid(); block = block.next()) {
        blocks.append(block);

        if (block.blockNumber() == last.blockNumber()) {
            break;
        }
    }

    return blocks;
}

void CodeEditor::applyBlockEdits(const QList<BlockEdit> &edits) {
    const QTextCursor cursor = textCursor();
    const EditorPoint anchor = pointOf(cursor.anchor());
    const EditorPoint position = pointOf(cursor.position());

    QTextCursor edit = cursor;
    edit.beginEditBlock();

    for (const BlockEdit &change : edits) {
        const QTextBlock block = document()->findBlockByNumber(change.blockNumber);

        if (!block.isValid()) {
            continue;
        }

        QTextCursor line(block);
        line.setPosition(block.position() + change.offset);

        if (change.insertion) {
            line.insertText(change.text);
        } else {
            line.setPosition(
                block.position() + change.offset + change.length,
                QTextCursor::KeepAnchor
            );
            line.removeSelectedText();
        }
    }

    edit.endEditBlock();

    QTextCursor restored(document());
    restored.setPosition(positionOf(anchor, edits));
    restored.setPosition(positionOf(position, edits), QTextCursor::KeepAnchor);
    setTextCursor(restored);
}

CodeEditor::EditorPoint CodeEditor::pointOf(int position) const {
    const QTextBlock block = document()->findBlock(position);

    return EditorPoint{block.blockNumber(), position - block.position()};
}

int CodeEditor::mappedColumn(
    const QList<BlockEdit> &edits,
    int blockNumber,
    int column
) const {
    for (const BlockEdit &change : edits) {
        if (change.blockNumber != blockNumber) {
            continue;
        }

        if (change.insertion) {
            return column >= change.offset ? column + change.length : column;
        }

        if (column <= change.offset) {
            return column;
        }

        if (column >= change.offset + change.length) {
            return column - change.length;
        }

        return change.offset;
    }

    return column;
}

int CodeEditor::positionOf(
    const EditorPoint &point,
    const QList<BlockEdit> &edits
) const {
    const QTextBlock block = document()->findBlockByNumber(point.blockNumber);

    if (!block.isValid()) {
        return 0;
    }

    const int column = mappedColumn(edits, point.blockNumber, point.column);
    const int last = block.position() + block.length() - 1;

    return qMin(block.position() + column, last);
}

DocumentEditor::DocumentEditor(QWidget *parent)
    : CodeEditor(parent) {
    title = untitledName();

    applyLineHeight();
    document()->clearUndoRedoStacks();
    document()->setModified(false);

    connect(
        document(),
        &QTextDocument::modificationChanged,
        this,
        [this](bool modified) {
            dirty = modified;
            emit dirtyChanged(this);
        }
    );
}

TabCloseButton::TabCloseButton(QWidget *parent)
    : QToolButton(parent) {
    setObjectName(QStringLiteral("tab_close_button"));
    setIconSize(QSize(12, 12));
    setAutoRaise(true);
    setFixedSize(18, 18);
    setFocusPolicy(Qt::NoFocus);
    setToolTip(QStringLiteral("Close the document"));
    refresh();
}

void TabCloseButton::setDirty(bool changed) {
    dirty = changed;
    refresh();
}

void TabCloseButton::enterEvent(QEnterEvent *event) {
    hovered = true;
    refresh();
    QToolButton::enterEvent(event);
}

void TabCloseButton::leaveEvent(QEvent *event) {
    hovered = false;
    refresh();
    QToolButton::leaveEvent(event);
}

void TabCloseButton::refresh() {
    setIcon(showsDirtyMarker() ? icons::dirtyDot() : icons::close());
    setProperty("dirty", dirty);
}

AnimatedTabBar::AnimatedTabBar(QWidget *parent)
    : QTabBar(parent) {
    animation = new QVariantAnimation(this);
    animation->setDuration(180);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(
        animation,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant &value) {
            indicator = value.toRect();
            update();
        }
    );

    connect(this, &QTabBar::currentChanged, this, [this](int index) {
        animateTo(index);
    });
}

QRect AnimatedTabBar::indicatorTarget() const {
    const int index = currentIndex();

    return index < 0 ? QRect() : tabRect(index);
}

bool AnimatedTabBar::isAnimating() const {
    return animation->state() == QAbstractAnimation::Running;
}

void AnimatedTabBar::animateTo(int index) {
    if (index < 0) {
        return;
    }

    const QRect target = tabRect(index);

    if (!target.isValid()) {
        return;
    }

    if (!indicator.isValid()) {
        indicator = target;
        update();
        return;
    }

    animation->stop();

    if (indicator == target) {
        update();
        return;
    }

    animation->setStartValue(indicator);
    animation->setEndValue(target);
    animation->start();
}

void AnimatedTabBar::snapToCurrent() {
    animation->stop();

    const int index = currentIndex();

    if (index < 0) {
        indicator = QRect();
        update();
        return;
    }

    const QRect target = tabRect(index);

    if (target.isValid()) {
        indicator = target;
        update();
    }
}

void AnimatedTabBar::resizeEvent(QResizeEvent *event) {
    QTabBar::resizeEvent(event);
    snapToCurrent();
}

void AnimatedTabBar::tabLayoutChange() {
    QTabBar::tabLayoutChange();

    if (!isAnimating()) {
        snapToCurrent();
    }
}

void AnimatedTabBar::paintEvent(QPaintEvent *event) {
    QTabBar::paintEvent(event);

    const int index = currentIndex();

    if (index < 0) {
        return;
    }

    QRect rect = indicator;

    if (!rect.isValid()) {
        rect = tabRect(index);
    }

    if (!rect.isValid()) {
        return;
    }

    const QRect line(
        rect.left() + 10,
        rect.y() + rect.height() - 3,
        qMax(0, rect.width() - 20),
        2
    );

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(theme::accent));
    painter.drawRoundedRect(line, 1, 1);
}

DocumentTabs::DocumentTabs(QWidget *parent)
    : QTabWidget(parent) {
    setTabBar(new AnimatedTabBar(this));
    setDocumentMode(true);
    setMovable(true);
    setElideMode(Qt::ElideRight);
    setMinimumSize(320, 200);
}

void DocumentTabs::tabInserted(int index) {
    QTabWidget::tabInserted(index);
    emit tabCountChanged();
}

void DocumentTabs::tabRemoved(int index) {
    QTabWidget::tabRemoved(index);
    emit tabCountChanged();
}

WelcomePage::WelcomePage(QWidget *parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("welcome"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *strip = new QWidget(this);
    strip->setObjectName(QStringLiteral("welcome_strip"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    auto *stripLayout = new QHBoxLayout(strip);
    stripLayout->setContentsMargins(0, 0, 0, 0);
    stripLayout->setSpacing(0);

    auto *stripBar = new AnimatedTabBar(strip);
    stripBar->setDocumentMode(true);
    stripBar->addTab(QStringLiteral("welcome"));
    stripBar->setExpanding(false);
    stripTabBar = stripBar;

    stripCloseButton = new TabCloseButton(stripBar);
    stripCloseButton->setToolTip(QStringLiteral("Close the welcome tab"));
    stripBar->setTabButton(0, QTabBar::RightSide, stripCloseButton);
    connect(stripCloseButton, &QToolButton::clicked, this, &WelcomePage::closeRequested);

    stripLayout->addWidget(stripBar);
    stripLayout->addStretch(1);
    root->addWidget(strip);

    auto *body = new QWidget(this);
    root->addWidget(body, 1);

    auto *outer = new QVBoxLayout(body);
    outer->setContentsMargins(64, 0, 40, 0);
    outer->setSpacing(0);
    outer->addStretch(3);

    auto *brand = new QHBoxLayout;
    brand->setSpacing(12);

    auto *iconLabel = new QLabel(this);

    QPixmap logo(studio::iconPath());
    iconLabel->setPixmap(
        logo.isNull()
            ? icons::pebble()
            : logo.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    iconLabel->setFixedSize(32, 32);
    brand->addWidget(iconLabel);

    auto *title = new QLabel(QStringLiteral("Pebble Studio"), this);
    title->setObjectName(QStringLiteral("welcome_title"));
    brand->addWidget(title);
    brand->addStretch(1);
    outer->addLayout(brand);

    auto *subtitle = new QLabel(
        QStringLiteral("A programming environment for Pebble."),
        this
    );
    subtitle->setObjectName(QStringLiteral("muted"));
    outer->addWidget(subtitle);
    outer->addSpacing(28);

    auto *start = new QLabel(QStringLiteral("Get Started"), this);
    start->setObjectName(QStringLiteral("section"));
    outer->addWidget(start);
    outer->addSpacing(6);

    struct Step {
        QString label;
        QString shortcut;
        QPushButton **button;
    };

    const Step steps[] = {
        {QStringLiteral("New File"), QStringLiteral("Ctrl+N"), &newFileButtonWidget},
        {QStringLiteral("Open File…"), QStringLiteral("Ctrl+O"), &openFileButtonWidget},
        {QStringLiteral("Open Example…"), QString(), &openExampleButtonWidget}
    };

    for (const Step &step : steps) {
        auto *row = new QHBoxLayout;

        *step.button = addLink(this, step.label);
        (*step.button)->setFixedWidth(160);

        auto *hint = new QLabel(step.shortcut, this);
        hint->setObjectName(QStringLiteral("hint"));

        row->addWidget(*step.button);
        row->addWidget(hint);
        row->addStretch(1);
        outer->addLayout(row);
    }

    outer->addSpacing(26);

    auto *recentHeading = new QLabel(QStringLiteral("Recent Files"), this);
    recentHeading->setObjectName(QStringLiteral("section"));
    outer->addWidget(recentHeading);
    outer->addSpacing(6);

    recentHost = new QWidget(this);
    auto *recentLayout = new QVBoxLayout(recentHost);
    recentLayout->setContentsMargins(0, 0, 0, 0);
    recentLayout->setSpacing(0);
    outer->addWidget(recentHost);

    outer->addStretch(4);

    connect(newFileButtonWidget, &QPushButton::clicked, this, [this]() {
        emit newFileRequested();
    });
    connect(openFileButtonWidget, &QPushButton::clicked, this, [this]() {
        emit openFileRequested();
    });
    connect(openExampleButtonWidget, &QPushButton::clicked, this, [this]() {
        emit openExampleRequested();
    });

    setRecentFiles(QStringList());
}

QPushButton *WelcomePage::addLink(QWidget *parent, const QString &label) {
    auto *button = new QPushButton(label, parent);
    button->setObjectName(QStringLiteral("link"));
    button->setCursor(Qt::PointingHandCursor);

    return button;
}

bool WelcomePage::showsNoRecentFiles() const {
    return recentPaths.isEmpty();
}

void WelcomePage::setRecentFiles(const QStringList &paths) {
    recentPaths = paths;
    recentButtonList.clear();

    QLayout *layout = recentHost->layout();

    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }

        delete item;
    }

    if (paths.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("No Recent Files Yet"), recentHost);
        empty->setObjectName(QStringLiteral("muted"));
        layout->addWidget(empty);
        return;
    }

    for (const QString &path : paths) {
        const QFileInfo info(path);

        auto *row = new QWidget(recentHost);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        QPushButton *button = addLink(row, info.fileName());
        button->setToolTip(path);

        auto *location = new QLabel(info.absolutePath(), row);
        location->setObjectName(QStringLiteral("hint"));
        location->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        rowLayout->addWidget(button);
        rowLayout->addWidget(location, 1);
        layout->addWidget(row);

        recentButtonList.append(button);

        connect(button, &QPushButton::clicked, this, [this, path]() {
            emit recentFileRequested(path);
        });
    }
}

FindReplaceBar::FindReplaceBar(QWidget *parent)
    : QWidget(parent) {
    setObjectName("findbar");

    findFieldWidget = new QLineEdit(this);
    findFieldWidget->setObjectName("find_input");
    findFieldWidget->setPlaceholderText("find");

    replaceFieldWidget = new QLineEdit(this);
    replaceFieldWidget->setObjectName("replace_input");
    replaceFieldWidget->setPlaceholderText("replace");

    matchCaseWidget = new QToolButton(this);
    matchCaseWidget->setObjectName("match_case");
    matchCaseWidget->setText("Aa");
    matchCaseWidget->setCheckable(true);
    matchCaseWidget->setToolTip("Match Case");

    previousButtonWidget = new QPushButton(QStringLiteral("↑"), this);
    previousButtonWidget->setToolTip("Previous (Shift+F3)");

    nextButtonWidget = new QPushButton(QStringLiteral("↓"), this);
    nextButtonWidget->setToolTip("Next (F3)");

    replaceButtonWidget = new QPushButton("Replace", this);
    replaceButtonWidget->setToolTip("Replace the current match");

    replaceAllButtonWidget = new QPushButton("Replace All", this);
    replaceAllButtonWidget->setToolTip("Replace every match");

    closeButtonWidget = new QPushButton(QStringLiteral("×"), this);
    closeButtonWidget->setToolTip("Close (Esc)");

    for (QPushButton *bar : {previousButtonWidget, nextButtonWidget,
                             replaceButtonWidget, replaceAllButtonWidget,
                             closeButtonWidget}) {
        bar->setObjectName("barbutton");
    }

    statusWidget = new QLabel(this);
    statusWidget->setObjectName("hint");
    statusWidget->setMinimumWidth(90);

    auto *top = new QHBoxLayout();
    top->addWidget(findFieldWidget, 1);
    top->addWidget(matchCaseWidget);
    top->addWidget(previousButtonWidget);
    top->addWidget(nextButtonWidget);
    top->addWidget(statusWidget);
    top->addWidget(closeButtonWidget);

    replaceRowWidget = new QWidget(this);
    auto *replaceLayout = new QHBoxLayout(replaceRowWidget);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->addWidget(replaceFieldWidget, 1);
    replaceLayout->addWidget(replaceButtonWidget);
    replaceLayout->addWidget(replaceAllButtonWidget);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);
    layout->addLayout(top);
    layout->addWidget(replaceRowWidget);

    connect(
        findFieldWidget,
        &QLineEdit::textChanged,
        this,
        &FindReplaceBar::findTextChanged
    );

    connect(
        findFieldWidget,
        &QLineEdit::returnPressed,
        this,
        [this]() {
            emit findSubmitted(findFieldWidget->text());
        }
    );

    connect(
        replaceFieldWidget,
        &QLineEdit::returnPressed,
        this,
        [this]() {
            emit replaceSubmitted(replaceFieldWidget->text());
        }
    );

    connect(
        matchCaseWidget,
        &QToolButton::toggled,
        this,
        &FindReplaceBar::matchCaseChanged
    );

    connect(
        previousButtonWidget,
        &QPushButton::clicked,
        this,
        [this]() {
            emit findPreviousRequested();
        }
    );

    connect(
        nextButtonWidget,
        &QPushButton::clicked,
        this,
        [this]() {
            emit findNextRequested();
        }
    );

    connect(
        replaceButtonWidget,
        &QPushButton::clicked,
        this,
        [this]() {
            emit replaceRequested(replaceFieldWidget->text());
        }
    );

    connect(
        replaceAllButtonWidget,
        &QPushButton::clicked,
        this,
        [this]() {
            emit replaceAllRequested(replaceFieldWidget->text());
        }
    );

    connect(
        closeButtonWidget,
        &QPushButton::clicked,
        this,
        &FindReplaceBar::closeRequested
    );

    replaceRowWidget->setVisible(false);
    hide();
}

void FindReplaceBar::open(bool withReplace) {
    opened = true;
    replaceRowWidget->setVisible(withReplace);
    show();
}

void FindReplaceBar::closeBar() {
    opened = false;
    replaceRowWidget->setVisible(false);
    hide();
}

QString FindReplaceBar::findText() const {
    return findFieldWidget->text();
}

void FindReplaceBar::setFindText(const QString &text) {
    findFieldWidget->setText(text);
}

QString FindReplaceBar::replaceText() const {
    return replaceFieldWidget->text();
}

void FindReplaceBar::setReplaceText(const QString &text) {
    replaceFieldWidget->setText(text);
}

bool FindReplaceBar::matchCase() const {
    return matchCaseWidget->isChecked();
}

void FindReplaceBar::setMatchCase(bool on) {
    matchCaseWidget->setChecked(on);
}

QString FindReplaceBar::status() const {
    return statusWidget->text();
}

void FindReplaceBar::setStatus(const QString &text) {
    statusWidget->setText(text);
}

bool FindReplaceBar::replaceRowVisible() const {
    return replaceRowWidget->isVisible();
}

void FindReplaceBar::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        emit closeRequested();
        return;
    }

    QWidget::keyPressEvent(event);
}

ProjectTree::ProjectTree(QWidget *parent)
    : QTreeWidget(parent) {
}

void ProjectTree::showEvent(QShowEvent *event) {
    QTreeWidget::showEvent(event);

    emit refreshRequested();
}

MainWindow::MainWindow(
    const QString &organisation,
    const QString &application,
    QWidget *parent
)
    : QMainWindow(parent),
      settingsOrganisation(organisation),
      settingsApplication(application) {
    projectManifestWatcher = new QFileSystemWatcher(this);
    connect(projectManifestWatcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &) { refreshIntelliSenseProjectPackages(); });
    setWindowTitle("Pebble Studio");

    QSettings settings(settingsOrganisation, settingsApplication);
    resize(settings.value("size", QSize(1280, 820)).toSize());

    if (settings.contains("projectDir")) {
        projectDir = settings.value("projectDir").toString();
    }

    {
        const QIcon icon = studio::icon();

        if (!icon.isNull()) {
            setWindowIcon(icon);
        } else {
            setWindowIcon(QIcon(icons::pebble()));
        }
    }

    backgroundAssetName = settings.value("wallpaper_name").toString();

    if (backgroundAssetName.isEmpty()) {
        backgroundFile = settings.contains("background_path")
            ? settings.value("background_path").toString()
            : defaultBackgroundPath();
    }
    darkness = qBound(
        0,
        settings.value("background_dim", defaultBackgroundDarkness).toInt(),
        245
    );
    bool blurValueValid = false;
    const int storedBlur = settings.value(
        "glass_blur", theme::defaultGlassBlur
    ).toInt(&blurValueValid);
    glassBlur = blurValueValid ? qBound(0, storedBlur, 36)
                               : theme::defaultGlassBlur;
    bool opacityValueValid = false;
    const int storedOpacity = settings.value(
        "glass_opacity", theme::defaultGlassTintOpacity
    ).toInt(&opacityValueValid);
    glassOpacity = opacityValueValid
        ? qBound(0, storedOpacity, 92)
        : theme::defaultGlassTintOpacity;
    const QColor storedTint(settings.value(
        "glass_tint",
        QString::fromLatin1(theme::glassTint)
    ).toString());
    glassTint = storedTint.isValid() ? storedTint : QColor(theme::glassTint);
    applyTheme();

    splitter = new QSplitter(this);
    auto *explorerSurface = new GlassSurface(&glassBackdrop, splitter);
    explorerSurface->setObjectName("explorer_glass");
    auto *explorerLayout = new QVBoxLayout(explorerSurface);
    explorerLayout->setContentsMargins(0, 0, 0, 0);
    explorerLayout->setSpacing(0);

    tree = new ProjectTree(explorerSurface);
    tree->setObjectName("project_tree");
    tree->setHeaderLabel("Explorer");
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    tree->setIndentation(14);
    tree->setUniformRowHeights(true);
    tree->setTextElideMode(Qt::ElideMiddle);
    tree->setAutoFillBackground(false);
    tree->setBackgroundRole(QPalette::NoRole);
    tree->setAttribute(Qt::WA_TranslucentBackground);
    tree->viewport()->setAutoFillBackground(false);
    tree->viewport()->setBackgroundRole(QPalette::NoRole);
    tree->viewport()->setAttribute(Qt::WA_NoSystemBackground);
    tree->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    QPalette treePalette = tree->palette();
    treePalette.setColor(QPalette::Base, QColor(0, 0, 0, 0));
    treePalette.setColor(QPalette::Window, QColor(0, 0, 0, 0));
    tree->setPalette(treePalette);
    tree->viewport()->setPalette(treePalette);
    explorerLayout->addWidget(tree);
    connect(
        tree,
        &QWidget::customContextMenuRequested,
        this,
        &MainWindow::treeContextMenu
    );

    connect(
        tree,
        &ProjectTree::refreshRequested,
        this,
        &MainWindow::refreshProjectTree
    );

    editorPane = new GlassSurface(&glassBackdrop, splitter);
    editorPane->setObjectName("editor_glass");
    auto *editorLayout = new QVBoxLayout(editorPane);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    findBar = new FindReplaceBar(editorPane);
    editorLayout->addWidget(findBar);

    editorStack = new QStackedWidget(editorPane);

    auto *documentTabs = new DocumentTabs(editorStack);
    tabs = documentTabs;
    tabs->setObjectName("tabs");
    tabs->setTabsClosable(false);
    tabs->setDocumentMode(true);

    welcome = new WelcomePage(editorStack);

    blankPage = new QWidget(editorStack);
    blankPage->setObjectName("blank_page");
    blankPage->setAttribute(Qt::WA_StyledBackground, true);

    editorStack->addWidget(tabs);
    editorStack->addWidget(welcome);
    editorStack->addWidget(blankPage);
    editorLayout->addWidget(editorStack);

    connect(documentTabs, &DocumentTabs::tabCountChanged, this, [this]() {
        welcomeClosed = false;
        refreshWelcome();
    });
    connect(welcome, &WelcomePage::closeRequested, this, [this]() {
        welcomeClosed = true;
        refreshWelcome();
    });
    connect(welcome, &WelcomePage::newFileRequested, this, &MainWindow::newFile);
    connect(welcome, &WelcomePage::openFileRequested, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Open File",
            defaultDirectory(),
            pebbleFileFilter()
        );

        if (!path.isEmpty()) {
            openPath(path);
        }
    });
    connect(
        welcome,
        &WelcomePage::openExampleRequested,
        this,
        &MainWindow::openExample
    );
    connect(
        welcome,
        &WelcomePage::recentFileRequested,
        this,
        &MainWindow::openRecentFile
    );

    output = new QPlainTextEdit(this);
    output->setReadOnly(true);
    output->setFont(studioCodeFont(10));
    output->setObjectName("output");
    output->setAutoFillBackground(false);
    output->setAttribute(Qt::WA_TranslucentBackground);
    output->viewport()->setAutoFillBackground(false);
    output->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    QPalette outputPalette = output->palette();
    outputPalette.setColor(QPalette::Base, QColor(0, 0, 0, 0));
    outputPalette.setColor(QPalette::Window, QColor(0, 0, 0, 0));
    output->setPalette(outputPalette);
    output->viewport()->setPalette(outputPalette);
    output->document()->setMaximumBlockCount(20000);
    output->document()->setDocumentMargin(12);
    new OutputHighlighter(output->document());

    auto *bottom = new GlassSurface(&glassBackdrop, this);
    bottom->setObjectName("bottom_panel");
    bottom->setAttribute(Qt::WA_StyledBackground, true);
    bottomPanel = bottom;
    auto *bottomLayout = new QVBoxLayout(bottom);
    bottomLayout->setContentsMargins(6, 6, 6, 6);
    bottomLayout->setSpacing(6);

    auto *outputHeader = new QWidget(bottom);
    auto *outputHeaderLayout = new QHBoxLayout(outputHeader);
    outputHeaderLayout->setContentsMargins(0, 0, 0, 0);

    auto *outputTitle = new QLabel("Output", outputHeader);
    outputTitle->setObjectName("panel_title");
    outputHeaderLayout->addWidget(outputTitle);
    outputHeaderLayout->addStretch(1);

    clearOutputButtonWidget = new QPushButton("Clear", outputHeader);
    clearOutputButtonWidget->setObjectName("clear_output_button");
    clearOutputButtonWidget->setToolTip("Clear the output");
    outputHeaderLayout->addWidget(clearOutputButtonWidget);

    bottomLayout->addWidget(outputHeader);
    bottomLayout->addWidget(output);

    programInput = new QLineEdit(bottom);
    programInput->setObjectName("program_input");
    programInput->setFont(studioCodeFont(10));
    programInput->setEnabled(false);
    programInput->setPlaceholderText(
        QStringLiteral("Program input — available while a program is running")
    );
    bottomLayout->addWidget(programInput);
    connect(
        programInput,
        &QLineEdit::returnPressed,
        this,
        &MainWindow::submitProgramInput
    );

    diagnosticsTitleLabel = new QLabel(bottom);
    diagnosticsTitleLabel->setObjectName("diagnostics_title");
    bottomLayout->addWidget(diagnosticsTitleLabel);

    diagnostics = new QListWidget(this);
    diagnostics->setObjectName("diagnostics");
    diagnostics->setAutoFillBackground(false);
    diagnostics->setAttribute(Qt::WA_TranslucentBackground);
    diagnostics->viewport()->setAutoFillBackground(false);
    diagnostics->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    QPalette diagnosticsPalette = diagnostics->palette();
    diagnosticsPalette.setColor(QPalette::Base, QColor(0, 0, 0, 0));
    diagnosticsPalette.setColor(QPalette::Window, QColor(0, 0, 0, 0));
    diagnostics->setPalette(diagnosticsPalette);
    diagnostics->viewport()->setPalette(diagnosticsPalette);
    bottomLayout->addWidget(diagnostics);

    bottomLayout->setStretch(0, 0);
    bottomLayout->setStretch(1, 3);
    bottomLayout->setStretch(2, 0);
    bottomLayout->setStretch(3, 0);
    bottomLayout->setStretch(4, 2);
    output->setMinimumHeight(80);
    diagnostics->setMinimumHeight(56);

    connect(
        clearOutputButtonWidget,
        &QPushButton::clicked,
        this,
        &MainWindow::clearOutput
    );

    connect(
        diagnostics,
        &QListWidget::itemDoubleClicked,
        this,
        &MainWindow::navigateDiagnostic
    );

    activityBarWidget = new GlassSurface(&glassBackdrop, this);
    activityBarWidget->setObjectName("activitybar");
    activityBarWidget->setFixedWidth(44);

    auto *activityLayout = new QVBoxLayout(activityBarWidget);
    activityLayout->setContentsMargins(0, 0, 0, 0);
    activityLayout->setSpacing(0);

    explorerButton = new QToolButton(activityBarWidget);
    explorerButton->setObjectName("explorer_button");
    explorerButton->setIcon(icons::explorer());
    explorerButton->setIconSize(QSize(22, 22));
    explorerButton->setToolTip("Explorer (Ctrl+B)");
    explorerButton->setCheckable(true);
    explorerButton->setAutoRaise(true);
    explorerButton->setFixedSize(44, 44);
    activityLayout->addWidget(explorerButton);

    outputButton = new QToolButton(activityBarWidget);
    outputButton->setObjectName("output_button");
    outputButton->setIcon(icons::output());
    outputButton->setIconSize(QSize(22, 22));
    outputButton->setToolTip("Output (Ctrl+J)");
    outputButton->setCheckable(true);
    outputButton->setAutoRaise(true);
    outputButton->setFixedSize(44, 44);
    activityLayout->addWidget(outputButton);

    activityLayout->addStretch(1);

    connect(
        explorerButton,
        &QToolButton::toggled,
        this,
        &MainWindow::setExplorerVisible
    );

    connect(
        outputButton,
        &QToolButton::toggled,
        this,
        &MainWindow::setOutputPaneVisible
    );

    mainSplitter = new QSplitter(Qt::Vertical, this);
    mainSplitter->addWidget(splitter);
    mainSplitter->addWidget(bottom);

    mainSplitter->setHandleWidth(5);
    splitter->setHandleWidth(5);
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({560, 200});

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({250, 1000});
    tree->setMinimumWidth(150);

    auto *central = new QWidget(this);
    auto *centralLayout = new QHBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(activityBarWidget);
    centralLayout->addWidget(mainSplitter, 1);
    setCentralWidget(central);

    auto *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New File", this, &MainWindow::newFile);
    fileMenu->addAction("Open File…", this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Open File",
            defaultDirectory(),
            pebbleFileFilter()
        );

        if (!path.isEmpty()) {
            openPath(path);
        }
    });
    fileMenu->addAction("Open Example…", this, &MainWindow::openExample);
    fileMenu->addAction("Open Project…", this, &MainWindow::openProject);
    fileMenu->addAction("Save", this, &MainWindow::saveCurrent);
    fileMenu->addAction("Save As…", this, &MainWindow::saveAsCurrent);
    fileMenu->addSeparator();

    recentFilesMenuWidget = fileMenu->addMenu("Recent Files");
    recentFilesMenuWidget->setEnabled(false);

    fileMenu->addSeparator();
    fileMenu->addAction("Close Tab", this, [this]() {
        if (tabs->count() > 0) {
            closeDocument(tabs->currentIndex());
        }
    });
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    auto *editMenu = menuBar()->addMenu("&Edit");

    undoAction = new QAction("Undo", tabs);
    undoAction->setShortcuts({QKeySequence("Ctrl+Z")});
    undoAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(undoAction, &QAction::triggered, this, &MainWindow::undoActiveDocument);
    editMenu->addAction(undoAction);
    tabs->addAction(undoAction);

    redoAction = new QAction("Redo", tabs);
    redoAction->setShortcuts({QKeySequence("Ctrl+Shift+Z"), QKeySequence("Ctrl+Y")});
    redoAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(redoAction, &QAction::triggered, this, &MainWindow::redoActiveDocument);
    editMenu->addAction(redoAction);
    tabs->addAction(redoAction);

    editMenu->addSeparator();

    selectAllAction = new QAction("Select All", tabs);
    selectAllAction->setShortcut(QKeySequence("Ctrl+A"));
    selectAllAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(
        selectAllAction,
        &QAction::triggered,
        this,
        &MainWindow::selectAllActiveDocument
    );
    editMenu->addAction(selectAllAction);
    tabs->addAction(selectAllAction);

    editMenu->addSeparator();

    editMenu->addAction("Find", this, [this]() {
        openFindBar(false);
    });
    editMenu->addAction("Replace", this, [this]() {
        openFindBar(true);
    });
    editMenu->addAction("Find Next", this, &MainWindow::findNext);
    editMenu->addAction("Find Previous", this, &MainWindow::findPrevious);

    nextTabAction = new QAction("Next Tab", this);
    nextTabAction->setShortcut(QKeySequence("Ctrl+PgDown"));
    connect(nextTabAction, &QAction::triggered, this, &MainWindow::nextDocument);
    addAction(nextTabAction);

    previousTabAction = new QAction("Previous Tab", this);
    previousTabAction->setShortcut(QKeySequence("Ctrl+PgUp"));
    connect(
        previousTabAction,
        &QAction::triggered,
        this,
        &MainWindow::previousDocument
    );
    addAction(previousTabAction);

    auto *viewMenu = menuBar()->addMenu("&View");

    explorerAction = viewMenu->addAction("Explorer");
    explorerAction->setCheckable(true);
    explorerAction->setShortcut(QKeySequence("Ctrl+B"));
    connect(
        explorerAction,
        &QAction::toggled,
        this,
        &MainWindow::setExplorerVisible
    );

    outputAction = viewMenu->addAction("Output");
    outputAction->setCheckable(true);
    outputAction->setShortcut(QKeySequence("Ctrl+J"));
    connect(
        outputAction,
        &QAction::toggled,
        this,
        &MainWindow::setOutputPaneVisible
    );

    viewMenu->addSeparator();
    viewMenu->addAction("Clear Output", this, &MainWindow::clearOutput);

    viewMenu->addSeparator();
    auto *appearanceMenu = viewMenu->addMenu("Appearance");
    appearanceMenu->addAction(
        "Syntax Colours…",
        this,
        &MainWindow::chooseSyntaxColours
    );
    appearanceMenu->addSeparator();
    appearanceMenu->addAction(
        "Configure Glass…",
        this,
        &MainWindow::configureGlassAppearance
    );
    appearanceMenu->addSeparator();
    appearanceMenu->addAction(
        "Wallpaper…",
        this,
        &MainWindow::chooseWallpaper
    );
    appearanceMenu->addAction(
        "Choose Background Image…",
        this,
        &MainWindow::chooseBackground
    );
    appearanceMenu->addAction(
        "Background Darkness…",
        this,
        &MainWindow::chooseBackgroundDarkness
    );
    appearanceMenu->addSeparator();
    appearanceMenu->addAction("Remove Background Image", this, [this]() {
        setBackgroundImage(QString());
    });
    appearanceMenu->addAction("Restore Default Background", this, [this]() {
        setBackgroundImage(defaultBackgroundPath());
    });

    auto *buildMenu = menuBar()->addMenu("&Run");
    buildAction = buildMenu->addAction("Build", this, &MainWindow::build);
    buildFileAction = buildMenu->addAction("Build File", this, &MainWindow::buildCurrentFile);
    buildMenu->addSeparator();
    runAction = buildMenu->addAction("Run", this, &MainWindow::runProject);
    runFileAction = buildMenu->addAction("Run File", this, &MainWindow::runCurrentFile);
    stopAction = buildMenu->addAction("Stop", this, &MainWindow::stopProgram);
    stopAction->setEnabled(false);

    {
        QFont primary = menuBar()->font();
        primary.setBold(true);

        runAction->setFont(primary);
        runFileAction->setFont(primary);
        stopAction->setFont(primary);
    }

    auto *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction(
        "Pebble Language Ref",
        this,
        &MainWindow::showLanguageReference
    );
    helpMenu->addAction("About", this, &MainWindow::showAbout);

    {
        QAction *runEntry = buildMenu->menuAction();
        QAction *viewEntry = viewMenu->menuAction();
        menuBar()->removeAction(runEntry);
        menuBar()->insertAction(viewEntry, runEntry);
    }

    menuBar()->addSeparator();
    menuBar()->addAction(runFileAction);
    menuBar()->addAction(stopAction);

    connect(
        tree,
        &QTreeWidget::itemDoubleClicked,
        this,
        &MainWindow::openTreeFile
    );
    connect(
        tabs,
        &QTabWidget::tabCloseRequested,
        this,
        [this](int index) {
            closeDocument(index);
        }
    );

    connect(
        tabs,
        &QTabWidget::currentChanged,
        this,
        &MainWindow::activeDocumentChanged
    );

    connect(
        findBar,
        &FindReplaceBar::findNextRequested,
        this,
        &MainWindow::findNext
    );

    connect(
        findBar,
        &FindReplaceBar::findPreviousRequested,
        this,
        &MainWindow::findPrevious
    );

    connect(
        findBar,
        &FindReplaceBar::replaceRequested,
        this,
        &MainWindow::replace
    );

    connect(
        findBar,
        &FindReplaceBar::replaceAllRequested,
        this,
        &MainWindow::replaceAll
    );

    connect(
        findBar,
        &FindReplaceBar::matchCaseChanged,
        this,
        &MainWindow::matchCaseToggled
    );

    connect(
        findBar,
        &FindReplaceBar::closeRequested,
        this,
        &MainWindow::closeFindBar
    );

    connect(
        findBar,
        &FindReplaceBar::findTextChanged,
        this,
        &MainWindow::findTextEdited
    );

    connect(
        findBar,
        &FindReplaceBar::findSubmitted,
        this,
        &MainWindow::findSubmitted
    );

    connect(
        findBar,
        &FindReplaceBar::replaceSubmitted,
        this,
        &MainWindow::replaceSubmitted
    );

    auto *newShortcut = new QShortcut(QKeySequence("Ctrl+N"), this);
    connect(newShortcut, &QShortcut::activated, this, &MainWindow::newFile);

    auto *openShortcut = new QShortcut(QKeySequence("Ctrl+O"), this);
    connect(openShortcut, &QShortcut::activated, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Open File",
            defaultDirectory(),
            pebbleFileFilter()
        );

        if (!path.isEmpty()) {
            openPath(path);
        }
    });

    auto *saveShortcut = new QShortcut(QKeySequence("Ctrl+S"), this);
    connect(saveShortcut, &QShortcut::activated, this, &MainWindow::saveCurrent);

    auto *saveAsShortcut = new QShortcut(QKeySequence("Ctrl+Shift+S"), this);
    connect(saveAsShortcut, &QShortcut::activated, this, &MainWindow::saveAsCurrent);

    auto *closeShortcut = new QShortcut(QKeySequence("Ctrl+W"), this);
    connect(closeShortcut, &QShortcut::activated, this, [this]() {
        if (tabs->count() > 0) {
            closeDocument(tabs->currentIndex());
        }
    });

    auto *stopShortcut = new QShortcut(QKeySequence("Shift+F5"), this);
    connect(stopShortcut, &QShortcut::activated, this, &MainWindow::stopProgram);

    auto *runFileShortcut = new QShortcut(QKeySequence("F5"), this);
    connect(runFileShortcut, &QShortcut::activated, this, &MainWindow::runCurrentFile);

    auto *findShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        openFindBar(false);
    });

    auto *replaceShortcut = new QShortcut(QKeySequence("Ctrl+H"), this);
    connect(replaceShortcut, &QShortcut::activated, this, [this]() {
        openFindBar(true);
    });

    auto *findNextShortcut = new QShortcut(QKeySequence("F3"), this);
    connect(findNextShortcut, &QShortcut::activated, this, &MainWindow::findNext);

    auto *findPreviousShortcut = new QShortcut(QKeySequence("Shift+F3"), this);
    connect(
        findPreviousShortcut,
        &QShortcut::activated,
        this,
        &MainWindow::findPrevious
    );

    escapeShortcut = new QShortcut(QKeySequence("Esc"), this);
    connect(escapeShortcut, &QShortcut::activated, this, &MainWindow::closeFindBar);
    escapeShortcut->setEnabled(false);

    statusDocumentLabel = new QLabel(this);
    statusDocumentLabel->setObjectName("document");
    statusCursorLabel = new QLabel(this);
    statusCursorLabel->setObjectName("cursor");
    statusStateLabel = new QLabel(this);
    statusStateLabel->setObjectName("state");
    statusEncodingLabel = new QLabel(QStringLiteral("UTF-8"), this);
    statusEncodingLabel->setObjectName("encoding");
    statusBar()->addPermanentWidget(statusDocumentLabel);
    statusBar()->addPermanentWidget(statusCursorLabel);
    statusBar()->addPermanentWidget(statusEncodingLabel);
    statusBar()->addPermanentWidget(statusStateLabel);
    statusEncodingLabel->setVisible(false);

    buildProcess = new QProcess(this);
    runProcess = new QProcess(this);
    connectProcess(buildProcess, ProcessKind::Build);
    connectProcess(runProcess, ProcessKind::Run);

    killTimer = new QTimer(this);
    killTimer->setSingleShot(true);
    killTimer->setInterval(1500);
    connect(killTimer, &QTimer::timeout, this, &MainWindow::killProcess);

    explorerShown = settings.value("explorerShown", true).toBool();
    outputShown = settings.value("outputShown", true).toBool();

    if (settings.contains("splitterState")) {
        splitter->restoreState(settings.value("splitterState").toByteArray());
    }

    if (settings.contains("mainSplitterState")) {
        mainSplitter->restoreState(
            settings.value("mainSplitterState").toByteArray()
        );
    }

    loadSyntaxColours();

    loadRecentFiles();

    const QStringList openFiles = settings.value("openFiles").toStringList();

    restoringSession = true;

    for (const QString &file : openFiles) {
        if (QFile::exists(file)) {
            openPath(file);
        }
    }

    restoringSession = false;

    restoreProject();

    updateEditActions();

    tree->setVisible(explorerShown);
    bottomPanel->setVisible(outputShown);
    syncWorkspaceActions();

    updateDiagnosticsTitle();

    const QMap<QString, QString> hints = {
        {"New File", "Ctrl+N"}, {"Open File…", "Ctrl+O"}, {"Save", "Ctrl+S"},
        {"Save As…", "Ctrl+Shift+S"}, {"Close Tab", "Ctrl+W"},
        {"Find", "Ctrl+F"}, {"Replace", "Ctrl+H"}, {"Find Next", "F3"},
        {"Find Previous", "Shift+F3"}, {"Run File", "F5"}, {"Stop", "Shift+F5"}
    };

    const auto menus = menuBar()->findChildren<QMenu *>();

    for (QMenu *menu : menus) {
        for (QAction *entry : menu->actions()) {
            if (hints.contains(entry->text()) && entry->shortcut().isEmpty()) {
                entry->setShortcut(QKeySequence(hints.value(entry->text())));
                entry->setShortcutContext(Qt::WidgetShortcut);
            }
        }
    }

    refreshWelcome();

    loadWallpaper();
}

MainWindow::~MainWindow() = default;

int MainWindow::documentCount() const {
    return tabs->count();
}

QString MainWindow::documentTitle(int index) const {
    const DocumentEditor *document = documentAt(index);
    return document != nullptr ? document->title : QString();
}

QString MainWindow::documentLabel(int index) const {
    if (index < 0 || index >= tabs->count()) {
        return QString();
    }

    return tabs->tabText(index);
}

QString MainWindow::documentPath(int index) const {
    const DocumentEditor *document = documentAt(index);
    return document != nullptr ? document->path : QString();
}

bool MainWindow::documentDirty(int index) const {
    const DocumentEditor *document = documentAt(index);
    return document != nullptr && document->dirty;
}

QString MainWindow::documentText(int index) const {
    const DocumentEditor *document = documentAt(index);
    return document != nullptr ? document->toPlainText() : QString();
}

DocumentEditor *MainWindow::documentAt(int index) const {
    if (index < 0 || index >= tabs->count()) {
        return nullptr;
    }

    return qobject_cast<DocumentEditor *>(tabs->widget(index));
}

QString MainWindow::projectDirectory() const {
    return projectDir;
}

int MainWindow::definitionHistoryDepth() const {
    return static_cast<int>(definitionHistory.size());
}

bool MainWindow::restoreNavigationLocation(const NavigationLocation &location) {
    int documentIndex = -1;
    if (!location.path.isEmpty()) {
        DocumentEditor *active = currentDocument();
        if (active != nullptr && active->indexedSourcePath() == location.path) {
            documentIndex = tabs->currentIndex();
        } else {
            if (!QFileInfo(location.path).isFile() || !openPath(location.path)) return false;
            for (int index = 0; index < tabs->count(); ++index) {
                const DocumentEditor *document = documentAt(index);
                if (document != nullptr && document->path == QFileInfo(location.path).absoluteFilePath()) {
                    documentIndex = index;
                    break;
                }
            }
        }
    } else if (location.tabIndex >= 0 && location.tabIndex < tabs->count()) {
        documentIndex = location.tabIndex;
    }
    if (documentIndex < 0) return false;
    tabs->setCurrentIndex(documentIndex);
    DocumentEditor *document = documentAt(documentIndex);
    if (document == nullptr) return false;
    QTextBlock block = document->document()->findBlockByNumber(qMax(0, location.line - 1));
    if (!block.isValid()) return false;
    QTextCursor cursor(document->document());
    const int column = qMax(0, location.column - 1);
    cursor.setPosition(qMin(block.position() + column,
                            block.position() + block.length() - 1));
    document->setTextCursor(cursor);
    document->setFocus(Qt::OtherFocusReason);
    return true;
}

bool MainWindow::navigateToDefinition(const QString &path, int line, int column) {
    if (line < 1 || column < 1 || currentDocument() == nullptr) return false;
    DocumentEditor *current = currentDocument();
    const QString absoluteTarget = path.isEmpty() ? QString()
        : QFileInfo(path).absoluteFilePath();
    const bool currentDocumentTarget = absoluteTarget.isEmpty() ||
        absoluteTarget == current->indexedSourcePath() || absoluteTarget == current->path;
    if (!currentDocumentTarget && !QFileInfo(absoluteTarget).isFile()) return false;

    const int tabIndex = tabs->currentIndex();
    const int currentLine = current->textCursor().blockNumber() + 1;
    const int currentColumn = current->textCursor().positionInBlock() + 1;
    if (current->path != absoluteTarget || currentLine != line || currentColumn != column) {
        definitionHistory.append({current->path, tabIndex, currentLine, currentColumn});
    }
    const NavigationLocation target{
        currentDocumentTarget ? current->path : absoluteTarget,
        currentDocumentTarget ? tabIndex : -1,
        line,
        column
    };
    if (restoreNavigationLocation(target)) return true;
    if (!definitionHistory.isEmpty()) definitionHistory.removeLast();
    return false;
}

bool MainWindow::returnFromDefinition() {
    if (definitionHistory.isEmpty()) return false;
    const NavigationLocation location = definitionHistory.takeLast();
    return restoreNavigationLocation(location);
}

void MainWindow::returnToDefinition() {
    static_cast<void>(returnFromDefinition());
}

void MainWindow::setUnsavedPrompt(UnsavedPrompt prompt) {
    unsavedPrompt = std::move(prompt);
}

void MainWindow::setNotifier(Notifier newNotifier) {
    notifier = std::move(newNotifier);
}

void MainWindow::newFile() {
    auto *document = new DocumentEditor;
    attachHighlighter(document);

    const int index = tabs->addTab(document, document->title);
    tabs->setTabToolTip(index, document->title);
    installTabCloseButton(index);
    tabs->setCurrentIndex(index);

    connect(
        document,
        &DocumentEditor::dirtyChanged,
        this,
        &MainWindow::refreshDocumentLabel
    );

    watchDocument(document);

    refreshDocumentLabel(document);
    statusBar()->showMessage("New file");
    updateEditActions();
    document->setFocus(Qt::OtherFocusReason);
}

bool MainWindow::openPath(const QString &path) {
    const QFileInfo info(path);

    if (!info.exists() || !info.isFile()) {
        notify("Open failed", QString("Cannot open '%1'.").arg(path));
        return false;
    }

    const QString absolutePath = info.absoluteFilePath();

    for (int index = 0; index < tabs->count(); ++index) {
        const DocumentEditor *open = documentAt(index);

        if (open != nullptr && open->path == absolutePath) {
            tabs->setCurrentIndex(index);
            return true;
        }
    }

    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        notify("Open failed", file.errorString());
        return false;
    }

    const QString source = QTextStream(&file).readAll();
    file.close();

    auto *document = new DocumentEditor;
    document->setPlainText(source);
    document->applyLineHeight();
    document->document()->clearUndoRedoStacks();
    document->document()->setModified(false);
    document->path = absolutePath;
    document->title = info.fileName();

    attachHighlighter(document);

    const int index = tabs->addTab(document, document->title);
    tabs->setTabToolTip(index, absolutePath);
    installTabCloseButton(index);
    tabs->setCurrentIndex(index);

    connect(
        document,
        &DocumentEditor::dirtyChanged,
        this,
        &MainWindow::refreshDocumentLabel
    );

    watchDocument(document);

    refreshDocumentLabel(document);
    statusBar()->showMessage("Opened " + absolutePath);
    updateEditActions();
    document->setFocus(Qt::OtherFocusReason);

    recordRecentFile(absolutePath);

    return true;
}

bool MainWindow::saveCurrent() {
    DocumentEditor *document = currentDocument();

    if (document == nullptr) {
        return false;
    }

    if (document->path.isEmpty()) {
        return saveAsDialog();
    }

    return writeDocument(document, document->path);
}

bool MainWindow::saveAsPath(const QString &path) {
    DocumentEditor *document = currentDocument();

    if (document == nullptr || path.isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    QString target = info.absoluteFilePath();

    if (info.suffix().isEmpty()) {
        target += "." + sourceSuffix();
    }

    for (int index = 0; index < tabs->count(); ++index) {
        const DocumentEditor *open = documentAt(index);

        if (open != nullptr && open != document && open->path == target) {
            notify("Already open", "File already open in another tab.");
            tabs->setCurrentIndex(index);
            return false;
        }
    }

    return writeDocument(document, target);
}

void MainWindow::installTabCloseButton(int index) {
    auto *button = new TabCloseButton(tabs->tabBar());

    if (const DocumentEditor *document = documentAt(index)) {
        button->setDirty(document->dirty);
    }

    connect(
        button,
        &QToolButton::clicked,
        this,
        [this, button]() {
            const int current = tabs->tabBar()->tabAt(
                button->geometry().center()
            );

            if (current >= 0) {
                closeDocument(current);
            }
        }
    );

    tabs->tabBar()->setTabButton(index, QTabBar::RightSide, button);
}

void MainWindow::closeDocument(int index) {
    DocumentEditor *document = documentAt(index);

    if (document == nullptr) {
        return;
    }

    if (document->dirty) {
        const UnsavedChoice choice = askUnsaved(document);

        if (choice == UnsavedChoice::Cancel) {
            return;
        }

        if (choice == UnsavedChoice::Save) {
            tabs->setCurrentIndex(index);

            if (!saveCurrent() || document->dirty) {
                return;
            }
        }
    }

    tabs->removeTab(index);
    document->deleteLater();

    updateEditActions();
}

void MainWindow::refreshDocumentLabel(DocumentEditor *document) {
    const int index = tabs->indexOf(document);

    if (index < 0) {
        return;
    }

    tabs->setTabText(index, document->title);

    if (TabCloseButton *button = tabButtonAt(index)) {
        button->setDirty(document->dirty);
    }

    tabs->setTabToolTip(
        index,
        document->path.isEmpty() ? document->title : document->path
    );

    updateWindowTitle();
    updateStatus();
}

QWidget *MainWindow::activityBar() const {
    return activityBarWidget;
}

bool MainWindow::explorerVisible() const {
    return explorerShown;
}

bool MainWindow::outputPaneVisible() const {
    return outputShown;
}

void MainWindow::setExplorerVisible(bool visible) {
    explorerShown = visible;
    tree->setVisible(visible);
    syncWorkspaceActions();
}

void MainWindow::setOutputPaneVisible(bool visible) {
    outputShown = visible;
    bottomPanel->setVisible(visible);
    syncWorkspaceActions();
}

void MainWindow::syncWorkspaceActions() {
    if (explorerButton != nullptr) {
        const bool blocked = explorerButton->blockSignals(true);
        explorerButton->setChecked(explorerShown);
        explorerButton->blockSignals(blocked);
    }

    if (outputButton != nullptr) {
        const bool blocked = outputButton->blockSignals(true);
        outputButton->setChecked(outputShown);
        outputButton->blockSignals(blocked);
    }

    if (explorerAction != nullptr) {
        const bool blocked = explorerAction->blockSignals(true);
        explorerAction->setChecked(explorerShown);
        explorerAction->blockSignals(blocked);
    }

    if (outputAction != nullptr) {
        const bool blocked = outputAction->blockSignals(true);
        outputAction->setChecked(outputShown);
        outputAction->blockSignals(blocked);
    }
}

void MainWindow::openProject() {
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        "Open Pebble Project"
    );

    if (dir.isEmpty()) {
        return;
    }

    if (!QFile::exists(dir + "/pebble.toml")) {
        QMessageBox::warning(this, "Not a project", "No pebble.toml found.");
        return;
    }

    projectDir = dir;
    refreshIntelliSenseProjectPackages();

    clearDiagnostics();

    refreshProjectTree();
    statusBar()->showMessage("Opened " + dir);
}

void MainWindow::populateTree(const QString &dir, QTreeWidgetItem *parent) {
    QDir directory(dir);

    const QFileInfoList entries = directory.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot
    );

    for (const QFileInfo &info : entries) {
        if (info.fileName().startsWith(".") || info.fileName() == "build") {
            continue;
        }

        const bool isDirectory = info.isDir();
        const bool isSource = info.isFile() && info.suffix() == sourceSuffix();

        if (!isDirectory && !isSource) {
            continue;
        }

        auto *item = new QTreeWidgetItem(
            parent != nullptr
                ? parent
                : static_cast<QTreeWidgetItem *>(nullptr),
            QStringList(info.fileName())
        );

        if (!parent) {
            tree->addTopLevelItem(item);
        }

        item->setData(0, Qt::UserRole, info.absoluteFilePath());
        item->setToolTip(0, info.absoluteFilePath());

        if (info.isDir()) {
            item->setForeground(0, QBrush(QColor(theme::textDim)));
            populateTree(info.absoluteFilePath(), item);
        }
    }
}

void MainWindow::refreshProjectTree() {
    if (projectDir.isEmpty()) {
        return;
    }

    const QFileInfo info(projectDir);

    if (!info.exists() || !info.isDir() || !QDir(projectDir).isReadable()) {
        return;
    }

    const QString selected = selectedTreePath();

    tree->clear();
    populateTree(projectDir, nullptr);

    selectTreePath(selected);
}

void MainWindow::collectTreeEntries(
    const QTreeWidgetItem *item,
    const QString &prefix,
    QStringList &entries
) const {
    const QString path = prefix.isEmpty()
        ? item->text(0)
        : prefix + "/" + item->text(0);

    entries << path;

    for (int index = 0; index < item->childCount(); ++index) {
        collectTreeEntries(item->child(index), path, entries);
    }
}

QStringList MainWindow::projectTreeEntries() const {
    QStringList entries;

    for (int index = 0; index < tree->topLevelItemCount(); ++index) {
        collectTreeEntries(tree->topLevelItem(index), QString(), entries);
    }

    return entries;
}

QTreeWidget *MainWindow::projectTree() const {
    return tree;
}

QTreeWidgetItem *MainWindow::findTreeItem(
    QTreeWidgetItem *item,
    const QString &path
) const {
    if (item == nullptr) {
        return nullptr;
    }

    if (item->data(0, Qt::UserRole).toString() == path) {
        return item;
    }

    for (int index = 0; index < item->childCount(); ++index) {
        QTreeWidgetItem *found = findTreeItem(item->child(index), path);

        if (found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

QString MainWindow::selectedTreePath() const {
    QTreeWidgetItem *item = tree->currentItem();

    if (item == nullptr) {
        return QString();
    }

    return item->data(0, Qt::UserRole).toString();
}

void MainWindow::selectTreePath(const QString &path) {
    if (path.isEmpty()) {
        return;
    }

    for (int index = 0; index < tree->topLevelItemCount(); ++index) {
        QTreeWidgetItem *item = findTreeItem(tree->topLevelItem(index), path);

        if (item != nullptr) {
            tree->setCurrentItem(item);
            return;
        }
    }
}

bool MainWindow::restoreProject() {
    if (projectDir.isEmpty()) {
        return false;
    }

    const QFileInfo info(projectDir);

    if (!info.exists() || !info.isDir()) {
        statusBar()->showMessage(
            QString("The saved project '%1' is no longer available.").arg(
                projectDir
            )
        );
        projectDir.clear();
        return false;
    }

    if (!QDir(projectDir).isReadable()) {
        statusBar()->showMessage(
            QString("The saved project '%1' cannot be read.").arg(projectDir)
        );
        projectDir.clear();
        return false;
    }

    if (!QFile::exists(info.absoluteFilePath() + "/pebble.toml")) {
        statusBar()->showMessage(
            QString("The saved project '%1' is not a Pebble project.").arg(
                projectDir
            )
        );
        projectDir.clear();
        return false;
    }

    projectDir = info.absoluteFilePath();
    refreshIntelliSenseProjectPackages();
    refreshProjectTree();
    statusBar()->showMessage("Restored project " + projectDir);

    return true;
}

void MainWindow::openTreeFile(QTreeWidgetItem *item, int) {
    if (item == nullptr) {
        return;
    }

    const QString path = item->data(0, Qt::UserRole).toString();

    if (QFileInfo(path).isDir()) {
        return;
    }

    openPath(path);
}

bool MainWindow::writeDocument(DocumentEditor *document, const QString &path) {
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        notify("Save failed", file.errorString());
        return false;
    }

    QTextStream(&file) << document->toPlainText();
    file.close();

    document->path = QFileInfo(path).absoluteFilePath();
    document->title = QFileInfo(path).fileName();
    document->setIntelliSenseSourcePath(document->path);
    document->document()->setModified(false);
    document->dirty = false;

    refreshDocumentLabel(document);
    refreshProjectTree();
    statusBar()->showMessage("Saved " + document->path);

    recordRecentFile(document->path);

    return true;
}

bool MainWindow::saveAsDialog() {
    DocumentEditor *document = currentDocument();

    if (document == nullptr) {
        return false;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        "Save As",
        defaultSavePath(document),
        pebbleFileFilter()
    );

    if (path.isEmpty()) {
        return false;
    }

    return saveAsPath(path);
}

MainWindow::UnsavedChoice MainWindow::askUnsaved(
    const DocumentEditor *document
) {
    if (unsavedPrompt) {
        return unsavedPrompt(document->title);
    }

    const auto answer = QMessageBox::question(
        this,
        "Unsaved",
        QString("Save changes to %1?").arg(document->title),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
    );

    if (answer == QMessageBox::Save) {
        return UnsavedChoice::Save;
    }

    if (answer == QMessageBox::Discard) {
        return UnsavedChoice::Discard;
    }

    return UnsavedChoice::Cancel;
}

void MainWindow::notify(const QString &title, const QString &text) {
    if (notifier) {
        notifier(title, text);
        return;
    }

    QMessageBox::warning(this, title, text);
}

void MainWindow::createFile(QTreeWidgetItem *item) {
    QString base = item != nullptr
        ? item->data(0, Qt::UserRole).toString()
        : projectDir;

    if (base.isEmpty()) {
        base = defaultDirectory();
    }

    const QFileInfo info(base);
    const QString directory = info.isDir() ? base : info.absolutePath();

    bool ok = false;
    const QString name = QInputDialog::getText(
        this,
        "New File",
        "File name:",
        QLineEdit::Normal,
        QString(),
        &ok
    );

    if (!ok || name.isEmpty()) {
        return;
    }

    createFileAt(directory, name);
}

bool MainWindow::createFileAt(const QString &directory, const QString &name) {
    if (name.isEmpty()) {
        return false;
    }

    const QString path = directory + "/" + name;

    if (QFile::exists(path)) {
        notify("Exists", "File already exists.");
        return false;
    }

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly)) {
        notify("Error", file.errorString());
        return false;
    }

    file.close();
    refreshProjectTree();

    return true;
}

void MainWindow::createDirectory(QTreeWidgetItem *item) {
    QString base = item != nullptr
        ? item->data(0, Qt::UserRole).toString()
        : projectDir;

    if (base.isEmpty()) {
        base = defaultDirectory();
    }

    const QFileInfo info(base);
    const QString directory = info.isDir() ? base : info.absolutePath();

    bool ok = false;
    const QString name = QInputDialog::getText(
        this,
        "New Directory",
        "Directory name:",
        QLineEdit::Normal,
        QString(),
        &ok
    );

    if (!ok || name.isEmpty()) {
        return;
    }

    createDirectoryAt(directory, name);
}

bool MainWindow::createDirectoryAt(
    const QString &directory,
    const QString &name
) {
    if (name.isEmpty()) {
        return false;
    }

    const QString path = directory + "/" + name;

    if (QDir(path).exists()) {
        notify("Exists", "Directory already exists.");
        return false;
    }

    if (!QDir().mkpath(path)) {
        notify("Error", "Cannot create directory.");
        return false;
    }

    refreshProjectTree();

    return true;
}

void MainWindow::renameItem(QTreeWidgetItem *item) {
    if (item == nullptr) {
        return;
    }

    const QString path = item->data(0, Qt::UserRole).toString();
    const QFileInfo info(path);

    bool ok = false;
    const QString newName = QInputDialog::getText(
        this,
        "Rename",
        "New name:",
        QLineEdit::Normal,
        info.fileName(),
        &ok
    );

    if (!ok || newName.isEmpty()) {
        return;
    }

    renamePath(path, newName);
}

bool MainWindow::renamePath(const QString &path, const QString &newName) {
    const QFileInfo info(path);

    if (newName.isEmpty() || newName == info.fileName()) {
        return false;
    }

    const QString newPath = info.absolutePath() + "/" + newName;

    if (QFile::exists(newPath)) {
        notify("Exists", "Name already exists.");
        return false;
    }

    if (!QFile::rename(path, newPath)) {
        notify("Error", "Rename failed.");
        return false;
    }

    for (int index = 0; index < tabs->count(); ++index) {
        DocumentEditor *document = documentAt(index);

        if (document == nullptr || document->path != path) {
            continue;
        }

        document->path = QFileInfo(newPath).absoluteFilePath();
        document->title = QFileInfo(newPath).fileName();
        document->setIntelliSenseSourcePath(document->path);
        refreshDocumentLabel(document);
    }

    renameRecentFile(path, newPath);

    refreshProjectTree();

    return true;
}

void MainWindow::deleteItem(QTreeWidgetItem *item) {
    if (item == nullptr) {
        return;
    }

    const QString path = item->data(0, Qt::UserRole).toString();

    const auto answer = QMessageBox::question(
        this,
        "Delete",
        "Delete " + QFileInfo(path).fileName() + "?",
        QMessageBox::Yes | QMessageBox::No
    );

    if (answer != QMessageBox::Yes) {
        return;
    }

    deletePath(path);
}

bool MainWindow::deletePath(const QString &path) {
    const QFileInfo info(path);

    for (int index = tabs->count() - 1; index >= 0; --index) {
        DocumentEditor *document = documentAt(index);

        if (document == nullptr) {
            continue;
        }

        const QString openPath = document->path;

        if (openPath != path && !openPath.startsWith(path + "/")) {
            continue;
        }

        closeDocument(index);

        if (tabs->indexOf(document) >= 0) {
            return false;
        }
    }

    const bool removed = info.isDir()
        ? QDir(path).removeRecursively()
        : QFile::remove(path);

    if (!removed) {
        notify("Error", "Delete failed.");
        return false;
    }

    pruneRecentFiles();
    saveRecentFiles();
    refreshRecentFilesMenu();

    refreshProjectTree();

    return true;
}

void MainWindow::treeContextMenu(const QPoint &position) {
    QTreeWidgetItem *item = tree->itemAt(position);
    QMenu menu(this);

    QAction *openAction = menu.addAction("Open");
    connect(openAction, &QAction::triggered, this, [this, item]() {
        if (item) {
            openTreeFile(item, 0);
        }
    });

    QAction *newFileAction = menu.addAction("New File");
    connect(newFileAction, &QAction::triggered, this, [this, item]() {
        createFile(item);
    });

    QAction *newDirectoryAction = menu.addAction("New Directory");
    connect(newDirectoryAction, &QAction::triggered, this, [this, item]() {
        createDirectory(item);
    });

    QAction *renameAction = menu.addAction("Rename");
    connect(renameAction, &QAction::triggered, this, [this, item]() {
        renameItem(item);
    });

    QAction *deleteAction = menu.addAction("Delete");
    connect(deleteAction, &QAction::triggered, this, [this, item]() {
        deleteItem(item);
    });

    menu.exec(tree->viewport()->mapToGlobal(position));
}

void MainWindow::saveAsCurrent() {
    saveAsDialog();
}

void MainWindow::build() {
    if (projectDir.isEmpty()) {
        notify("No project", "Open a project first.");
        return;
    }

    if (isBuildRunning()) {
        notify("Build", "A build is already running.");
        return;
    }

    startProcess(
        ProcessKind::Build,
        QStringList() << "build",
        projectDir
    );
}

void MainWindow::buildCurrentFile() {
    DocumentEditor *document = currentDocument();

    if (document == nullptr) {
        notify("Build File", "Open a .peb file before building it.");
        return;
    }

    if (document->path.isEmpty()) {
        notify("Build File", "Save the file before building it.");
        return;
    }

    const QFileInfo info(document->path);

    if (info.suffix().compare(QLatin1String("peb"), Qt::CaseInsensitive) != 0) {
        notify("Build File", "Pebble Studio builds .peb files.");
        return;
    }

    if (document->dirty && !writeDocument(document, document->path)) {
        notify("Build File", "The file could not be saved, so it was not built.");
        return;
    }

    if (isBuildRunning()) {
        notify("Build", "A build is already running.");
        return;
    }

    startProcess(
        ProcessKind::Build,
        QStringList() << "build" << document->path,
        workingDirectoryFor(document->path)
    );
}

void MainWindow::runProject() {
    if (projectDir.isEmpty()) {
        notify("No project", "Open a project first.");
        return;
    }

    if (isProcessRunning()) {
        notify("Run", "A program is already running.");
        return;
    }

    startProcess(
        ProcessKind::Run,
        QStringList() << "run",
        projectDir
    );
}

void MainWindow::runCurrentFile() {
    if (isProcessRunning()) {
        notify("Run File", "A program is already running.");
        return;
    }

    DocumentEditor *document = currentDocument();

    if (document == nullptr) {
        notify("Run File", "Open a .peb file before running it.");
        return;
    }

    if (document->path.isEmpty()) {
        notify("Run File", "Save the file before running it.");
        return;
    }

    const QFileInfo info(document->path);

    if (info.suffix().compare(QLatin1String("peb"), Qt::CaseInsensitive) != 0) {
        notify("Run File", "Pebble Studio runs .peb files.");
        return;
    }

    if (!info.exists() || !info.isFile()) {
        notify("Run File", QString("Cannot run '%1'.").arg(document->path));
        return;
    }

    if (document->dirty && !writeDocument(document, document->path)) {
        notify("Run File", "The file could not be saved, so it was not run.");
        return;
    }

    startProcess(
        ProcessKind::Run,
        QStringList() << "run" << document->path,
        workingDirectoryFor(document->path)
    );
}

QString MainWindow::workingDirectoryFor(const QString &path) const {
    const QString absolute = QFileInfo(path).absoluteFilePath();

    if (!projectDir.isEmpty()) {
        const QString base = QDir(projectDir).absolutePath();

        if (absolute.startsWith(base + "/")) {
            return base;
        }
    }

    return QFileInfo(absolute).absolutePath();
}

void MainWindow::startProcess(
    ProcessKind kind,
    const QStringList &arguments,
    const QString &workingDirectory
) {
    QProcess *process = kind == ProcessKind::Build ? buildProcess : runProcess;

    if (kind == ProcessKind::Run) {
        stopRequested = false;
    }

    clearDiagnostics();
    buildStderrBuffer.clear();
    runStderrBuffer.clear();
    buildStdoutBuffer.clear();
    runStdoutBuffer.clear();

    const QString executable = resolveExecutable();

    if (executable.isEmpty()) {
        updateProcessActions();
        return;
    }

    process->setWorkingDirectory(workingDirectory);

    if (kind == ProcessKind::Run) {
        const QStringList roots = exampleAssetDirectories();

        if (!roots.isEmpty()) {
            QProcessEnvironment environment =
                QProcessEnvironment::systemEnvironment();
            QStringList paths = roots;

            const QString existing = environment.value("PEBBLE_RESOURCE_PATH");

            if (!existing.isEmpty()) {
                paths.prepend(existing);
            }

            environment.insert(
                "PEBBLE_RESOURCE_PATH",
                paths.join(QDir::listSeparator())
            );
            process->setProcessEnvironment(environment);
        }
    }

    process->start(executable, arguments);

    updateProcessActions();
}

void MainWindow::connectProcess(QProcess *process, ProcessKind kind) {
    connect(
        process,
        &QProcess::readyReadStandardOutput,
        this,
        [this, process]() {
            readProcessOutput(process, OutputLevel::Normal);
        }
    );

    connect(
        process,
        &QProcess::readyReadStandardError,
        this,
        [this, process]() {
            readProcessOutput(process, OutputLevel::Error);
        }
    );

    connect(
        process,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this,
        [this, process, kind](int code, QProcess::ExitStatus status) {
            processFinished(process, kind, code, status);
        }
    );

    connect(
        process,
        &QProcess::errorOccurred,
        this,
        [this, process, kind](QProcess::ProcessError error) {
            processError(process, kind, error);
        }
    );

    connect(
        process,
        &QProcess::stateChanged,
        this,
        [this]() {
            updateProcessActions();
        }
    );
}

void MainWindow::readProcessOutput(QProcess *process, OutputLevel level) {
    if (process == runProcess) {
        if (level == OutputLevel::Normal) {
            feedLines(
                runStdoutBuffer,
                QString::fromLocal8Bit(process->readAllStandardOutput()),
                [this](const QString &line) {
                    appendOutput(line, OutputLevel::Normal);
                }
            );
            return;
        }

        feedLines(
            runStderrBuffer,
            QString::fromLocal8Bit(process->readAllStandardError()),
            [this](const QString &line) {
                appendOutput(line, outputLevelOfLine(line));
                handleDiagnosticLine(line);
            }
        );
        return;
    }

    if (level == OutputLevel::Normal) {
        feedLines(
            buildStdoutBuffer,
            QString::fromLocal8Bit(process->readAllStandardOutput()),
            [this](const QString &line) {
                appendOutput(line, OutputLevel::Normal);
            }
        );
        return;
    }

    feedLines(
        buildStderrBuffer,
        QString::fromLocal8Bit(process->readAllStandardError()),
        [this](const QString &line) {
            appendOutput(line, outputLevelOfLine(line));
            handleDiagnosticLine(line);
        }
    );
}

void MainWindow::appendOutput(const QString &text, OutputLevel level) {
    if (output == nullptr) {
        return;
    }

    output->appendPlainText(outputPrefix(level) + text);
}

void MainWindow::flushProcessOutput(QProcess *process) {
    if (process == runProcess) {
        if (!runStdoutBuffer.isEmpty()) {
            appendOutput(runStdoutBuffer, OutputLevel::Normal);
            runStdoutBuffer.clear();
        }

        if (!runStderrBuffer.isEmpty()) {
            appendOutput(runStderrBuffer, outputLevelOfLine(runStderrBuffer));
            handleDiagnosticLine(runStderrBuffer);
            runStderrBuffer.clear();
        }

        return;
    }

    if (!buildStdoutBuffer.isEmpty()) {
        appendOutput(buildStdoutBuffer, OutputLevel::Normal);
        buildStdoutBuffer.clear();
    }

    if (!buildStderrBuffer.isEmpty()) {
        appendOutput(buildStderrBuffer, outputLevelOfLine(buildStderrBuffer));
        handleDiagnosticLine(buildStderrBuffer);
        buildStderrBuffer.clear();
    }
}

void MainWindow::stopProgram() {
    if (!isProcessRunning()) {
        reportProcessMessage("There is no running program to stop.");
        return;
    }

    requestStop();
}

void MainWindow::requestStop() {
    if (stopRequested || !isProcessRunning()) {
        return;
    }

    stopRequested = true;
    updateProcessActions();

    reportProcessMessage("Stopping the Pebble program...");

    stoppingProcess = runProcess;
    runProcess->terminate();
    killTimer->start();
}

void MainWindow::killProcess() {
    if (stoppingProcess == nullptr) {
        return;
    }

    if (stoppingProcess->state() == QProcess::NotRunning) {
        stoppingProcess = nullptr;
        return;
    }

    reportProcessMessage("The process did not stop, killing it.");
    stoppingProcess->kill();
}

void MainWindow::find(const QString &text, bool matchCase) {
    findTerm = text;
    setMatchCase(matchCase);
    if (text.isEmpty()) {
        setFindStatus("no matches");
        return;
    }
    findNext();
}

void MainWindow::findNext() {
    if (findTerm.isEmpty()) { setFindStatus("no matches"); return; }
    auto *doc = currentDocument();
    if (!doc) { setFindStatus("no matches"); return; }
    QTextDocument *d = doc->document();
    QTextCursor c = doc->textCursor();
    QTextDocument::FindFlags flags = findMatchCase ? QTextDocument::FindCaseSensitively : QTextDocument::FindFlags();
    QTextCursor found = d->find(findTerm, c, flags);
    bool wrapped = false;
    if (found.isNull()) {
        QTextCursor start(d);
        start.movePosition(QTextCursor::Start);
        found = d->find(findTerm, start, flags);
        wrapped = !found.isNull();
    }
    if (found.isNull()) { setFindStatus("no matches"); return; }
    doc->setTextCursor(found);
    doc->ensureCursorVisible();
    setFindStatus(wrapped ? "wrapped" : QString());
}

void MainWindow::findPrevious() {
    if (findTerm.isEmpty()) { setFindStatus("no matches"); return; }
    auto *doc = currentDocument();
    if (!doc) { setFindStatus("no matches"); return; }
    QTextDocument *d = doc->document();
    QTextCursor c = doc->textCursor();
    QTextDocument::FindFlags flags = QTextDocument::FindBackward | (findMatchCase ? QTextDocument::FindCaseSensitively : QTextDocument::FindFlags());
    QTextCursor found = d->find(findTerm, c, flags);
    bool wrapped = false;
    if (found.isNull()) {
        QTextCursor end(d);
        end.movePosition(QTextCursor::End);
        found = d->find(findTerm, end, flags);
        wrapped = !found.isNull();
    }
    if (found.isNull()) { setFindStatus("no matches"); return; }
    doc->setTextCursor(found);
    doc->ensureCursorVisible();
    setFindStatus(wrapped ? "wrapped" : QString());
}

void MainWindow::replace(const QString &replacement) {
    auto *doc = currentDocument();
    if (!doc || findTerm.isEmpty()) {
        setFindStatus("no matches");
        return;
    }
    QTextCursor c = doc->textCursor();
    if (c.hasSelection() && selectionMatchesTerm(c.selectedText())) {
        c.insertText(replacement);
        doc->setTextCursor(c);
    }
    findNext();
}

int MainWindow::replaceAll(const QString &replacement) {
    auto *doc = currentDocument();
    if (!doc || findTerm.isEmpty()) {
        setFindStatus("no matches");
        return 0;
    }
    QTextDocument *d = doc->document();
    QTextDocument::FindFlags flags = findMatchCase ? QTextDocument::FindCaseSensitively : QTextDocument::FindFlags();
    int count = 0;
    const int limit = d->characterCount() + 1;
    int from = 0;
    QTextCursor edit(d);
    edit.beginEditBlock();
    while (count < limit) {
        QTextCursor found = d->find(findTerm, from, flags);
        if (found.isNull()) {
            break;
        }
        const int start = found.selectionStart();
        found.insertText(replacement);
        ++count;
        int next = start + static_cast<int>(replacement.size());
        if (next < from) {
            next = from;
        }
        from = next;
    }
    edit.endEditBlock();
    setFindStatus(QString("replaced %1").arg(count));
    return count;
}

void MainWindow::setMatchCase(bool on) {
    findMatchCase = on;
    if (findBar != nullptr) {
        const bool blocked = findBar->matchCaseCheck()->blockSignals(true);
        findBar->setMatchCase(on);
        findBar->matchCaseCheck()->blockSignals(blocked);
    }
}

void MainWindow::setFindStatus(const QString &text) {
    findStatusMessage = text;

    if (findBar != nullptr) {
        findBar->setStatus(text);
    }
}

bool MainWindow::selectionMatchesTerm(const QString &selected) const {
    if (selected.isEmpty() || findTerm.isEmpty()) {
        return false;
    }

    if (findMatchCase) {
        return selected == findTerm;
    }

    return selected.compare(findTerm, Qt::CaseInsensitive) == 0;
}

FindReplaceBar *MainWindow::findReplaceBar() const {
    return findBar;
}

bool MainWindow::findBarVisible() const {
    return findBar != nullptr && findBar->isOpen();
}

void MainWindow::openFindBar(bool withReplace) {
    if (findBar == nullptr) {
        return;
    }

    auto *doc = currentDocument();
    if (doc != nullptr) {
        const QString selected = doc->textCursor().selectedText();
        if (!selected.isEmpty() && !selected.contains(QChar(0x2029))) {
            findBar->setFindText(selected);
        }
    }

    findBar->open(withReplace);

    if (escapeShortcut != nullptr) {
        escapeShortcut->setEnabled(true);
    }

    if (withReplace && !findBar->findText().isEmpty()) {
        findBar->replaceField()->setFocus();
        findBar->replaceField()->selectAll();
        return;
    }

    findBar->findField()->setFocus();
    findBar->findField()->selectAll();
}

void MainWindow::closeFindBar() {
    if (findBar == nullptr) {
        return;
    }

    findBar->closeBar();

    if (escapeShortcut != nullptr) {
        escapeShortcut->setEnabled(false);
    }

    auto *doc = currentDocument();
    if (doc != nullptr) {
        doc->setFocus();
    }
}

void MainWindow::activeDocumentChanged(int) {
    setFindStatus(QString());

    updateEditActions();

    updateStatus();
    updateWindowTitle();
}

void MainWindow::undoActiveDocument() {
    DocumentEditor *document = currentDocument();

    if (document == nullptr) {
        return;
    }

    document->undo();
}

void MainWindow::redoActiveDocument() {
    DocumentEditor *document = currentDocument();

    if (document == nullptr) {
        return;
    }

    document->redo();
}

void MainWindow::selectAllActiveDocument() {
    DocumentEditor *document = currentDocument();

    if (document == nullptr) {
        return;
    }

    document->selectAll();
}

void MainWindow::nextDocument() {
    if (tabs->count() < 1) {
        return;
    }

    const int next = (tabs->currentIndex() + 1) % tabs->count();
    tabs->setCurrentIndex(next);
}

void MainWindow::previousDocument() {
    if (tabs->count() < 1) {
        return;
    }

    const int count = tabs->count();
    const int previous = (tabs->currentIndex() - 1 + count) % count;
    tabs->setCurrentIndex(previous);
}

void MainWindow::updateEditActions() {
    DocumentEditor *document = currentDocument();
    const bool hasDocument = document != nullptr;

    if (undoAction != nullptr) {
        undoAction->setEnabled(
            hasDocument && document->document()->isUndoAvailable()
        );
    }

    if (redoAction != nullptr) {
        redoAction->setEnabled(
            hasDocument && document->document()->isRedoAvailable()
        );
    }

    if (selectAllAction != nullptr) {
        selectAllAction->setEnabled(hasDocument);
    }
}

void MainWindow::watchDocument(DocumentEditor *document) {
    connect(
        document->document(),
        &QTextDocument::undoAvailable,
        this,
        [this](bool) {
            updateEditActions();
        }
    );

    connect(
        document->document(),
        &QTextDocument::redoAvailable,
        this,
        [this](bool) {
            updateEditActions();
        }
    );

    connect(
        document,
        &QPlainTextEdit::cursorPositionChanged,
        this,
        [this]() {
            updateStatus();
        }
    );
}

QString MainWindow::statusDocumentName() const {
    const DocumentEditor *document = currentDocument();

    if (document == nullptr) {
        return QString();
    }

    if (document->path.isEmpty()) {
        return document->title;
    }

    return QFileInfo(document->path).fileName();
}

QString MainWindow::statusCursorPosition() const {
    const DocumentEditor *document = currentDocument();

    if (document == nullptr) {
        return QString();
    }

    const QTextCursor cursor = document->textCursor();

    return QString("line %1, column %2")
        .arg(cursor.blockNumber() + 1)
        .arg(cursor.positionInBlock() + 1);
}

QString MainWindow::statusState() const {
    if (isProcessRunning()) {
        return stopRequested ? QString("Stopping") : QString("Running");
    }

    if (isBuildRunning()) {
        return QString("Building");
    }

    const DocumentEditor *document = currentDocument();

    if (document == nullptr) {
        return QString("Ready");
    }

    return document->dirty ? QString("Modified") : QString("Saved");
}

QString MainWindow::windowTitleText() const {
    const DocumentEditor *document = currentDocument();

    if (document == nullptr) {
        return QStringLiteral("Pebble Studio");
    }

    const QString name = document->path.isEmpty()
        ? document->title
        : QFileInfo(document->path).fileName();

    return QString("%1%2 — Pebble Studio")
        .arg(name, document->dirty ? QStringLiteral(" *") : QString());
}

void MainWindow::updateStatus() {
    if (statusDocumentLabel != nullptr) {
        const DocumentEditor *shown = currentDocument();
        statusDocumentLabel->setText(
            statusDocumentName()
            + (shown != nullptr && shown->dirty ? QStringLiteral("  ●") : QString())
        );
    }

    if (statusCursorLabel != nullptr) {
        statusCursorLabel->setText(statusCursorPosition());
    }

    if (statusEncodingLabel != nullptr) {
        statusEncodingLabel->setVisible(currentDocument() != nullptr);
    }

    if (statusStateLabel != nullptr) {
        statusStateLabel->setText(statusState());

        const DocumentEditor *document = currentDocument();
        const bool modified = document != nullptr && document->dirty;
        const bool busy = isProcessRunning() || isBuildRunning();

        if (statusStateLabel->property("modified").toBool() != modified) {
            statusStateLabel->setProperty("modified", modified);
            statusStateLabel->style()->unpolish(statusStateLabel);
            statusStateLabel->style()->polish(statusStateLabel);
        }

        if (statusStateLabel->property("busy").toBool() != busy) {
            statusStateLabel->setProperty("busy", busy);
            statusStateLabel->style()->unpolish(statusStateLabel);
            statusStateLabel->style()->polish(statusStateLabel);
        }
    }
}

void MainWindow::updateWindowTitle() {
    setWindowTitle(windowTitleText());
}

void MainWindow::findTextEdited(const QString &text) {
    findTerm = text;
    setFindStatus(QString());
}

void MainWindow::findSubmitted(const QString &text) {
    const bool matchCase = findBar != nullptr ? findBar->matchCase() : findMatchCase;
    find(text, matchCase);
}

void MainWindow::replaceSubmitted(const QString &replacement) {
    replace(replacement);
}

void MainWindow::matchCaseToggled(bool on) {
    setMatchCase(on);

    if (findBar == nullptr || findTerm.isEmpty()) {
        return;
    }

    find(findBar->findText(), on);
}

void MainWindow::processFinished(
    QProcess *process,
    ProcessKind kind,
    int code,
    QProcess::ExitStatus status
) {
    flushProcessOutput(process);

    if (process == stoppingProcess) {
        killTimer->stop();
        stoppingProcess = nullptr;
    }

    const bool stopped = kind == ProcessKind::Run && stopRequested;

    if (kind == ProcessKind::Run) {
        stopRequested = false;
    }

    QString message;

    if (stopped) {
        message = QString("Program stopped.");
    } else if (kind == ProcessKind::Run) {
        if (status == QProcess::CrashExit) {
            message = QString("The Pebble program crashed.");
        } else if (code == 0) {
            message = QString("Program finished successfully. Exit code 0.");
        } else {
            message = QString("Program exited with code %1.").arg(code);
        }
    } else if (status == QProcess::CrashExit) {
        message = QString("The build crashed.");
    } else if (code == 0) {
        message = QString("Build finished with exit code 0.");
    } else {
        message = QString("Build failed with exit code %1.").arg(code);
    }

    reportProcessMessage(message);
    updateProcessActions();

    if (closePending && !isProcessRunning() && !isBuildRunning()) {
        closePending = false;
        close();
    }
}

void MainWindow::processError(
    QProcess *process,
    ProcessKind kind,
    QProcess::ProcessError error
) {
    if (error == QProcess::FailedToStart) {
        if (kind == ProcessKind::Run) {
            killTimer->stop();
            stopRequested = false;
        }

        flushProcessOutput(process);

        reportProcessMessage(
            QString("Failed to start the Pebble CLI: %1").arg(
                process->errorString()
            )
        );
    }

    updateProcessActions();

    if (closePending && !isProcessRunning() && !isBuildRunning()) {
        closePending = false;
        close();
    }
}

void MainWindow::updateProcessActions() {
    const bool building = isBuildRunning();
    const bool running = isProcessRunning();

    if (buildAction != nullptr) {
        buildAction->setEnabled(!building);
    }

    if (buildFileAction != nullptr) {
        buildFileAction->setEnabled(!building);
    }

    if (runAction != nullptr) {
        runAction->setEnabled(!running && !building);
    }

    if (runFileAction != nullptr) {
        runFileAction->setEnabled(!running && !building);
    }

    if (stopAction != nullptr) {
        stopAction->setEnabled(running && !stopRequested);
    }

    if (programInput != nullptr) {
        programInput->setEnabled(running);
        programInput->setPlaceholderText(
            running
                ? QStringLiteral("Type your answer and press Enter")
                : QStringLiteral("Program input — available while a program is running")
        );
    }

    updateStatus();
}

void MainWindow::reportProcessMessage(const QString &message) {
    appendOutput(message, OutputLevel::Info);
    statusBar()->showMessage(message);
}

bool MainWindow::isProcessRunning() const {
    return runProcess != nullptr && runProcess->state() != QProcess::NotRunning;
}

bool MainWindow::isBuildRunning() const {
    return buildProcess != nullptr && buildProcess->state() != QProcess::NotRunning;
}

QString MainWindow::outputText() const {
    return output != nullptr ? output->toPlainText() : QString();
}

void MainWindow::clearOutput() {
    if (output == nullptr) {
        return;
    }

    output->clear();
}

int MainWindow::outputBlockCount() const {
    if (output == nullptr) {
        return 0;
    }

    return static_cast<int>(output->document()->blockCount());
}

int MainWindow::outputMaximumBlockCount() const {
    if (output == nullptr) {
        return 0;
    }

    return output->document()->maximumBlockCount();
}

void MainWindow::setOutputMaximumBlockCount(int count) {
    if (output == nullptr) {
        return;
    }

    output->document()->setMaximumBlockCount(count);
}

QString MainWindow::diagnosticsTitle() const {
    return QStringLiteral("Diagnostics (%1)").arg(diagnosticCount());
}

void MainWindow::setProcessProgram(const QString &program) {
    processProgram = program;
    programResolved = false;
}

QString MainWindow::resolveExecutable() {
    if (programResolved) {
        return resolvedProgram;
    }

    if (!processProgram.isEmpty() && processProgram != QLatin1String("pebble")) {
        resolvedProgram = processProgram;
        programResolved = true;
        return resolvedProgram;
    }

    const ExecutableResolution resolution = resolvePebbleExecutable(
        QCoreApplication::applicationFilePath(),
        QString::fromLocal8Bit(qgetenv("PEBBLE")),
        QString::fromLocal8Bit(qgetenv("PATH"))
    );

    if (!resolution.found) {
        resolvedProgram.clear();
        programResolved = true;
        notify("Pebble CLI not found", resolution.reason);
        return QString();
    }

    resolvedProgram = resolution.path;
    programResolved = true;
    return resolvedProgram;
}

void MainWindow::feedCompilerOutput(const QString &text) {
    feedLines(
        buildStderrBuffer,
        text,
        [this](const QString &line) {
            handleDiagnosticLine(line);
        }
    );
}

void MainWindow::flushCompilerOutput() {
    if (buildStderrBuffer.isEmpty()) {
        return;
    }

    const QString line = buildStderrBuffer;
    buildStderrBuffer.clear();

    handleDiagnosticLine(line);
}

void MainWindow::handleDiagnosticLine(const QString &line) {
    DiagnosticEntry entry;

    if (!parseDiagnosticLine(line, entry)) {
        return;
    }

    addDiagnostic(entry);
}

void MainWindow::addDiagnostic(const DiagnosticEntry &entry) {
    auto *item = new QListWidgetItem(
        entry.severity + ": " + entry.message + " (" + entry.path + ":" +
        QString::number(entry.line) + ")"
    );

    item->setData(
        Qt::UserRole,
        QStringList{
            entry.path,
            QString::number(entry.line),
            QString::number(entry.column),
            entry.severity,
            entry.message
        }
    );

    diagnostics->addItem(item);

    updateDiagnosticsTitle();
}

void MainWindow::clearDiagnostics() {
    diagnostics->clear();
    updateDiagnosticsTitle();
}

void MainWindow::updateDiagnosticsTitle() {
    if (diagnosticsTitleLabel != nullptr) {
        diagnosticsTitleLabel->setText(diagnosticsTitle());
    }

    const bool any = diagnostics != nullptr && diagnostics->count() > 0;

    if (diagnosticsTitleLabel != nullptr) {
        diagnosticsTitleLabel->setVisible(any);
    }

    if (diagnostics != nullptr) {
        diagnostics->setVisible(any);
    }
}

int MainWindow::diagnosticCount() const {
    return diagnostics != nullptr ? diagnostics->count() : 0;
}

QString MainWindow::diagnosticText(int index) const {
    if (diagnostics == nullptr) {
        return QString();
    }

    const QListWidgetItem *item = diagnostics->item(index);

    return item != nullptr ? item->text() : QString();
}

QListWidget *MainWindow::diagnosticsList() const {
    return diagnostics;
}

QString MainWindow::resolveDiagnosticPath(const QString &path) const {
    if (path.isEmpty()) {
        return QString();
    }

    const QFileInfo info(path);

    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }

    const QString base = projectDir.isEmpty()
        ? QDir::currentPath()
        : projectDir;

    return QFileInfo(QDir(base).absoluteFilePath(path)).absoluteFilePath();
}

void MainWindow::navigateDiagnostic(QListWidgetItem *item) {
    if (item == nullptr) {
        return;
    }

    navigateToDiagnostic(diagnostics->row(item));
}

void MainWindow::navigateToDiagnostic(int index) {
    if (diagnostics == nullptr || index < 0 || index >= diagnostics->count()) {
        return;
    }

    const QStringList parts = diagnostics->item(index)
        ->data(Qt::UserRole)
        .toStringList();

    if (parts.size() < 3) {
        return;
    }

    const QString path = resolveDiagnosticPath(parts[0]);

    if (path.isEmpty()) {
        notify(
            "Cannot open the diagnostic",
            QString("Cannot resolve '%1'.").arg(parts[0])
        );
        return;
    }

    const QFileInfo info(path);

    if (!info.exists() || !info.isFile()) {
        notify(
            "Cannot open the diagnostic",
            QString("'%1' does not exist.").arg(path)
        );
        return;
    }

    if (!openPath(path)) {
        return;
    }

    int documentIndex = -1;

    for (int current = 0; current < tabs->count(); ++current) {
        const DocumentEditor *document = documentAt(current);

        if (document != nullptr && document->path == path) {
            documentIndex = current;
            break;
        }
    }

    if (documentIndex < 0) {
        return;
    }

    tabs->setCurrentIndex(documentIndex);

    auto *document = documentAt(documentIndex);

    if (document == nullptr) {
        return;
    }

    const int line = parts[1].toInt() - 1;

    if (line < 0) {
        return;
    }

    QTextCursor cursor = document->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line);

    const int column = parts[2].toInt() - 1;

    if (column > 0) {
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
    }

    document->setTextCursor(cursor);
    document->setFocus();
}

int MainWindow::currentDocumentIndex() const {
    return tabs != nullptr ? tabs->currentIndex() : -1;
}

QString MainWindow::defaultBackgroundPath() const {
    return studio::wallpaperPath();
}

void MainWindow::applyTheme() {
    setStyleSheet(studioStyleSheet());
}

void MainWindow::loadWallpaper() {
    const QString storedAssetName = backgroundAssetName;
    const QString storedPath = backgroundFile;
    QPixmap image;
    QString target = backgroundFile;

    if (!backgroundAssetName.isEmpty()) {
        const QString resolved = studio::wallpaperPathFor(backgroundAssetName);

        if (resolved.isEmpty()) {
            statusBar()->showMessage(
                "The chosen wallpaper is no longer available. Restored the default wallpaper."
            );
            target = defaultBackgroundPath();
        } else {
            target = resolved;
        }
    }

    if (target.isEmpty()) {
        target = defaultBackgroundPath();
    }

    if (!target.isEmpty()) {
        image = readWallpaperPixmap(target);
    }

    if (image.isNull() && !target.isEmpty()
        && target != defaultBackgroundPath()) {
        statusBar()->showMessage(
            "Background image is unavailable. Restored the default wallpaper."
        );
        target = defaultBackgroundPath();
        image = readWallpaperPixmap(target);
    }

    backgroundFile = target;
    backgroundAssetName = assetWallpaperName(target);

    if (!backgroundFile.startsWith(QStringLiteral(":/"))
        && (backgroundAssetName != storedAssetName
            || (backgroundAssetName.isEmpty()
                && backgroundFile != storedPath))) {
        persistWallpaperSelection();
    }

    applyWallpaper(image);
}

void MainWindow::applyWallpaper(const QPixmap &image) {
    wallpaper = image;
    scaledWallpaper = QPixmap();
    scaledSize = QSize();
    scaledRatio = 0;
    glassBackdrop = QImage();
    glassBackdropSize = QSize();
    glassBackdropRatio = 0;
    glassBackdropDirty = true;
    update();
    refreshGlassSurfaces();
}

void MainWindow::persistWallpaperSelection() {
    QSettings settings(settingsOrganisation, settingsApplication);

    if (backgroundAssetName.isEmpty()) {
        settings.setValue("background_path", backgroundFile);
        settings.remove("wallpaper_name");
        return;
    }

    settings.setValue("wallpaper_name", backgroundAssetName);
    settings.remove("background_path");
}

QString MainWindow::assetWallpaperName(const QString &path) const {
    if (path.isEmpty()) {
        return QString();
    }

    const QString directory = studio::wallpaperDirectory();

    if (directory.isEmpty()) {
        return QString();
    }

    const QFileInfo info(path);

    if (QDir::cleanPath(info.absolutePath()) != QDir::cleanPath(directory)) {
        return QString();
    }

    const QString name = info.fileName();
    QImageReader reader(path);

    if (!reader.canRead()) {
        return QString();
    }

    return name;
}

void MainWindow::setBackgroundImage(const QString &path) {
    QPixmap candidate;
    if (!path.isEmpty()) {
        candidate = readWallpaperPixmap(path);

        if (candidate.isNull()) {
            statusBar()->showMessage(
                "That image could not be read. The current wallpaper was kept."
            );
            return;
        }
    }

    backgroundFile = path;
    backgroundAssetName = assetWallpaperName(path);
    persistWallpaperSelection();
    applyWallpaper(candidate);
}

void MainWindow::setBackgroundWallpaper(const QString &name) {
    const QString path = studio::wallpaperPathFor(name);

    if (path.isEmpty()) {
        statusBar()->showMessage(
            "That wallpaper is no longer available. The current wallpaper was kept."
        );
        return;
    }

    setBackgroundImage(path);
}

QStringList MainWindow::availableWallpapers() const {
    const QString directory = studio::wallpaperDirectory();
    QStringList paths;

    if (directory.isEmpty()) {
        return paths;
    }

    for (const QString &name : studio::wallpaperFileNames()) {
        paths << QDir(directory).filePath(name);
    }

    return paths;
}

void MainWindow::setBackgroundDarkness(int value) {
    darkness = qBound(0, value, 245);
    QSettings(settingsOrganisation, settingsApplication)
        .setValue("background_dim", darkness);
    update();
}

void MainWindow::setGlassBlurStrength(int value) {
    const int clamped = qBound(0, value, 36);

    if (glassBlur == clamped) {
        return;
    }

    glassBlur = clamped;
    QSettings(settingsOrganisation, settingsApplication)
        .setValue("glass_blur", glassBlur);
    glassBackdropDirty = true;
    update();
}

void MainWindow::setGlassTintOpacity(int value) {
    const int clamped = qBound(0, value, 92);

    if (glassOpacity == clamped) {
        return;
    }

    glassOpacity = clamped;
    QSettings(settingsOrganisation, settingsApplication)
        .setValue("glass_opacity", glassOpacity);
    refreshGlassSurfaces();
}

void MainWindow::setGlassTintColour(const QColor &colour) {
    if (!colour.isValid()) {
        return;
    }

    QColor clamped = colour;
    clamped.setRed(qMin(clamped.red(), 80));
    clamped.setGreen(qMin(clamped.green(), 80));
    clamped.setBlue(qMin(clamped.blue(), 80));
    clamped.setAlpha(255);
    if (glassTint == clamped) {
        return;
    }

    glassTint = clamped;
    QSettings(settingsOrganisation, settingsApplication)
        .setValue("glass_tint", glassTint.name(QColor::HexRgb));
    refreshGlassSurfaces();
}

void MainWindow::resetAppearanceDefaults() {
    setBackgroundImage(defaultBackgroundPath());
    setGlassBlurStrength(theme::defaultGlassBlur);
    setGlassTintOpacity(theme::defaultGlassTintOpacity);
    setGlassTintColour(QColor(theme::glassTint));
}

void MainWindow::chooseBackground() {
    const QString start = backgroundFile.isEmpty()
        ? defaultDirectory()
        : QFileInfo(backgroundFile).absolutePath();
    const QString file = QFileDialog::getOpenFileName(
        this,
        "Choose Background Image",
        start,
        "Images (*.png *.jpg *.jpeg *.webp *.bmp *.svg)"
    );

    if (file.isEmpty()) {
        return;
    }

    setBackgroundImage(file);
}

void MainWindow::chooseWallpaper() {
    WallpaperPickerDialog dialog(
        availableWallpapers(),
        backgroundFile,
        defaultBackgroundPath(),
        this
    );

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString chosen = dialog.selectedPath();

    if (chosen.isEmpty()) {
        return;
    }

    setBackgroundImage(chosen);
}

void MainWindow::chooseBackgroundDarkness() {
    bool accepted = false;
    const int value = QInputDialog::getInt(
        this,
        "Background Darkness",
        "Dark Overlay (Higher Means Darker):",
        darkness,
        0,
        245,
        5,
        &accepted
    );

    if (accepted) {
        setBackgroundDarkness(value);
    }
}

void MainWindow::configureGlassAppearance() {
    QDialog dialog(this);
    dialog.setObjectName("glass_appearance_dialog");
    dialog.setWindowTitle("Obsidian Glass Appearance");
    dialog.setMinimumWidth(420);

    auto *layout = new QVBoxLayout(&dialog);
    auto *intro = new QLabel(
        "Choose a wallpaper and tune the blurred glass behind Studio panels.",
        &dialog
    );
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *wallpaperRow = new QHBoxLayout;
    auto *wallpaperButton = new QPushButton("Wallpaper…", &dialog);
    wallpaperButton->setObjectName("wallpaper_picker_button");
    auto *wallpaperFileButton = new QPushButton("Choose from File…", &dialog);
    auto *wallpaperName = new QLabel(
        backgroundFile.isEmpty()
            ? QStringLiteral("No wallpaper")
            : QFileInfo(backgroundFile).fileName(),
        &dialog
    );
    wallpaperName->setObjectName("wallpaper_name");
    wallpaperName->setMinimumWidth(0);
    wallpaperName->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *restoreButton = new QPushButton("Restore Default", &dialog);
    wallpaperRow->addWidget(wallpaperButton);
    wallpaperRow->addWidget(wallpaperFileButton);
    wallpaperRow->addWidget(wallpaperName, 1);
    wallpaperRow->addWidget(restoreButton);
    layout->addLayout(wallpaperRow);

    auto *form = new QFormLayout;
    auto *blurSlider = new QSlider(Qt::Horizontal, &dialog);
    blurSlider->setObjectName("glass_blur_slider");
    blurSlider->setRange(0, 36);
    blurSlider->setValue(glassBlur);
    blurSlider->setTracking(false);
    auto *blurValue = new QLabel(&dialog);
    auto *blurRow = new QHBoxLayout;
    blurRow->addWidget(blurSlider, 1);
    blurRow->addWidget(blurValue);
    form->addRow("Blur strength", blurRow);

    auto *opacitySlider = new QSlider(Qt::Horizontal, &dialog);
    opacitySlider->setObjectName("glass_opacity_slider");
    opacitySlider->setRange(0, 92);
    opacitySlider->setValue(glassOpacity);
    auto *opacityValue = new QLabel(&dialog);
    auto *opacityRow = new QHBoxLayout;
    opacityRow->addWidget(opacitySlider, 1);
    opacityRow->addWidget(opacityValue);
    form->addRow("Tint opacity", opacityRow);
    layout->addLayout(form);

    auto *opacityHint = new QLabel(
        "Higher opacity makes panels darker and reveals less wallpaper.",
        &dialog
    );
    opacityHint->setObjectName("text_dim");
    layout->addWidget(opacityHint);

    auto *tintRow = new QHBoxLayout;
    auto *tintLabel = new QLabel("Dark glass tint", &dialog);
    auto *tintButton = new QPushButton(&dialog);
    tintButton->setObjectName("glass_tint_button");
    tintRow->addWidget(tintLabel);
    tintRow->addStretch(1);
    tintRow->addWidget(tintButton);
    layout->addLayout(tintRow);

    const auto refreshValues = [this, blurValue, opacityValue, blurSlider,
                                opacitySlider, tintButton]() {
        blurValue->setText(QString::number(blurSlider->value()));
        opacityValue->setText(QStringLiteral("%1%").arg(opacitySlider->value()));
        tintButton->setText(glassTint.name(QColor::HexRgb));
        tintButton->setStyleSheet(QStringLiteral(
            "background: %1; color: #f5f5f7;"
        ).arg(glassTint.name(QColor::HexRgb)));
    };
    refreshValues();

    auto *blurTimer = new QTimer(&dialog);
    blurTimer->setSingleShot(true);
    blurTimer->setInterval(140);
    connect(&dialog, &QDialog::finished, this, [this, blurSlider, blurTimer]() {
        blurTimer->stop();
        setGlassBlurStrength(blurSlider->value());
    });
    connect(blurTimer, &QTimer::timeout, this, [this, blurSlider]() {
        setGlassBlurStrength(blurSlider->value());
    });
    connect(blurSlider, &QSlider::valueChanged, &dialog, [blurTimer, refreshValues]() {
        refreshValues();
        blurTimer->start();
    });
    connect(blurSlider, &QSlider::sliderReleased, &dialog, [this, blurTimer, blurSlider]() {
        blurTimer->stop();
        setGlassBlurStrength(blurSlider->value());
    });
    connect(opacitySlider, &QSlider::valueChanged, this,
            [this, refreshValues](int value) {
        setGlassTintOpacity(value);
        refreshValues();
    });

    connect(wallpaperButton, &QPushButton::clicked, this,
            [this, wallpaperName]() {
                WallpaperPickerDialog picker(
                    availableWallpapers(),
                    backgroundFile,
                    defaultBackgroundPath(),
                    this
                );

                if (picker.exec() != QDialog::Accepted) {
                    return;
                }

                const QString chosen = picker.selectedPath();

                if (chosen.isEmpty()) {
                    return;
                }

                setBackgroundImage(chosen);
                wallpaperName->setText(QFileInfo(backgroundFile).fileName());
            });
    connect(wallpaperFileButton, &QPushButton::clicked, this,
            [this, wallpaperName]() {
        const QString start = backgroundFile.isEmpty()
            ? defaultDirectory()
            : QFileInfo(backgroundFile).absolutePath();
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Choose Studio Wallpaper",
            start,
            "Images (*.png *.jpg *.jpeg *.webp *.bmp *.svg);;All files (*)"
        );
        if (!path.isEmpty()) {
            const QString previous = backgroundFile;
            setBackgroundImage(path);
            if (backgroundFile != previous || path == previous) {
                wallpaperName->setText(QFileInfo(backgroundFile).fileName());
            }
        }
    });
    connect(restoreButton, &QPushButton::clicked, this,
            [this, wallpaperName]() {
        setBackgroundImage(defaultBackgroundPath());
        wallpaperName->setText(QFileInfo(backgroundFile).fileName());
    });
    connect(tintButton, &QPushButton::clicked, &dialog, [this, &dialog, refreshValues]() {
        const QColor selected = QColorDialog::getColor(
            glassTint,
            &dialog,
            "Choose a dark glass tint"
        );
        if (selected.isValid()) {
            setGlassTintColour(selected);
            refreshValues();
        }
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto *resetButton = buttons->addButton(
        "Reset to Defaults",
        QDialogButtonBox::ResetRole
    );
    connect(resetButton, &QPushButton::clicked, &dialog,
            [this, blurSlider, opacitySlider, wallpaperName, refreshValues, blurTimer]() {
        blurTimer->stop();
        resetAppearanceDefaults();
        blurSlider->setValue(theme::defaultGlassBlur);
        opacitySlider->setValue(theme::defaultGlassTintOpacity);
        wallpaperName->setText(QFileInfo(backgroundFile).fileName());
        refreshValues();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(theme::wallpaper));

    if (!wallpaper.isNull()) {
        const qreal ratio = devicePixelRatioF();

        if (scaledSize != size() || !qFuzzyCompare(scaledRatio, ratio)) {
            scaledWallpaper = wallpaper.scaled(
                size() * ratio,
                Qt::KeepAspectRatioByExpanding,
                Qt::SmoothTransformation
            );
            scaledWallpaper.setDevicePixelRatio(ratio);
            scaledSize = size();
            scaledRatio = ratio;
            glassBackdropDirty = true;
        }

        const QSize shown = (scaledWallpaper.size() / ratio);
        painter.drawPixmap(
            (width() - shown.width()) / 2,
            (height() - shown.height()) / 2,
            scaledWallpaper
        );
    }

    QColor overlay(theme::wallpaper);
    overlay.setAlpha(darkness);

    painter.fillRect(event->rect(), overlay);

    const qreal ratio = devicePixelRatioF();
    if (glassBackdropDirty || glassBackdropSize != size() ||
        !qFuzzyCompare(glassBackdropRatio, ratio)) {
        QImage source;

        if (!scaledWallpaper.isNull()) {
            const QImage expanded = scaledWallpaper.toImage();
            const QSize targetSize(
                qRound(width() * ratio),
                qRound(height() * ratio)
            );
            const QSize cropSize(
                qMin(expanded.width(), targetSize.width()),
                qMin(expanded.height(), targetSize.height())
            );
            const QRect crop(
                (expanded.width() - cropSize.width()) / 2,
                (expanded.height() - cropSize.height()) / 2,
                cropSize.width(),
                cropSize.height()
            );
            source = expanded.copy(crop);
            source.setDevicePixelRatio(ratio);
        }

        glassBackdrop = makeGlassBackdrop(source, glassBlur, ratio);
        glassBackdropSize = size();
        glassBackdropRatio = ratio;
        glassBackdropDirty = false;
        refreshGlassSurfaces();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    scaledSize = QSize();
    glassBackdropDirty = true;
}

void MainWindow::refreshGlassSurfaces() {
    const QList<QWidget *> widgets = findChildren<QWidget *>();

    for (QWidget *widget : widgets) {
        if (auto *surface = dynamic_cast<GlassSurface *>(widget)) {
            surface->setAppearance(glassTint, glassOpacity);
        }
    }
}

QTabBar *MainWindow::documentTabBar() const {
    return tabs->tabBar();
}

TabCloseButton *MainWindow::tabButtonAt(int index) const {
    if (index < 0 || index >= tabs->count()) {
        return nullptr;
    }

    return qobject_cast<TabCloseButton *>(
        tabs->tabBar()->tabButton(index, QTabBar::RightSide)
    );
}

bool MainWindow::documentShowsDirtyMarker(int index) const {
    const TabCloseButton *button = tabButtonAt(index);

    return button != nullptr && button->showsDirtyMarker();
}

void MainWindow::attachHighlighter(DocumentEditor *document) {
    new PebbleHighlighter(document->document(), syntaxColourMap);
    connect(document, &CodeEditor::goToDefinitionRequested, this,
            [this](const QString &path, int line, int column) {
                static_cast<void>(navigateToDefinition(path, line, column));
            });
    connect(document, &CodeEditor::returnFromDefinitionRequested,
            this, &MainWindow::returnToDefinition);
    document->setIntelliSenseProjectPackages(projectPackageSearchPaths,
                                              projectPackageNames);
    const QString sourcePath = document->path.isEmpty()
        ? (projectDir.isEmpty()
            ? QString()
            : QDir(projectDir).filePath(QStringLiteral("__pebble_editor__.peb")))
        : document->path;
    document->setIntelliSenseSourcePath(sourcePath);
}

void MainWindow::refreshIntelliSenseProjectPackages() {
    projectPackageSearchPaths.clear();
    projectPackageNames.clear();
    const QStringList watchedManifests = projectManifestWatcher->files();
    if (!watchedManifests.isEmpty()) projectManifestWatcher->removePaths(watchedManifests);

    if (!projectDir.isEmpty()) {
        const QString manifestPath = QDir(projectDir).filePath(QStringLiteral("pebble.toml"));
        if (QFileInfo::exists(manifestPath)) {
            projectManifestWatcher->addPath(manifestPath);
        }
        PebbleProject project = {};
        PebbleManifestDiagnostics manifestDiagnostics = {};
        const QByteArray root = QFile::encodeName(projectDir);
        if (pebble_project_load(root.constData(), &project, &manifestDiagnostics)) {
            static_cast<void>(pebble_project_resolve(&project, &manifestDiagnostics));
            for (std::size_t index = 0; index < project.packages.count; ++index) {
                const PebbleResolvedPackage &package = project.packages.items[index];
                if (package.directory != nullptr) {
                    projectPackageSearchPaths.append(QString::fromUtf8(package.directory));
                }
                if (package.name != nullptr) {
                    projectPackageNames.append(QString::fromUtf8(package.name));
                }
            }
        }
        pebble_manifest_diagnostics_destroy(&manifestDiagnostics);
        pebble_project_destroy(&project);
    }

    for (int index = 0; index < tabs->count(); ++index) {
        DocumentEditor *document = documentAt(index);
        if (document == nullptr) continue;
        document->setIntelliSenseProjectPackages(projectPackageSearchPaths,
                                                  projectPackageNames);
        const QString sourcePath = document->path.isEmpty()
            ? (projectDir.isEmpty()
                ? QString()
                : QDir(projectDir).filePath(QStringLiteral("__pebble_editor__.peb")))
            : document->path;
        document->setIntelliSenseSourcePath(sourcePath);
    }
}

void MainWindow::inform(const QString &title, const QString &text) {
    if (notifier) {
        notifier(title, text);
        return;
    }

    QMessageBox::information(this, title, text);
}

void MainWindow::refreshWelcome() {
    if (welcome == nullptr || editorStack == nullptr || tabs == nullptr) {
        return;
    }

    QWidget *page = static_cast<QWidget *>(tabs);

    if (tabs->count() == 0) {
        page = welcomeClosed ? blankPage : static_cast<QWidget *>(welcome);
    }

    editorStack->setCurrentWidget(page);
}

bool MainWindow::welcomePageVisible() const {
    return editorStack != nullptr && editorStack->currentWidget() == welcome;
}

QStringList MainWindow::examplesDirectories() const {
    QStringList directories;

    const auto add = [&directories](const QString &path) {
        const QString cleaned = QDir::cleanPath(QDir(path).absolutePath());

        if (!directories.contains(cleaned)) {
            directories << cleaned;
        }
    };

    if (!projectDir.isEmpty()) {
        add(QDir(projectDir).filePath(QStringLiteral("examples")));
    }

    const QDir application(QCoreApplication::applicationDirPath());
    add(application.filePath(QStringLiteral("examples")));
    add(application.filePath(QStringLiteral("../examples")));
    add(application.filePath(QStringLiteral("../../examples")));
    add(application.filePath(QStringLiteral("../share/pebble/examples")));
    add(application.filePath(QStringLiteral("../../share/pebble/examples")));

    return directories;
}

QStringList MainWindow::examplePaths() const {
    for (const QString &path : examplesDirectories()) {
        const QDir directory(path);

        if (!directory.exists()) {
            continue;
        }

        QStringList paths;

        QDirIterator iterator(
            path,
            QStringList{QStringLiteral("*.") + sourceSuffix()},
            QDir::Files,
            QDirIterator::Subdirectories
        );

        while (iterator.hasNext()) {
            iterator.next();
            paths << iterator.filePath();
        }

        if (paths.isEmpty()) {
            continue;
        }

        paths.sort();
        return paths;
    }

    return QStringList();
}

QString MainWindow::exampleLabel(const QString &path) const {
    const QString absolute = QFileInfo(path).absoluteFilePath();

    for (const QString &root : examplesDirectories()) {
        const QString base = QDir(root).absolutePath();

        if (absolute.startsWith(base + "/")) {
            return absolute.mid(base.size() + 1);
        }
    }

    return QFileInfo(path).fileName();
}

QStringList MainWindow::exampleAssetDirectories() const {
    QStringList directories;

    for (const QString &root : examplesDirectories()) {
        const QDir directory(root);

        if (!directory.exists()) {
            continue;
        }

        QDirIterator iterator(
            root,
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
            QDirIterator::Subdirectories
        );

        while (iterator.hasNext()) {
            iterator.next();

            const QFileInfo info(iterator.filePath());

            if (!info.isDir()) {
                continue;
            }

            const QString path = info.absoluteFilePath();

            if (directories.contains(path)) {
                continue;
            }

            if (QDir(path).entryList(QDir::Files).isEmpty()) {
                continue;
            }

            directories << path;
        }
    }

    return directories;
}


void MainWindow::openExample() {
    const QStringList paths = examplePaths();

    if (paths.isEmpty()) {
        inform(
            QStringLiteral("No examples"),
            QStringLiteral("No Pebble examples were found in:\n%1")
                .arg(examplesDirectories().join(QLatin1Char('\n')))
        );
        return;
    }

    QStringList names;

    for (const QString &path : paths) {
        names << exampleLabel(path);
    }

    bool accepted = false;
    const QString chosen = QInputDialog::getItem(
        this,
        QStringLiteral("Open Example"),
        QStringLiteral("Choose an example:"),
        names,
        0,
        false,
        &accepted
    );

    if (!accepted) {
        return;
    }

    const int index = static_cast<int>(names.indexOf(chosen));

    if (index >= 0) {
        openPath(paths.at(index));
    }
}

bool MainWindow::sendProgramInput(const QString &answer) {
    if (runProcess == nullptr || runProcess->state() != QProcess::Running) {
        return false;
    }

    const qint64 written = runProcess->write((answer + QLatin1Char('\n')).toUtf8());

    if (written < 0) {
        appendOutput(
            QStringLiteral("could not send input: %1").arg(runProcess->errorString()),
            OutputLevel::Error
        );
        return false;
    }

    appendOutput(QStringLiteral("> ") + answer, OutputLevel::Normal);

    return true;
}

void MainWindow::submitProgramInput() {
    if (programInput != nullptr && sendProgramInput(programInput->text())) {
        programInput->clear();
    }
}

void MainWindow::loadSyntaxColours() {
    QSettings settings(settingsOrganisation, settingsApplication);
    SyntaxColourMap stored;

    for (const QString &key : syntaxColourKeys()) {
        stored.insert(key, settings.value(QStringLiteral("syntax/") + key).toString());
    }

    syntaxColourMap = normalisedSyntaxColours(stored);
}

void MainWindow::applySyntaxColours(const SyntaxColourMap &colours) {
    syntaxColourMap = normalisedSyntaxColours(colours);

    QSettings settings(settingsOrganisation, settingsApplication);

    for (const QString &key : syntaxColourKeys()) {
        settings.setValue(QStringLiteral("syntax/") + key, syntaxColourMap.value(key));
    }

    for (int index = 0; index < tabs->count(); ++index) {
        DocumentEditor *document = documentAt(index);

        if (document == nullptr) {
            continue;
        }

        const auto highlighters = document->document()->findChildren<PebbleHighlighter *>();

        for (PebbleHighlighter *highlighter : highlighters) {
            highlighter->setColours(syntaxColourMap);
        }
    }
}

void MainWindow::chooseSyntaxColours() {
    SyntaxColoursDialog dialog(syntaxColourMap, this);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    applySyntaxColours(dialog.colours());
    statusBar()->showMessage(QStringLiteral("Syntax colours updated"));
}

void MainWindow::showLanguageReference() {
    inform(
        QStringLiteral("Pebble Language Reference"),
        QStringLiteral(
            "fn main() { ... }\nThe entry point of a program.\n\n"
            "let name = \"Pebble\";\nDeclare a variable that cannot change.\n\n"
            "var count = 0;\nDeclare a variable that can change.\n\n"
            "print(\"Hello\", 42);\nPrint any number of values.\n\n"
            "if condition { ... } else { ... }\nConditional execution.\n\n"
            "while condition { ... }\nRepeat a block while the condition holds.\n\n"
            "fn add(a: Int, b: Int) -> Int { return a + b; }\nDefine a function.\n\n"
            "import package math\nUse the functions of a package.\n\n"
            "// A comment\nThe rest of the line is ignored."
        )
    );
}

void MainWindow::showAbout() {
    inform(
        QStringLiteral("About Pebble Studio"),
        QStringLiteral(
            "Pebble Studio is a small programming environment for the Pebble "
            "language.\n\n(Built with C++ and Qt 6)"
        )
    );
}

QString MainWindow::statusEncoding() const {
    if (currentDocument() == nullptr || statusEncodingLabel == nullptr) {
        return QString();
    }

    return statusEncodingLabel->text();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (isProcessRunning()) {
        closePending = true;
        requestStop();
        event->ignore();
        return;
    }

    if (isBuildRunning()) {
        closePending = true;
        stoppingProcess = buildProcess;
        buildProcess->terminate();
        killTimer->start();
        event->ignore();
        return;
    }

    for (int index = 0; index < tabs->count(); ++index) {
        DocumentEditor *document = documentAt(index);

        if (document == nullptr || !document->dirty) {
            continue;
        }

        tabs->setCurrentIndex(index);

        const UnsavedChoice choice = askUnsaved(document);

        if (choice == UnsavedChoice::Cancel) {
            event->ignore();
            return;
        }

        if (choice == UnsavedChoice::Save) {
            if (!saveCurrent() || document->dirty) {
                event->ignore();
                return;
            }
        }
    }

    persistSettings();
    event->accept();
}

void MainWindow::persistSettings() const {
    QSettings settings(settingsOrganisation, settingsApplication);
    settings.setValue("size", size());

    if (projectDir.isEmpty()) {
        settings.remove("projectDir");
    } else {
        settings.setValue("projectDir", projectDir);
    }

    QStringList openFiles;

    for (int index = 0; index < tabs->count(); ++index) {
        const DocumentEditor *document = documentAt(index);

        if (document != nullptr && !document->path.isEmpty()) {
            openFiles << document->path;
        }
    }

    settings.setValue("openFiles", openFiles);

    settings.setValue("explorerShown", explorerShown);
    settings.setValue("outputShown", outputShown);
    settings.setValue("splitterState", splitter->saveState());
    settings.setValue("mainSplitterState", mainSplitter->saveState());
}

QStringList MainWindow::recentFiles() const {
    return recentFilePaths;
}

QMenu *MainWindow::recentFilesMenu() const {
    return recentFilesMenuWidget;
}

void MainWindow::recordRecentFile(const QString &path) {
    if (restoringSession) {
        return;
    }

    const QString normalized = normalizePath(path);

    if (normalized.isEmpty()) {
        return;
    }

    recentFilePaths.removeAll(normalized);
    recentFilePaths.prepend(normalized);

    while (recentFilePaths.size() > kRecentFileLimit) {
        recentFilePaths.removeLast();
    }

    saveRecentFiles();
    refreshRecentFilesMenu();
}

void MainWindow::loadRecentFiles() {
    QSettings settings(settingsOrganisation, settingsApplication);
    recentFilePaths = settings.value("recentFiles").toStringList();

    pruneRecentFiles();
    saveRecentFiles();
    refreshRecentFilesMenu();
}

void MainWindow::pruneRecentFiles() {
    QStringList kept;

    for (const QString &path : recentFilePaths) {
        const QString normalized = normalizePath(path);

        if (normalized.isEmpty() || kept.contains(normalized)) {
            continue;
        }

        if (!QFileInfo(normalized).isFile()) {
            continue;
        }

        kept << normalized;

        if (kept.size() == kRecentFileLimit) {
            break;
        }
    }

    recentFilePaths = kept;
}

void MainWindow::saveRecentFiles() const {
    QSettings settings(settingsOrganisation, settingsApplication);
    settings.setValue("recentFiles", recentFilePaths);
}

void MainWindow::renameRecentFile(const QString &path, const QString &newPath) {
    const qsizetype index = recentFilePaths.indexOf(normalizePath(path));

    if (index < 0) {
        return;
    }

    recentFilePaths[index] = normalizePath(newPath);
    saveRecentFiles();
    refreshRecentFilesMenu();
}

void MainWindow::refreshRecentFilesMenu() {
    if (welcome != nullptr) {
        welcome->setRecentFiles(recentFilePaths);
    }

    if (recentFilesMenuWidget == nullptr) {
        return;
    }

    recentFilesMenuWidget->clear();

    for (const QString &path : recentFilePaths) {
        const QFileInfo info(path);

        auto *action = recentFilesMenuWidget->addAction(info.fileName());
        action->setToolTip(path);

        connect(
            action,
            &QAction::triggered,
            this,
            [this, path]() {
                openRecentFile(path);
            }
        );
    }

    recentFilesMenuWidget->setEnabled(!recentFilePaths.isEmpty());
}

void MainWindow::openRecentFile(const QString &path) {
    if (openPath(path)) {
        return;
    }

    recentFilePaths.removeAll(normalizePath(path));
    saveRecentFiles();
    refreshRecentFilesMenu();
}

QString MainWindow::normalizePath(const QString &path) {
    if (path.isEmpty()) {
        return QString();
    }

    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

DocumentEditor *MainWindow::currentDocument() const {
    return qobject_cast<DocumentEditor *>(tabs->currentWidget());
}

QString MainWindow::defaultDirectory() const {
    if (!projectDir.isEmpty()) {
        return projectDir;
    }

    return QDir::homePath();
}

QString MainWindow::defaultSavePath(const DocumentEditor *document) const {
    if (!document->path.isEmpty()) {
        return document->path;
    }

    return defaultDirectory() + "/" + untitledName();
}
