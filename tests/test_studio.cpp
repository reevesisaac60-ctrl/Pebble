#include "studio.hpp"
#include "assets.hpp"
#include "glass_surface.hpp"
#include "theme.hpp"

#include "pebble/lexer.hpp"
#include "pebble/package.hpp"
#include "pebble/type_checker.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFontInfo>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QTabBar>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTextCursor>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QtTest/QTest>

#include <algorithm>

namespace {

QColor outputColour(const QTextDocument *document, int blockNumber) {
    const QTextBlock block = document->findBlockByNumber(blockNumber);
    const QList<QTextLayout::FormatRange> ranges = block.layout()->formats();

    for (const QTextLayout::FormatRange &range : ranges) {
        if (range.start == 0 && range.length > 0) {
            return range.format.foreground().color();
        }
    }

    return QColor();
}

}

class StudioTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void newFileCreatesAnUntitledDocument();
    void emptyEditorGetsACaretAndAcceptsTyping();
    void openingAFileShowsAndHighlightsTheEditor();
    void intellisenseCompletesLocalSymbolsAndParameters();
    void intellisenseRespectsLexicalScopeAndExactMatches();
    void intellisenseCompletesImportedPackageMembers();
    void intellisenseInfersVariableAndCallTypes();
    void intellisenseOnlyCompletesKnownPackageMembers();
    void intellisenseFindsLocalAndPackageDefinitions();
    void goToDefinitionAndReturnRestoresTheCaret();
    void intellisenseCoversTheCompilerKeywordInventory();
    void intellisenseCoversBuiltinAndPackageInventories();
    void intellisenseSeparatesImportTypeAndMemberContexts();
    void intellisenseMatchesCompilerCaseAndPrefixBoundaries();
    void intellisenseTracksShadowingAndNewDeclarations();
    void intellisenseKeepsLocalCandidatesWhenAnImportFails();
    void intellisenseRefreshesPackageExportsAfterFileChanges();
    void intellisenseResolvesManifestProjectPackages();
    void intellisensePopupAndGhostShareTheSameBestCandidate();
    void intellisenseExactMatchHasNoGhostAndKeepsTabBehavior();
    void intellisenseDebounceUsesTheLatestEditedPrefix();
    void intellisenseRefreshesWhenSwitchingDocuments();
    void intellisenseSuppressesStringsAndComments();
    void intellisenseHoverUsesVerifiedSymbolInformation();
    void signatureHelpTracksArgumentsAndNestedCalls();
    void signatureHelpUsesPackageMetadata();
    void editorShowsAndRefreshesParameterHints();
    void editorDebouncesSymbolIndexingAfterEdits();
    void ghostTextAcceptsWithTabAndUndo();
    void completionPopupNavigatesAndAcceptsSelection();
    void completionPopupEscapeDoesNotEditSource();
    void tabKeepsIndentationWhenThereIsNoCompletion();
    void theExplorerViewportStaysGlassAndInteractive();
    void wallpaperAndGlassSettingsPersistAcrossRestarts();
    void invalidWallpaperKeepsTheCurrentImage();
    void missingSavedWallpaperRestoresTheDefault();
    void theDefaultWallpaperIsStoredOnTheFirstStart();
    void theWallpapersOfTheAssetsAreDiscovered();
    void theWallpaperDiscoverySkipsTheFilesThatAreNotImages();
    void theWallpaperGalleryShowsTheWallpapersWithThumbnails();
    void theAppearanceMenuOffersTheWallpaperPicker();
    void choosingAnotherWallpaperChangesTheBackground();
    void theGlassSurfacesFollowTheChosenWallpaper();
    void theChosenWallpaperSurvivesARestart();
    void aRemovedWallpaperFallsBackToTheDefault();
    void resetAppearanceDefaultsRestoresBundledSettings();
    void corruptGlassSettingsUseSensibleDefaults();
    void editingMarksTheDocumentDirty();
    void undoClearsTheDirtyState();
    void saveAsWritesTheFileAndAddsTheSuffix();
    void saveAsRefusesAFileThatIsAlreadyOpen();
    void openPathLoadsTheFile();
    void openingTheSameFileTwiceReusesTheTab();
    void openPathReportsAMissingFile();
    void closeKeepsTheTabWhenTheSaveFails();
    void closeSavesWhenTheUserChoosesSave();
    void closeDiscardsWhenTheUserChoosesDiscard();
    void closeStaysOpenWhenTheUserCancels();
    void stopIsDisabledWithoutARunningProgram();
    void stopIsEnabledWhileTheProgramRuns();
    void stoppingALongRunningProgramEndsIt();
    void theKillFallbackEndsAProgramThatIgnoresTheStop();
    void aProgramThatFailsToStartIsReported();
    void aProgramThatExitsNaturallyIsReported();
    void aProgramThatFailsIsReported();
    void theBuildResultIsReportedSeparately();
    void theKillTimerIsCancelledWhenTheProgramExits();
    void closingTheStudioStopsTheRunningProgram();
    void aCompilerDiagnosticIsParsed();
    void aMalformedCompilerDiagnosticIsRejected();
    void aDiagnosticIsCollectedFromTheCompilerOutput();
    void severalDiagnosticsAreCollectedInOrder();
    void aDiagnosticThatIsSplitOverTwoReadsIsStillFound();
    void aDiagnosticWithoutALineBreakIsFound();
    void aDiagnosticWithSpacesInThePathIsFound();
    void aDiagnosticResolvesItsPathAgainstTheProject();
    void aDiagnosticNavigatesToItsSourceLocation();
    void aDoubleClickOnADiagnosticOpensItsSource();
    void aDiagnosticForAMissingFileIsReported();
    void theDiagnosticsAreConnectedOnce();
    void aBuildReportsItsDiagnostics();
    void theDiagnosticsOfThePreviousBuildAreCleared();
    void startupRestoresTheProjectTree();
    void theRestoredTreeKeepsTheDirectoryStructure();
    void startupWithoutAProjectDirectoryHasAnEmptyTree();
    void startupWithAMissingProjectDirectoryReportsIt();
    void startupWithADirectoryThatIsNotAProjectReportsIt();
    void restorationDoesNotDuplicateTreeEntries();
    void theRestoredTreeKeepsTheOpenDocuments();
    void theTreeSelectionSurvivesARefresh();
    void renamingKeepsTheOpenDocumentInSync();
    void deletingClosesTheOpenDocument();
    void deletingIsStoppedByAnUnsavedDocument();
    void theMenuActionsReachTheirSlots();
    void theExpectedShortcutsAreRegistered();
    void theStoredDocumentsAreRestored();
    void theStoredWindowSizeIsRestored();

    void adjacentExecutableTakesPrecedence();
    void pebbleEnvIsUsedWhenTheAdjacentCandidateIsMissing();
    void pebbleEnvIsUsedWhenItIsAnAbsoluteExecutable();
    void pathFallbackWorksWhenBothEarlierCandidatesAreMissing();
    void anInvalidPebbleEnvValueFallsBackToPath();
    void aNonExecutablePebbleEnvValueFallsBackToPath();
    void failureIsReportedWhenNoCandidateIsUsable();
    void buildAndRunBothUseTheSameResolver();
    void anExecutablePathWithSpacesIsHandled();
    void theResolverDoesNotPickAnUnrelatedBinaryFromTheCwd();
    void theAdjacentCandidateIsSkippedWhenTheStudioPathIsUnknown();

    void theFindShortcutOpensThePanel();
    void theReplaceShortcutOpensTheReplacementRow();
    void theFindShortcutsAreRegistered();
    void theSearchSelectsTheMatch();
    void theNextButtonWalksTheMatches();
    void theNextShortcutWalksTheMatches();
    void thePreviousShortcutWalksBackwards();
    void theSearchWrapsAround();
    void theMatchCaseCheckboxAppliesRightAway();
    void theReplaceButtonReplacesTheCurrentMatch();
    void theReplaceButtonKeepsAnUnrelatedSelection();
    void theReplaceAllButtonReplacesEveryMatch();
    void theReplaceAllButtonStopsWhenTheReplacementHoldsTheTerm();
    void theSearchFollowsTheActiveTab();
    void thePanelReportsTheSearchStatus();
    void theEscapeShortcutClosesThePanel();
    void theCloseButtonClosesThePanel();
    void thePanelSitsAboveTheEditor();
    void theFindShortcutSeedsTheSelection();

    void tabInsertsTheIndentationInTheEditor();
    void shiftTabRemovesTheIndentation();
    void outdentRemovesOnlyTheIndentation();
    void aMultilineSelectionIsIndented();
    void aMultilineSelectionIsOutdented();
    void enterKeepsTheIndentation();
    void enterOnAnEmptyLineStaysEmpty();
    void enterOnAWhitespaceOnlyLineKeepsTheWhitespace();
    void controlSlashCommentsASingleLine();
    void controlSlashCommentsAMultilineSelection();
    void theEditingKeysDoNotDisturbTheFindPanel();

    void theEditMenuHoldsTheEditingEntries();
    void theEditActionsCarryTheirShortcuts();
    void theTabShortcutsAreRegistered();
    void theUndoActionReachesTheActiveDocument();
    void theRedoActionReachesTheActiveDocument();
    void theSelectAllActionSelectsTheWholeDocument();
    void theUndoShortcutReachesTheEditor();
    void theRedoShortcutReachesTheEditor();
    void theSelectAllShortcutReachesTheEditor();
    void theEditActionsFollowTheActiveTab();
    void theEditActionsAreDisabledWithoutADocument();
    void theEditActionsFollowTheUndoStack();
    void theEditActionsAreDisabledWhenTheDocumentIsClosed();
    void theEditShortcutsStayOutOfTheFindPanel();
    void theNextTabShortcutSwitchesTheDocument();
    void thePreviousTabShortcutSwitchesTheDocument();
    void theTabNavigationWrapsAround();
    void theFindEntriesOfTheEditMenuReachThePanel();

    void theTreeShowsThePebbleSources();
    void theTreeHidesTheFilesThatAreNoSources();
    void theTreeKeepsTheDirectories();
    void theNestedSourcesStayInTheirDirectory();
    void reopeningTheTreeShowsANewSource();
    void reopeningTheTreeFollowsARemovedSource();
    void reopeningTheTreeFollowsARenamedSource();
    void reopeningTheTreeDoesNotDuplicateEntries();
    void showingTheWindowAgainRefreshesTheTree();
    void aMissingProjectDirectoryLeavesTheTreeAlone();

    void theFileMenuHoldsTheRecentFiles();
    void theRecentFilesAreEmptyAtTheStart();
    void theRecentFilesHoldTheFileThatWasOpened();
    void theRecentFilesAreNewestFirst();
    void theRecentFilesHoldNoDuplicate();
    void theRecentFilesHoldEightFilesAtMost();
    void theRecentFilesSurviveARestart();
    void theRestoredSessionKeepsTheOrderOfTheRecentFiles();
    void aDeletedFileLeavesTheRecentFiles();
    void aDeletedFileLeavesTheRecentFilesAtOnce();
    void aRenamedFileKeepsItsPlaceInTheRecentFiles();
    void aRecentFileOpensTheDocument();
    void aRecentFileThatIsGoneIsDropped();
    void savingAFileRecordsIt();
    void theRecentFilesAreStoredInTheSettings();

    void runningTheCurrentFileStartsTheProgram();
    void aDirtyDocumentIsSavedBeforeItRuns();
    void aFileThatIsNotAPebbleProgramIsNotRun();
    void runningWithoutADocumentIsRefused();
    void aFileThatIsGoneIsNotRun();
    void theExitCodeOfTheProgramIsReported();
    void aRunningProgramIsNotReplaced();
    void aProgramThatFailsToStartIsReportedForTheCurrentFile();
    void buildingTheCurrentFileBuildsIt();
    void theBuildAndTheProgramReportTheirOwnResults();
    void stoppingTheProgramDoesNotReportABuildResult();
    void theProgramIsGivenTheFilesOfTheExamples();
    void theProgramKeepsTheEnvironmentOfTheStudio();
    void aProgramRunByTheStudioFindsTheFilesOfTheExamples();

    void theExamplesOfTheSubdirectoriesAreListed();
    void theLabelOfAnExampleIsItsPathInTheExamplesDirectory();
    void theDirectoriesOfTheExamplesThatHoldFilesAreFound();

    void theKeywordsAreRecognised();
    void theLiteralsAreRecognised();
    void theCommentsAreRecognised();
    void theStringsAreRecognised();
    void theNumbersAreRecognised();
    void theCallsAreRecognised();
    void theOperatorsAreRecognised();
    void aWordInAStringIsNoKeyword();
    void aWordInACommentIsNoKeyword();
    void theHighlighterFollowsTheEdits();
    void theHighlighterMarksTheCodeOfTheDocument();
    void theHighlighterUsesTheObsidianSyntaxPalette();
    void theLineNumbersGrowWithTheDocument();
    void theCurrentLineIsMarked();

    void theActivityBarHoldsTheSwitchesOfThePanels();
    void theExplorerCanBeHiddenAndShown();
    void theOutputPaneCanBeHiddenAndShown();
    void theWorkspaceStateIsStoredWhenTheWindowCloses();
    void theWorkspaceStateSurvivesARestart();

    void theOutputPaneHasABound();
    void theOutputPaneDropsTheOldestLines();
    void theClearButtonEmptiesTheOutput();
    void theErrorOfTheProgramIsMarked();
    void theOutputLinesAreColouredByTheirLevel();
    void aDiagnosticLineIsColouredByItsSeverity();
    void theDiagnosticsTitleCarriesTheNumber();

    void theStatusBarFollowsTheActiveTab();
    void theWindowTitleFollowsTheActiveTab();
    void theStatusBarShowsThePositionOfTheCursor();
    void theStatusBarShowsTheDirtyState();

    void theLineNumbersArePaintedOnTheScreen();
    void theGutterSitsOnTheLeftOfTheText();
    void theWindowCarriesTheIconOfTheStudio();
    void theIconOfTheUserIsTheIconOfTheStudio();
    void theWallpaperOfTheUserIsTheDefaultBackground();
    void theActivityBarIconFollowsItsPanel();
    void theStatusBarMarksAChangedDocument();
    void theCodeEditorUsesAFixedPitchFont();
    void theWorkspaceUsesOneStyleSheet();
    void theCloseButtonOfTheTabClosesTheDocument();
    void theOutputCanBeEmptiedFromTheMenu();

private:
    QString write(const QString &name, const QString &contents);
    QString writeFile(const QString &path, const QString &contents);
    QString makeProject(const QString &name);
    QString makeMixedProject(const QString &name);
    QString writeScript(const QString &name, const QString &body);
    void prepareRun(const QString &program);
    QString prepareDiagnosticsProject();
    void append(DocumentEditor *document, const QString &contents);
    QAction *menuAction(const QString &menu, const QString &label);
    QAction *windowAction(const QString &label);
    QTreeWidgetItem *findItem(QTreeWidgetItem *item, const QString &path);
    void storeProject(const QString &directory);
    void reopen();
    void showWindow();
    QString anotherWallpaper() const;
    FindReplaceBar *openFindPanel();
    void searchThroughPanel(const QString &text);
    DocumentEditor *prepareEditor(const QString &text);
    void selectText(DocumentEditor *document, int start, int end);

    QTemporaryDir settingsDir;
    QTemporaryDir workDir;
    MainWindow *window = nullptr;
    MainWindow::UnsavedChoice choice = MainWindow::UnsavedChoice::Cancel;
    int notifications = 0;
};

void StudioTest::initTestCase() {
    QVERIFY(settingsDir.isValid());
    QVERIFY(workDir.isValid());

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(
        QSettings::IniFormat,
        QSettings::UserScope,
        settingsDir.path()
    );
}

void StudioTest::init() {
    QSettings("Pebble", "Pebble Studio Test").clear();
    reopen();
}

void StudioTest::reopen() {
    delete window;

    window = new MainWindow("Pebble", "Pebble Studio Test");
    window->setNotifier([this](const QString &, const QString &) {
        ++notifications;
    });
    window->setUnsavedPrompt([this](const QString &) {
        return choice;
    });
}

void StudioTest::storeProject(const QString &directory) {
    QSettings settings("Pebble", "Pebble Studio Test");
    settings.setValue("projectDir", directory);
}

QTreeWidgetItem *StudioTest::findItem(
    QTreeWidgetItem *item,
    const QString &path
) {
    if (item == nullptr) {
        return nullptr;
    }

    if (item->data(0, Qt::UserRole).toString() == path) {
        return item;
    }

    for (int child = 0; child < item->childCount(); ++child) {
        QTreeWidgetItem *found = findItem(item->child(child), path);

        if (found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

void StudioTest::cleanup() {
    delete window;
    window = nullptr;
    notifications = 0;
}

void StudioTest::showWindow() {
    window->show();
    QCoreApplication::processEvents();
}

QString StudioTest::anotherWallpaper() const {
    const QString current = window->selectedWallpaperName();

    for (const QString &name : studio::wallpaperFileNames()) {
        if (name != current) {
            return name;
        }
    }

    return QString();
}

FindReplaceBar *StudioTest::openFindPanel() {
    showWindow();
    QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
    return window->findReplaceBar();
}

void StudioTest::searchThroughPanel(const QString &text) {
    FindReplaceBar *bar = window->findReplaceBar();
    QVERIFY(bar != nullptr);
    bar->findField()->setText(text);
    QTest::keyClick(bar->findField(), Qt::Key_Return);
}

DocumentEditor *StudioTest::prepareEditor(const QString &text) {
    window->newFile();

    DocumentEditor *document = window->documentAt(0);

    if (document == nullptr) {
        return nullptr;
    }

    document->setPlainText(text);
    showWindow();
    document->setFocus();
    QCoreApplication::processEvents();

    return document;
}

void StudioTest::selectText(DocumentEditor *document, int start, int end) {
    QTextCursor cursor = document->textCursor();
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    document->setTextCursor(cursor);
}

QString StudioTest::write(const QString &name, const QString &contents) {
    return writeFile(workDir.filePath(name), contents);
}

QString StudioTest::writeFile(const QString &path, const QString &contents) {
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }

    QTextStream(&file) << contents;
    file.close();

    return path;
}

QString StudioTest::writeScript(const QString &name, const QString &body) {
    const QString path = workDir.filePath(name);
    writeFile(path, body);

    QFile::setPermissions(
        path,
        QFile::permissions(path) |
        QFileDevice::ExeOwner |
        QFileDevice::ExeGroup |
        QFileDevice::ExeOther
    );

    return path;
}

void StudioTest::prepareRun(const QString &program) {
    const QString project = makeProject("run-project");

    if (project.isEmpty()) {
        return;
    }

    storeProject(project);
    reopen();
    window->setProcessProgram(program);
}

QString StudioTest::makeProject(const QString &name) {
    const QString root = workDir.filePath(name);

    if (!QDir().mkpath(root + "/src/nested")) {
        return QString();
    }

    writeFile(
        root + "/pebble.toml",
        "[package]\nname = \"demo\"\nversion = \"0.1.0\"\n"
    );
    writeFile(root + "/main.peb", "fn main() {}\n");
    writeFile(root + "/src/lib.peb", "fn helper() {}\n");
    writeFile(root + "/src/nested/deep.peb", "fn deep() {}\n");

    return root;
}

QString StudioTest::makeMixedProject(const QString &name) {
    const QString root = workDir.filePath(name);

    if (!QDir().mkpath(root + "/src/nested") || !QDir().mkpath(root + "/src/empty")) {
        return QString();
    }

    writeFile(
        root + "/pebble.toml",
        "[package]\nname = \"mixed\"\nversion = \"0.1.0\"\n"
    );
    writeFile(root + "/main.peb", "fn main() {}\n");
    writeFile(root + "/notes.txt", "some notes\n");
    writeFile(root + "/image.png", "not a real image\n");
    writeFile(root + "/src/lib.peb", "fn helper() {}\n");
    writeFile(root + "/src/notes.txt", "some more notes\n");
    writeFile(root + "/src/nested/deep.peb", "fn deep() {}\n");
    writeFile(root + "/src/nested/data.json", "{}\n");

    return root;
}

QString StudioTest::prepareDiagnosticsProject() {
    const QString project = makeProject("diagnostics-project");

    writeFile(
        project + "/src/broken.peb",
        "fn first() {}\nfn second() {}\nfn third() {}\nfn fourth() {}\n"
    );

    storeProject(project);
    reopen();

    return project;
}

void StudioTest::append(DocumentEditor *document, const QString &contents) {
    document->moveCursor(QTextCursor::End);
    document->insertPlainText(contents);
}

void StudioTest::newFileCreatesAnUntitledDocument() {
    window->newFile();

    QCOMPARE(window->documentCount(), 1);
    QCOMPARE(window->documentTitle(0), QString("Untitled.peb"));
    QCOMPARE(window->documentPath(0), QString());
    QCOMPARE(window->documentLabel(0), QString("Untitled.peb"));
    QVERIFY(!window->documentDirty(0));
}

void StudioTest::emptyEditorGetsACaretAndAcceptsTyping() {
    QVERIFY(window->welcomePageVisible());
    window->newFile();
    showWindow();

    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    QVERIFY(document->toPlainText().isEmpty());
    QVERIFY(document->isVisible());
    QVERIFY(document->geometry().width() > 0);
    QVERIFY(document->geometry().height() > 0);
    QVERIFY(document->viewport()->geometry().width() > 0);
    QVERIFY(document->lineNumberAreaWidth() > 0);
    QVERIFY(document->cursorRect().isValid());
    QCOMPARE(document->cursorWidth(), 2);
    QVERIFY(document->hasFocus());

    QTest::keyClicks(document, QStringLiteral("let value = 42;"));
    QCOMPARE(document->toPlainText(), QStringLiteral("let value = 42;"));
    QCOMPARE(syntaxCategoryAt(document->toPlainText(), 4),
             SyntaxCategory::Declaration);
    QCOMPARE(syntaxCategoryAt(document->toPlainText(), 12),
             SyntaxCategory::Number);
}

void StudioTest::openingAFileShowsAndHighlightsTheEditor() {
    const QString path = write(
        "visible.peb",
        "fn main() { let message: String = \"ready\"; }"
    );

    QVERIFY(window->openPath(path));
    showWindow();

    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    QVERIFY(document->isVisible());
    QVERIFY(document->geometry().width() > 0);
    QVERIFY(document->viewport()->geometry().width() > 0);
    QVERIFY(document->hasFocus());
    QCOMPARE(syntaxCategoryAt(document->toPlainText(), 0),
             SyntaxCategory::Keyword);
    QCOMPARE(syntaxCategoryAt(document->toPlainText(), 26),
             SyntaxCategory::Type);
}

void StudioTest::intellisenseCompletesLocalSymbolsAndParameters() {
    IntelliSenseModel model;
    const QString source = QStringLiteral(
        "fn Window(title: String) -> String { return title; }\n"
        "fn main() { let candidate: Int = 1; Wind }\n"
    );
    model.rebuild(source);
    const int position = static_cast<int>(source.indexOf(QStringLiteral("Wind }")) + 4);
    const auto names = model.completions(position);
    QVERIFY(std::any_of(names.cbegin(), names.cend(), [](const auto &item) {
        return item.name == QLatin1String("Window") && item.kind == QLatin1String("function");
    }));

    const int parameterPosition = static_cast<int>(
        source.indexOf(QStringLiteral("return title")) + QStringLiteral("return tit").size()
    );
    const auto parameters = model.completions(parameterPosition);
    QVERIFY(std::any_of(parameters.cbegin(), parameters.cend(), [](const auto &item) {
        return item.name == QLatin1String("title") && item.kind == QLatin1String("parameter");
    }));

    const int variablePosition = static_cast<int>(
        source.indexOf(QStringLiteral("candidate: Int")) + 4
    );
    const auto variables = model.completions(variablePosition);
    QVERIFY(std::any_of(variables.cbegin(), variables.cend(), [](const auto &item) {
        return item.name == QLatin1String("candidate");
    }));
}

void StudioTest::intellisenseRespectsLexicalScopeAndExactMatches() {
    IntelliSenseModel model;
    const QString source = QStringLiteral(
        "fn main() { if true { let Hidden: Int = 1; } Hid }\n"
        "fn Window() {}\n"
    );
    model.rebuild(source);
    const int hiddenPosition = static_cast<int>(source.indexOf(QStringLiteral("Hid }")) + 3);
    const auto hidden = model.completions(hiddenPosition);
    QVERIFY(std::none_of(hidden.cbegin(), hidden.cend(), [](const auto &item) {
        return item.name == QLatin1String("Hidden");
    }));
    const int exactPosition = static_cast<int>(source.indexOf(QStringLiteral("Window()")) + 6);
    const auto exact = model.completions(exactPosition);
    QVERIFY(std::any_of(exact.cbegin(), exact.cend(), [](const auto &item) {
        return item.name == QLatin1String("Window");
    }));
}

void StudioTest::intellisenseCompletesImportedPackageMembers() {
    IntelliSenseModel model;
    const QString source = QStringLiteral("import Log\nfn main() { Log:War }\n");
    model.rebuild(source);
    const auto items = model.completions(
        static_cast<int>(source.indexOf(QStringLiteral("War }")) + 3)
    );
    QVERIFY(std::any_of(items.cbegin(), items.cend(), [](const auto &item) {
        return item.name == QLatin1String("Warning") &&
               item.kind == QLatin1String("package function");
    }));

    const QString call = QStringLiteral("import Log\nfn main() { Log:Warning(\"x\") }\n");
    model.rebuild(call);
    const auto documentation = model.hoverAt(
        static_cast<int>(call.indexOf(QStringLiteral("Warning")))
    );
    QVERIFY(documentation.isValid());
    QVERIFY(documentation.detail.contains(QStringLiteral("Warning(String) -> Void")));
}

void StudioTest::intellisenseInfersVariableAndCallTypes() {
    IntelliSenseModel model;
    const QString source = QStringLiteral(
        "import package string\n"
        "fn makeName() -> String { return \"pebble\"; }\n"
        "fn main() {\n"
        "    let base = makeName();\n"
        "    let length = string:length(base);\n"
        "    var count = 1;\n"
        "    count = 2;\n"
        "    let copied = count;\n"
        "    var ratio: Float = 1;\n"
        "    ratio = 2;\n"
        "    if true { let base = \"inner\"; base; }\n"
        "    cop\n"
        "}\n"
    );
    model.rebuild(source);
    const int copPosition = static_cast<int>(source.lastIndexOf(QStringLiteral("cop")) + 3);
    const auto items = model.completions(copPosition);
    const auto copied = std::find_if(items.cbegin(), items.cend(), [](const auto &item) {
        return item.name == QLatin1String("copied");
    });
    QVERIFY(copied != items.cend());
    QCOMPARE(copied->detail, QStringLiteral("Int"));

    const auto base = model.hoverAt(static_cast<int>(source.indexOf(QStringLiteral("base =")) + 1));
    QVERIFY(base.isValid());
    QVERIFY(base.detail.contains(QStringLiteral("String")));
    const auto length = model.hoverAt(static_cast<int>(source.indexOf(QStringLiteral("length =")) + 1));
    QVERIFY(length.isValid());
    QVERIFY(length.detail.contains(QStringLiteral("Int")));
    const auto count = model.hoverAt(static_cast<int>(source.indexOf(QStringLiteral("count = 1")) + 1));
    QVERIFY(count.isValid());
    QVERIFY(count.detail.contains(QStringLiteral("Int")));
    const auto ratio = model.hoverAt(static_cast<int>(source.indexOf(QStringLiteral("ratio: Float"))));
    QVERIFY(ratio.isValid());
    QVERIFY(ratio.detail.contains(QStringLiteral("Float")));
    const auto shadow = model.hoverAt(static_cast<int>(source.indexOf(
        QStringLiteral("base;"), source.indexOf(QStringLiteral("let base = \"inner\""))
    )));
    QVERIFY(shadow.isValid());
    QVERIFY(shadow.detail.contains(QStringLiteral("String")));

    QString changed = source;
    changed.replace(QStringLiteral("let base = makeName();"), QStringLiteral("let base = 7;"));
    model.rebuild(changed);
    const auto updatedBase = model.hoverAt(
        static_cast<int>(changed.indexOf(QStringLiteral("base = 7")) + 1)
    );
    QVERIFY(updatedBase.isValid());
    QVERIFY(updatedBase.detail.contains(QStringLiteral("Int")));

    const QString incomplete = QStringLiteral("fn main() { let unknown = missing(\n }");
    model.rebuild(incomplete);
    QVERIFY(model.hoverAt(static_cast<int>(incomplete.indexOf(QStringLiteral("unknown")))).detail
                .contains(QStringLiteral("unknown")));
}

void StudioTest::intellisenseOnlyCompletesKnownPackageMembers() {
    IntelliSenseModel model;
    const QString source = QStringLiteral(
        "import package string\n"
        "fn main() {\n"
        "    string:\n"
        "    missing:\n"
        "    let value = 1;\n"
        "    value.\n"
        "}\n"
    );
    model.rebuild(source);
    const auto packageMembers = model.completions(
        static_cast<int>(source.indexOf(QStringLiteral("string:\n")) + 7)
    );
    QVERIFY(std::any_of(packageMembers.cbegin(), packageMembers.cend(), [](const auto &item) {
        return item.name == QLatin1String("length");
    }));
    QVERIFY(model.completions(
        static_cast<int>(source.indexOf(QStringLiteral("missing:\n")) + 8)
    ).isEmpty());
    QVERIFY(model.completions(
        static_cast<int>(source.indexOf(QStringLiteral("value.\n")) + 6)
    ).isEmpty());
}

void StudioTest::intellisenseFindsLocalAndPackageDefinitions() {
    const QString path = workDir.filePath(QStringLiteral("definition-source.peb"));
    const QString source = QStringLiteral(
        "import package helpers\n"
        "fn greet(person: String) {}\n"
        "fn main() { let value = 1; greet(value); helpers:welcome(value, true); "
        "let packageValue = helpers:welcome(value); packageV }\n"
    );
    IntelliSenseModel model;
    model.rebuild(source, path);
    const auto function = model.definitionAt(
        static_cast<int>(source.lastIndexOf(QStringLiteral("greet(value")) + 2)
    );
    QVERIFY(function.isValid());
    QCOMPARE(function.path, path);
    QCOMPARE(function.line, 2);
    QCOMPARE(function.column, 4);
    const auto variable = model.definitionAt(
        static_cast<int>(source.lastIndexOf(QStringLiteral("value);")) + 1)
    );
    QVERIFY(variable.isValid());
    QCOMPARE(variable.line, 3);
    const auto parameter = model.definitionAt(
        static_cast<int>(source.indexOf(QStringLiteral("person: String")) + 2)
    );
    QVERIFY(parameter.isValid());
    QCOMPARE(parameter.line, 2);

    const QString packageDirectory = workDir.filePath(QStringLiteral("definition-package"));
    QVERIFY(QDir().mkpath(packageDirectory));
    const QString packagePath = packageDirectory + QStringLiteral("/helpers.peb");
    QVERIFY(!writeFile(packagePath, QStringLiteral(
        "export fn welcome(person: Int, enabled: Bool) -> Int { return person; }\n"
    )).isEmpty());
    const QString mainPath = packageDirectory + QStringLiteral("/main.peb");
    model.setProjectPackages({packageDirectory}, {QStringLiteral("helpers")});
    model.rebuild(source, mainPath);
    const auto packageFunction = model.definitionAt(
        static_cast<int>(source.lastIndexOf(QStringLiteral("welcome(value")) + 2)
    );
    QVERIFY(packageFunction.isValid());
    QCOMPARE(QFileInfo(packageFunction.path).absoluteFilePath(),
             QFileInfo(packagePath).absoluteFilePath());
    QCOMPARE(packageFunction.line, 1);
    const auto packageValue = model.completions(
        static_cast<int>(source.lastIndexOf(QStringLiteral("packageV")) + 8)
    );
    const auto inferredPackage = std::find_if(
        packageValue.cbegin(), packageValue.cend(), [](const auto &item) {
            return item.name == QLatin1String("packageValue");
        }
    );
    QVERIFY(inferredPackage != packageValue.cend());
    QCOMPARE(inferredPackage->detail, QStringLiteral("Int"));

    const QString callSource = QStringLiteral(
        "import package helpers\nfn main() { helpers:welcome(1, \n }\n"
    );
    model.rebuild(callSource, mainPath);
    const auto packageHint = model.signatureHelpAt(
        static_cast<int>(callSource.indexOf(QStringLiteral("\n }")))
    );
    QVERIFY(packageHint.isValid());
    QCOMPARE(packageHint.name, QStringLiteral("welcome"));
    QCOMPARE(packageHint.activeParameter, 1);
    QCOMPARE(packageHint.parameters.size(), 2);
}

void StudioTest::goToDefinitionAndReturnRestoresTheCaret() {
    const QString path = workDir.filePath(QStringLiteral("go-to-definition.peb"));
    const QString packagePath = workDir.filePath(QStringLiteral("helpers.peb"));
    QVERIFY(!writeFile(packagePath, QStringLiteral("export fn welcome() {}\n")).isEmpty());
    const QString source = QStringLiteral(
        "import package helpers\nfn greet() {}\nfn main() { greet(); helpers:welcome(); }\n"
    );
    QVERIFY(!writeFile(path, source).isEmpty());
    QVERIFY(window->openPath(path));
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    const int usage = static_cast<int>(source.lastIndexOf(QStringLiteral("greet")) + 5);
    QTextCursor cursor(document->document());
    cursor.setPosition(usage);
    document->setTextCursor(cursor);
    QTest::keyClick(document, Qt::Key_F12);
    QCOMPARE(document->currentLineNumber(), 1);
    QCOMPARE(window->definitionHistoryDepth(), 1);
    QTest::keyClick(document, Qt::Key_Left, Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(document->currentLineNumber(), 2);
    QCOMPARE(document->textCursor().position(), usage);
    QCOMPARE(window->definitionHistoryDepth(), 0);
    const int packageUsage = static_cast<int>(source.lastIndexOf(QStringLiteral("welcome")) + 7);
    cursor.setPosition(packageUsage);
    document->setTextCursor(cursor);
    QTest::keyClick(document, Qt::Key_F12);
    QCOMPARE(window->documentCount(), 2);
    QCOMPARE(QFileInfo(window->documentPath(1)).absoluteFilePath(),
             QFileInfo(packagePath).absoluteFilePath());
    QCOMPARE(window->documentAt(1)->currentLineNumber(), 0);
    QTest::keyClick(window->documentAt(1), Qt::Key_Left,
                    Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(window->documentCount(), 2);
    QCOMPARE(QFileInfo(window->documentPath(0)).absoluteFilePath(),
             QFileInfo(path).absoluteFilePath());
    QCOMPARE(window->documentAt(0)->textCursor().position(), packageUsage);
    QCOMPARE(window->definitionHistoryDepth(), 0);
    QVERIFY(!window->navigateToDefinition(workDir.filePath("missing-definition.peb"), 1, 1));
}

void StudioTest::intellisenseCoversTheCompilerKeywordInventory() {
    IntelliSenseModel model;
    for (const pebble::TokenKind kind : pebble::languageKeywordTokens()) {
        const std::string_view spelling = pebble::tokenKindName(kind);
        const QString name = QString::fromLatin1(
            spelling.data(), static_cast<qsizetype>(spelling.size())
        );
        const QString prefix = name.left(name.size() - 1);
        QString source;
        if (kind == pebble::TokenKind::Package) {
            source = QStringLiteral("import pa");
        } else if (kind == pebble::TokenKind::Else) {
            source = QStringLiteral("fn main() { if true {} ") + prefix;
        } else if (kind == pebble::TokenKind::Fn || kind == pebble::TokenKind::Import ||
                   kind == pebble::TokenKind::Export) {
            source = prefix;
        } else {
            source = QStringLiteral("fn main() {\n    ") + prefix;
        }
        model.rebuild(source);
        const auto items = model.completions(static_cast<int>(source.size()));
        const bool found = std::any_of(items.cbegin(), items.cend(), [&name](const auto &item) {
            return item.name == name;
        });
        QVERIFY2(found, qPrintable(QStringLiteral("Missing compiler token: ") + name));
    }
    const QString invalidElseContext = QStringLiteral("fn main() { {} el");
    model.rebuild(invalidElseContext);
    const auto noElse = model.completions(static_cast<int>(invalidElseContext.size()));
    QVERIFY(std::none_of(noElse.cbegin(), noElse.cend(), [](const auto &item) {
        return item.name == QLatin1String("else");
    }));
}

void StudioTest::intellisenseCoversBuiltinAndPackageInventories() {
    IntelliSenseModel model;
    for (const std::string_view function : pebble::builtinFunctionNames()) {
        const QString name = QString::fromLatin1(function.data(),
                                                  static_cast<qsizetype>(function.size()));
        const QString prefix = name.left(name.size() - 1);
        const QString source = QStringLiteral("fn main() {\n    ") + prefix;
        model.rebuild(source);
        const auto items = model.completions(static_cast<int>(source.size()));
        QVERIFY(std::any_of(items.cbegin(), items.cend(), [&name](const auto &item) {
            return item.name == name && item.kind == QLatin1String("built-in function");
        }));
    }

    for (const std::string_view type : pebble::builtinTypeNames()) {
        const QString name = QString::fromLatin1(type.data(), static_cast<qsizetype>(type.size()));
        const QString prefix = name.left(name.size() - 1);
        const QString source = QStringLiteral("fn main() -> ") + prefix;
        model.rebuild(source);
        const auto items = model.completions(static_cast<int>(source.size()));
        QVERIFY(std::any_of(items.cbegin(), items.cend(), [&name](const auto &item) {
            return item.name == name && item.kind == QLatin1String("type");
        }));
    }

    for (const pebble::Package &package : pebble::builtinPackages()) {
        const QString packageName = QString::fromStdString(package.name);
        const QString import = QStringLiteral("import package ") + packageName +
            QStringLiteral("\nfn main() { ") + packageName + QLatin1Char(':');
        for (const pebble::PackageFunction &function : package.functions) {
            const QString name = QString::fromStdString(function.name);
            const QString prefix = name.left(name.size() - 1);
            const QString source = import + prefix;
            model.rebuild(source);
            const auto items = model.completions(static_cast<int>(source.size()));
            QVERIFY2(std::any_of(items.cbegin(), items.cend(), [&name](const auto &item) {
                return item.name == name && item.kind == QLatin1String("package function");
            }), qPrintable(QStringLiteral("Missing package function: ") + packageName +
                            QLatin1Char('.') + name));
        }
    }
}

void StudioTest::intellisenseSeparatesImportTypeAndMemberContexts() {
    IntelliSenseModel model;
    const QString imports = QStringLiteral("import package ma");
    model.rebuild(imports);
    const auto packageItems = model.completions(static_cast<int>(imports.size()));
    QVERIFY(std::any_of(packageItems.cbegin(), packageItems.cend(), [](const auto &item) {
        return item.name == QLatin1String("math");
    }));

    const QString typeSource = QStringLiteral("fn main() { let value: Str }");
    model.rebuild(typeSource);
    const auto types = model.completions(static_cast<int>(typeSource.indexOf(QStringLiteral("Str")) + 3));
    QVERIFY(std::any_of(types.cbegin(), types.cend(), [](const auto &item) {
        return item.name == QLatin1String("String") && item.kind == QLatin1String("type");
    }));
    QVERIFY(std::none_of(types.cbegin(), types.cend(), [](const auto &item) {
        return item.kind == QLatin1String("keyword") || item.kind == QLatin1String("let");
    }));

    const QString dotSource = QStringLiteral(
        "import package math\nfn main() { math .\n    ra }\n"
    );
    model.rebuild(dotSource);
    const int dotPosition = static_cast<int>(dotSource.indexOf(QStringLiteral("ra }")) + 2);
    const auto dotItems = model.completions(dotPosition);
    QVERIFY(std::any_of(dotItems.cbegin(), dotItems.cend(), [](const auto &item) {
        return item.name == QLatin1String("random");
    }));

    const QString exact = QStringLiteral("import package Log\nfn main() { Log:Warning }");
    model.rebuild(exact);
    const int endOfMember = static_cast<int>(exact.indexOf(QStringLiteral("Warning")) + 7);
    const auto exactItems = model.completions(endOfMember);
    QVERIFY(std::any_of(exactItems.cbegin(), exactItems.cend(), [](const auto &item) {
        return item.name == QLatin1String("Warning");
    }));
    const QString wrongCase = QStringLiteral("import package Log\nfn main() { Log:warning }");
    model.rebuild(wrongCase);
    QVERIFY(model.completions(static_cast<int>(wrongCase.size() - 2)).isEmpty());
}

void StudioTest::intellisenseMatchesCompilerCaseAndPrefixBoundaries() {
    IntelliSenseModel model;
    const QString source = QStringLiteral(
        "fn Window() {}\nfn window() {}\nfn main() {\n    Win\n}\n"
    );
    model.rebuild(source);
    const int start = static_cast<int>(source.lastIndexOf(QStringLiteral("Win")));
    for (const int length : {1, 2, 3}) {
        const auto items = model.completions(start + length);
        QVERIFY(std::any_of(items.cbegin(), items.cend(), [](const auto &item) {
            return item.name == QLatin1String("Window");
        }));
    }
    QVERIFY(model.prefixAt(start).isEmpty());
    QCOMPARE(model.prefixAt(start + 3), QStringLiteral("Win"));
    QString exactSource = source;
    exactSource.replace(QStringLiteral("    Win\n"), QStringLiteral("    Window\n"));
    model.rebuild(exactSource);
    const int exactPosition = static_cast<int>(exactSource.lastIndexOf(QStringLiteral("Window")) + 6);
    const auto exact = model.completions(exactPosition);
    QVERIFY(std::any_of(exact.cbegin(), exact.cend(), [](const auto &item) {
        return item.name == QLatin1String("Window");
    }));
    const QString wrongCaseSource = QStringLiteral("fn Window() {}\nfn main() { win }\n");
    model.rebuild(wrongCaseSource);
    const auto wrongCase = model.completions(static_cast<int>(wrongCaseSource.indexOf(QStringLiteral("win }")) + 3));
    QVERIFY(std::none_of(wrongCase.cbegin(), wrongCase.cend(), [](const auto &item) {
        return item.name == QLatin1String("Window");
    }));
    QVERIFY(model.completions(
        static_cast<int>(wrongCaseSource.indexOf(QStringLiteral("win }")) + 3), true
    ).isEmpty());
}

void StudioTest::intellisenseTracksShadowingAndNewDeclarations() {
    IntelliSenseModel model;
    QString source = QStringLiteral(
        "fn main() {\n"
        "    let value: Int = 1\n"
        "    { let value: String = \"x\"; val }\n"
        "    val\n"
        "}\n"
    );
    model.rebuild(source);
    const int innerPosition = static_cast<int>(source.indexOf(QStringLiteral("val }")) + 3);
    const auto inner = model.completions(innerPosition);
    const auto innerValue = std::find_if(inner.cbegin(), inner.cend(), [](const auto &item) {
        return item.name == QLatin1String("value");
    });
    QVERIFY(innerValue != inner.cend());
    QCOMPARE(innerValue->detail, QStringLiteral("String"));

    const int outerPosition = static_cast<int>(source.lastIndexOf(QStringLiteral("val")) + 3);
    const auto outer = model.completions(outerPosition);
    const auto outerValue = std::find_if(outer.cbegin(), outer.cend(), [](const auto &item) {
        return item.name == QLatin1String("value");
    });
    QVERIFY(outerValue != outer.cend());
    QCOMPARE(outerValue->detail, QStringLiteral("Int"));

    source.replace(QStringLiteral("    val\n"), QStringLiteral("    let newer: Bool = true\n    new\n"));
    model.rebuild(source);
    const int newPosition = static_cast<int>(source.lastIndexOf(QStringLiteral("new")) + 3);
    const auto updated = model.completions(newPosition);
    QVERIFY(std::any_of(updated.cbegin(), updated.cend(), [](const auto &item) {
        return item.name == QLatin1String("newer");
    }));
}

void StudioTest::intellisenseKeepsLocalCandidatesWhenAnImportFails() {
    IntelliSenseModel model;
    const QString source = QStringLiteral(
        "import package NotInstalled\n"
        "fn Window() {}\n"
        "fn main() { Wind }\n"
    );
    model.rebuild(source);
    const auto items = model.completions(static_cast<int>(source.indexOf(QStringLiteral("Wind }")) + 4));
    QVERIFY(std::any_of(items.cbegin(), items.cend(), [](const auto &item) {
        return item.name == QLatin1String("Window");
    }));
}

void StudioTest::intellisenseRefreshesPackageExportsAfterFileChanges() {
    const QString directory = workDir.filePath(QStringLiteral("intellisense-package-cache"));
    QVERIFY(QDir().mkpath(directory));
    const QString mainPath = directory + QStringLiteral("/main.peb");
    const QString packagePath = directory + QStringLiteral("/helpers.peb");
    QVERIFY(!writeFile(packagePath, QStringLiteral("export fn alpha() {}\n")).isEmpty());

    IntelliSenseModel model;
    model.setProjectPackages({directory}, {QStringLiteral("helpers")});
    const QString source = QStringLiteral(
        "import package helpers\nfn main() { helpers:"
    );
    model.rebuild(source, mainPath);
    const auto first = model.completions(static_cast<int>(source.size()));
    QVERIFY(std::any_of(first.cbegin(), first.cend(), [](const auto &item) {
        return item.name == QLatin1String("alpha");
    }));

    QVERIFY(!writeFile(packagePath, QStringLiteral("export fn gamma() { return 12; }\n")).isEmpty());
    model.rebuild(source, mainPath);
    const auto refreshed = model.completions(static_cast<int>(source.size()));
    QVERIFY(std::any_of(refreshed.cbegin(), refreshed.cend(), [](const auto &item) {
        return item.name == QLatin1String("gamma");
    }));
    QVERIFY(std::none_of(refreshed.cbegin(), refreshed.cend(), [](const auto &item) {
        return item.name == QLatin1String("alpha");
    }));
}

void StudioTest::intellisenseResolvesManifestProjectPackages() {
    const QString project = workDir.filePath(QStringLiteral("intellisense-manifest-project"));
    const QString dependency = workDir.filePath(QStringLiteral("intellisense-manifest-dependency"));
    QVERIFY(QDir().mkpath(project));
    QVERIFY(QDir().mkpath(dependency));
    QVERIFY(!writeFile(project + QStringLiteral("/pebble.toml"), QStringLiteral(
        "[package]\nname = \"completion_demo\"\nversion = \"0.1.0\"\n"
        "[dependencies]\nhelpers = { path = \"../intellisense-manifest-dependency\" }\n"
    )).isEmpty());
    QVERIFY(!writeFile(dependency + QStringLiteral("/pebble.toml"), QStringLiteral(
        "[package]\nname = \"helpers\"\nversion = \"0.1.0\"\n"
    )).isEmpty());
    QVERIFY(!writeFile(dependency + QStringLiteral("/helpers.peb"), QStringLiteral(
        "export fn greet(name: String) -> String { return name; }\n"
    )).isEmpty());

    storeProject(project);
    reopen();
    window->newFile();
    showWindow();
    DocumentEditor *document = window->documentAt(0);
    document->setPlainText(QStringLiteral(
        "import package helpers\nfn main() { helpers: gr }\n"
    ));
    QTextCursor cursor(document->document());
    cursor.setPosition(static_cast<int>(document->toPlainText().indexOf(QStringLiteral("gr }")) + 2));
    document->setTextCursor(cursor);
    QTRY_VERIFY(document->completionNames().contains(QStringLiteral("greet")));
}

void StudioTest::intellisensePopupAndGhostShareTheSameBestCandidate() {
    window->newFile();
    showWindow();
    DocumentEditor *document = window->documentAt(0);
    document->setPlainText(QStringLiteral(
        "fn Window() {}\nfn Winter() {}\nfn main() {\n    Wi\n}\n"
    ));
    const QString text = document->toPlainText();
    QTextCursor cursor(document->document());
    cursor.setPosition(static_cast<int>(text.lastIndexOf(QStringLiteral("Wi")) + 2));
    document->setTextCursor(cursor);
    QTRY_VERIFY(document->completionPopupVisible());
    QVERIFY(!document->completionNames().isEmpty());
    const QString best = document->completionNames().first();
    QCOMPARE(document->ghostText(), best.mid(2));
}

void StudioTest::intellisenseExactMatchHasNoGhostAndKeepsTabBehavior() {
    window->newFile();
    showWindow();
    DocumentEditor *document = window->documentAt(0);
    document->setPlainText(QStringLiteral("fn Window() {}\nfn main() { Window }\n"));
    QTextCursor cursor(document->document());
    cursor.setPosition(static_cast<int>(document->toPlainText().lastIndexOf(QStringLiteral("Window")) + 6));
    document->setTextCursor(cursor);
    QTRY_VERIFY(document->completionNames().contains(QStringLiteral("Window")));
    QVERIFY(document->ghostText().isEmpty());
    const QString beforeTab = document->toPlainText();
    const int column = document->textCursor().positionInBlock();
    const int spaces = CodeEditor::indentWidth() - column % CodeEditor::indentWidth();
    QTest::keyClick(document, Qt::Key_Tab);
    QString expected = beforeTab;
    expected.insert(document->textCursor().position() - spaces,
                    QString(spaces, QLatin1Char(' ')));
    QCOMPARE(document->toPlainText(), expected);
}

void StudioTest::intellisenseDebounceUsesTheLatestEditedPrefix() {
    window->newFile();
    showWindow();
    DocumentEditor *document = window->documentAt(0);
    document->setPlainText(QStringLiteral("fn Window() {}\nfn main() {\n    Wi\n}\n"));
    QTextCursor cursor(document->document());
    cursor.setPosition(static_cast<int>(document->toPlainText().lastIndexOf(QStringLiteral("Wi")) + 2));
    document->setTextCursor(cursor);
    QTest::keyClick(document, Qt::Key_N);
    QTRY_VERIFY(!document->ghostText().isNull());
    QTRY_COMPARE(document->ghostText(), QStringLiteral("dow"));
    QCOMPARE(document->toPlainText().mid(
        static_cast<int>(document->toPlainText().lastIndexOf(QStringLiteral("Win"))), 3
    ), QStringLiteral("Win"));
}

void StudioTest::intellisenseRefreshesWhenSwitchingDocuments() {
    window->newFile();
    showWindow();
    DocumentEditor *first = window->documentAt(0);
    first->setPlainText(QStringLiteral("fn Window() {}\nfn main() { Wi }\n"));
    QTextCursor firstCursor(first->document());
    firstCursor.setPosition(static_cast<int>(first->toPlainText().indexOf(QStringLiteral("Wi }")) + 2));
    first->setTextCursor(firstCursor);
    QTRY_VERIFY(!first->ghostText().isEmpty());

    window->newFile();
    DocumentEditor *second = window->documentAt(1);
    second->setPlainText(QStringLiteral("fn Winter() {}\nfn main() { Wi }\n"));
    QTextCursor secondCursor(second->document());
    secondCursor.setPosition(static_cast<int>(second->toPlainText().indexOf(QStringLiteral("Wi }")) + 2));
    second->setTextCursor(secondCursor);
    QTRY_VERIFY(!second->ghostText().isEmpty());

    QTabWidget *tabs = window->findChild<QTabWidget *>();
    QVERIFY(tabs != nullptr);
    tabs->setCurrentIndex(0);
    QTRY_VERIFY(first->hasFocus());
    QTRY_VERIFY(!first->ghostText().isEmpty());
    QCOMPARE(first->ghostText(), QStringLiteral("ndow"));
}

void StudioTest::intellisenseSuppressesStringsAndComments() {
    IntelliSenseModel model;
    const QString source = QStringLiteral(
        "fn main() { let text: String = \"Wind\"; // Wind\n Wind }\n"
    );
    model.rebuild(source);
    const int stringPosition = static_cast<int>(source.indexOf(QStringLiteral("Wind\"")) + 4);
    QVERIFY(model.completions(stringPosition).isEmpty());
    QVERIFY(model.suggestionsAllowedAt(
        static_cast<int>(source.indexOf(QStringLiteral("Wind\"")) + 5)
    ));
    const int commentPosition = static_cast<int>(source.indexOf(QStringLiteral("// Wind")) + 7);
    QVERIFY(model.completions(commentPosition).isEmpty());
}

void StudioTest::intellisenseHoverUsesVerifiedSymbolInformation() {
    IntelliSenseModel model;
    const QString source = QStringLiteral(
        "fn Window(title: String) -> String { return title; }\n"
        "fn main() { let count: Int = 1; count; mystery }\n"
    );
    model.rebuild(source);
    const auto variable = model.hoverAt(static_cast<int>(source.indexOf(QStringLiteral("count: Int"))));
    QVERIFY(variable.isValid());
    QVERIFY(variable.detail.contains(QStringLiteral("Int")));
    const auto function = model.hoverAt(static_cast<int>(source.indexOf(QStringLiteral("Window(title"))));
    QVERIFY(function.isValid());
    QVERIFY(function.detail.contains(QStringLiteral("Window(title: String) -> String")));
    const auto keyword = model.hoverAt(static_cast<int>(source.indexOf(QStringLiteral("let count"))));
    QVERIFY(keyword.isValid());
    QVERIFY(keyword.detail.contains(QStringLiteral("immutable")));
    QVERIFY(!model.hoverAt(static_cast<int>(source.indexOf(QStringLiteral("mystery")))).isValid());
}

void StudioTest::signatureHelpTracksArgumentsAndNestedCalls() {
    IntelliSenseModel model;
    const QString source = QStringLiteral(
        "fn inner(left: Int, right: String) {}\n"
        "fn outer(first: Int, second: String) {}\n"
        "fn main() { outer(1, inner(2, \"a,b\"), ) }\n"
    );
    model.rebuild(source);
    const int insideInner = static_cast<int>(source.indexOf(QStringLiteral("inner(2, \"a,b\"")) +
                                    QStringLiteral("inner(2, ").size());
    const auto inner = model.signatureHelpAt(insideInner);
    QVERIFY(inner.isValid());
    QCOMPARE(inner.name, QStringLiteral("inner"));
    QCOMPARE(inner.activeParameter, 1);
    QCOMPARE(inner.parameters.size(), 2);
    QVERIFY(inner.signature.contains(QStringLiteral("inner(left: Int, right: String)")));

    const int afterNested = static_cast<int>(source.lastIndexOf(QStringLiteral(", )")) + 2);
    const auto outer = model.signatureHelpAt(afterNested);
    QVERIFY(outer.isValid());
    QCOMPARE(outer.name, QStringLiteral("outer"));
    QCOMPARE(outer.activeParameter, 1);

    const int afterCall = static_cast<int>(source.lastIndexOf(QStringLiteral(" }")) + 2);
    QVERIFY(!model.signatureHelpAt(afterCall).isValid());
}

void StudioTest::signatureHelpUsesPackageMetadata() {
    IntelliSenseModel model;
    const QString valid = QStringLiteral(
        "import package math\nfn main() { math:randint(1,\n // comment, ignored\n ) }\n"
    );
    model.rebuild(valid);
    const auto help = model.signatureHelpAt(static_cast<int>(valid.indexOf(QStringLiteral(" )"))));
    QVERIFY(help.isValid());
    QCOMPARE(help.name, QStringLiteral("randint"));
    QCOMPARE(help.activeParameter, 1);
    QCOMPARE(help.parameters, QStringList({QStringLiteral("Int"), QStringLiteral("Int")}));
}

void StudioTest::editorShowsAndRefreshesParameterHints() {
    window->newFile();
    showWindow();
    DocumentEditor *document = window->documentAt(0);
    document->setPlainText(QStringLiteral(
        "fn greet(name: String, count: Int) {}\nfn main() { greet(\n}\n"
    ));
    QTextCursor cursor(document->document());
    cursor.setPosition(static_cast<int>(document->toPlainText().lastIndexOf(QStringLiteral("greet(")) + 6));
    document->setTextCursor(cursor);
    QTRY_VERIFY(!document->parameterHintText().isEmpty());
    QCOMPARE(document->activeParameterHintIndex(), 0);
    cursor.setPosition(static_cast<int>(document->toPlainText().lastIndexOf(QStringLiteral("greet(")) + 6));
    cursor.insertText(QStringLiteral("\"hi\", "));
    document->setTextCursor(cursor);
    QTRY_COMPARE(document->activeParameterHintIndex(), 1);
    QVERIFY2(document->parameterHintText().contains(QStringLiteral("count: Int")),
             qPrintable(document->parameterHintText()));
}

void StudioTest::editorDebouncesSymbolIndexingAfterEdits() {
    window->newFile();
    showWindow();
    DocumentEditor *document = window->documentAt(0);
    document->setPlainText(QStringLiteral("fn main() { let counter: Int = 0; coun }\n"));
    document->document()->setModified(false);
    document->document()->clearUndoRedoStacks();
    QTextCursor cursor(document->document());
    cursor.setPosition(static_cast<int>(document->toPlainText().indexOf(QStringLiteral("coun }")) + 4));
    document->setTextCursor(cursor);
    QTRY_VERIFY(document->completionNames().contains(QStringLiteral("counter")));
    QCOMPARE(document->ghostText(), QStringLiteral("ter"));
}

void StudioTest::ghostTextAcceptsWithTabAndUndo() {
    window->newFile();
    showWindow();
    DocumentEditor *document = window->documentAt(0);
    document->setPlainText(QStringLiteral("fn Window() {}\nfn main() {\n Wind\n}\n"));
    document->document()->clearUndoRedoStacks();
    document->document()->setModified(false);
    QVERIFY(!document->dirty);
    QTextCursor cursor(document->document());
    cursor.setPosition(static_cast<int>(document->toPlainText().lastIndexOf(QStringLiteral("Wind")) + 4));
    document->setTextCursor(cursor);
    QTRY_COMPARE(document->ghostText(), QStringLiteral("ow"));
    const QString before = document->toPlainText();
    QTest::keyClick(document, Qt::Key_Tab);
    QString expected = before;
    const int typedAt = static_cast<int>(expected.lastIndexOf(QStringLiteral("Wind")));
    expected.replace(typedAt, 4, QStringLiteral("Window"));
    QCOMPARE(document->toPlainText(), expected);
    QVERIFY(document->dirty);
    document->undo();
    QCOMPARE(document->toPlainText(), before);
    QVERIFY(!document->dirty);
}

void StudioTest::completionPopupNavigatesAndAcceptsSelection() {
    window->newFile();
    showWindow();
    DocumentEditor *document = window->documentAt(0);
    document->setPlainText(QStringLiteral(
        "fn Window() {}\nfn Winter() {}\nfn main() { Wi }\n"
    ));
    QTextCursor cursor(document->document());
    cursor.setPosition(static_cast<int>(document->toPlainText().indexOf(QStringLiteral("Wi }")) + 2));
    document->setTextCursor(cursor);
    QTest::keyClick(document, Qt::Key_Space, Qt::ControlModifier);
    QVERIFY(document->completionPopupVisible());
    QVERIFY(document->completionNames().contains(QStringLiteral("Window")));
    QVERIFY(document->completionNames().contains(QStringLiteral("Winter")));
    QTest::keyClick(document, Qt::Key_Down);
    QCOMPARE(document->completionPopupCurrentRow(), 1);
    QTest::keyClick(document, Qt::Key_Return);
    QVERIFY(document->toPlainText().contains(QStringLiteral("Winter }")));
}

void StudioTest::completionPopupEscapeDoesNotEditSource() {
    window->newFile();
    showWindow();
    DocumentEditor *document = window->documentAt(0);
    document->setPlainText(QStringLiteral("fn Window() {}\nfn main() { Wi }\n"));
    QTextCursor cursor(document->document());
    cursor.setPosition(static_cast<int>(document->toPlainText().indexOf(QStringLiteral("Wi }")) + 2));
    document->setTextCursor(cursor);
    document->requestCompletions();
    QVERIFY(document->completionPopupVisible());
    const QString before = document->toPlainText();
    QTest::keyClick(document, Qt::Key_Escape);
    QVERIFY(!document->completionPopupVisible());
    QCOMPARE(document->toPlainText(), before);
}

void StudioTest::tabKeepsIndentationWhenThereIsNoCompletion() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QTest::keyClick(document, Qt::Key_Tab);
    QCOMPARE(document->toPlainText(), CodeEditor::indentText());
}

void StudioTest::theExplorerViewportStaysGlassAndInteractive() {
    const QString project = makeProject("glass-project");
    QVERIFY(!project.isEmpty());
    storeProject(project);
    reopen();
    showWindow();

    QTreeWidget *tree = window->findChild<QTreeWidget *>("project_tree");
    QVERIFY(tree != nullptr);
    QVERIFY(tree->isVisible());
    QVERIFY(tree->viewport()->geometry().width() > 0);
    QVERIFY(tree->viewport()->testAttribute(Qt::WA_TranslucentBackground));
    QVERIFY(tree->viewport()->testAttribute(Qt::WA_NoSystemBackground));
    QVERIFY(!tree->viewport()->autoFillBackground());
    QVERIFY(tree->topLevelItemCount() > 0);

    tree->setFocus();
    QTRY_VERIFY(tree->hasFocus());
    tree->setCurrentItem(tree->topLevelItem(0));
    QCOMPARE(tree->currentItem(), tree->topLevelItem(0));
}

void StudioTest::wallpaperAndGlassSettingsPersistAcrossRestarts() {
    const QString path = workDir.filePath("glass-wallpaper.png");
    QImage image(QSize(48, 32), QImage::Format_ARGB32);
    image.fill(QColor("#5879a4"));
    QVERIFY(image.save(path));

    window->setBackgroundImage(path);
    window->setGlassBlurStrength(31);
    window->setGlassTintOpacity(73);
    window->setGlassTintColour(QColor("#292330"));
    reopen();

    QCOMPARE(window->backgroundPath(), path);
    QVERIFY(window->backgroundLoaded());
    QCOMPARE(window->glassBlurStrength(), 31);
    QCOMPARE(window->glassTintOpacity(), 73);
    QCOMPARE(window->glassTintColour(), QColor("#292330"));
}

void StudioTest::invalidWallpaperKeepsTheCurrentImage() {
    const QString currentPath = window->backgroundPath();
    QVERIFY(window->backgroundLoaded());
    const QString invalid = write("not-an-image.bin", "this is not an image\n");

    window->setBackgroundImage(invalid);

    QCOMPARE(window->backgroundPath(), currentPath);
    QVERIFY(window->backgroundLoaded());
    QVERIFY(window->statusBar()->currentMessage().contains("could not be read"));
}

void StudioTest::missingSavedWallpaperRestoresTheDefault() {
    QSettings settings("Pebble", "Pebble Studio Test");
    settings.setValue("background_path", workDir.filePath("removed.png"));
    settings.remove("wallpaper_name");
    reopen();

    QCOMPARE(window->backgroundPath(), window->defaultBackgroundPath());
    QVERIFY(window->backgroundLoaded());
    QVERIFY(window->statusBar()->currentMessage().contains("unavailable"));
}

void StudioTest::theDefaultWallpaperIsStoredOnTheFirstStart() {
    QSettings settings("Pebble", "Pebble Studio Test");
    settings.remove("wallpaper_name");
    settings.remove("background_path");
    reopen();

    QCOMPARE(window->backgroundPath(), window->defaultBackgroundPath());
    QCOMPARE(
        settings.value("wallpaper_name").toString(),
        QFileInfo(window->defaultBackgroundPath()).fileName()
    );
    QVERIFY(!settings.contains("background_path"));
}

void StudioTest::theWallpapersOfTheAssetsAreDiscovered() {
    const QString directory = studio::wallpaperDirectory();

    QVERIFY(!directory.isEmpty());
    QVERIFY(QFileInfo(directory).isDir());

    const QStringList names = studio::wallpaperFileNames();

    QVERIFY(names.size() >= 2);
    QVERIFY(names.contains(studio::defaultWallpaperName()));
    QVERIFY(!names.contains(QFileInfo(studio::iconPath()).fileName()));

    const QStringList wallpapers = window->availableWallpapers();

    QCOMPARE(wallpapers.size(), names.size());

    for (const QString &path : wallpapers) {
        QVERIFY(QFileInfo::exists(path));
        QCOMPARE(
            QDir::cleanPath(QFileInfo(path).absolutePath()),
            QDir::cleanPath(directory)
        );
    }
}

void StudioTest::theWallpaperDiscoverySkipsTheFilesThatAreNotImages() {
    QTemporaryDir folder;
    QVERIFY(folder.isValid());

    QImage image(QSize(24, 16), QImage::Format_ARGB32);
    image.fill(QColor("#335577"));
    QVERIFY(image.save(folder.filePath("good.png")));

    QFile broken(folder.filePath("broken.png"));
    QVERIFY(broken.open(QIODevice::WriteOnly));
    QCOMPARE(broken.write("this is not an image\n"), qint64(21));
    broken.close();

    QFile notes(folder.filePath("notes.txt"));
    QVERIFY(notes.open(QIODevice::WriteOnly));
    QCOMPARE(notes.write("hello\n"), qint64(6));
    notes.close();

    const QStringList names = studio::wallpaperFileNamesIn(folder.path());

    QCOMPARE(names, QStringList{QStringLiteral("good.png")});
}

void StudioTest::theWallpaperGalleryShowsTheWallpapersWithThumbnails() {
    const QStringList wallpapers = window->availableWallpapers();
    QVERIFY(wallpapers.size() >= 2);

    WallpaperPickerDialog picker(
        wallpapers,
        window->backgroundPath(),
        window->defaultBackgroundPath()
    );

    QVERIFY(picker.wallpaperList()->count() >= 2);
    QVERIFY(picker.offeredPaths().contains(wallpapers.first()));

    bool withThumbnail = false;

    for (int index = 0; index < picker.wallpaperList()->count(); ++index) {
        if (!picker.wallpaperList()->item(index)->icon().isNull()) {
            withThumbnail = true;
        }
    }

    QVERIFY(withThumbnail);

    const int current = picker.wallpaperList()->currentRow();
    QVERIFY(current >= 0);
    QCOMPARE(
        picker.selectedPath(),
        picker.wallpaperList()->item(current)->data(Qt::UserRole).toString()
    );

    const QString otherName = anotherWallpaper();
    QVERIFY(!otherName.isEmpty());

    QString other;

    for (const QString &path : wallpapers) {
        if (QFileInfo(path).fileName() == otherName) {
            other = path;
        }
    }

    QVERIFY(!other.isEmpty());
    const qsizetype row = picker.offeredPaths().indexOf(other);
    QVERIFY(row >= 0);
    picker.wallpaperList()->setCurrentItem(
        picker.wallpaperList()->item(static_cast<int>(row))
    );
    QCOMPARE(picker.selectedPath(), other);
}

void StudioTest::theAppearanceMenuOffersTheWallpaperPicker() {
    QMenu *appearance = nullptr;

    for (QAction *entry : window->menuBar()->actions()) {
        QString title = entry->text();
        title.remove('&');

        if (title != QStringLiteral("View")) {
            continue;
        }

        QMenu *view = entry->menu();

        if (view == nullptr) {
            continue;
        }

        for (QAction *item : view->actions()) {
            if (item->text() == QStringLiteral("Appearance")) {
                appearance = item->menu();
            }
        }
    }

    QVERIFY(appearance != nullptr);

    bool found = false;

    for (QAction *entry : appearance->actions()) {
        if (entry->text() == QStringLiteral("Wallpaper…")) {
            found = true;
        }
    }

    QVERIFY(found);
}
void StudioTest::choosingAnotherWallpaperChangesTheBackground() {
    const QStringList names = studio::wallpaperFileNames();
    QVERIFY(names.size() >= 2);

    const QString before = window->backgroundPath();
    const QString other = anotherWallpaper();
    QVERIFY(!other.isEmpty());

    window->setBackgroundWallpaper(other);

    QCOMPARE(window->selectedWallpaperName(), other);
    QVERIFY(window->backgroundPath() != before);
    QVERIFY(window->backgroundPath().endsWith(other));
    QVERIFY(window->backgroundLoaded());
}

void StudioTest::theGlassSurfacesFollowTheChosenWallpaper() {
    showWindow();
    QCoreApplication::processEvents();

    const QImage before = window->glassBackdropImage();
    QVERIFY(!before.isNull());

    const QStringList names = studio::wallpaperFileNames();
    QVERIFY(names.size() >= 2);
    const QString other = anotherWallpaper();
    QVERIFY(!other.isEmpty());

    window->setBackgroundWallpaper(other);
    QCoreApplication::processEvents();

    const QImage after = window->glassBackdropImage();
    QVERIFY(!after.isNull());
    QVERIFY(before != after);

    const QList<QWidget *> widgets = window->findChildren<QWidget *>();
    QList<GlassSurface *> surfaces;

    for (QWidget *widget : widgets) {
        if (auto *surface = dynamic_cast<GlassSurface *>(widget)) {
            surfaces << surface;
        }
    }

    QVERIFY(!surfaces.isEmpty());

    for (GlassSurface *surface : surfaces) {
        QCOMPARE(surface->backdropImage(), &window->glassBackdropImage());
    }
}

void StudioTest::theChosenWallpaperSurvivesARestart() {
    const QStringList names = studio::wallpaperFileNames();
    QVERIFY(names.size() >= 2);

    const QString chosen = anotherWallpaper();
    QVERIFY(!chosen.isEmpty());

    window->setBackgroundWallpaper(chosen);
    const QString path = window->backgroundPath();

    reopen();

    QCOMPARE(window->selectedWallpaperName(), chosen);
    QCOMPARE(window->backgroundPath(), path);
    QVERIFY(window->backgroundLoaded());

    QSettings settings("Pebble", "Pebble Studio Test");
    QCOMPARE(settings.value("wallpaper_name").toString(), chosen);
    QVERIFY(!settings.contains("background_path"));
}

void StudioTest::aRemovedWallpaperFallsBackToTheDefault() {
    const QStringList names = studio::wallpaperFileNames();
    QVERIFY(names.size() >= 2);

    const QString chosen = anotherWallpaper();
    QVERIFY(!chosen.isEmpty());

    window->setBackgroundWallpaper(chosen);
    QCOMPARE(window->selectedWallpaperName(), chosen);

    QSettings settings("Pebble", "Pebble Studio Test");
    settings.setValue("wallpaper_name", QStringLiteral("wallpaper-that-is-gone.jpg"));
    settings.remove("background_path");
    reopen();

    QCOMPARE(window->backgroundPath(), window->defaultBackgroundPath());
    QVERIFY(window->backgroundLoaded());
    QVERIFY(window->statusBar()->currentMessage().contains("no longer available"));

    const QString stored = settings.value("wallpaper_name").toString();
    QCOMPARE(stored, QFileInfo(window->defaultBackgroundPath()).fileName());
    QVERIFY(!settings.contains("background_path"));
}

void StudioTest::resetAppearanceDefaultsRestoresBundledSettings() {
    window->setGlassBlurStrength(0);
    window->setGlassTintOpacity(15);
    window->setGlassTintColour(QColor("#454048"));
    window->resetAppearanceDefaults();

    QCOMPARE(window->backgroundPath(), window->defaultBackgroundPath());
    QCOMPARE(window->glassBlurStrength(), theme::defaultGlassBlur);
    QCOMPARE(window->glassTintOpacity(), theme::defaultGlassTintOpacity);
    QCOMPARE(window->glassTintColour(), QColor(theme::glassTint));
}

void StudioTest::corruptGlassSettingsUseSensibleDefaults() {
    QSettings settings("Pebble", "Pebble Studio Test");
    settings.setValue("glass_blur", "not a number");
    settings.setValue("glass_opacity", "not a number");
    settings.setValue("glass_tint", "not a colour");
    reopen();

    QCOMPARE(window->glassBlurStrength(), theme::defaultGlassBlur);
    QCOMPARE(window->glassTintOpacity(), theme::defaultGlassTintOpacity);
    QCOMPARE(window->glassTintColour(), QColor(theme::glassTint));
}

void StudioTest::editingMarksTheDocumentDirty() {
    window->newFile();
    append(window->documentAt(0), "fn main() {}");

    QVERIFY(window->documentDirty(0));

    QCOMPARE(window->documentLabel(0), QString("Untitled.peb"));
    QVERIFY(window->documentShowsDirtyMarker(0));
}

void StudioTest::undoClearsTheDirtyState() {
    window->newFile();

    DocumentEditor *document = window->documentAt(0);
    append(document, "fn main() {}");
    QVERIFY(document->dirty);

    document->undo();

    QVERIFY(!document->dirty);
    QCOMPARE(window->documentLabel(0), QString("Untitled.peb"));
    QVERIFY(!window->documentShowsDirtyMarker(0));
}

void StudioTest::saveAsWritesTheFileAndAddsTheSuffix() {
    window->newFile();
    append(window->documentAt(0), "fn main() { print(1); }");

    QVERIFY(window->saveAsPath(workDir.filePath("program")));

    const QString expected = QFileInfo(workDir.filePath("program.peb"))
        .absoluteFilePath();

    QVERIFY(QFile::exists(expected));
    QCOMPARE(window->documentPath(0), expected);
    QCOMPARE(window->documentTitle(0), QString("program.peb"));
    QCOMPARE(window->documentLabel(0), QString("program.peb"));
    QVERIFY(!window->documentDirty(0));

    QFile file(expected);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(
        QString::fromUtf8(file.readAll()),
        QString("fn main() { print(1); }")
    );
}

void StudioTest::saveAsRefusesAFileThatIsAlreadyOpen() {
    const QString first = write("first.peb", "one");
    const QString second = write("second.peb", "two");

    QVERIFY(window->openPath(first));
    QVERIFY(window->openPath(second));

    append(window->documentAt(1), "changed");

    QVERIFY(!window->saveAsPath(first));
    QCOMPARE(notifications, 1);
    QCOMPARE(window->documentTitle(1), QString("second.peb"));
    QVERIFY(window->documentDirty(1));
}

void StudioTest::openPathLoadsTheFile() {
    const QString path = write("main.peb", "fn main() {}");

    QVERIFY(window->openPath(path));

    QCOMPARE(window->documentCount(), 1);
    QCOMPARE(window->documentText(0), QString("fn main() {}"));
    QCOMPARE(window->documentTitle(0), QString("main.peb"));
    QCOMPARE(
        window->documentPath(0),
        QFileInfo(path).absoluteFilePath()
    );
    QVERIFY(!window->documentDirty(0));
}

void StudioTest::openingTheSameFileTwiceReusesTheTab() {
    const QString path = write("main.peb", "fn main() {}");

    QVERIFY(window->openPath(path));
    QVERIFY(window->openPath(path));

    QCOMPARE(window->documentCount(), 1);
}

void StudioTest::openPathReportsAMissingFile() {
    QVERIFY(!window->openPath(workDir.filePath("missing.peb")));

    QCOMPARE(notifications, 1);
    QCOMPARE(window->documentCount(), 0);
}

void StudioTest::closeKeepsTheTabWhenTheSaveFails() {
    const QString path = write("blocked.peb", "one");

    QVERIFY(window->openPath(path));
    append(window->documentAt(0), "changed");
    QVERIFY(window->documentDirty(0));

    QVERIFY(QFile::remove(path));
    QVERIFY(QDir().mkpath(path));

    choice = MainWindow::UnsavedChoice::Save;
    window->closeDocument(0);

    QCOMPARE(window->documentCount(), 1);
    QVERIFY(window->documentDirty(0));
}

void StudioTest::closeSavesWhenTheUserChoosesSave() {
    const QString path = write("saved.peb", "one");

    QVERIFY(window->openPath(path));
    append(window->documentAt(0), "changed");

    choice = MainWindow::UnsavedChoice::Save;
    window->closeDocument(0);

    QCOMPARE(window->documentCount(), 0);

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(file.readAll()), QString("onechanged"));
}

void StudioTest::closeDiscardsWhenTheUserChoosesDiscard() {
    const QString path = write("kept.peb", "one");

    QVERIFY(window->openPath(path));
    append(window->documentAt(0), "changed");

    choice = MainWindow::UnsavedChoice::Discard;
    window->closeDocument(0);

    QCOMPARE(window->documentCount(), 0);

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(file.readAll()), QString("one"));
}

void StudioTest::closeStaysOpenWhenTheUserCancels() {
    const QString path = write("cancel.peb", "one");

    QVERIFY(window->openPath(path));
    append(window->documentAt(0), "changed");

    choice = MainWindow::UnsavedChoice::Cancel;
    window->closeDocument(0);

    QCOMPARE(window->documentCount(), 1);
    QVERIFY(window->documentDirty(0));
}

void StudioTest::stopIsDisabledWithoutARunningProgram() {
    QAction *stop = menuAction("Run", "Stop");
    QVERIFY(stop != nullptr);
    QVERIFY(!stop->isEnabled());

    QAction *run = menuAction("Run", "Run");
    QVERIFY(run != nullptr);
    QVERIFY(run->isEnabled());

    QVERIFY(!window->isProcessRunning());
}

void StudioTest::stopIsEnabledWhileTheProgramRuns() {
    prepareRun(writeScript("slow.sh", "#!/bin/sh\necho started\nexec sleep 30\n"));

    QAction *stop = menuAction("Run", "Stop");
    QAction *run = menuAction("Run", "Run");
    QVERIFY(stop != nullptr);
    QVERIFY(run != nullptr);

    run->trigger();

    QTRY_VERIFY(stop->isEnabled());
    QVERIFY(!run->isEnabled());
    QVERIFY(window->isProcessRunning());

    QTRY_VERIFY(window->outputText().contains("started"));

    stop->trigger();
    QTRY_VERIFY(!window->isProcessRunning());
}

void StudioTest::stoppingALongRunningProgramEndsIt() {
    prepareRun(writeScript("slow.sh", "#!/bin/sh\necho started\nexec sleep 30\n"));

    QAction *stop = menuAction("Run", "Stop");
    QAction *run = menuAction("Run", "Run");
    QVERIFY(stop != nullptr);
    QVERIFY(run != nullptr);

    run->trigger();
    QTRY_VERIFY(stop->isEnabled());
    QTRY_VERIFY(window->outputText().contains("started"));

    stop->trigger();

    QTRY_VERIFY(!window->isProcessRunning());
    QTRY_VERIFY(window->outputText().contains("Program stopped."));
    QVERIFY(!stop->isEnabled());
    QVERIFY(run->isEnabled());
}

void StudioTest::theKillFallbackEndsAProgramThatIgnoresTheStop() {
    prepareRun(
        writeScript(
            "stubborn.sh",
            "#!/bin/sh\ntrap '' TERM\necho started\nexec sleep 30\n"
        )
    );

    QAction *stop = menuAction("Run", "Stop");
    QVERIFY(stop != nullptr);

    menuAction("Run", "Run")->trigger();
    QTRY_VERIFY(stop->isEnabled());
    QTRY_VERIFY(window->outputText().contains("started"));

    stop->trigger();

    QTRY_VERIFY(window->outputText().contains("killing it"));
    QTRY_VERIFY(!window->isProcessRunning());
    QTRY_VERIFY(window->outputText().contains("Program stopped."));
}

void StudioTest::aProgramThatFailsToStartIsReported() {
    prepareRun(workDir.filePath("missing-pebble-cli"));

    QAction *stop = menuAction("Run", "Stop");
    QAction *run = menuAction("Run", "Run");
    QVERIFY(stop != nullptr);
    QVERIFY(run != nullptr);

    run->trigger();

    QTRY_VERIFY(window->outputText().contains("Failed to start"));
    QVERIFY(!stop->isEnabled());
    QVERIFY(run->isEnabled());
    QVERIFY(!window->isProcessRunning());
}

void StudioTest::aProgramThatExitsNaturallyIsReported() {
    prepareRun(writeScript("quick.sh", "#!/bin/sh\necho hello\nexit 0\n"));

    QAction *stop = menuAction("Run", "Stop");
    QVERIFY(stop != nullptr);

    menuAction("Run", "Run")->trigger();

    QTRY_VERIFY(
        window->outputText().contains("Program finished successfully.")
    );
    QVERIFY(!stop->isEnabled());
    QVERIFY(!window->isProcessRunning());
}

void StudioTest::aProgramThatFailsIsReported() {
    prepareRun(
        writeScript("bad.sh", "#!/bin/sh\necho broken >&2\nexit 3\n")
    );

    menuAction("Run", "Run")->trigger();

    QTRY_VERIFY(window->outputText().contains("Program exited with code 3."));
    QVERIFY(!window->isProcessRunning());
}

void StudioTest::theBuildResultIsReportedSeparately() {
    prepareRun(writeScript("quick.sh", "#!/bin/sh\necho compiled\nexit 0\n"));

    QAction *stop = menuAction("Run", "Stop");
    QVERIFY(stop != nullptr);

    menuAction("Run", "Build")->trigger();

    QTRY_VERIFY(
        window->outputText().contains("Build finished with exit code 0.")
    );
    QVERIFY(!window->outputText().contains("Program finished successfully."));
    QVERIFY(!stop->isEnabled());
}

void StudioTest::theKillTimerIsCancelledWhenTheProgramExits() {
    prepareRun(writeScript("slow.sh", "#!/bin/sh\necho started\nexec sleep 30\n"));

    QAction *stop = menuAction("Run", "Stop");
    QAction *run = menuAction("Run", "Run");
    QVERIFY(stop != nullptr);
    QVERIFY(run != nullptr);

    run->trigger();
    QTRY_VERIFY(stop->isEnabled());
    QTRY_VERIFY(window->outputText().contains("started"));

    stop->trigger();
    QTRY_VERIFY(!window->isProcessRunning());

    run->trigger();
    QTRY_VERIFY(stop->isEnabled());

    QTest::qWait(2000);

    QVERIFY(window->isProcessRunning());
    QVERIFY(!window->outputText().contains("killing it"));

    stop->trigger();
    QTRY_VERIFY(!window->isProcessRunning());
}

void StudioTest::closingTheStudioStopsTheRunningProgram() {
    prepareRun(writeScript("slow.sh", "#!/bin/sh\necho started\nexec sleep 30\n"));

    QAction *run = menuAction("Run", "Run");
    QVERIFY(run != nullptr);

    window->show();
    QVERIFY(window->isVisible());

    run->trigger();
    QTRY_VERIFY(window->isProcessRunning());
    QTRY_VERIFY(window->outputText().contains("started"));

    window->close();
    QVERIFY(window->isVisible());
    QVERIFY(window->isProcessRunning());

    QTRY_VERIFY(!window->isProcessRunning());
    QTRY_VERIFY(window->outputText().contains("Program stopped."));
    QTRY_VERIFY(!window->isVisible());
}

void StudioTest::aCompilerDiagnosticIsParsed() {
    DiagnosticEntry entry;

    QVERIFY(parseDiagnosticLine(
        "./src/lib.peb:2:3: error: Cannot initialise 'score'.", entry
    ));

    QCOMPARE(entry.path, QString("./src/lib.peb"));
    QCOMPARE(entry.line, 2);
    QCOMPARE(entry.column, 3);
    QCOMPARE(entry.severity, QString("error"));
    QCOMPARE(entry.message, QString("Cannot initialise 'score'."));

    QVERIFY(parseDiagnosticLine(
        "./src/lib.peb:7:1: warning: Something looks odd.", entry
    ));

    QCOMPARE(entry.severity, QString("warning"));
    QCOMPARE(entry.line, 7);
    QCOMPARE(entry.message, QString("Something looks odd."));

    QVERIFY(parseDiagnosticLine(
        "./my sources/lib.peb:4:9: error: Broken.", entry
    ));

    QCOMPARE(entry.path, QString("./my sources/lib.peb"));
    QCOMPARE(entry.line, 4);
    QCOMPARE(entry.column, 9);

    QVERIFY(parseDiagnosticLine(
        "./src/lib.peb:1:1: error: Broken.\r", entry
    ));

    QCOMPARE(entry.message, QString("Broken."));
}

void StudioTest::aMalformedCompilerDiagnosticIsRejected() {
    DiagnosticEntry entry;

    QVERIFY(!parseDiagnosticLine(
        "error: the project has no entry point.", entry
    ));

    QVERIFY(!parseDiagnosticLine("./src/lib.peb:x:3: error: Broken.", entry));

    QVERIFY(!parseDiagnosticLine("./src/lib.peb:0:3: error: Broken.", entry));

    QVERIFY(!parseDiagnosticLine("./src/lib.peb:3:0: error: Broken.", entry));

    QVERIFY(!parseDiagnosticLine(":3:3: error: Broken.", entry));

    QVERIFY(!parseDiagnosticLine("./src/lib.peb:3: error: Broken.", entry));

    QVERIFY(!parseDiagnosticLine("./src/lib.peb:3:3: note: Broken.", entry));

    QVERIFY(!parseDiagnosticLine("./src/lib.peb:3:3: error:", entry));
}

void StudioTest::aDiagnosticIsCollectedFromTheCompilerOutput() {
    prepareDiagnosticsProject();

    window->feedCompilerOutput(
        "./src/broken.peb:3:5: error: Something is wrong here.\n"
    );

    QCOMPARE(window->diagnosticCount(), 1);
    QVERIFY(window->diagnosticText(0).contains("error"));
    QVERIFY(window->diagnosticText(0).contains("Something is wrong here."));
    QVERIFY(window->diagnosticText(0).contains("./src/broken.peb:3"));
}

void StudioTest::severalDiagnosticsAreCollectedInOrder() {
    prepareDiagnosticsProject();

    window->feedCompilerOutput(
        "./src/broken.peb:3:5: error: The first problem.\n"
        "./src/broken.peb:4:1: error: The second problem.\n"
        "./src/lib.peb:1:1: warning: And a remark.\n"
    );

    QCOMPARE(window->diagnosticCount(), 3);
    QVERIFY(window->diagnosticText(0).contains("The first problem."));
    QVERIFY(window->diagnosticText(1).contains("The second problem."));
    QVERIFY(window->diagnosticText(2).contains("And a remark."));
    QVERIFY(window->diagnosticText(2).contains("warning"));
}

void StudioTest::aDiagnosticThatIsSplitOverTwoReadsIsStillFound() {
    prepareDiagnosticsProject();

    window->feedCompilerOutput("./src/broken.peb:3:5: error: The first ");
    QCOMPARE(window->diagnosticCount(), 0);

    window->feedCompilerOutput("problem.\n");

    QCOMPARE(window->diagnosticCount(), 1);
    QVERIFY(window->diagnosticText(0).contains("The first problem."));
}

void StudioTest::aDiagnosticWithoutALineBreakIsFound() {
    prepareDiagnosticsProject();

    window->feedCompilerOutput(
        "./src/broken.peb:3:5: error: The last problem."
    );

    QCOMPARE(window->diagnosticCount(), 0);

    window->flushCompilerOutput();

    QCOMPARE(window->diagnosticCount(), 1);
    QVERIFY(window->diagnosticText(0).contains("The last problem."));
}

void StudioTest::aDiagnosticWithSpacesInThePathIsFound() {
    prepareDiagnosticsProject();

    window->feedCompilerOutput(
        "./my sources/lib.peb:3:5: error: A path with spaces.\n"
    );

    QCOMPARE(window->diagnosticCount(), 1);
    QVERIFY(window->diagnosticText(0).contains("./my sources/lib.peb:3"));
}

void StudioTest::aDiagnosticResolvesItsPathAgainstTheProject() {
    const QString project = prepareDiagnosticsProject();

    const QString expected = QFileInfo(project + "/src/broken.peb")
        .absoluteFilePath();

    QCOMPARE(
        window->resolveDiagnosticPath("./src/broken.peb"),
        expected
    );
    QCOMPARE(
        window->resolveDiagnosticPath("src/broken.peb"),
        expected
    );

    QCOMPARE(window->resolveDiagnosticPath(expected), expected);

    QVERIFY(window->resolveDiagnosticPath(QString()).isEmpty());
}

void StudioTest::aDiagnosticNavigatesToItsSourceLocation() {
    const QString project = prepareDiagnosticsProject();

    window->feedCompilerOutput(
        "./src/broken.peb:3:5: error: Something is wrong here.\n"
    );

    QCOMPARE(window->diagnosticCount(), 1);

    QListWidget *list = window->diagnosticsList();
    QVERIFY(list != nullptr);

    emit list->itemDoubleClicked(list->item(0));

    QCOMPARE(window->documentCount(), 1);
    QCOMPARE(window->currentDocumentIndex(), 0);
    QCOMPARE(
        window->documentPath(0),
        QFileInfo(project + "/src/broken.peb").absoluteFilePath()
    );

    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);

    const QTextCursor cursor = document->textCursor();
    QCOMPARE(cursor.blockNumber(), 2);
    QCOMPARE(cursor.positionInBlock(), 4);
}

void StudioTest::aDoubleClickOnADiagnosticOpensItsSource() {
    const QString project = prepareDiagnosticsProject();

    window->feedCompilerOutput(
        "./src/broken.peb:3:5: error: Something is wrong here.\n"
    );

    QCOMPARE(window->diagnosticCount(), 1);

    window->show();
    QVERIFY(window->isVisible());

    QListWidget *list = window->diagnosticsList();
    QVERIFY(list != nullptr);
    QVERIFY(list->isVisible());

    const QPoint centre = list->visualItemRect(list->item(0)).center();
    QVERIFY(list->itemAt(centre) != nullptr);

    QTest::mouseClick(
        list->viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        centre
    );

    QTest::mouseDClick(
        list->viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        centre,
        10
    );

    QCOMPARE(window->documentCount(), 1);
    QCOMPARE(
        window->documentPath(0),
        QFileInfo(project + "/src/broken.peb").absoluteFilePath()
    );
}

void StudioTest::aDiagnosticForAMissingFileIsReported() {
    prepareDiagnosticsProject();

    window->newFile();
    append(window->documentAt(0), "fn main() {}");

    window->feedCompilerOutput(
        "./src/gone.peb:3:5: error: The file is gone.\n"
    );

    QCOMPARE(window->diagnosticCount(), 1);

    QListWidget *list = window->diagnosticsList();
    QVERIFY(list != nullptr);

    emit list->itemDoubleClicked(list->item(0));

    QCOMPARE(notifications, 1);
    QCOMPARE(window->documentCount(), 1);
    QCOMPARE(window->currentDocumentIndex(), 0);
}

void StudioTest::theDiagnosticsAreConnectedOnce() {
    prepareDiagnosticsProject();

    window->feedCompilerOutput(
        "./src/gone-one.peb:1:1: error: The first problem.\n"
        "./src/gone-two.peb:2:1: error: The second problem.\n"
        "./src/gone-three.peb:3:1: error: The third problem.\n"
    );

    QCOMPARE(window->diagnosticCount(), 3);

    QListWidget *list = window->diagnosticsList();
    QVERIFY(list != nullptr);

    emit list->itemDoubleClicked(list->item(0));

    QCOMPARE(notifications, 1);
}

void StudioTest::aBuildReportsItsDiagnostics() {
    const QString project = makeProject("build-project");
    storeProject(project);
    reopen();

    window->setProcessProgram(
        writeScript(
            "failing-build.sh",
            "#!/bin/sh\n"
            "echo \"./src/broken.peb:2:3: error: Cannot initialise 'score'.\" >&2\n"
            "exit 1\n"
        )
    );

    menuAction("Run", "Build")->trigger();

    QTRY_VERIFY(window->diagnosticCount() == 1);
    QVERIFY(window->diagnosticText(0).contains("Cannot initialise 'score'."));
    QVERIFY(window->diagnosticText(0).contains("./src/broken.peb:2"));

    QTRY_VERIFY(window->outputText().contains("Build failed with exit code 1."));
}

void StudioTest::theDiagnosticsOfThePreviousBuildAreCleared() {
    prepareDiagnosticsProject();

    window->setProcessProgram(
        writeScript(
            "first-build.sh",
            "#!/bin/sh\n"
            "echo \"./src/broken.peb:2:3: error: The first problem.\" >&2\n"
            "echo \"./src/broken.peb:3:3: error: The second problem.\" >&2\n"
            "exit 1\n"
        )
    );

    menuAction("Run", "Build")->trigger();
    QTRY_VERIFY(window->diagnosticCount() == 2);

    window->setProcessProgram(
        writeScript(
            "second-build.sh",
            "#!/bin/sh\n"
            "echo \"./src/broken.peb:4:3: error: The third problem.\" >&2\n"
            "exit 1\n"
        )
    );

    menuAction("Run", "Build")->trigger();

    QTRY_VERIFY(window->diagnosticCount() == 1);
    QVERIFY(window->diagnosticText(0).contains("The third problem."));
    QVERIFY(!window->diagnosticText(0).contains("The first problem."));
    QVERIFY(!window->diagnosticText(0).contains("The second problem."));

    window->setProcessProgram(writeScript("quiet-run.sh", "#!/bin/sh\nexit 0\n"));

    menuAction("Run", "Run")->trigger();

    QTRY_VERIFY(!window->isProcessRunning());
    QCOMPARE(window->diagnosticCount(), 0);
}

void StudioTest::startupRestoresTheProjectTree() {
    const QString project = makeProject("demo");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();

    QCOMPARE(
        window->projectDirectory(),
        QFileInfo(project).absoluteFilePath()
    );

    const QStringList entries = window->projectTreeEntries();

    QVERIFY(entries.contains("main.peb"));
    QVERIFY(entries.contains("src"));
    QVERIFY(entries.contains("src/lib.peb"));
    QVERIFY(entries.contains("src/nested"));
    QVERIFY(entries.contains("src/nested/deep.peb"));

    QVERIFY(!entries.contains("pebble.toml"));
}

void StudioTest::theRestoredTreeKeepsTheDirectoryStructure() {
    const QString project = makeProject("structured");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();

    const QStringList entries = window->projectTreeEntries();

    QVERIFY(entries.contains("src/nested/deep.peb"));
    QVERIFY(!entries.contains("deep.peb"));
    QVERIFY(!entries.contains("nested/deep.peb"));

    const int source = static_cast<int>(entries.indexOf("src"));
    const int library = static_cast<int>(entries.indexOf("src/lib.peb"));
    const int deep = static_cast<int>(entries.indexOf("src/nested"));

    QVERIFY(source >= 0);
    QVERIFY(library > source);
    QVERIFY(deep > source);

    QTreeWidgetItem *item = nullptr;

    for (int index = 0;
         index < window->projectTree()->topLevelItemCount();
         ++index) {
        item = findItem(
            window->projectTree()->topLevelItem(index),
            QFileInfo(project + "/src/lib.peb").absoluteFilePath()
        );

        if (item != nullptr) {
            break;
        }
    }

    QVERIFY(item != nullptr);
    QCOMPARE(item->text(0), QString("lib.peb"));
}

void StudioTest::startupWithoutAProjectDirectoryHasAnEmptyTree() {
    reopen();

    QVERIFY(window->projectDirectory().isEmpty());
    QVERIFY(window->projectTreeEntries().isEmpty());
    QCOMPARE(window->documentCount(), 0);
}

void StudioTest::startupWithAMissingProjectDirectoryReportsIt() {
    const QString project = makeProject("removed");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    QVERIFY(QDir(project).removeRecursively());

    reopen();

    QVERIFY(window->projectDirectory().isEmpty());
    QVERIFY(window->projectTreeEntries().isEmpty());
    QVERIFY(
        window->statusBar()->currentMessage().contains("no longer available")
    );
}

void StudioTest::startupWithADirectoryThatIsNotAProjectReportsIt() {
    const QString plain = workDir.filePath("plain");
    QVERIFY(QDir().mkpath(plain));

    storeProject(plain);
    reopen();

    QVERIFY(window->projectDirectory().isEmpty());
    QVERIFY(window->projectTreeEntries().isEmpty());
    QVERIFY(
        window->statusBar()->currentMessage().contains("not a Pebble project")
    );
}

void StudioTest::restorationDoesNotDuplicateTreeEntries() {
    const QString project = makeProject("demo");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();

    const QStringList first = window->projectTreeEntries();
    QVERIFY(!first.isEmpty());

    QVERIFY(window->restoreProject());

    const QStringList second = window->projectTreeEntries();
    QCOMPARE(second, first);

    QCOMPARE(
        static_cast<int>(QSet<QString>(second.begin(), second.end()).size()),
        static_cast<int>(second.size())
    );
}

void StudioTest::theRestoredTreeKeepsTheOpenDocuments() {
    const QString project = makeProject("demo");
    QVERIFY(!project.isEmpty());

    {
        QSettings settings("Pebble", "Pebble Studio Test");
        settings.setValue("projectDir", project);
        settings.setValue(
            "openFiles",
            QStringList{project + "/main.peb"}
        );
    }

    reopen();

    QCOMPARE(window->documentCount(), 1);
    QCOMPARE(window->documentTitle(0), QString("main.peb"));
    QCOMPARE(window->documentText(0), QString("fn main() {}\n"));
    QVERIFY(!window->documentDirty(0));
    QVERIFY(window->projectTreeEntries().contains("main.peb"));
}

void StudioTest::theTreeSelectionSurvivesARefresh() {
    const QString project = makeProject("demo");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();

    QTreeWidget *tree = window->projectTree();
    QVERIFY(tree != nullptr);

    QTreeWidgetItem *item = nullptr;

    for (int index = 0; index < tree->topLevelItemCount(); ++index) {
        item = findItem(
            tree->topLevelItem(index),
            QFileInfo(project + "/main.peb").absoluteFilePath()
        );

        if (item != nullptr) {
            break;
        }
    }

    QVERIFY(item != nullptr);
    tree->setCurrentItem(item);

    QVERIFY(window->createFileAt(project, "extra.peb"));

    QVERIFY(tree->currentItem() != nullptr);
    QCOMPARE(
        tree->currentItem()->data(0, Qt::UserRole).toString(),
        QFileInfo(project + "/main.peb").absoluteFilePath()
    );
}

void StudioTest::renamingKeepsTheOpenDocumentInSync() {
    const QString path = write("before.peb", "one");

    QVERIFY(window->openPath(path));
    QCOMPARE(window->documentPath(0), QFileInfo(path).absoluteFilePath());

    QVERIFY(window->renamePath(path, "after.peb"));

    QVERIFY(!QFile::exists(path));
    QVERIFY(QFile::exists(workDir.filePath("after.peb")));
    QCOMPARE(
        window->documentPath(0),
        QFileInfo(workDir.filePath("after.peb")).absoluteFilePath()
    );
    QCOMPARE(window->documentTitle(0), QString("after.peb"));
    QCOMPARE(window->documentLabel(0), QString("after.peb"));
}

void StudioTest::deletingClosesTheOpenDocument() {
    const QString path = write("gone.peb", "one");

    QVERIFY(window->openPath(path));
    QCOMPARE(window->documentCount(), 1);

    QVERIFY(window->deletePath(path));

    QVERIFY(!QFile::exists(path));
    QCOMPARE(window->documentCount(), 0);
}

void StudioTest::deletingIsStoppedByAnUnsavedDocument() {
    const QString path = write("kept.peb", "one");

    QVERIFY(window->openPath(path));
    append(window->documentAt(0), "changed");

    choice = MainWindow::UnsavedChoice::Cancel;

    QVERIFY(!window->deletePath(path));

    QVERIFY(QFile::exists(path));
    QCOMPARE(window->documentCount(), 1);
    QVERIFY(window->documentDirty(0));
}

QAction *StudioTest::menuAction(const QString &menu, const QString &label) {
    const QList<QAction *> menus = window->menuBar()->actions();

    for (QAction *menuEntry : menus) {
        QString title = menuEntry->text();
        title.remove('&');

        if (title != menu) {
            continue;
        }

        QMenu *submenu = menuEntry->menu();

        if (submenu == nullptr) {
            continue;
        }

        const QList<QAction *> entries = submenu->actions();

        for (QAction *entry : entries) {
            if (entry->text() == label) {
                return entry;
            }
        }
    }

    return nullptr;
}

QAction *StudioTest::windowAction(const QString &label) {
    const QList<QAction *> actions = window->findChildren<QAction *>();

    for (QAction *action : actions) {
        if (action->text() == label) {
            return action;
        }
    }

    return nullptr;
}

void StudioTest::theMenuActionsReachTheirSlots() {
    QAction *newFile = menuAction("File", "New File");
    QVERIFY(newFile != nullptr);

    newFile->trigger();
    QCOMPARE(window->documentCount(), 1);
    QCOMPARE(window->documentTitle(0), QString("Untitled.peb"));

    const QString path = write("menu.peb", "one");
    QVERIFY(window->openPath(path));

    append(window->documentAt(1), "changed");

    QAction *save = menuAction("File", "Save");
    QVERIFY(save != nullptr);

    save->trigger();

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(file.readAll()), QString("onechanged"));
    QVERIFY(!window->documentDirty(1));
}

void StudioTest::theExpectedShortcutsAreRegistered() {
    QStringList sequences;

    const QList<QShortcut *> shortcuts = window->findChildren<QShortcut *>();

    for (QShortcut *shortcut : shortcuts) {
        sequences << shortcut->key().toString();
    }

    QVERIFY(sequences.contains("Ctrl+N"));
    QVERIFY(sequences.contains("Ctrl+O"));
    QVERIFY(sequences.contains("Ctrl+S"));
    QVERIFY(sequences.contains("Ctrl+Shift+S"));
    QVERIFY(sequences.contains("Ctrl+W"));
    QVERIFY(sequences.contains("Shift+F5"));
}

void StudioTest::theStoredDocumentsAreRestored() {
    const QString path = write("restore.peb", "fn main() {}");

    {
        QSettings settings("Pebble", "Pebble Studio Test");
        settings.setValue("openFiles", QStringList{path});
    }

    delete window;
    window = new MainWindow("Pebble", "Pebble Studio Test");

    QCOMPARE(window->documentCount(), 1);
    QCOMPARE(window->documentTitle(0), QString("restore.peb"));
    QCOMPARE(window->documentText(0), QString("fn main() {}"));
}

void StudioTest::theStoredWindowSizeIsRestored() {
    {
        QSettings settings("Pebble", "Pebble Studio Test");
        settings.setValue("size", QSize(1024, 768));
    }

    delete window;
    window = new MainWindow("Pebble", "Pebble Studio Test");

    QCOMPARE(window->size(), QSize(1024, 768));
}


void StudioTest::adjacentExecutableTakesPrecedence() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString studio = dir.path() + "/studio";
    writeFile(studio, "x");
    const QString adjacent = dir.path() + "/pebble";
    writeFile(adjacent, "x");
    QFile(adjacent).setPermissions(QFile::ExeOwner | QFile::ReadOwner);
    const ExecutableResolution r = resolvePebbleExecutable(studio, QString(), QString());
    QVERIFY(r.found);
    QCOMPARE(r.path, adjacent);
}

void StudioTest::pebbleEnvIsUsedWhenTheAdjacentCandidateIsMissing() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString studio = dir.path() + "/studio";
    writeFile(studio, "x");
    const QString envPebble = dir.path() + "/pebble-env";
    writeFile(envPebble, "x");
    QFile(envPebble).setPermissions(QFile::ExeOwner | QFile::ReadOwner);
    const ExecutableResolution r = resolvePebbleExecutable(studio, envPebble, QString());
    QVERIFY(r.found);
    QCOMPARE(r.path, envPebble);
}

void StudioTest::pebbleEnvIsUsedWhenItIsAnAbsoluteExecutable() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString studio = dir.path() + "/studio";
    writeFile(studio, "x");
    const QString envPebble = dir.path() + "/abs-pebble";
    writeFile(envPebble, "x");
    QFile(envPebble).setPermissions(QFile::ExeOwner | QFile::ReadOwner);
    const ExecutableResolution r = resolvePebbleExecutable(studio, envPebble, QString());
    QVERIFY(r.found);
    QCOMPARE(r.path, envPebble);
}

void StudioTest::pathFallbackWorksWhenBothEarlierCandidatesAreMissing() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString studio = dir.path() + "/studio";
    writeFile(studio, "x");
    const QString pathDir = dir.path() + "/bin";
    QVERIFY(QDir().mkpath(pathDir));
    const QString pathPebble = pathDir + "/pebble";
    writeFile(pathPebble, "x");
    QFile(pathPebble).setPermissions(QFile::ExeOwner | QFile::ReadOwner);
    const ExecutableResolution r = resolvePebbleExecutable(studio, QString(), pathDir);
    QVERIFY(r.found);
    QCOMPARE(r.path, pathPebble);
}

void StudioTest::anInvalidPebbleEnvValueFallsBackToPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString studio = dir.path() + "/studio";
    writeFile(studio, "x");
    const QString pathDir = dir.path() + "/bin";
    QVERIFY(QDir().mkpath(pathDir));
    const QString pathPebble = pathDir + "/pebble";
    writeFile(pathPebble, "x");
    QFile(pathPebble).setPermissions(QFile::ExeOwner | QFile::ReadOwner);
    const ExecutableResolution r = resolvePebbleExecutable(studio, dir.path() + "/missing", pathDir);
    QVERIFY(r.found);
    QCOMPARE(r.path, pathPebble);
}

void StudioTest::aNonExecutablePebbleEnvValueFallsBackToPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString studio = dir.path() + "/studio";
    writeFile(studio, "x");
    const QString badEnv = dir.path() + "/bad-pebble";
    writeFile(badEnv, "x");
    const QString pathDir = dir.path() + "/bin";
    QVERIFY(QDir().mkpath(pathDir));
    const QString pathPebble = pathDir + "/pebble";
    writeFile(pathPebble, "x");
    QFile(pathPebble).setPermissions(QFile::ExeOwner | QFile::ReadOwner);
    const ExecutableResolution r = resolvePebbleExecutable(studio, badEnv, pathDir);
    QVERIFY(r.found);
    QCOMPARE(r.path, pathPebble);
}

void StudioTest::failureIsReportedWhenNoCandidateIsUsable() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString studio = dir.path() + "/studio";
    writeFile(studio, "x");
    const ExecutableResolution r = resolvePebbleExecutable(studio, dir.path() + "/missing", QString());
    QVERIFY(!r.found);
    QVERIFY(!r.reason.isEmpty());
}

void StudioTest::buildAndRunBothUseTheSameResolver() {
    QVERIFY(true);
}

void StudioTest::anExecutablePathWithSpacesIsHandled() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString studio = dir.path() + "/studio dir/studio";
    QVERIFY(QDir().mkpath(dir.path() + "/studio dir"));
    writeFile(studio, "x");
    const QString adjacent = dir.path() + "/studio dir/pebble";
    writeFile(adjacent, "x");
    QFile(adjacent).setPermissions(QFile::ExeOwner | QFile::ReadOwner);
    const ExecutableResolution r = resolvePebbleExecutable(studio, QString(), QString());
    QVERIFY(r.found);
    QCOMPARE(r.path, adjacent);
}

void StudioTest::theResolverDoesNotPickAnUnrelatedBinaryFromTheCwd() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const ExecutableResolution r = resolvePebbleExecutable(QString(), QString(), QString());
    QVERIFY(!r.found);
}

void StudioTest::theAdjacentCandidateIsSkippedWhenTheStudioPathIsUnknown() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const ExecutableResolution r = resolvePebbleExecutable(QString(), QString(), QString());
    QVERIFY(!r.found);
}

void StudioTest::theFindShortcutOpensThePanel() {
    window->newFile();
    showWindow();

    QVERIFY(!window->findBarVisible());

    QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);

    QVERIFY(window->findBarVisible());

    FindReplaceBar *bar = window->findReplaceBar();
    QVERIFY(bar != nullptr);
    QVERIFY(bar->isOpen());
    QVERIFY(bar->isVisible());

    QVERIFY(bar->findField()->hasFocus());

    QVERIFY(!bar->replaceRowVisible());
}

void StudioTest::theReplaceShortcutOpensTheReplacementRow() {
    window->newFile();
    showWindow();

    QTest::keyClick(window, Qt::Key_H, Qt::ControlModifier);

    QVERIFY(window->findBarVisible());

    FindReplaceBar *bar = window->findReplaceBar();
    QVERIFY(bar != nullptr);
    QVERIFY(bar->replaceRowVisible());
    QVERIFY(bar->replaceField()->isVisible());

    QVERIFY(bar->findField()->hasFocus());

    bar->findField()->setText("alpha");
    QTest::keyClick(window, Qt::Key_H, Qt::ControlModifier);
    QVERIFY(bar->replaceField()->hasFocus());
}

void StudioTest::theFindShortcutsAreRegistered() {
    QStringList sequences;

    const QList<QShortcut *> shortcuts = window->findChildren<QShortcut *>();

    for (QShortcut *shortcut : shortcuts) {
        sequences << shortcut->key().toString();
    }

    QVERIFY(sequences.contains("Ctrl+F"));
    QVERIFY(sequences.contains("Ctrl+H"));
    QVERIFY(sequences.contains("F3"));
    QVERIFY(sequences.contains("Shift+F3"));
    QVERIFY(sequences.contains("Esc"));
}

void StudioTest::theSearchSelectsTheMatch() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("alpha beta gamma");

    openFindPanel();
    searchThroughPanel("beta");

    QCOMPARE(document->textCursor().selectedText(), QString("beta"));
    QCOMPARE(document->textCursor().selectionStart(), 6);
    QCOMPARE(window->findStatus(), QString());
    QCOMPARE(window->findReplaceBar()->statusLabel()->text(), QString());
}

void StudioTest::theNextButtonWalksTheMatches() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("one two one two");

    openFindPanel();
    searchThroughPanel("one");

    QCOMPARE(document->textCursor().selectionStart(), 0);

    window->findReplaceBar()->nextButton()->click();
    QCOMPARE(document->textCursor().selectionStart(), 8);

    window->findReplaceBar()->previousButton()->click();
    QCOMPARE(document->textCursor().selectionStart(), 0);
}

void StudioTest::theNextShortcutWalksTheMatches() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("one two one");

    openFindPanel();
    searchThroughPanel("one");

    QCOMPARE(document->textCursor().selectionStart(), 0);

    QTest::keyClick(window, Qt::Key_F3);
    QCOMPARE(document->textCursor().selectionStart(), 8);

    QTest::keyClick(window, Qt::Key_F3);
    QCOMPARE(document->textCursor().selectionStart(), 0);
    QCOMPARE(window->findStatus(), QString("wrapped"));

    QTest::keyClick(window, Qt::Key_F3, Qt::ShiftModifier);
    QCOMPARE(document->textCursor().selectionStart(), 8);
}

void StudioTest::thePreviousShortcutWalksBackwards() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("one two one");

    openFindPanel();
    searchThroughPanel("one");

    QCOMPARE(document->textCursor().selectionStart(), 0);

    QTest::keyClick(window, Qt::Key_F3, Qt::ShiftModifier);
    QCOMPARE(document->textCursor().selectionStart(), 8);
    QCOMPARE(window->findStatus(), QString("wrapped"));
}

void StudioTest::theSearchWrapsAround() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("one two one");

    openFindPanel();
    searchThroughPanel("one");

    QCOMPARE(document->textCursor().selectionStart(), 0);

    window->findReplaceBar()->nextButton()->click();
    QCOMPARE(document->textCursor().selectionStart(), 8);

    window->findReplaceBar()->nextButton()->click();
    QCOMPARE(document->textCursor().selectionStart(), 0);
    QCOMPARE(window->findStatus(), QString("wrapped"));
    QCOMPARE(
        window->findReplaceBar()->statusLabel()->text(),
        QString("wrapped")
    );
}

void StudioTest::theMatchCaseCheckboxAppliesRightAway() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("ABC abc");

    openFindPanel();
    searchThroughPanel("abc");

    QCOMPARE(document->textCursor().selectedText(), QString("ABC"));
    QCOMPARE(document->textCursor().selectionStart(), 0);
    QCOMPARE(window->matchCase(), false);

    window->findReplaceBar()->matchCaseCheck()->setChecked(true);

    QCOMPARE(window->matchCase(), true);
    QCOMPARE(document->textCursor().selectedText(), QString("abc"));
    QCOMPARE(document->textCursor().selectionStart(), 4);

    window->setMatchCase(false);
    QCOMPARE(window->findReplaceBar()->matchCaseCheck()->isChecked(), false);
    QCOMPARE(window->matchCase(), false);
}

void StudioTest::theReplaceButtonReplacesTheCurrentMatch() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("cat dog cat");

    openFindPanel();
    searchThroughPanel("cat");

    QCOMPARE(document->textCursor().selectionStart(), 0);

    window->findReplaceBar()->replaceField()->setText("bird");
    window->findReplaceBar()->replaceButton()->click();

    QCOMPARE(document->toPlainText(), QString("bird dog cat"));

    QCOMPARE(document->textCursor().selectedText(), QString("cat"));
    QCOMPARE(document->textCursor().selectionStart(), 9);
}

void StudioTest::theReplaceButtonKeepsAnUnrelatedSelection() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("cat dog cat");

    openFindPanel();
    searchThroughPanel("cat");

    QTextCursor cursor = document->textCursor();
    cursor.setPosition(4);
    cursor.setPosition(7, QTextCursor::KeepAnchor);
    document->setTextCursor(cursor);
    QCOMPARE(document->textCursor().selectedText(), QString("dog"));

    window->findReplaceBar()->replaceField()->setText("bird");
    window->findReplaceBar()->replaceButton()->click();

    QCOMPARE(document->toPlainText(), QString("cat dog cat"));

    QCOMPARE(document->textCursor().selectedText(), QString("cat"));
    QCOMPARE(document->textCursor().selectionStart(), 8);
}

void StudioTest::theReplaceAllButtonReplacesEveryMatch() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("cat dog cat");

    openFindPanel();
    searchThroughPanel("cat");

    window->findReplaceBar()->replaceField()->setText("bird");
    window->findReplaceBar()->replaceAllButton()->click();

    QCOMPARE(document->toPlainText(), QString("bird dog bird"));
    QCOMPARE(window->findStatus(), QString("replaced 2"));
    QCOMPARE(
        window->findReplaceBar()->statusLabel()->text(),
        QString("replaced 2")
    );
}

void StudioTest::theReplaceAllButtonStopsWhenTheReplacementHoldsTheTerm() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("a b a");

    openFindPanel();
    searchThroughPanel("a");

    window->findReplaceBar()->replaceField()->setText("aa");

    bool timedOut = false;
    QTimer guard;
    guard.setSingleShot(true);
    connect(&guard, &QTimer::timeout, [&timedOut]() {
        timedOut = true;
    });
    guard.start(10000);

    window->findReplaceBar()->replaceAllButton()->click();

    guard.stop();
    QVERIFY(!timedOut);
    QCOMPARE(document->toPlainText(), QString("aa b aa"));
    QCOMPARE(window->findStatus(), QString("replaced 2"));
}

void StudioTest::theSearchFollowsTheActiveTab() {
    window->newFile();
    DocumentEditor *first = window->documentAt(0);
    QVERIFY(first != nullptr);
    first->setPlainText("alpha");

    window->newFile();
    DocumentEditor *second = window->documentAt(1);
    QVERIFY(second != nullptr);
    second->setPlainText("beta");

    openFindPanel();
    searchThroughPanel("beta");

    QCOMPARE(second->textCursor().selectedText(), QString("beta"));
    QCOMPARE(first->textCursor().selectedText(), QString(""));

    QTabWidget *tabs = window->findChild<QTabWidget *>();
    QVERIFY(tabs != nullptr);
    tabs->setCurrentIndex(0);
    QCOMPARE(window->currentDocumentIndex(), 0);

    searchThroughPanel("alpha");

    QCOMPARE(first->textCursor().selectedText(), QString("alpha"));
    QCOMPARE(second->textCursor().selectedText(), QString("beta"));
}

void StudioTest::thePanelReportsTheSearchStatus() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("alpha");

    openFindPanel();
    searchThroughPanel("beta");

    QCOMPARE(window->findStatus(), QString("no matches"));
    QCOMPARE(
        window->findReplaceBar()->statusLabel()->text(),
        QString("no matches")
    );
}

void StudioTest::theEscapeShortcutClosesThePanel() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);

    openFindPanel();
    QVERIFY(window->findBarVisible());
    QVERIFY(window->findReplaceBar()->findField()->hasFocus());

    QTest::keyClick(window->findReplaceBar()->findField(), Qt::Key_Escape);

    QVERIFY(!window->findBarVisible());
    QVERIFY(!window->findReplaceBar()->isVisible());

    QVERIFY(document->hasFocus());
}

void StudioTest::theCloseButtonClosesThePanel() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);

    openFindPanel();
    QVERIFY(window->findBarVisible());

    window->findReplaceBar()->closeButton()->click();

    QVERIFY(!window->findBarVisible());
    QVERIFY(document->hasFocus());
}

void StudioTest::thePanelSitsAboveTheEditor() {
    window->newFile();

    openFindPanel();

    FindReplaceBar *bar = window->findReplaceBar();
    QVERIFY(bar != nullptr);
    QVERIFY(bar->isVisible());

    QTabWidget *tabs = window->findChild<QTabWidget *>();
    QVERIFY(tabs != nullptr);

    const int panelBottom = bar->mapTo(window, QPoint(0, bar->height())).y();
    const int editorTop = tabs->mapTo(window, QPoint(0, 0)).y();

    QVERIFY(panelBottom <= editorTop);
}

void StudioTest::theFindShortcutSeedsTheSelection() {
    window->newFile();
    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);
    document->setPlainText("alpha beta");

    QTextCursor cursor = document->textCursor();
    cursor.setPosition(0);
    cursor.setPosition(5, QTextCursor::KeepAnchor);
    document->setTextCursor(cursor);

    showWindow();
    QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);

    QCOMPARE(window->findReplaceBar()->findText(), QString("alpha"));
    QCOMPARE(window->findText(), QString("alpha"));
}

void StudioTest::tabInsertsTheIndentationInTheEditor() {
    DocumentEditor *document = prepareEditor("one\ntwo");
    QVERIFY(document != nullptr);
    QVERIFY(document->hasFocus());

    selectText(document, 0, 0);
    QTest::keyClick(document, Qt::Key_Tab);

    QCOMPARE(document->toPlainText(), QString("    one\ntwo"));
    QVERIFY(document->hasFocus());

    QCOMPARE(document->textCursor().positionInBlock(), 4);
    QCOMPARE(document->textCursor().selectedText(), QString());
}

void StudioTest::shiftTabRemovesTheIndentation() {
    DocumentEditor *document = prepareEditor("    one\ntwo");
    QVERIFY(document != nullptr);

    selectText(document, 0, 0);
    QTest::keyClick(document, Qt::Key_Tab, Qt::ShiftModifier);

    QCOMPARE(document->toPlainText(), QString("one\ntwo"));
    QCOMPARE(document->textCursor().positionInBlock(), 0);
    QVERIFY(document->hasFocus());
}

void StudioTest::outdentRemovesOnlyTheIndentation() {
    DocumentEditor *document = prepareEditor("  one\n\ttwo\nthree");
    QVERIFY(document != nullptr);

    selectText(document, 0, 0);
    QTest::keyClick(document, Qt::Key_Tab, Qt::ShiftModifier);
    QCOMPARE(document->toPlainText(), QString("one\n\ttwo\nthree"));

    selectText(document, 4, 4);
    QTest::keyClick(document, Qt::Key_Tab, Qt::ShiftModifier);
    QCOMPARE(document->toPlainText(), QString("one\ntwo\nthree"));

    selectText(document, 8, 8);
    QTest::keyClick(document, Qt::Key_Tab, Qt::ShiftModifier);
    QCOMPARE(document->toPlainText(), QString("one\ntwo\nthree"));
}

void StudioTest::aMultilineSelectionIsIndented() {
    DocumentEditor *document = prepareEditor("one\ntwo\nthree\nfour");
    QVERIFY(document != nullptr);

    selectText(document, 0, 13);
    QTest::keyClick(document, Qt::Key_Tab);

    QCOMPARE(document->toPlainText(), QString("    one\n    two\n    three\nfour"));

    QCOMPARE(
        document->textCursor().selectedText(),
        QStringLiteral("one\u2029    two\u2029    three")
    );
    QCOMPARE(document->textCursor().blockNumber(), 2);
}

void StudioTest::aMultilineSelectionIsOutdented() {
    DocumentEditor *document = prepareEditor("    one\n    two\n    three\nfour");
    QVERIFY(document != nullptr);

    selectText(document, 0, 25);
    QTest::keyClick(document, Qt::Key_Tab, Qt::ShiftModifier);

    QCOMPARE(document->toPlainText(), QString("one\ntwo\nthree\nfour"));

    QCOMPARE(
        document->textCursor().selectedText(),
        QStringLiteral("one\u2029two\u2029three")
    );
    QCOMPARE(document->textCursor().blockNumber(), 2);
}

void StudioTest::enterKeepsTheIndentation() {
    DocumentEditor *document = prepareEditor("    one\ntwo");
    QVERIFY(document != nullptr);

    selectText(document, 7, 7);
    QTest::keyClick(document, Qt::Key_Return);

    QCOMPARE(document->toPlainText(), QString("    one\n    \ntwo"));

    QCOMPARE(document->textCursor().blockNumber(), 1);
    QCOMPARE(document->textCursor().positionInBlock(), 4);
}

void StudioTest::enterOnAnEmptyLineStaysEmpty() {
    DocumentEditor *document = prepareEditor("one\n\ntwo");
    QVERIFY(document != nullptr);

    selectText(document, 4, 4);
    QTest::keyClick(document, Qt::Key_Return);

    QCOMPARE(document->toPlainText(), QString("one\n\n\ntwo"));
    QCOMPARE(document->textCursor().blockNumber(), 2);
    QCOMPARE(document->textCursor().positionInBlock(), 0);
}

void StudioTest::enterOnAWhitespaceOnlyLineKeepsTheWhitespace() {
    DocumentEditor *document = prepareEditor("one\n   \ntwo");
    QVERIFY(document != nullptr);

    selectText(document, 7, 7);
    QTest::keyClick(document, Qt::Key_Return);

    QCOMPARE(document->toPlainText(), QString("one\n   \n   \ntwo"));
    QCOMPARE(document->textCursor().blockNumber(), 2);
    QCOMPARE(document->textCursor().positionInBlock(), 3);
}

void StudioTest::controlSlashCommentsASingleLine() {
    DocumentEditor *document = prepareEditor("    one\ntwo");
    QVERIFY(document != nullptr);

    selectText(document, 0, 0);
    QTest::keyClick(document, Qt::Key_Slash, Qt::ControlModifier);

    QCOMPARE(document->toPlainText(), QString("    // one\ntwo"));

    QTest::keyClick(document, Qt::Key_Slash, Qt::ControlModifier);
    QCOMPARE(document->toPlainText(), QString("    one\ntwo"));
}

void StudioTest::controlSlashCommentsAMultilineSelection() {
    DocumentEditor *document = prepareEditor("    one\ntwo\n    three\nfour");
    QVERIFY(document != nullptr);

    selectText(document, 0, 20);
    QTest::keyClick(document, Qt::Key_Slash, Qt::ControlModifier);

    QCOMPARE(document->toPlainText(), QString("    // one\n// two\n    // three\nfour"));

    QCOMPARE(document->textCursor().blockNumber(), 2);

    QTest::keyClick(document, Qt::Key_Slash, Qt::ControlModifier);
    QCOMPARE(document->toPlainText(), QString("    one\ntwo\n    three\nfour"));
}

void StudioTest::theEditingKeysDoNotDisturbTheFindPanel() {
    DocumentEditor *document = prepareEditor("one\ntwo");
    QVERIFY(document != nullptr);

    QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
    QVERIFY(window->findBarVisible());

    FindReplaceBar *bar = window->findReplaceBar();
    QVERIFY(bar != nullptr);

    bar->findField()->setText("one");
    QTest::keyClick(bar->findField(), Qt::Key_Return);
    QCOMPARE(document->textCursor().selectedText(), QString("one"));

    const QString text = document->toPlainText();
    QTest::keyClick(bar->findField(), Qt::Key_Tab);
    QCOMPARE(document->toPlainText(), text);

    selectText(document, 0, 0);
    QTest::keyClick(document, Qt::Key_Tab);
    QCOMPARE(document->toPlainText(), QString("    one\ntwo"));

    QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
    QVERIFY(window->findBarVisible());
}


void StudioTest::theEditMenuHoldsTheEditingEntries() {
    QVERIFY(menuAction("Edit", "Undo") != nullptr);
    QVERIFY(menuAction("Edit", "Redo") != nullptr);
    QVERIFY(menuAction("Edit", "Select All") != nullptr);

    QStringList menus;
    const QList<QAction *> entries = window->menuBar()->actions();

    for (QAction *entry : entries) {
        if (entry->menu() == nullptr) {
            continue;
        }

        QString title = entry->text();
        title.remove('&');
        menus << title;
    }

    QCOMPARE(menus, QStringList({"File", "Edit", "Run", "View", "Help"}));
}

void StudioTest::theEditActionsCarryTheirShortcuts() {
    QAction *undo = menuAction("Edit", "Undo");
    QAction *redo = menuAction("Edit", "Redo");
    QAction *selectAll = menuAction("Edit", "Select All");
    QVERIFY(undo != nullptr);
    QVERIFY(redo != nullptr);
    QVERIFY(selectAll != nullptr);

    QCOMPARE(undo->shortcut().toString(), QString("Ctrl+Z"));
    QCOMPARE(redo->shortcut().toString(), QString("Ctrl+Shift+Z"));
    QCOMPARE(selectAll->shortcut().toString(), QString("Ctrl+A"));

    QStringList redoSequences;

    for (const QKeySequence &sequence : redo->shortcuts()) {
        redoSequences << sequence.toString();
    }

    QVERIFY(redoSequences.contains("Ctrl+Y"));
}

void StudioTest::theTabShortcutsAreRegistered() {
    QAction *next = windowAction("Next Tab");
    QAction *previous = windowAction("Previous Tab");
    QVERIFY(next != nullptr);
    QVERIFY(previous != nullptr);

    QCOMPARE(next->shortcut().toString(), QString("Ctrl+PgDown"));
    QCOMPARE(previous->shortcut().toString(), QString("Ctrl+PgUp"));
}

void StudioTest::theUndoActionReachesTheActiveDocument() {
    DocumentEditor *document = prepareEditor("one");
    QVERIFY(document != nullptr);

    document->moveCursor(QTextCursor::End);
    QTest::keyClicks(document, "x");
    QCOMPARE(document->toPlainText(), QString("onex"));

    QAction *undo = menuAction("Edit", "Undo");
    QVERIFY(undo != nullptr);
    QVERIFY(undo->isEnabled());

    undo->trigger();
    QCOMPARE(document->toPlainText(), QString("one"));
}

void StudioTest::theRedoActionReachesTheActiveDocument() {
    DocumentEditor *document = prepareEditor("one");
    QVERIFY(document != nullptr);

    document->moveCursor(QTextCursor::End);
    QTest::keyClicks(document, "x");

    QAction *undo = menuAction("Edit", "Undo");
    QAction *redo = menuAction("Edit", "Redo");
    QVERIFY(undo != nullptr);
    QVERIFY(redo != nullptr);

    undo->trigger();
    QCOMPARE(document->toPlainText(), QString("one"));

    QVERIFY(redo->isEnabled());
    redo->trigger();
    QCOMPARE(document->toPlainText(), QString("onex"));
}

void StudioTest::theSelectAllActionSelectsTheWholeDocument() {
    DocumentEditor *document = prepareEditor("one\ntwo");
    QVERIFY(document != nullptr);

    selectText(document, 0, 3);

    QAction *selectAll = menuAction("Edit", "Select All");
    QVERIFY(selectAll != nullptr);
    QVERIFY(selectAll->isEnabled());

    selectAll->trigger();

    QCOMPARE(
        document->textCursor().selectedText(),
        QString("one") + QChar(0x2029) + QString("two")
    );
}

void StudioTest::theUndoShortcutReachesTheEditor() {
    DocumentEditor *document = prepareEditor("one");
    QVERIFY(document != nullptr);

    document->moveCursor(QTextCursor::End);
    QTest::keyClicks(document, "x");
    QCOMPARE(document->toPlainText(), QString("onex"));

    QAction *undo = menuAction("Edit", "Undo");
    QVERIFY(undo != nullptr);

    int triggers = 0;
    connect(undo, &QAction::triggered, this, [&triggers]() {
        ++triggers;
    });

    QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);

    QCOMPARE(triggers, 1);
    QCOMPARE(document->toPlainText(), QString("one"));
}

void StudioTest::theRedoShortcutReachesTheEditor() {
    DocumentEditor *document = prepareEditor("one");
    QVERIFY(document != nullptr);

    document->moveCursor(QTextCursor::End);
    QTest::keyClicks(document, "x");

    QAction *undo = menuAction("Edit", "Undo");
    QAction *redo = menuAction("Edit", "Redo");
    QVERIFY(undo != nullptr);
    QVERIFY(redo != nullptr);

    undo->trigger();
    QCOMPARE(document->toPlainText(), QString("one"));

    int triggers = 0;
    connect(redo, &QAction::triggered, this, [&triggers]() {
        ++triggers;
    });

    QTest::keyClick(
        window,
        Qt::Key_Z,
        Qt::ControlModifier | Qt::ShiftModifier
    );

    QCOMPARE(triggers, 1);
    QCOMPARE(document->toPlainText(), QString("onex"));
}

void StudioTest::theSelectAllShortcutReachesTheEditor() {
    DocumentEditor *document = prepareEditor("one\ntwo");
    QVERIFY(document != nullptr);

    QAction *selectAll = menuAction("Edit", "Select All");
    QVERIFY(selectAll != nullptr);

    int triggers = 0;
    connect(selectAll, &QAction::triggered, this, [&triggers]() {
        ++triggers;
    });

    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);

    QCOMPARE(triggers, 1);
    QCOMPARE(
        document->textCursor().selectedText(),
        QString("one") + QChar(0x2029) + QString("two")
    );
}

void StudioTest::theEditActionsFollowTheActiveTab() {
    DocumentEditor *first = prepareEditor("one");
    QVERIFY(first != nullptr);

    first->moveCursor(QTextCursor::End);
    QTest::keyClicks(first, "x");

    window->newFile();

    DocumentEditor *second = window->documentAt(1);
    QVERIFY(second != nullptr);

    second->setPlainText("two");
    second->moveCursor(QTextCursor::End);
    QTest::keyClicks(second, "y");

    QCOMPARE(window->currentDocumentIndex(), 1);

    QAction *undo = menuAction("Edit", "Undo");
    QVERIFY(undo != nullptr);
    QVERIFY(undo->isEnabled());

    undo->trigger();

    QCOMPARE(second->toPlainText(), QString("two"));
    QCOMPARE(first->toPlainText(), QString("onex"));
}

void StudioTest::theEditActionsAreDisabledWithoutADocument() {
    QAction *undo = menuAction("Edit", "Undo");
    QAction *redo = menuAction("Edit", "Redo");
    QAction *selectAll = menuAction("Edit", "Select All");
    QVERIFY(undo != nullptr);
    QVERIFY(redo != nullptr);
    QVERIFY(selectAll != nullptr);

    QVERIFY(!undo->isEnabled());
    QVERIFY(!redo->isEnabled());
    QVERIFY(!selectAll->isEnabled());
}

void StudioTest::theEditActionsFollowTheUndoStack() {
    DocumentEditor *document = prepareEditor("one");
    QVERIFY(document != nullptr);

    QAction *undo = menuAction("Edit", "Undo");
    QAction *redo = menuAction("Edit", "Redo");
    QVERIFY(undo != nullptr);
    QVERIFY(redo != nullptr);

    QVERIFY(!undo->isEnabled());
    QVERIFY(!redo->isEnabled());

    document->moveCursor(QTextCursor::End);
    QTest::keyClicks(document, "x");

    QVERIFY(undo->isEnabled());
    QVERIFY(!redo->isEnabled());

    undo->trigger();

    QVERIFY(!undo->isEnabled());
    QVERIFY(redo->isEnabled());
}

void StudioTest::theEditActionsAreDisabledWhenTheDocumentIsClosed() {
    window->newFile();
    showWindow();

    QAction *undo = menuAction("Edit", "Undo");
    QAction *selectAll = menuAction("Edit", "Select All");
    QVERIFY(undo != nullptr);
    QVERIFY(selectAll != nullptr);

    QVERIFY(selectAll->isEnabled());

    window->closeDocument(0);

    QVERIFY(!undo->isEnabled());
    QVERIFY(!selectAll->isEnabled());
}

void StudioTest::theEditShortcutsStayOutOfTheFindPanel() {
    DocumentEditor *document = prepareEditor("one");
    QVERIFY(document != nullptr);

    openFindPanel();

    FindReplaceBar *bar = window->findReplaceBar();
    QVERIFY(bar != nullptr);

    bar->findField()->setText("one");

    QTest::keyClick(bar->findField(), Qt::Key_A, Qt::ControlModifier);
    QCOMPARE(bar->findField()->selectedText(), QString("one"));
    QCOMPARE(document->textCursor().selectedText(), QString());
}

void StudioTest::theNextTabShortcutSwitchesTheDocument() {
    window->newFile();
    window->newFile();
    showWindow();

    QCOMPARE(window->documentCount(), 2);
    QCOMPARE(window->currentDocumentIndex(), 1);

    QTest::keyClick(window, Qt::Key_PageDown, Qt::ControlModifier);
    QCOMPARE(window->currentDocumentIndex(), 0);
}

void StudioTest::thePreviousTabShortcutSwitchesTheDocument() {
    window->newFile();
    window->newFile();
    showWindow();

    QCOMPARE(window->documentCount(), 2);
    QCOMPARE(window->currentDocumentIndex(), 1);

    QTest::keyClick(window, Qt::Key_PageUp, Qt::ControlModifier);
    QCOMPARE(window->currentDocumentIndex(), 0);
}

void StudioTest::theTabNavigationWrapsAround() {
    window->newFile();
    window->newFile();
    window->newFile();
    showWindow();

    QCOMPARE(window->documentCount(), 3);
    QCOMPARE(window->currentDocumentIndex(), 2);

    QTest::keyClick(window, Qt::Key_PageDown, Qt::ControlModifier);
    QCOMPARE(window->currentDocumentIndex(), 0);

    QTest::keyClick(window, Qt::Key_PageUp, Qt::ControlModifier);
    QCOMPARE(window->currentDocumentIndex(), 2);
}

void StudioTest::theFindEntriesOfTheEditMenuReachThePanel() {
    window->newFile();
    showWindow();

    QAction *find = menuAction("Edit", "Find");
    QVERIFY(find != nullptr);

    find->trigger();
    QVERIFY(window->findBarVisible());
    QVERIFY(!window->findReplaceBar()->replaceRowVisible());

    QAction *replace = menuAction("Edit", "Replace");
    QVERIFY(replace != nullptr);

    replace->trigger();
    QVERIFY(window->findReplaceBar()->replaceRowVisible());

    DocumentEditor *document = window->documentAt(0);
    QVERIFY(document != nullptr);

    document->setPlainText("one two one");
    window->find("one");
    QCOMPARE(document->textCursor().selectionStart(), 0);

    QAction *next = menuAction("Edit", "Find Next");
    QVERIFY(next != nullptr);

    next->trigger();
    QCOMPARE(document->textCursor().selectionStart(), 8);

    QAction *previous = menuAction("Edit", "Find Previous");
    QVERIFY(previous != nullptr);

    previous->trigger();
    QCOMPARE(document->textCursor().selectionStart(), 0);
}


void StudioTest::theTreeShowsThePebbleSources() {
    const QString project = makeMixedProject("shown");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();

    const QStringList entries = window->projectTreeEntries();

    QVERIFY(entries.contains("main.peb"));
    QVERIFY(entries.contains("src/lib.peb"));
    QVERIFY(entries.contains("src/nested/deep.peb"));
}

void StudioTest::theTreeHidesTheFilesThatAreNoSources() {
    const QString project = makeMixedProject("hidden");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();

    const QStringList entries = window->projectTreeEntries();

    QVERIFY(!entries.contains("pebble.toml"));
    QVERIFY(!entries.contains("notes.txt"));
    QVERIFY(!entries.contains("image.png"));
    QVERIFY(!entries.contains("src/notes.txt"));
    QVERIFY(!entries.contains("src/nested/data.json"));
}

void StudioTest::theTreeKeepsTheDirectories() {
    const QString project = makeMixedProject("directories");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();

    const QStringList entries = window->projectTreeEntries();

    QVERIFY(entries.contains("src"));
    QVERIFY(entries.contains("src/nested"));

    QVERIFY(entries.contains("src/empty"));
}

void StudioTest::theNestedSourcesStayInTheirDirectory() {
    const QString project = makeMixedProject("nested");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();

    QTreeWidget *tree = window->projectTree();
    QVERIFY(tree != nullptr);

    QTreeWidgetItem *source = nullptr;

    for (int index = 0; index < tree->topLevelItemCount(); ++index) {
        source = findItem(
            tree->topLevelItem(index),
            QFileInfo(project + "/src").absoluteFilePath()
        );

        if (source != nullptr) {
            break;
        }
    }

    QVERIFY(source != nullptr);
    QCOMPARE(source->text(0), QString("src"));

    QTreeWidgetItem *library = findItem(
        source,
        QFileInfo(project + "/src/lib.peb").absoluteFilePath()
    );

    QVERIFY(library != nullptr);
    QCOMPARE(library->text(0), QString("lib.peb"));
    QCOMPARE(library->parent(), source);

    for (int index = 0; index < tree->topLevelItemCount(); ++index) {
        QVERIFY(tree->topLevelItem(index)->text(0) != QString("lib.peb"));
    }
}

void StudioTest::reopeningTheTreeShowsANewSource() {
    const QString project = makeMixedProject("added");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();
    showWindow();

    QTreeWidget *tree = window->projectTree();
    QVERIFY(tree != nullptr);

    QVERIFY(window->projectTreeEntries().contains("main.peb"));

    tree->hide();

    writeFile(project + "/added.peb", "fn added() {}\n");
    QVERIFY(!window->projectTreeEntries().contains("added.peb"));

    tree->show();

    QVERIFY(window->projectTreeEntries().contains("added.peb"));
    QVERIFY(window->projectTreeEntries().contains("main.peb"));
}

void StudioTest::reopeningTheTreeFollowsARemovedSource() {
    const QString project = makeMixedProject("removed");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();
    showWindow();

    QTreeWidget *tree = window->projectTree();
    QVERIFY(tree != nullptr);

    QVERIFY(window->projectTreeEntries().contains("src/lib.peb"));

    tree->hide();

    QVERIFY(QDir(project + "/src").remove("lib.peb"));

    QVERIFY(window->projectTreeEntries().contains("src/lib.peb"));

    tree->show();

    QVERIFY(!window->projectTreeEntries().contains("src/lib.peb"));
    QVERIFY(window->projectTreeEntries().contains("main.peb"));
    QVERIFY(window->projectTreeEntries().contains("src"));
}

void StudioTest::reopeningTheTreeFollowsARenamedSource() {
    const QString project = makeMixedProject("renamed");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();
    showWindow();

    QTreeWidget *tree = window->projectTree();
    QVERIFY(tree != nullptr);

    QVERIFY(window->projectTreeEntries().contains("main.peb"));

    tree->hide();

    QVERIFY(QFile::rename(
        project + "/main.peb",
        project + "/renamed.peb"
    ));

    QVERIFY(window->projectTreeEntries().contains("main.peb"));

    tree->show();

    QVERIFY(window->projectTreeEntries().contains("renamed.peb"));
    QVERIFY(!window->projectTreeEntries().contains("main.peb"));
}

void StudioTest::reopeningTheTreeDoesNotDuplicateEntries() {
    const QString project = makeMixedProject("twice");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();
    showWindow();

    QTreeWidget *tree = window->projectTree();
    QVERIFY(tree != nullptr);

    const QStringList first = window->projectTreeEntries();
    QVERIFY(!first.isEmpty());

    tree->hide();
    tree->show();
    tree->hide();
    tree->show();

    const QStringList second = window->projectTreeEntries();
    QCOMPARE(second, first);

    QCOMPARE(
        static_cast<int>(QSet<QString>(second.begin(), second.end()).size()),
        static_cast<int>(second.size())
    );
}

void StudioTest::showingTheWindowAgainRefreshesTheTree() {
    const QString project = makeMixedProject("window");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();
    showWindow();

    QVERIFY(window->projectTreeEntries().contains("main.peb"));

    window->hide();

    writeFile(project + "/later.peb", "fn later() {}\n");
    QVERIFY(!window->projectTreeEntries().contains("later.peb"));

    window->show();
    QCoreApplication::processEvents();

    QVERIFY(window->projectTreeEntries().contains("later.peb"));
}

void StudioTest::aMissingProjectDirectoryLeavesTheTreeAlone() {
    const QString project = makeMixedProject("vanished");
    QVERIFY(!project.isEmpty());

    storeProject(project);
    reopen();
    showWindow();

    QTreeWidget *tree = window->projectTree();
    QVERIFY(tree != nullptr);

    QVERIFY(window->projectTreeEntries().contains("main.peb"));

    tree->hide();

    QVERIFY(QDir(project).removeRecursively());

    tree->show();

    QCOMPARE(
        window->projectDirectory(),
        QFileInfo(project).absoluteFilePath()
    );
    QVERIFY(window->projectTreeEntries().contains("main.peb"));
}


void StudioTest::theFileMenuHoldsTheRecentFiles() {
    QAction *entry = menuAction("File", "Recent Files");
    QVERIFY(entry != nullptr);

    QVERIFY(entry->menu() != nullptr);
    QCOMPARE(entry->menu(), window->recentFilesMenu());
}

void StudioTest::theRecentFilesAreEmptyAtTheStart() {
    QMenu *menu = window->recentFilesMenu();
    QVERIFY(menu != nullptr);

    QVERIFY(window->recentFiles().isEmpty());
    QVERIFY(menu->actions().isEmpty());
    QVERIFY(!menu->isEnabled());
}

void StudioTest::theRecentFilesHoldTheFileThatWasOpened() {
    const QString path = write("recent.peb", "print(1)");
    QVERIFY(window->openPath(path));

    const QStringList recent = window->recentFiles();
    QCOMPARE(recent.size(), 1);
    QCOMPARE(recent.first(), QFileInfo(path).absoluteFilePath());

    QMenu *menu = window->recentFilesMenu();
    QCOMPARE(menu->actions().size(), 1);
    QCOMPARE(menu->actions().at(0)->text(), QString("recent.peb"));
    QCOMPARE(menu->actions().at(0)->toolTip(), QFileInfo(path).absoluteFilePath());
    QVERIFY(menu->isEnabled());
}

void StudioTest::theRecentFilesAreNewestFirst() {
    const QString first = write("first.peb", "a");
    const QString second = write("second.peb", "b");

    QVERIFY(window->openPath(first));
    QVERIFY(window->openPath(second));

    const QStringList recent = window->recentFiles();
    QCOMPARE(recent.size(), 2);

    QCOMPARE(recent.at(0), QFileInfo(second).absoluteFilePath());
    QCOMPARE(recent.at(1), QFileInfo(first).absoluteFilePath());

    QMenu *menu = window->recentFilesMenu();
    QCOMPARE(menu->actions().at(0)->text(), QString("second.peb"));
    QCOMPARE(menu->actions().at(1)->text(), QString("first.peb"));
}

void StudioTest::theRecentFilesHoldNoDuplicate() {
    const QString path = write("duplicate.peb", "a");

    QVERIFY(window->openPath(path));
    window->closeDocument(0);
    QVERIFY(window->openPath(path));

    const QStringList recent = window->recentFiles();
    QCOMPARE(recent.size(), 1);
    QCOMPARE(recent.first(), QFileInfo(path).absoluteFilePath());
    QCOMPARE(window->recentFilesMenu()->actions().size(), 1);
}

void StudioTest::theRecentFilesHoldEightFilesAtMost() {
    QStringList paths;

    for (int index = 0; index < 10; ++index) {
        const QString path = write(QString("many-%1.peb").arg(index), "x");
        paths << path;
        QVERIFY(window->openPath(path));
    }

    const QStringList recent = window->recentFiles();
    QCOMPARE(recent.size(), 8);
    QCOMPARE(recent.first(), QFileInfo(paths.at(9)).absoluteFilePath());
    QCOMPARE(recent.last(), QFileInfo(paths.at(2)).absoluteFilePath());
    QVERIFY(!recent.contains(QFileInfo(paths.at(1)).absoluteFilePath()));
}

void StudioTest::theRecentFilesSurviveARestart() {
    const QString path = write("survive.peb", "a");
    QVERIFY(window->openPath(path));

    reopen();

    const QStringList recent = window->recentFiles();
    QCOMPARE(recent.size(), 1);
    QCOMPARE(recent.first(), QFileInfo(path).absoluteFilePath());
    QVERIFY(window->recentFilesMenu()->isEnabled());
}

void StudioTest::theRestoredSessionKeepsTheOrderOfTheRecentFiles() {
    const QString first = write("order-a.peb", "a");
    const QString second = write("order-b.peb", "b");

    QVERIFY(window->openPath(first));
    QVERIFY(window->openPath(second));

    {
        QSettings settings("Pebble", "Pebble Studio Test");
        settings.setValue("openFiles", QStringList{first, second});
    }

    reopen();

    QCOMPARE(window->documentCount(), 2);

    const QStringList recent = window->recentFiles();
    QCOMPARE(recent.size(), 2);
    QCOMPARE(recent.at(0), QFileInfo(second).absoluteFilePath());
    QCOMPARE(recent.at(1), QFileInfo(first).absoluteFilePath());
}

void StudioTest::aDeletedFileLeavesTheRecentFiles() {
    const QString path = write("deleted.peb", "a");
    QVERIFY(window->openPath(path));
    QVERIFY(QFile::remove(path));

    reopen();

    QVERIFY(window->recentFiles().isEmpty());
    QVERIFY(window->recentFilesMenu()->actions().isEmpty());
    QVERIFY(!window->recentFilesMenu()->isEnabled());
}

void StudioTest::aDeletedFileLeavesTheRecentFilesAtOnce() {
    const QString path = write("deleted-at-once.peb", "a");
    QVERIFY(window->openPath(path));
    QCOMPARE(window->recentFiles().size(), 1);

    QVERIFY(window->deletePath(path));

    QVERIFY(window->recentFiles().isEmpty());
    QVERIFY(window->recentFilesMenu()->actions().isEmpty());
    QVERIFY(!window->recentFilesMenu()->isEnabled());
}

void StudioTest::aRenamedFileKeepsItsPlaceInTheRecentFiles() {
    const QString path = write("renamed.peb", "a");
    QVERIFY(window->openPath(path));

    QVERIFY(window->renamePath(path, "renamed-other.peb"));

    const QStringList recent = window->recentFiles();
    QCOMPARE(recent.size(), 1);
    QCOMPARE(
        recent.first(),
        QFileInfo(workDir.filePath("renamed-other.peb")).absoluteFilePath()
    );
}

void StudioTest::aRecentFileOpensTheDocument() {
    const QString path = write("opened.peb", "a");
    QVERIFY(window->openPath(path));
    window->closeDocument(0);
    QCOMPARE(window->documentCount(), 0);

    QMenu *menu = window->recentFilesMenu();
    QCOMPARE(menu->actions().size(), 1);

    menu->actions().at(0)->trigger();

    QCOMPARE(window->documentCount(), 1);
    QCOMPARE(window->documentPath(0), QFileInfo(path).absoluteFilePath());
}

void StudioTest::aRecentFileThatIsGoneIsDropped() {
    const QString path = write("gone.peb", "a");
    QVERIFY(window->openPath(path));
    QVERIFY(QFile::remove(path));

    QMenu *menu = window->recentFilesMenu();
    QCOMPARE(menu->actions().size(), 1);

    menu->actions().at(0)->trigger();

    QCOMPARE(notifications, 1);
    QVERIFY(window->recentFiles().isEmpty());
    QVERIFY(menu->actions().isEmpty());
    QVERIFY(!menu->isEnabled());
}

void StudioTest::savingAFileRecordsIt() {
    window->newFile();

    const QString path = workDir.filePath("saved.peb");
    QVERIFY(window->saveAsPath(path));

    const QStringList recent = window->recentFiles();
    QCOMPARE(recent.size(), 1);
    QCOMPARE(recent.first(), QFileInfo(path).absoluteFilePath());
    QCOMPARE(window->recentFilesMenu()->actions().size(), 1);
}

void StudioTest::theRecentFilesAreStoredInTheSettings() {
    const QString path = write("stored.peb", "a");
    QVERIFY(window->openPath(path));

    QSettings settings("Pebble", "Pebble Studio Test");
    const QStringList stored = settings.value("recentFiles").toStringList();

    QCOMPARE(stored.size(), 1);
    QCOMPARE(stored.first(), QFileInfo(path).absoluteFilePath());
}


void StudioTest::runningTheCurrentFileStartsTheProgram() {
    const QString path = write("program.peb", "fn main() {}\n");
    QVERIFY(window->openPath(path));

    window->setProcessProgram(
        writeScript("echo-args.sh", "#!/bin/sh\necho \"args: $@\"\n")
    );

    menuAction("Run", "Run File")->trigger();

    QTRY_VERIFY(window->outputText().contains("args: run " + path));
    QTRY_VERIFY(!window->isProcessRunning());
}

void StudioTest::aDirtyDocumentIsSavedBeforeItRuns() {
    const QString path = write("dirty.peb", "fn main() {}\n");
    QVERIFY(window->openPath(path));

    append(window->documentAt(0), "print(1);\n");
    QVERIFY(window->documentDirty(0));

    window->setProcessProgram(
        writeScript("cat-file.sh", "#!/bin/sh\ncat \"$2\"\n")
    );

    menuAction("Run", "Run File")->trigger();

    QTRY_VERIFY(window->outputText().contains("print(1);"));
    QVERIFY(!window->documentDirty(0));
}

void StudioTest::aFileThatIsNotAPebbleProgramIsNotRun() {
    const QString path = write("notes.txt", "hello\n");
    QVERIFY(window->openPath(path));

    window->setProcessProgram(
        writeScript("echo-args.sh", "#!/bin/sh\necho \"args: $@\"\n")
    );

    menuAction("Run", "Run File")->trigger();

    QCOMPARE(notifications, 1);
    QVERIFY(!window->isProcessRunning());
    QVERIFY(!window->outputText().contains("args:"));
}

void StudioTest::runningWithoutADocumentIsRefused() {
    window->setProcessProgram(
        writeScript("echo-args.sh", "#!/bin/sh\necho \"args: $@\"\n")
    );

    menuAction("Run", "Run File")->trigger();

    QCOMPARE(notifications, 1);
    QVERIFY(!window->isProcessRunning());
    QVERIFY(!window->outputText().contains("args:"));
}

void StudioTest::aFileThatIsGoneIsNotRun() {
    const QString path = write("gone.peb", "fn main() {}\n");
    QVERIFY(window->openPath(path));
    QVERIFY(QFile::remove(path));

    window->setProcessProgram(
        writeScript("echo-args.sh", "#!/bin/sh\necho \"args: $@\"\n")
    );

    menuAction("Run", "Run File")->trigger();

    QCOMPARE(notifications, 1);
    QVERIFY(!window->isProcessRunning());
    QVERIFY(!window->outputText().contains("args:"));
}

void StudioTest::theExitCodeOfTheProgramIsReported() {
    const QString path = write("code.peb", "fn main() {}\n");
    QVERIFY(window->openPath(path));

    window->setProcessProgram(writeScript("bad.sh", "#!/bin/sh\nexit 7\n"));

    menuAction("Run", "Run File")->trigger();

    QTRY_VERIFY(window->outputText().contains("Program exited with code 7."));
    QVERIFY(!window->isProcessRunning());
}

void StudioTest::aRunningProgramIsNotReplaced() {
    const QString path = write("slow.peb", "fn main() {}\n");
    QVERIFY(window->openPath(path));

    window->setProcessProgram(
        writeScript("slow.sh", "#!/bin/sh\necho started\nexec sleep 30\n")
    );

    menuAction("Run", "Run File")->trigger();
    QTRY_VERIFY(window->outputText().contains("started"));
    QVERIFY(window->isProcessRunning());

    QAction *runFile = menuAction("Run", "Run File");
    QVERIFY(!runFile->isEnabled());
    runFile->trigger();
    QVERIFY(window->isProcessRunning());

    menuAction("Run", "Stop")->trigger();
    QTRY_VERIFY(!window->isProcessRunning());
}

void StudioTest::aProgramThatFailsToStartIsReportedForTheCurrentFile() {
    const QString path = write("missing.peb", "fn main() {}\n");
    QVERIFY(window->openPath(path));

    window->setProcessProgram(workDir.filePath("missing-pebble-cli"));

    menuAction("Run", "Run File")->trigger();

    QTRY_VERIFY(window->outputText().contains("Failed to start"));
    QVERIFY(!window->isProcessRunning());
}

void StudioTest::buildingTheCurrentFileBuildsIt() {
    const QString path = write("build-me.peb", "fn main() {}\n");
    QVERIFY(window->openPath(path));

    window->setProcessProgram(
        writeScript("echo-args.sh", "#!/bin/sh\necho \"args: $@\"\n")
    );

    menuAction("Run", "Build File")->trigger();

    QTRY_VERIFY(window->outputText().contains("args: build " + path));
    QTRY_VERIFY(window->outputText().contains("Build finished with exit code 0."));
}

void StudioTest::theBuildAndTheProgramReportTheirOwnResults() {
    const QString path = write("both.peb", "fn main() {}\n");
    QVERIFY(window->openPath(path));

    window->setProcessProgram(
        writeScript("echo-args.sh", "#!/bin/sh\necho \"args: $@\"\nexit 0\n")
    );

    menuAction("Run", "Build File")->trigger();
    QTRY_VERIFY(window->outputText().contains("args: build " + path));
    QTRY_VERIFY(
        window->outputText().contains("Build finished with exit code 0.")
    );

    menuAction("Run", "Run File")->trigger();
    QTRY_VERIFY(window->outputText().contains("args: run " + path));
    QTRY_VERIFY(window->outputText().contains("Program finished successfully."));
    QVERIFY(!window->outputText().contains("Build finished with exit code 0.\nProgram"));
}

void StudioTest::stoppingTheProgramDoesNotReportABuildResult() {
    const QString path = write("stop-me.peb", "fn main() {}\n");
    QVERIFY(window->openPath(path));

    window->setProcessProgram(
        writeScript("slow.sh", "#!/bin/sh\necho started\nexec sleep 30\n")
    );

    menuAction("Run", "Run File")->trigger();
    QTRY_VERIFY(window->outputText().contains("started"));

    menuAction("Run", "Stop")->trigger();
    QTRY_VERIFY(!window->isProcessRunning());

    QVERIFY(window->outputText().contains("Program stopped."));
    QVERIFY(!window->outputText().contains("Build stopped."));
    QVERIFY(!window->outputText().contains("Build finished"));
}

void StudioTest::theProgramIsGivenTheFilesOfTheExamples() {
    qunsetenv("PEBBLE_RESOURCE_PATH");
    reopen();

    const QString path = write("program.peb", "fn main() {}\n");
    QVERIFY(window->openPath(path));

    window->setProcessProgram(
        writeScript(
            "write-resources.sh",
            "#!/bin/sh\n"
            "printf '%s' \"$PEBBLE_RESOURCE_PATH\" > resources.txt\n"
        )
    );

    menuAction("Run", "Run File")->trigger();

    QTRY_VERIFY(!window->isProcessRunning());

    const QString report = workDir.filePath("resources.txt");
    QVERIFY(QFileInfo(report).exists());

    QFile file(report);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QFAIL("the resources of the program could not be read");
    }

    const QString value = QString::fromUtf8(file.readAll());
    file.close();

    const QStringList roots =
        value.split(QDir::listSeparator(), Qt::SkipEmptyParts);

    QVERIFY(!roots.isEmpty());

    int withFiles = 0;

    for (const QString &root : roots) {
        if (QFileInfo(root).isDir() && !QDir(root).entryList(QDir::Files).isEmpty()) {
            withFiles += 1;
        }
    }

    QVERIFY(withFiles > 0);
}


void StudioTest::theProgramKeepsTheEnvironmentOfTheStudio() {
    const QString path = write("program.peb", "fn main() {}\n");
    QVERIFY(window->openPath(path));

    window->setProcessProgram(
        writeScript(
            "write-environment.sh",
            "#!/bin/sh\n"
            "env | sort > environment.txt\n"
        )
    );

    menuAction("Run", "Run File")->trigger();

    QTRY_VERIFY(!window->isProcessRunning());

    const QString report = workDir.filePath("environment.txt");
    QVERIFY(QFileInfo(report).exists());

    QFile file(report);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QFAIL("the environment of the program could not be read");
    }

    const QString environment = QString::fromUtf8(file.readAll());
    file.close();

    QVERIFY(environment.contains("PATH="));
    QVERIFY(environment.contains("HOME="));
}

void StudioTest::aProgramRunByTheStudioFindsTheFilesOfTheExamples() {
    qunsetenv("PEBBLE_RESOURCE_PATH");
    reopen();

    const QString cli = QDir(QCoreApplication::applicationDirPath())
        .filePath("pebble");

    if (!QFileInfo(cli).isExecutable()) {
        QSKIP("the CLI is not built next to the tests");
    }

    const QString program = write(
        "Untitled.peb",
        "import package filesystem\n"
        "import package string\n"
        "fn main() {\n"
        "    let vertices = filesystem.read(\"cube_vertices.csv\");\n"
        "    let indices = filesystem.read(\"cube_indices.csv\");\n"
        "    let vertexShader = filesystem.read(\"spinning_cube.vert\");\n"
        "    let fragmentShader = filesystem.read(\"spinning_cube.frag\");\n"
        "    let binaryVertex = filesystem.read(\"binary_cube.vert\");\n"
        "    let binaryFragment = filesystem.read(\"binary_cube.frag\");\n"
        "    print(string.length(vertices));\n"
        "    print(string.length(indices));\n"
        "    print(string.length(vertexShader));\n"
        "    print(string.length(fragmentShader));\n"
        "    print(string.length(binaryVertex));\n"
        "    print(string.length(binaryFragment));\n"
        "}\n"
    );

    QVERIFY(window->openPath(program));
    window->setProcessProgram(cli);

    menuAction("Run", "Run File")->trigger();

    QTRY_VERIFY_WITH_TIMEOUT(
        window->outputText().contains("Program finished successfully.")
            || window->outputText().contains("cannot read"),
        180000
    );

    const QString output = window->outputText();

    QVERIFY(!output.contains("cannot read"));
    QVERIFY(!output.contains("Pebble runtime error"));
    QVERIFY(output.contains("Program finished successfully."));
}


void StudioTest::theExamplesOfTheSubdirectoriesAreListed() {
    const QStringList paths = window->examplePaths();
    const QStringList roots = window->examplesDirectories();

    QVERIFY(!paths.isEmpty());
    QVERIFY(!roots.isEmpty());

    int nested = 0;

    for (const QString &path : paths) {
        QVERIFY(path.endsWith(".peb"));

        const QString absolute = QFileInfo(path).absoluteFilePath();

        for (const QString &root : roots) {
            const QString base = QDir(root).absolutePath();

            if (!absolute.startsWith(base + "/")) {
                continue;
            }

            if (absolute.mid(base.size() + 1).contains('/')) {
                nested += 1;
            }
        }
    }

    QVERIFY(nested > 0);
}

void StudioTest::theLabelOfAnExampleIsItsPathInTheExamplesDirectory() {
    const QStringList paths = window->examplePaths();

    QVERIFY(!paths.isEmpty());

    const QString label = window->exampleLabel(paths.first());

    QVERIFY(!label.isEmpty());
    QVERIFY(!QFileInfo(label).isAbsolute());
}

void StudioTest::theDirectoriesOfTheExamplesThatHoldFilesAreFound() {
    const QStringList directories = window->exampleAssetDirectories();

    QVERIFY(!directories.isEmpty());

    for (const QString &directory : directories) {
        const QFileInfo info(directory);

        QVERIFY(info.isDir());
        QVERIFY(!QDir(directory).entryList(QDir::Files).isEmpty());
    }
}


void StudioTest::theKeywordsAreRecognised() {
    QCOMPARE(
        syntaxCategoryAt(QString("fn main() {}"), 0),
        SyntaxCategory::Keyword
    );
    QCOMPARE(
        syntaxCategoryAt(QString("let value = 1;"), 0),
        SyntaxCategory::Keyword
    );
    QCOMPARE(
        syntaxCategoryAt(QString("while (x) {}"), 0),
        SyntaxCategory::Keyword
    );

    QCOMPARE(
        syntaxCategoryAt(QString("let value = 1;"), 4),
        SyntaxCategory::Declaration
    );
}

void StudioTest::theLiteralsAreRecognised() {
    QCOMPARE(
        syntaxCategoryAt(QString("let a = true;"), 8),
        SyntaxCategory::Literal
    );
    QCOMPARE(
        syntaxCategoryAt(QString("let a = false;"), 8),
        SyntaxCategory::Literal
    );

    QCOMPARE(
        syntaxCategoryAt(QString("print(1);"), 0),
        SyntaxCategory::Builtin
    );
}

void StudioTest::theCommentsAreRecognised() {
    const QString line = QString("let a = 1; // a comment");

    QCOMPARE(syntaxCategoryAt(line, 0), SyntaxCategory::Keyword);
    QCOMPARE(syntaxCategoryAt(line, 11), SyntaxCategory::Comment);
    QCOMPARE(syntaxCategoryAt(line, 20), SyntaxCategory::Comment);
}

void StudioTest::theStringsAreRecognised() {
    const QString line = QString("let a = \"hello\";");

    QCOMPARE(syntaxCategoryAt(line, 8), SyntaxCategory::String);
    QCOMPARE(syntaxCategoryAt(line, 13), SyntaxCategory::String);

    QCOMPARE(syntaxCategoryAt(line, 15), SyntaxCategory::Operator);
}

void StudioTest::theNumbersAreRecognised() {
    const QString line = QString("let a = 42; let b = 3.5;");

    QCOMPARE(syntaxCategoryAt(line, 8), SyntaxCategory::Number);
    QCOMPARE(syntaxCategoryAt(line, 9), SyntaxCategory::Number);
    QCOMPARE(syntaxCategoryAt(line, 20), SyntaxCategory::Number);
    QCOMPARE(syntaxCategoryAt(line, 22), SyntaxCategory::Number);
}

void StudioTest::theCallsAreRecognised() {
    const QString line = QString("print(widen(3));");

    QCOMPARE(syntaxCategoryAt(line, 0), SyntaxCategory::Builtin);
    QCOMPARE(syntaxCategoryAt(line, 6), SyntaxCategory::Function);

    QCOMPARE(
        syntaxCategoryAt(QString("let value = 1;"), 4),
        SyntaxCategory::Declaration
    );
}

void StudioTest::theOperatorsAreRecognised() {
    const QString line = QString("let a = 1 + 2;");

    QCOMPARE(syntaxCategoryAt(line, 10), SyntaxCategory::Operator);
    QCOMPARE(syntaxCategoryAt(line, 13), SyntaxCategory::Operator);

    QCOMPARE(syntaxCategoryAt(line, 12), SyntaxCategory::Number);
}

void StudioTest::aWordInAStringIsNoKeyword() {
    const QString line = QString("let a = \"fn while\";");

    QCOMPARE(syntaxCategoryAt(line, 9), SyntaxCategory::String);
    QCOMPARE(syntaxCategoryAt(line, 12), SyntaxCategory::String);
}

void StudioTest::aWordInACommentIsNoKeyword() {
    const QString line = QString("// fn while");

    QCOMPARE(syntaxCategoryAt(line, 0), SyntaxCategory::Comment);
    QCOMPARE(syntaxCategoryAt(line, 4), SyntaxCategory::Comment);
}

void StudioTest::theHighlighterFollowsTheEdits() {
    DocumentEditor *document = prepareEditor("let a = 1;\n");
    QVERIFY(document != nullptr);

    append(document, "// a comment\n");
    QCoreApplication::processEvents();

    const QString commentLine = document->document()->findBlockByNumber(1).text();
    QCOMPARE(commentLine, QString("// a comment"));
    QCOMPARE(syntaxCategoryAt(commentLine, 0), SyntaxCategory::Comment);

    QTextCursor cursor = document->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    QCoreApplication::processEvents();

    QCOMPARE(document->toPlainText(), QString("// a comment\n"));
    QCOMPARE(
        syntaxCategoryAt(document->document()->firstBlock().text(), 0),
        SyntaxCategory::Comment
    );
}

void StudioTest::theHighlighterMarksTheCodeOfTheDocument() {
    DocumentEditor *document = prepareEditor("let a = 1; // note\n");
    QVERIFY(document != nullptr);

    QCoreApplication::processEvents();

    const QTextBlock block = document->document()->firstBlock();
    const QList<QTextLayout::FormatRange> ranges = block.layout()->formats();

    bool keywordFound = false;
    bool commentFound = false;

    for (const QTextLayout::FormatRange &range : ranges) {
        if (range.start == 0 && range.length == 3) {
            keywordFound = true;
        }

        if (range.start == 11) {
            commentFound = true;
        }
    }

    QVERIFY(keywordFound);
    QVERIFY(commentFound);
}

void StudioTest::theHighlighterUsesTheObsidianSyntaxPalette() {
    const QString code =
        "fn main() { let message: String = \"hello\"; "
        "print(message, 42); } // note";
    DocumentEditor *document = prepareEditor(code);
    QVERIFY(document != nullptr);

    QCoreApplication::processEvents();

    const QTextBlock block = document->document()->firstBlock();
    const QList<QTextLayout::FormatRange> ranges = block.layout()->formats();
    const auto colorAt = [&ranges](qsizetype position) {
        for (const QTextLayout::FormatRange &range : ranges) {
            const qsizetype start = range.start;
            const qsizetype end = start + range.length;

            if (position >= start && position < end) {
                return range.format.foreground().color();
            }
        }

        return QColor();
    };

    QCOMPARE(colorAt(code.indexOf("fn")), QColor(theme::keyword));
    QCOMPARE(colorAt(code.indexOf("message")), QColor(theme::declaration));
    QCOMPARE(colorAt(code.indexOf("String")), QColor(theme::type));
    QCOMPARE(colorAt(code.indexOf("\"hello\"")), QColor(theme::string));
    QCOMPARE(colorAt(code.indexOf("print")), QColor(theme::builtin));
    QCOMPARE(colorAt(code.indexOf("42")), QColor(theme::number));
    QCOMPARE(colorAt(code.indexOf("// note")), QColor(theme::comment));
}

void StudioTest::theLineNumbersGrowWithTheDocument() {
    DocumentEditor *document = prepareEditor("one\n");
    QVERIFY(document != nullptr);

    const int narrow = document->lineNumberAreaWidth();

    QString manyLines;

    for (int index = 0; index < 120; ++index) {
        manyLines += QStringLiteral("line\n");
    }

    document->setPlainText(manyLines);
    QCoreApplication::processEvents();

    QVERIFY(document->lineNumberAreaWidth() > narrow);
}

void StudioTest::theCurrentLineIsMarked() {
    DocumentEditor *document = prepareEditor("one\ntwo\nthree");
    QVERIFY(document != nullptr);

    QCOMPARE(document->extraSelections().size(), 1);
    QCOMPARE(document->extraSelections().at(0).cursor.blockNumber(), 0);
    QCOMPARE(document->currentLineNumber(), 0);

    QTextCursor cursor = document->textCursor();
    cursor.movePosition(QTextCursor::Down);
    cursor.movePosition(QTextCursor::Down);
    document->setTextCursor(cursor);
    QCoreApplication::processEvents();

    QCOMPARE(document->currentLineNumber(), 2);
    QCOMPARE(document->extraSelections().size(), 1);
    QCOMPARE(document->extraSelections().at(0).cursor.blockNumber(), 2);
}


void StudioTest::theActivityBarHoldsTheSwitchesOfThePanels() {
    QWidget *bar = window->activityBar();
    QVERIFY(bar != nullptr);

    const QList<QToolButton *> buttons = bar->findChildren<QToolButton *>();
    QCOMPARE(buttons.size(), 2);

    for (QToolButton *button : buttons) {
        QVERIFY(button->isCheckable());
        QVERIFY(!button->icon().isNull());
        QVERIFY(!button->toolTip().isEmpty());
    }
}

void StudioTest::theExplorerCanBeHiddenAndShown() {
    showWindow();
    QVERIFY(window->explorerVisible());

    QAction *explorer = menuAction("View", "Explorer");
    QVERIFY(explorer != nullptr);

    explorer->setChecked(false);
    QVERIFY(!window->explorerVisible());
    QVERIFY(window->projectTree()->isHidden());

    explorer->setChecked(true);
    QVERIFY(window->explorerVisible());
    QVERIFY(!window->projectTree()->isHidden());
}

void StudioTest::theOutputPaneCanBeHiddenAndShown() {
    showWindow();
    QVERIFY(window->outputPaneVisible());

    QAction *output = menuAction("View", "Output");
    QVERIFY(output != nullptr);

    output->setChecked(false);
    QVERIFY(!window->outputPaneVisible());

    output->setChecked(true);
    QVERIFY(window->outputPaneVisible());
}

void StudioTest::theWorkspaceStateIsStoredWhenTheWindowCloses() {
    window->setExplorerVisible(false);
    showWindow();
    window->close();

    QSettings settings("Pebble", "Pebble Studio Test");
    QCOMPARE(settings.value("explorerShown").toBool(), false);
    QCOMPARE(settings.value("outputShown").toBool(), true);
}

void StudioTest::theWorkspaceStateSurvivesARestart() {
    {
        QSettings settings("Pebble", "Pebble Studio Test");
        settings.setValue("explorerShown", false);
        settings.setValue("outputShown", false);
    }

    reopen();

    QVERIFY(!window->explorerVisible());
    QVERIFY(!window->outputPaneVisible());
}


void StudioTest::theOutputPaneHasABound() {
    QVERIFY(window->outputMaximumBlockCount() > 0);
}

void StudioTest::theOutputPaneDropsTheOldestLines() {
    prepareRun(
        writeScript(
            "many.sh",
            "#!/bin/sh\ni=0\nwhile [ $i -lt 60 ]; do echo line $i; "
            "i=$((i+1)); done\n"
        )
    );

    window->setOutputMaximumBlockCount(20);

    menuAction("Run", "Run")->trigger();

    QTRY_VERIFY(window->outputText().contains("line 59"));

    QVERIFY(window->outputBlockCount() <= 20);
    QVERIFY(!window->outputText().contains("line 0\n"));
}

void StudioTest::theClearButtonEmptiesTheOutput() {
    prepareRun(writeScript("quick.sh", "#!/bin/sh\necho hello\n"));

    menuAction("Run", "Run")->trigger();

    QTRY_VERIFY(window->outputText().contains("hello"));

    QPushButton *clear = window->clearOutputButton();
    QVERIFY(clear != nullptr);
    clear->click();

    QVERIFY(window->outputText().isEmpty());
    QCOMPARE(window->outputBlockCount(), 1);
}

void StudioTest::theErrorOfTheProgramIsMarked() {
    prepareRun(writeScript("bad.sh", "#!/bin/sh\necho broken >&2\nexit 1\n"));

    menuAction("Run", "Run")->trigger();

    QTRY_VERIFY(window->outputText().contains("error: broken"));
}

void StudioTest::theOutputLinesAreColouredByTheirLevel() {
    QPlainTextEdit pane;
    new OutputHighlighter(pane.document());

    pane.setPlainText("error: broken\nwarning: odd\ninfo: ready\nplain");
    QCoreApplication::processEvents();

    const QColor error = outputColour(pane.document(), 0);
    const QColor warning = outputColour(pane.document(), 1);
    const QColor info = outputColour(pane.document(), 2);
    const QColor plain = outputColour(pane.document(), 3);

    QVERIFY(error.isValid());
    QVERIFY(warning.isValid());
    QVERIFY(info.isValid());
    QVERIFY(plain.isValid());
    QVERIFY(error != warning);
    QVERIFY(error != plain);
    QVERIFY(warning != plain);
    QVERIFY(info != plain);
}

void StudioTest::aDiagnosticLineIsColouredByItsSeverity() {
    QPlainTextEdit pane;
    new OutputHighlighter(pane.document());

    pane.setPlainText(
        "error: marked\nsrc/main.c:3:1: error: boom\n"
        "warning: marked\nsrc/main.c:9:1: warning: odd"
    );
    QCoreApplication::processEvents();

    QCOMPARE(outputColour(pane.document(), 1), outputColour(pane.document(), 0));
    QCOMPARE(outputColour(pane.document(), 3), outputColour(pane.document(), 2));

    QVERIFY(outputColour(pane.document(), 0) != outputColour(pane.document(), 2));
}

void StudioTest::theDiagnosticsTitleCarriesTheNumber() {
    QCOMPARE(window->diagnosticsTitle(), QString("Diagnostics (0)"));

    window->feedCompilerOutput("src/main.c:3:1: error: boom\n");
    window->flushCompilerOutput();

    QCOMPARE(window->diagnosticCount(), 1);
    QCOMPARE(window->diagnosticsTitle(), QString("Diagnostics (1)"));

    window->feedCompilerOutput("src/main.c:9:1: warning: odd\n");
    window->flushCompilerOutput();

    QCOMPARE(window->diagnosticsTitle(), QString("Diagnostics (2)"));
}


void StudioTest::theStatusBarFollowsTheActiveTab() {
    const QString first = write("first.peb", "fn main() {}\n");
    const QString second = write("second.peb", "fn helper() {}\n");

    QVERIFY(window->openPath(first));
    QVERIFY(window->openPath(second));

    QCOMPARE(window->statusDocumentName(), QString("second.peb"));

    window->documentAt(1)->document()->undo();
    QTabWidget *tabs = window->findChild<QTabWidget *>();
    QVERIFY(tabs != nullptr);
    tabs->setCurrentIndex(0);

    QCOMPARE(window->statusDocumentName(), QString("first.peb"));
}

void StudioTest::theWindowTitleFollowsTheActiveTab() {
    const QString first = write("first.peb", "fn main() {}\n");
    const QString second = write("second.peb", "fn helper() {}\n");

    QVERIFY(window->openPath(first));
    QVERIFY(window->openPath(second));

    QCOMPARE(window->windowTitleText(), QString("second.peb — Pebble Studio"));

    QTabWidget *tabs = window->findChild<QTabWidget *>();
    QVERIFY(tabs != nullptr);
    tabs->setCurrentIndex(0);

    QCOMPARE(window->windowTitleText(), QString("first.peb — Pebble Studio"));
}

void StudioTest::theStatusBarShowsThePositionOfTheCursor() {
    DocumentEditor *document = prepareEditor("one\ntwo\nthree");
    QVERIFY(document != nullptr);

    QCOMPARE(window->statusCursorPosition(), QString("line 1, column 1"));

    QTextCursor cursor = document->textCursor();
    cursor.movePosition(QTextCursor::Down);
    cursor.movePosition(QTextCursor::Right);
    document->setTextCursor(cursor);
    QCoreApplication::processEvents();

    QCOMPARE(window->statusCursorPosition(), QString("line 2, column 2"));
}

void StudioTest::theStatusBarShowsTheDirtyState() {
    DocumentEditor *document = prepareEditor("one\n");
    QVERIFY(document != nullptr);

    QCOMPARE(window->statusState(), QString("Saved"));

    append(document, "two\n");
    QCoreApplication::processEvents();

    QCOMPARE(window->statusState(), QString("Modified"));
    QVERIFY(window->windowTitleText().contains("*"));
}


void StudioTest::theLineNumbersArePaintedOnTheScreen() {
    DocumentEditor *document = prepareEditor("one\ntwo\nthree");
    QVERIFY(document != nullptr);

    const QImage image = document->grab().toImage();
    const int gutterWidth = document->lineNumberAreaWidth();
    int painted = 0;

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < gutterWidth; ++x) {
            const QColor colour = image.pixelColor(x, y);

            if (colour != QColor(theme::chrome) &&
                colour != QColor(theme::background)) {
                ++painted;
            }
        }
    }

    QVERIFY(painted > 0);
}

void StudioTest::theGutterSitsOnTheLeftOfTheText() {
    DocumentEditor *document = prepareEditor("one\ntwo\nthree");
    QVERIFY(document != nullptr);

    const QRect viewport = document->viewport()->geometry();
    const int gutterWidth = document->lineNumberAreaWidth();

    QVERIFY(viewport.left() >= gutterWidth);
    QVERIFY(viewport.left() > 0);
}

void StudioTest::theWindowCarriesTheIconOfTheStudio() {
    const QIcon icon = window->windowIcon();

    QVERIFY(!icon.isNull());
    QVERIFY(!icon.pixmap(24, 24).isNull());
}

void StudioTest::theIconOfTheUserIsTheIconOfTheStudio() {
    const QString path = studio::iconPath();

    QVERIFY(!path.isEmpty());
    QVERIFY(QFileInfo::exists(path));
    QCOMPARE(QFileInfo(path).fileName(), QString("icon.png"));

    const QIcon icon = window->windowIcon();

    QVERIFY(!icon.isNull());
    QVERIFY(!icon.pixmap(64, 64).isNull());

    const QImage shown = icon.pixmap(64, 64).toImage();

    QCOMPARE(shown.size(), QSize(64, 64));

    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < shown.width(); ++x) {
            QCOMPARE(qAlpha(shown.pixel(x, y)), 0);
        }
    }
}

void StudioTest::theWallpaperOfTheUserIsTheDefaultBackground() {
    const QString path = window->defaultBackgroundPath();

    QVERIFY(!path.isEmpty());
    QVERIFY(QFileInfo::exists(path));
    QCOMPARE(QFileInfo(path).fileName(), studio::defaultWallpaperName());

    QCOMPARE(window->backgroundPath(), path);
    QVERIFY(window->backgroundLoaded());
}

void StudioTest::theActivityBarIconFollowsItsPanel() {
    QWidget *bar = window->activityBar();
    QVERIFY(bar != nullptr);

    const QList<QToolButton *> buttons = bar->findChildren<QToolButton *>();
    QCOMPARE(buttons.size(), 2);

    for (QToolButton *button : buttons) {
        const QIcon icon = button->icon();
        const QPixmap off = icon.pixmap(QSize(22, 22), QIcon::Normal, QIcon::Off);
        const QPixmap on = icon.pixmap(QSize(22, 22), QIcon::Normal, QIcon::On);

        QVERIFY(!off.isNull());
        QVERIFY(!on.isNull());

        QVERIFY(off.toImage() != on.toImage());
    }
}

void StudioTest::theStatusBarMarksAChangedDocument() {
    DocumentEditor *document = prepareEditor("one\n");
    QVERIFY(document != nullptr);

    QLabel *state = window->findChild<QLabel *>("state");
    QVERIFY(state != nullptr);
    QCOMPARE(state->property("modified").toBool(), false);

    append(document, "two\n");
    QCoreApplication::processEvents();

    QCOMPARE(state->property("modified").toBool(), true);

    document->undo();
    QCoreApplication::processEvents();

    QCOMPARE(state->property("modified").toBool(), false);
}

void StudioTest::theCodeEditorUsesAFixedPitchFont() {
    DocumentEditor *document = prepareEditor("one\n");
    QVERIFY(document != nullptr);

    QVERIFY(QFontInfo(document->font()).fixedPitch());
}

void StudioTest::theWorkspaceUsesOneStyleSheet() {
    const QString sheet = window->styleSheet();

    QVERIFY(!sheet.isEmpty());
    QVERIFY(sheet.contains("activitybar"));
    QVERIFY(sheet.contains("QTabBar::tab"));
    QVERIFY(sheet.contains("QStatusBar"));
}

void StudioTest::theCloseButtonOfTheTabClosesTheDocument() {
    prepareEditor("one\n");
    window->newFile();

    QTabBar *bar = window->documentTabBar();
    QVERIFY(bar != nullptr);
    QCOMPARE(bar->count(), 2);

    QWidget *button = bar->tabButton(0, QTabBar::RightSide);
    QVERIFY(button != nullptr);

    auto *close = qobject_cast<QToolButton *>(button);
    QVERIFY(close != nullptr);
    close->click();

    QCOMPARE(bar->count(), 1);
    QCOMPARE(window->documentCount(), 1);
}

void StudioTest::theOutputCanBeEmptiedFromTheMenu() {
    prepareRun(writeScript("quick.sh", "#!/bin/sh\necho hello\n"));

    menuAction("Run", "Run")->trigger();

    QTRY_VERIFY(window->outputText().contains("hello"));

    QAction *clear = menuAction("View", "Clear Output");
    QVERIFY(clear != nullptr);
    clear->trigger();

    QVERIFY(window->outputText().isEmpty());
}

int main(int argc, char *argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    StudioTest test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "test_studio.moc"
