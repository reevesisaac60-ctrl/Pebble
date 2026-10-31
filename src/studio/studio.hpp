#pragma once

#include "theme.hpp"
#include "intellisense.hpp"

#include <QColor>
#include <QDialog>
#include <QImage>
#include <QMainWindow>
#include <QMap>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QToolButton>
#include <QTreeWidget>
#include <QVariantAnimation>

#include <functional>

class QAction;
class QCheckBox;
class CodeEditor;
class QCloseEvent;
class QContextMenuEvent;
class QEvent;
class QFocusEvent;
class QKeyEvent;
class QLabel;
class QFrame;
class QFileSystemWatcher;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QPushButton;
class QShortcut;
class QSplitter;
class QStackedWidget;
class QTimer;
class QTreeWidgetItem;
class QShowEvent;
class QResizeEvent;
class QMoveEvent;
class QPaintEvent;

struct ExecutableResolution {
    bool found = false;
    QString path;
    QString reason;
};

ExecutableResolution resolvePebbleExecutable(
    const QString &studioBinary,
    const QString &envPebble,
    const QString &searchPath
);

struct DiagnosticEntry {
    QString path;
    int line = 0;
    int column = 0;
    QString severity;
    QString message;
};

bool parseDiagnosticLine(const QString &line, DiagnosticEntry &entry);

enum class SyntaxCategory {
    Plain,
    Keyword,
    Literal,
    Builtin,
    Function,
    Type,
    Declaration,
    String,
    Number,
    Comment,
    Operator
};

SyntaxCategory syntaxCategoryAt(const QString &line, int offset);

using SyntaxColourMap = QMap<QString, QString>;

QStringList syntaxColourKeys();
QString syntaxColourLabel(const QString &key);
SyntaxColourMap defaultSyntaxColours();
SyntaxColourMap normalisedSyntaxColours(const SyntaxColourMap &colours);

class PebbleHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit PebbleHighlighter(
        QTextDocument *document,
        const SyntaxColourMap &colours = SyntaxColourMap()
    );

    void setColours(const SyntaxColourMap &colours);
    SyntaxColourMap colours() const { return currentColours; }

protected:
    void highlightBlock(const QString &text) override;

private:
    SyntaxColourMap currentColours;
};

class SyntaxColoursDialog : public QDialog {
    Q_OBJECT

public:
    explicit SyntaxColoursDialog(
        const SyntaxColourMap &colours,
        QWidget *parent = nullptr
    );

    SyntaxColourMap colours() const { return currentColours; }
    void setColour(const QString &key, const QString &colour);
    void resetDefaults();
    void chooseColour(const QString &key);

    QPushButton *colourButton(const QString &key) const;
    QPushButton *resetButton() const { return resetButtonWidget; }
    QPlainTextEdit *previewEditor() const { return previewWidget; }

private:
    void refreshColour(const QString &key);

    SyntaxColourMap currentColours;
    QMap<QString, QPushButton *> buttons;
    QPlainTextEdit *previewWidget = nullptr;
    PebbleHighlighter *previewHighlighter = nullptr;
    QPushButton *resetButtonWidget = nullptr;
};

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditor *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    CodeEditor *editor = nullptr;
};

class WallpaperPickerDialog : public QDialog {
    Q_OBJECT

public:
    WallpaperPickerDialog(
        const QStringList &wallpapers,
        const QString &currentPath,
        const QString &defaultPath,
        QWidget *parent = nullptr
    );

    [[nodiscard]] QString selectedPath() const { return selected; }
    [[nodiscard]] QStringList offeredPaths() const;
    [[nodiscard]] QString displayedName(const QString &path) const;

    QListWidget *wallpaperList() const { return list; }
    QLabel *previewLabel() const { return preview; }
    QLabel *nameLabel() const { return nameLabelWidget; }

    static QString displayNameFor(const QString &path);

private:
    void refreshPreview(QListWidgetItem *item);

    QListWidget *list = nullptr;
    QLabel *preview = nullptr;
    QLabel *nameLabelWidget = nullptr;
    QString selected;
};

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    struct BlockEdit {
        int blockNumber = 0;
        int offset = 0;
        int length = 0;
        bool insertion = false;
        QString text;
    };

    struct EditorPoint {
        int blockNumber = 0;
        int column = 0;
    };

    explicit CodeEditor(QWidget *parent = nullptr);

    void setIntelliSenseSourcePath(const QString &path);
    void setIntelliSenseProjectPackages(
        const QStringList &searchPaths,
        const QStringList &packageNames
    );
    QString ghostText() const { return ghostSuffix; }
    QStringList completionNames() const;
    bool completionPopupVisible() const;
    int completionPopupCurrentRow() const;
    QString parameterHintText() const;
    int activeParameterHintIndex() const;
    IntelliSenseDefinition definitionAtCursor() const {
        return intelliSense.definitionAt(textCursor().position());
    }
    QString indexedSourcePath() const { return intelliSenseSourcePath; }
    void requestCompletions();
    IntelliSenseHover hoverDocumentationAt(int position) const {
        return intelliSense.hoverAt(position);
    }

    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

    int lineNumberAreaWidth() const;

    QTextBlock firstVisibleBlock() const;
    QRectF blockBoundingGeometry(const QTextBlock &block) const;
    QRectF blockBoundingRect(const QTextBlock &block) const;
    QPointF contentOffset() const;

    int currentLineNumber() const;
    bool isCurrentLine(const QTextBlock &block) const;

    static int indentWidth();
    static QString indentText();

    void indent();
    void outdent();
    void toggleComment();
    void newlineKeepIndent();
    void applyLineHeight();
    int currentLineTint() const { return lineTint; }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void paintWhitespaceMarkers();
    bool eventFilter(QObject *watched, QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

signals:
    void goToDefinitionRequested(const QString &path, int line, int column);
    void returnFromDefinitionRequested();

private:
    QList<QTextBlock> selectedBlocks() const;
    void insertIndentation();
    void applyBlockEdits(const QList<BlockEdit> &edits);
    EditorPoint pointOf(int position) const;
    int mappedColumn(
        const QList<BlockEdit> &edits,
        int blockNumber,
        int column
    ) const;
    int positionOf(const EditorPoint &point, const QList<BlockEdit> &edits) const;
    void updateLineNumberAreaWidth();
    void updateLineNumberAreaGeometry();
    void highlightCurrentLine();
    void rebuildIntelliSense();
    void refreshIntelliSenseWatchPaths();
    void refreshCompletions(bool explicitRequest = false);
    void hideCompletions();
    void acceptCompletion(int row = -1);
    void moveCompletionSelection(int delta);
    void positionCompletionPopup();
    void showHoverDocumentation();
    void hideHoverDocumentation();
    void refreshParameterHint();

    LineNumberArea *lineNumberArea = nullptr;
    int lineTint = 14;
    IntelliSenseModel intelliSense;
    QString intelliSenseSourcePath;
    QString ghostSuffix;
    QList<IntelliSenseCompletion> completionItems;
    QTimer *intelliSenseTimer = nullptr;
    QTimer *hoverTimer = nullptr;
    QFileSystemWatcher *intelliSenseWatcher = nullptr;
    QFrame *completionPopup = nullptr;
    QListWidget *completionList = nullptr;
    QFrame *parameterHint = nullptr;
    QLabel *parameterHintLabel = nullptr;
    int hoverPosition = -1;
    bool intelliSenseDirty = true;
    bool suggestionDismissed = false;
};

class DocumentEditor : public CodeEditor {
    Q_OBJECT

public:
    explicit DocumentEditor(QWidget *parent = nullptr);

    QString path;
    QString title;
    bool dirty = false;

signals:
    void dirtyChanged(DocumentEditor *document);
};

class TabCloseButton : public QToolButton {
    Q_OBJECT

public:
    explicit TabCloseButton(QWidget *parent = nullptr);

    void setDirty(bool changed);
    bool isDirty() const { return dirty; }
    bool showsDirtyMarker() const { return dirty && !hovered; }

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void refresh();

    bool dirty = false;
    bool hovered = false;
};

class AnimatedTabBar : public QTabBar {
    Q_OBJECT

public:
    explicit AnimatedTabBar(QWidget *parent = nullptr);

    QRect indicatorRect() const { return indicator; }
    QRect indicatorTarget() const;
    bool isAnimating() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void tabLayoutChange() override;

private:
    void animateTo(int index);
    void snapToCurrent();

    QRect indicator;
    QVariantAnimation *animation = nullptr;
};

class DocumentTabs : public QTabWidget {
    Q_OBJECT

public:
    explicit DocumentTabs(QWidget *parent = nullptr);

signals:
    void tabCountChanged();

protected:
    void tabInserted(int index) override;
    void tabRemoved(int index) override;
};

class WelcomePage : public QWidget {
    Q_OBJECT

public:
    explicit WelcomePage(QWidget *parent = nullptr);

    void setRecentFiles(const QStringList &paths);
    QStringList recentFiles() const { return recentPaths; }
    bool showsNoRecentFiles() const;

    QTabBar *tabBar() const { return stripTabBar; }
    TabCloseButton *closeButton() const { return stripCloseButton; }

    QPushButton *newFileButton() const { return newFileButtonWidget; }
    QPushButton *openFileButton() const { return openFileButtonWidget; }
    QPushButton *openExampleButton() const { return openExampleButtonWidget; }
    QList<QPushButton *> recentButtons() const { return recentButtonList; }

signals:
    void newFileRequested();
    void openFileRequested();
    void openExampleRequested();
    void recentFileRequested(const QString &path);
    void closeRequested();

private:
    QPushButton *addLink(QWidget *parent, const QString &label);

    QTabBar *stripTabBar = nullptr;
    TabCloseButton *stripCloseButton = nullptr;

    QWidget *recentHost = nullptr;
    QPushButton *newFileButtonWidget = nullptr;
    QPushButton *openFileButtonWidget = nullptr;
    QPushButton *openExampleButtonWidget = nullptr;
    QList<QPushButton *> recentButtonList;
    QStringList recentPaths;
};

class FindReplaceBar : public QWidget {
    Q_OBJECT

public:
    explicit FindReplaceBar(QWidget *parent = nullptr);

    void open(bool withReplace);
    void closeBar();
    bool isOpen() const { return opened; }

    QString findText() const;
    void setFindText(const QString &text);
    QString replaceText() const;
    void setReplaceText(const QString &text);
    bool matchCase() const;
    void setMatchCase(bool on);
    QString status() const;
    void setStatus(const QString &text);

    QLineEdit *findField() const { return findFieldWidget; }
    QLineEdit *replaceField() const { return replaceFieldWidget; }
    QToolButton *matchCaseCheck() const { return matchCaseWidget; }
    QPushButton *previousButton() const { return previousButtonWidget; }
    QPushButton *nextButton() const { return nextButtonWidget; }
    QPushButton *replaceButton() const { return replaceButtonWidget; }
    QPushButton *replaceAllButton() const { return replaceAllButtonWidget; }
    QPushButton *closeButton() const { return closeButtonWidget; }
    QLabel *statusLabel() const { return statusWidget; }
    bool replaceRowVisible() const;

signals:
    void findNextRequested();
    void findPreviousRequested();
    void replaceRequested(const QString &replacement);
    void replaceAllRequested(const QString &replacement);
    void matchCaseChanged(bool on);
    void closeRequested();
    void findTextChanged(const QString &text);
    void findSubmitted(const QString &text);
    void replaceSubmitted(const QString &replacement);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QLineEdit *findFieldWidget = nullptr;
    QLineEdit *replaceFieldWidget = nullptr;
    QToolButton *matchCaseWidget = nullptr;
    QPushButton *previousButtonWidget = nullptr;
    QPushButton *nextButtonWidget = nullptr;
    QPushButton *replaceButtonWidget = nullptr;
    QPushButton *replaceAllButtonWidget = nullptr;
    QPushButton *closeButtonWidget = nullptr;
    QLabel *statusWidget = nullptr;
    QWidget *replaceRowWidget = nullptr;
    bool opened = false;
};

class ProjectTree : public QTreeWidget {
    Q_OBJECT

public:
    explicit ProjectTree(QWidget *parent = nullptr);

signals:
    void refreshRequested();

protected:
    void showEvent(QShowEvent *event) override;
};

enum class OutputLevel {
    Normal,
    Info,
    Warning,
    Error
};

class OutputHighlighter : public QSyntaxHighlighter {
public:
    explicit OutputHighlighter(QTextDocument *document);

protected:
    void highlightBlock(const QString &text) override;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    enum class ProcessKind {
        Build,
        Run
    };

    enum class UnsavedChoice {
        Save,
        Discard,
        Cancel
    };

    using UnsavedPrompt = std::function<UnsavedChoice(const QString &title)>;
    using Notifier = std::function<void(const QString &title, const QString &text)>;

    explicit MainWindow(
        const QString &organisation = QStringLiteral("Pebble"),
        const QString &application = QStringLiteral("Pebble Studio"),
        QWidget *parent = nullptr
    );
    ~MainWindow() override;

    int documentCount() const;
    QString documentTitle(int index) const;
    QString documentLabel(int index) const;
    QString documentPath(int index) const;
    bool documentDirty(int index) const;
    QString documentText(int index) const;
    DocumentEditor *documentAt(int index) const;
    QString projectDirectory() const;
    bool navigateToDefinition(const QString &path, int line, int column);
    bool returnFromDefinition();
    int definitionHistoryDepth() const;

    void newFile();
    bool openPath(const QString &path);
    bool saveCurrent();
    bool saveAsPath(const QString &path);
    void closeDocument(int index);
    void installTabCloseButton(int index);
    bool createFileAt(const QString &directory, const QString &name);
    bool createDirectoryAt(const QString &directory, const QString &name);
    bool renamePath(const QString &path, const QString &newName);
    bool deletePath(const QString &path);
    bool restoreProject();
    QStringList projectTreeEntries() const;
    QTreeWidget *projectTree() const;

    QWidget *activityBar() const;
    bool explorerVisible() const;
    bool outputPaneVisible() const;
    void setExplorerVisible(bool visible);
    void setOutputPaneVisible(bool visible);
    void syncWorkspaceActions();
    void setProcessProgram(const QString &program);
    QString outputText() const;
    void clearOutput();
    int outputBlockCount() const;
    int outputMaximumBlockCount() const;
    void setOutputMaximumBlockCount(int count);
    QString diagnosticsTitle() const;
    QPushButton *clearOutputButton() const { return clearOutputButtonWidget; }
    bool isProcessRunning() const;
    bool isBuildRunning() const;

    QString statusDocumentName() const;
    QString statusCursorPosition() const;
    QString statusState() const;
    QString windowTitleText() const;

    QStringList recentFiles() const;
    QMenu *recentFilesMenu() const;

    QString resolvedProgramPath() const { return resolvedProgram; }

    QString resolveExecutable();

    void find(const QString &text, bool matchCase = false);
    void findNext();
    void findPrevious();
    void replace(const QString &replacement);
    int replaceAll(const QString &replacement);
    void setMatchCase(bool on);
    bool matchCase() const { return findMatchCase; }
    QString findText() const { return findTerm; }
    QString findStatus() const { return findStatusMessage; }

    FindReplaceBar *findReplaceBar() const;
    bool findBarVisible() const;
    void openFindBar(bool withReplace);
    void closeFindBar();

    void feedCompilerOutput(const QString &text);
    void flushCompilerOutput();
    int diagnosticCount() const;
    QString diagnosticText(int index) const;
    QListWidget *diagnosticsList() const;
    QString resolveDiagnosticPath(const QString &path) const;
    void navigateToDiagnostic(int index);
    int currentDocumentIndex() const;

    void setUnsavedPrompt(UnsavedPrompt prompt);
    void setNotifier(Notifier notifier);

    QTabBar *documentTabBar() const;
    bool documentShowsDirtyMarker(int index) const;

    WelcomePage *welcomePage() const { return welcome; }
    bool welcomePageVisible() const;
    bool welcomeDismissed() const { return welcomeClosed; }
    QStringList examplesDirectories() const;
    QStringList examplePaths() const;
    QString exampleLabel(const QString &path) const;
    QStringList exampleAssetDirectories() const;
    void openExample();

    QLineEdit *programInputField() const { return programInput; }
    bool sendProgramInput(const QString &answer);

    SyntaxColourMap syntaxColours() const { return syntaxColourMap; }
    void applySyntaxColours(const SyntaxColourMap &colours);
    void chooseSyntaxColours();

    void showLanguageReference();
    void showAbout();

    QString backgroundPath() const { return backgroundFile; }
    QString selectedWallpaperName() const { return backgroundAssetName; }
    int backgroundDarkness() const { return darkness; }
    bool backgroundLoaded() const { return !wallpaper.isNull(); }
    QString defaultBackgroundPath() const;
    void setBackgroundImage(const QString &path);
    void setBackgroundWallpaper(const QString &name);
    QStringList availableWallpapers() const;
    const QImage &glassBackdropImage() const { return glassBackdrop; }
    void setBackgroundDarkness(int value);
    void chooseBackground();
    void chooseWallpaper();
    void chooseBackgroundDarkness();
    int glassBlurStrength() const { return glassBlur; }
    int glassTintOpacity() const { return glassOpacity; }
    QColor glassTintColour() const { return glassTint; }
    void setGlassBlurStrength(int value);
    void setGlassTintOpacity(int value);
    void setGlassTintColour(const QColor &colour);
    void resetAppearanceDefaults();
    void configureGlassAppearance();

    QString statusEncoding() const;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void openProject();
    void openTreeFile(QTreeWidgetItem *item, int column);
    void createFile(QTreeWidgetItem *item);
    void createDirectory(QTreeWidgetItem *item);
    void renameItem(QTreeWidgetItem *item);
    void deleteItem(QTreeWidgetItem *item);
    void treeContextMenu(const QPoint &position);
    void saveAsCurrent();
    void build();
    void buildCurrentFile();
    void runProject();
    void runCurrentFile();
    void stopProgram();
    void killProcess();
    void navigateDiagnostic(QListWidgetItem *item);
    void refreshDocumentLabel(DocumentEditor *document);
    void activeDocumentChanged(int index);
    void findTextEdited(const QString &text);
    void findSubmitted(const QString &text);
    void replaceSubmitted(const QString &replacement);
    void matchCaseToggled(bool on);
    void returnToDefinition();

    void undoActiveDocument();
    void redoActiveDocument();
    void selectAllActiveDocument();
    void nextDocument();
    void previousDocument();

private:
    struct NavigationLocation {
        QString path;
        int tabIndex = -1;
        int line = 1;
        int column = 1;
    };
    DocumentEditor *currentDocument() const;
    bool restoreNavigationLocation(const NavigationLocation &location);
    void updateEditActions();
    void watchDocument(DocumentEditor *document);
    bool writeDocument(DocumentEditor *document, const QString &path);
    bool saveAsDialog();
    UnsavedChoice askUnsaved(const DocumentEditor *document);
    void notify(const QString &title, const QString &text);
    void startProcess(
        ProcessKind kind,
        const QStringList &arguments,
        const QString &workingDirectory
    );
    void connectProcess(QProcess *process, ProcessKind kind);
    void processFinished(
        QProcess *process,
        ProcessKind kind,
        int exitCode,
        QProcess::ExitStatus status
    );
    void processError(
        QProcess *process,
        ProcessKind kind,
        QProcess::ProcessError error
    );
    void readProcessOutput(QProcess *process, OutputLevel level);
    void appendOutput(const QString &text, OutputLevel level);
    void flushProcessOutput(QProcess *process);
    QString workingDirectoryFor(const QString &path) const;
    void requestStop();
    void updateProcessActions();
    void reportProcessMessage(const QString &message);
    void updateStatus();
    void updateWindowTitle();
    void handleDiagnosticLine(const QString &line);
    void addDiagnostic(const DiagnosticEntry &entry);
    void clearDiagnostics();
    void updateDiagnosticsTitle();
    void populateTree(const QString &directory, QTreeWidgetItem *parent);
    void collectTreeEntries(
        const QTreeWidgetItem *item,
        const QString &prefix,
        QStringList &entries
    ) const;
    QTreeWidgetItem *findTreeItem(
        QTreeWidgetItem *parent,
        const QString &path
    ) const;
    QString selectedTreePath() const;
    void selectTreePath(const QString &path);
    void refreshProjectTree();
    void persistSettings() const;
    QString defaultDirectory() const;
    QString defaultSavePath(const DocumentEditor *document) const;
    void setFindStatus(const QString &text);
    bool selectionMatchesTerm(const QString &selected) const;
    void refreshWelcome();
    void loadSyntaxColours();
    void loadWallpaper();
    void applyWallpaper(const QPixmap &image);
    void persistWallpaperSelection();
    QString assetWallpaperName(const QString &path) const;
    void refreshGlassSurfaces();
    void applyTheme();
    void submitProgramInput();
    TabCloseButton *tabButtonAt(int index) const;
    void attachHighlighter(DocumentEditor *document);
    void refreshIntelliSenseProjectPackages();
    void inform(const QString &title, const QString &text);

    void recordRecentFile(const QString &path);
    void loadRecentFiles();
    void pruneRecentFiles();
    void saveRecentFiles() const;
    void renameRecentFile(const QString &path, const QString &newPath);
    void refreshRecentFilesMenu();
    void openRecentFile(const QString &path);
    static QString normalizePath(const QString &path);

    QSplitter *splitter = nullptr;
    QSplitter *mainSplitter = nullptr;
    ProjectTree *tree = nullptr;
    QWidget *editorPane = nullptr;
    QWidget *bottomPanel = nullptr;
    QWidget *activityBarWidget = nullptr;
    QTabWidget *tabs = nullptr;
    QStackedWidget *editorStack = nullptr;
    WelcomePage *welcome = nullptr;
    QWidget *blankPage = nullptr;
    bool welcomeClosed = false;
    QLineEdit *programInput = nullptr;
    QLabel *statusEncodingLabel = nullptr;
    SyntaxColourMap syntaxColourMap;
    QString backgroundFile;
    QString backgroundAssetName;
    int darkness = 220;
    int glassBlur = theme::defaultGlassBlur;
    int glassOpacity = theme::defaultGlassTintOpacity;
    QColor glassTint = QColor(theme::glassTint);
    QPixmap wallpaper;
    QPixmap scaledWallpaper;
    QSize scaledSize;
    qreal scaledRatio = 0;
    QImage glassBackdrop;
    QSize glassBackdropSize;
    qreal glassBackdropRatio = 0;
    bool glassBackdropDirty = true;
    QPlainTextEdit *output = nullptr;
    QListWidget *diagnostics = nullptr;
    QPushButton *clearOutputButtonWidget = nullptr;
    QLabel *diagnosticsTitleLabel = nullptr;
    QProcess *buildProcess = nullptr;
    QProcess *runProcess = nullptr;
    QProcess *stoppingProcess = nullptr;
    QTimer *killTimer = nullptr;
    QAction *buildAction = nullptr;
    QAction *buildFileAction = nullptr;
    QAction *runAction = nullptr;
    QAction *runFileAction = nullptr;
    QAction *stopAction = nullptr;
    QAction *undoAction = nullptr;
    QAction *redoAction = nullptr;
    QAction *selectAllAction = nullptr;
    QAction *nextTabAction = nullptr;
    QAction *previousTabAction = nullptr;
    QAction *explorerAction = nullptr;
    QAction *outputAction = nullptr;
    QToolButton *explorerButton = nullptr;
    QToolButton *outputButton = nullptr;
    QMenu *recentFilesMenuWidget = nullptr;
    FindReplaceBar *findBar = nullptr;
    QShortcut *escapeShortcut = nullptr;
    QLabel *statusDocumentLabel = nullptr;
    QLabel *statusCursorLabel = nullptr;
    QLabel *statusStateLabel = nullptr;
    QString projectDir;
    QStringList projectPackageSearchPaths;
    QStringList projectPackageNames;
    QList<NavigationLocation> definitionHistory;
    QFileSystemWatcher *projectManifestWatcher = nullptr;
    QString settingsOrganisation;
    QString settingsApplication;
    bool explorerShown = true;
    bool outputShown = true;
    QString processProgram =QLatin1String("pebble");
    QString resolvedProgram;
    bool programResolved = false;
    QString findTerm;
    bool findMatchCase = false;
    QString findStatusMessage;
    QString buildStdoutBuffer;
    QString buildStderrBuffer;
    QString runStdoutBuffer;
    QString runStderrBuffer;
    QStringList recentFilePaths;
    bool stopRequested = false;
    bool closePending = false;
    bool restoringSession = false;
    UnsavedPrompt unsavedPrompt;
    Notifier notifier;
};
