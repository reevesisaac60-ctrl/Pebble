# Python/C++ Studio Feature Parity Report

## Scope and method

* Reference: `pebble_studio_qt.py` (PySide6, read-only, never modified).
* Native target: `src/studio/` (C++20 / Qt 6 Widgets).
* Method: every callable, every menu, every widget and every `QSettings` key of
  the reference was read and matched against the native implementation.
  Where the two disagree the native behaviour was read from the source, not
  assumed.

Two properties of the reference matter for every row below.

1. **The reference is not runnable in this repository.** It imports
   `pebble_studio_common` (line 1107) which does not exist here, and it expects
   `pebble_wallpaper.svg` and `assets/icons/pebble.png`, which do not exist
   either. It is therefore a *design* reference, and parity is judged against
   the behaviour it describes, not against a running program.
2. **The reference targets an older language dialect.** Its highlighter knows
   `rock`, `start`, `say`, `ask`, `otherwise`, `repeat`, `fun`, `give`
   (`pebble_studio_qt.py:121-124`). The language that is actually implemented
   and documented uses `fn`, `let`, `var`, `import package`, `export`, `if`,
   `else`, `while`, `return`, `print` (`docs/language.md`). Editor features
   that depend on language syntax must be adapted to the real dialect, never
   copied verbatim. The comment syntax differs as well: the implemented
   language comments a line with `//` (`src/lexer.cpp:286`), so the editor
   toggles `//` and never `#`.

## States

| State | Meaning |
| --- | --- |
| **Verified** | Present in C++ and covered by an automated test. |
| **Unverified** | Present in C++, but no automated test covers it. |
| **Partial** | Present in C++, but the behaviour differs from the reference. |
| **Missing** | Not present in C++. |
| **Deferred** | Deliberately not implemented (settings UI). |
| **N/A** | The reference feature depends on assets, modules or a dialect that do not exist in this repository. |

## Summary

| # | Feature | Python | C++ | State |
| --- | --- | --- | --- | --- |
| 1 | Main window, size, theme | yes | partial | **Verified** |
| 2 | Explorer / project tree | yes | partial | Partial |
| 3 | Tabs and dirty markers | yes | partial | Partial |
| 4 | New file | yes | yes | **Verified** |
| 5 | Open file (dialog) | yes | yes | **Verified** |
| 6 | Save / Save As | yes | yes | **Verified** |
| 7 | Unsaved-change handling | yes | yes | **Verified** |
| 8 | Session restore | no | yes | Partial |
| 9 | Find and replace | yes | yes | **Verified** |
| 10 | Editor keyboard editing | yes | yes | **Verified** |
| 11 | Line numbers | yes | yes | **Verified** |
| 12 | Syntax highlighting | yes | partial | **Verified** |
| 13 | Status bar and window title | yes | yes | **Verified** |
| 14 | Build | no | yes | **Verified** |
| 15 | Run | yes | yes | **Verified** |
| 16 | Program input (stdin) | yes | no | Deferred |
| 17 | Stop / process management | yes | yes | **Verified** |
| 18 | Diagnostics panel | no | yes | **Verified** |
| 19 | Tree context menu | no | yes | **Verified** |
| 20 | Menus and shortcuts | yes | partial | Partial |
| 21 | Recent files | yes | yes | **Verified** |
| 22 | Welcome page | yes | no | Deferred |
| 23 | Open example | yes | no | N/A |
| 24 | Help (reference, about) | yes | no | Deferred |
| 25 | Appearance: syntax colours | yes | no | Deferred |
| 26 | Appearance: wallpaper | yes | no | N/A |
| 27 | Activity bar and dock layout | yes | yes | **Verified** |
| 28 | Settings UI | no | no | Deferred |
| 29 | Settings persistence keys | yes | partial | Partial |
| 30 | Output pane and log levels | yes | yes | **Verified** |

The eleven rows in bold are covered by `tests/test_studio.cpp` (137 cases,
run by CTest as `pebble_studio`), together with the project restoration, the
process handling, the find and replace panel, the keyboard editing of the
editor, the *Edit* menu, the tab navigation and the project tree.

## Detailed findings

### 1. Main window, size, theme

* **Python** — `QMainWindow` resized to 1280x820 (`pebble_studio_qt.py:1144`),
  Fusion style, hand-built dark palette (`:364`) and a large stylesheet
  (`:134`). Nothing about the window is persisted by the reference.
* **C++** — 1280x820 default (`MainWindow` constructor, `src/studio/studio.cpp`),
  the size comes from the settings and survives a restart. The visual system
  lives in its own module (`src/studio/theme.hpp`, `src/studio/theme.cpp`):
  the colours of the surfaces, a `QPalette`, the style sheet, the fonts and
  the drawn icons. The application is put into the Fusion style before the
  window is built (`applyStudioTheme`), so that the look of the Studio does
  not depend on the desktop of the machine it runs on.
* **Impact** — cosmetic. The one functional consequence was that the persisted
  window size was ignored: `main()` called `w.resize(1280, 820)` after the
  constructor had already restored the stored size, so the stored size never
  survived. That call is gone; the size now comes from the settings.
* **Dependencies** — none. Window-size restore is part of finding 29.
* **Size / risk** — small / low.
* **Test** — store a size, close, reopen, compare the restored size. The
  visual system is covered by `theWorkspaceUsesOneStyleSheet`,
  `theWindowCarriesTheIconOfTheStudio` and
  `theActivityBarIconFollowsItsPanel`.

### 2. Explorer / project tree

* **Python** — `QFileSystemModel` rooted at the workspace, name filter `*.peb`
  with `setNameFilterDisables(False)` so directories stay visible
  (`pebble_studio_qt.py:1422-1426`); hidden header, dimmed directory names,
  full-path tooltips (`:421-429`); double-click and `activated` open a file
  (`:1758-1761`); read-only, no editing of the tree.
* **C++** — hand-rolled `QTreeWidget` recursion (`MainWindow::populateTree`)
  that lists the **sources** of the project: a file is only listed when its
  suffix is `peb`, so the manifests, the assets, the lockfiles and the other
  files that are no sources stay out of the tree. The directories stay
  visible, also when they hold no source at all, so that the sources of a
  nested folder can be reached; the hidden folders and the `build` folder
  stay hidden. The absolute path is stored in `Qt::UserRole` and a double
  click opens the file. The full path of an entry is not shown as a tooltip
  yet, and the directory names are not dimmed.
* **C++** — the tree refreshes itself: `ProjectTree::showEvent` asks the
  window for a refresh every time the tree becomes visible again, so a
  source that was added, removed or renamed while the tree was closed shows
  up right away (`MainWindow::refreshProjectTree`). The refresh keeps the
  selection of the explorer and never lists an entry twice. A project
  directory that is gone or that cannot be read leaves the tree as it is
  instead of emptying it.
* **Impact** — the tree shows the sources of the project and nothing else,
  and it follows the files that change on disk.
* **Dependencies** — needs a reliable project root (finding 8).
* **Size / risk** — small / low.
* **Test** — `tests/test_studio.cpp` opens a project that holds sources,
  files that are no sources and nested folders: the sources are listed, the
  files that are no sources are hidden, the directories stay visible, a
  nested source sits under its directory, a source that was added while the
  tree was closed shows up when the tree is opened again, a source that was
  removed or renamed while the tree was closed is gone or has its new name
  when the tree is opened again, a refresh never lists an entry twice, and a
  project directory that is gone leaves the tree alone.

### 3. Tabs and dirty markers

* **Python** — movable, elided tab bar with an animated mint indicator
  (`:1021-1104`); each tab carries a close button that shows a filled dot
  while the document is dirty and a cross on hover (`:943-971`); the title is
  the file name and the tooltip the full path (`:1549-1556`).
* **C++** — a plain `QTabWidget` in the document mode; the dirty marker is a
  `*` appended to the tab *text* (`MainWindow::refreshDocumentLabel`); tabs
  are neither movable nor elided. Every tab carries the button that closes
  it, and the button is drawn by the Studio itself
  (`MainWindow::installTabCloseButton`), so that it belongs to the visual
  system instead of to the desktop theme.
* **Impact** — the dirty marker is text that the application has to parse
  back (finding 7). The cosmetic differences are minor. The tab polish of
  the reference (a dot on the close button, elided names, movable tabs) is
  **deferred** (item R11).
* **Dependencies** — the dirty *state* work is part of finding 7.
* **Size / risk** — small / low.
* **Test** — tab text for a dirty and a clean document.

### 4. New file

* **Python** — `File > New File` (`Ctrl+N`) creates an untitled document
  called `Untitled.peb` (`pebble_studio_qt.py:1301`, `:1727-1730`).
* **C++** — **implemented** in this pass: `File > New File` and `Ctrl+N` open
  an untitled document named `Untitled.peb` with no path
  (`src/studio/studio.cpp`, `MainWindow::newFile`).
* **Impact** — the Studio can now start a new program without a project.
* **Dependencies** — the untitled-document model (finding 7) and the `.peb`
  suffix rule of finding 6.
* **Size / risk** — small / medium (touches save, save-as and close).
* **Test** — `newFileCreatesAnUntitledDocument`: the tab title is
  `Untitled.peb`, the path is empty and the document is not dirty.

### 5. Open file (dialog)

* **Python** — `File > Open File` (`Ctrl+O`) with the filter
  *"Pebble programs (\*.peb);;All files (\*)"* (`:1302`, `:1732-1738`).
* **C++** — **implemented** in this pass: `File > Open File` and `Ctrl+O` open
  the same filter. The file that is already open is reused instead of being
  opened twice.
* **Impact** — files outside the project directory can now be opened.
* **Dependencies** — the document model of finding 7.
* **Size / risk** — small / low.
* **Test** — `openPathLoadsTheFile`,
  `openingTheSameFileTwiceReusesTheTab`, `openPathReportsAMissingFile`.

### 6. Save / Save As

* **Python** — `Save` writes to the current path, or opens *Save As* when the
  document has never been saved (`:1763-1770`). *Save As* defaults to the
  current path or `Untitled.peb`, **appends `.peb` when the chosen name has no
  suffix** (`:1783-1784`), refuses to write a file that is already open in
  another tab and jumps to that tab (`:1787-1797`), writes UTF-8, clears the
  dirty flag, and refreshes the file list (`:1806-1811`).
* **C++** — **implemented** in this pass. `Save` writes to the path of the
  document and falls back to *Save As* for a document that has never been
  written; *Save As* defaults to the current path or `Untitled.peb`, adds
  `.peb` to a name without a suffix, refuses a target that is already open in
  another tab, writes UTF-8, clears the dirty flag and refreshes the tree.
* **Impact** — a document created in the Studio can be written to disk from
  the editor.
* **Dependencies** — finding 7.
* **Size / risk** — small / medium.
* **Test** — `saveAsWritesTheFileAndAddsTheSuffix`,
  `saveAsRefusesAFileThatIsAlreadyOpen`,
  `theMenuActionsReachTheirSlots`.

### 7. Unsaved-change handling

* **Python** — the document's `modificationChanged` signal drives a separate
  `dirty` flag (`:998`, `:1013-1014`); undoing back to the saved state clears
  it. Closing asks *Save / Discard / Cancel* and **only closes when the save
  succeeded** (`:1814-1834`).
* **C++** — **implemented** in this pass. The dirty state is a member of the
  document (`DocumentEditor::dirty`), driven by `modificationChanged`, so an
  undo back to the saved state clears it. Closing asks *Save / Discard /
  Cancel* and **keeps the tab open when the write fails**.
* **Impact** — unsaved work is no longer lost, and the dirty state is
  representable for a document that has no file name yet.
* **Dependencies** — this is the model that findings 4, 5, 6 and 8 need.
* **Size / risk** — medium / medium.
* **Test** — `editingMarksTheDocumentDirty`, `undoClearsTheDirtyState`,
  `closeKeepsTheTabWhenTheSaveFails`, `closeSavesWhenTheUserChoosesSave`,
  `closeDiscardsWhenTheUserChoosesDiscard`,
  `closeStaysOpenWhenTheUserCancels`.

### 8. Session restore

* **Python** — the reference persists **no** window size, **no** last project
  and **no** open tabs. It persists recent files, syntax colours and the
  background (`:1118-1133`, `:1571-1611`). The previous summary of this
  report claimed the opposite and was wrong.
* **C++** — `QSettings("Pebble", "Pebble Studio")` stores the window size, the
  project directory and the list of open files (`MainWindow` constructor,
  `MainWindow::persistSettings`). The restore was **broken**:
  the stored files were opened before the tab widget existed, so **the Studio
  dereferenced a null pointer and crashed on startup** whenever the previous
  session had a file open. The restore now happens after the widgets are
  built, and the stored window size is no longer overwritten in `main()`.
  The project directory is restored as well, and the Explorer is rebuilt from
  it (`MainWindow::restoreProject`), which reuses the existing tree
  population. A stored directory that no longer exists, that cannot be read
  or that holds no `pebble.toml` is reported in the status bar and dropped;
  no other directory is put in its place.
* **Impact** — the crash on the second start of a session is gone and the
  Explorer shows the project of the last session again.
* **Dependencies** — the restore order was part of finding 7; the tree
  population is the existing `MainWindow::populateTree`.
* **Size / risk** — small / low.
* **Test** — `theStoredDocumentsAreRestored`,
  `theStoredWindowSizeIsRestored`, `startupRestoresTheProjectTree`,
  `theRestoredTreeKeepsTheDirectoryStructure`,
  `startupWithoutAProjectDirectoryHasAnEmptyTree`,
  `startupWithAMissingProjectDirectoryReportsIt`,
  `startupWithADirectoryThatIsNotAProjectReportsIt`,
  `restorationDoesNotDuplicateTreeEntries`,
  `theRestoredTreeKeepsTheOpenDocuments`,
  `theTreeSelectionSurvivesARefresh`.

### 9. Find and replace

* **Python** — a `FindBar` above every editor (`:801-940`): a search field
  seeded with the selection, a *match case* toggle, next/previous with
  wrap-around, *Replace* and *Replace All*, a status message
  (`no matches`, `wrapped`, `replaced N`), `Esc` closes the bar. Shortcuts
  `Ctrl+F`, `Ctrl+H`, `F3`, `Shift+F3` (`:1308-1311`).
* **C++** — a `FindReplaceBar` above the tab widget
  (`MainWindow::openFindBar`, `src/studio/studio.cpp`): the same controls
  (find field, replace field, *Match Case*, *Previous*, *Next*, *Replace*,
  *Replace All*, a status label and *Close*), the same status messages and
  the same shortcuts (`Ctrl+F`, `Ctrl+H`, `F3`, `Shift+F3`, `Esc`). The panel
  is opened by `Ctrl+F` and `Ctrl+H` and closed by *Close* or `Esc`, which
  gives the focus back to the editor.
* **Impact** — searching and replacing in a file is possible; before this
  pass the editor was a plain `QPlainTextEdit` without any search.
* **Dependencies** — needs the editor widget to host the bar (finding 7
  provides the document model).
* **Size / risk** — medium / low.
* **Test** — `tests/test_studio.cpp` opens the real panel and drives it with
  the real shortcuts: the panel opens and takes the focus, the replacement
  row follows `Ctrl+H`, the search selects the match, *Next* and *Previous*
  walk the matches, the search wraps around, *Match Case* applies right
  away, *Replace* replaces the current match, *Replace* never overwrites a
  selection of some other text, *Replace All* replaces every match and
  reports the count, *Replace All* stops when the replacement holds the
  searched text, the panel follows the tab that is active, the status label
  reports the result, `Esc` and *Close* close the panel and give the focus
  back to the editor, the panel sits above the editor and the find field is
  seeded with the selection.

### 10. Editor keyboard editing

* **Python** — `Tab` indents the selection by four spaces or inserts spaces
  to the next tab stop, `Shift+Tab` outdents (`:737-775`), `Enter` keeps the
  current indentation (`:704-715`), `Ctrl+/` toggles `#` comments (`:777-798`).
* **C++** — the same behaviour in `CodeEditor::keyPressEvent`
  (`src/studio/studio.cpp`): `Tab` indents the lines of a selection that
  covers more than one line and inserts the spaces up to the next tab stop
  otherwise, `Shift+Tab` removes one unit of indentation, `Enter` keeps the
  indentation of the line and `Ctrl+/` toggles the `#` comment of the
  selected lines. The keys of the editor are only read when the editor is
  not read only, so the other widgets of the Studio keep their own keys.
* **Impact** — editing code with the keyboard produces the source that the
  reference produces.
* **Dependencies** — none, but it belongs to the same editor as finding 9.
* **Size / risk** — medium / medium (key handling is easy to get wrong).
* **Test** — `tests/test_studio.cpp` presses the real keys in the real
  editor: `Tab` indents and keeps the focus in the editor, `Shift+Tab`
  removes the indentation without touching the rest of the line, a
  selection that covers several lines is indented and outdented as a whole
  and keeps its selection, `Enter` keeps the indentation of the line, an
  empty line and a line that holds nothing but spaces are handled, `Ctrl+/`
  comments a line and removes the comment again, `Ctrl+/` comments every
  line of a selection and leaves the lines behind the selection alone, and
  the keys of the editor do not disturb the find and replace panel.

### 11. Line numbers

* **Python** — a gutter whose width follows the line count, the current line
  drawn brighter, painted from the block geometry and the content offset
  (`:608-656`).
* **C++** — a `LineNumberArea` beside the text of the editor
  (`LineNumberArea::paintEvent`). The numbers are painted from the block
  geometry and the content offset, so they stay on their lines while the
  document scrolls, and the width of the gutter follows the number of the
  digits of the last line (`CodeEditor::lineNumberAreaWidth`).
* **C++** — the gutter was broken and is fixed: the area was placed at
  `viewport()->rect().left() - width`, which is a negative coordinate, so
  the area was drawn outside the editor and the numbers never appeared. The
  area now sits in the margin that the editor keeps on the left
  (`CodeEditor::updateLineNumberAreaGeometry`).
* **Impact** — the gutter is now on the screen and follows the text.
* **Dependencies** — none.
* **Size / risk** — small / low.
* **Test** — `theLineNumbersArePaintedOnTheScreen` reads the pixels of the
  gutter and finds the numbers of the lines, and
  `theGutterSitsOnTheLeftOfTheText` checks that the text starts behind the
  gutter.

### 12. Syntax highlighting

* **Python** — eight categories (keyword, string, number, comment, builtin,
  boolean, function call, operator), each with its own colour, configurable
  in a dialog (`:431-482`, `:484-564`).
* **C++** — `PebbleHighlighter` covers the same eight categories with the
  colours of the visual system. The rules are the rules of the lexer of the
  compiler (`src/lexer.cpp`): a comment reaches to the end of the line and
  starts with `//`, a string reaches to its closing quote and honours the
  backslash escapes, a number is a run of digits with an optional fraction,
  and a name is a letter or an underscore followed by letters, digits and
  underscores. The keyword list is the list of the **implemented** dialect
  (`fn`, `let`, `var`, `import`, `package`, `export`, `if`, `else`, `while`,
  `return`), the literals are `true`, `false` and `null`, and a name that is
  followed by `(` is a call.
* **Impact** — the colours of the code cannot be changed; that is finding 25,
  which is **deferred** (item R20).
* **Dependencies** — the colour configuration is finding 25.
* **Size / risk** — small / low.
* **Test** — `theHighlighterMarksTheCodeOfTheDocument` and the cases around
  it format a sample line and compare the character formats.

### 13. Status bar and window title

* **Python** — a flash message that clears after five seconds, the document
  name with a dirty dot, the cursor line and column, the encoding, and a
  state word `Ready / Modified / Saved / Running` (`:1498-1529`, `:1894-1924`).
  The window title follows the current document (`:1916-1918`).
* **C++** — the status bar carries the name of the document that is active,
  the line and the column of the cursor and a state word (`Ready`,
  `Modified`, `Running`, `Stopped`); the state word is coloured when the
  document is changed. The window title names the document that is active and
  carries a `*` when the document is changed. The status follows the change
  of the active document, the move of the cursor, the change of the document
  and the state of the process (`MainWindow::updateStatus`,
  `MainWindow::updateWindowTitle`). A short-lived message is shown for the
  actions that need one (`MainWindow::notify`).
* **Impact** — the encoding is not shown; the reference shows `UTF-8`.
* **Dependencies** — the dirty state of finding 7.
* **Size / risk** — small / low.
* **Test** — `theStatusBarFollowsTheActiveTab`,
  `theWindowTitleFollowsTheActiveTab`, `theStatusBarShowsThePositionOfTheCursor`,
  `theStatusBarShowsTheDirtyState` and `theStatusBarMarksAChangedDocument`.

### 14. Build

* **Python** — **no** build command; the reference only runs the current file.
* **C++** — `Build > Build` runs `pebble build` in the project directory
  (`MainWindow::build`) and shows stdout and stderr in the output pane. The
  start is asynchronous, so the window is never blocked by the compiler, and
  the result is reported separately from a run. The executable is looked up in `PATH`, so the Studio cannot build
  unless `pebble` is installed system wide, even when it sits next to the
  Studio binary.
* **Impact** — build only works on a machine with a global install. The lookup
  itself is defect D9 (item R5) and is still open.
* **Dependencies** — none.
* **Size / risk** — small / low.
* **Test** — `theBuildResultIsReportedSeparately`: a build reports *"Build
  finished with exit code 0."* and not the wording of a run.

### 15. Run

* **Python** — runs **the current file** (`:1960-2000`): the document is
  saved first if it is dirty, the file must end in `.peb`, the run command is
  built by `build_pebble_run_command`, stdout and stderr are shown in the
  output pane, and the exit code is reported (`Program finished
  successfully.`, `Program exited with code N.`, `The Pebble program
  crashed.`).
* **C++** — `Build > Run` runs `pebble run` for the **whole project**
  (`MainWindow::runProject`) and `Build > Run File` (`F5`) runs the document
  that is active (`MainWindow::runCurrentFile`). The single file is saved
  before it is run, the file must end in `.peb` and must exist, and a program
  that is already running is never replaced. The exit is reported per kind of
  process: a run ends with *"Program finished successfully."*, *"Program
  exited with code N."* or *"The Pebble program crashed."*, a build with
  *"Build finished with exit code 0."* or *"Build failed with exit code
  N."*, and a program that the user stopped with *"Program stopped."*
  (`MainWindow::processFinished`). A build and a run are two processes of
  their own (`MainWindow::buildProcess`, `MainWindow::runProcess`), so they
  never share their state, their output or their exit code, and stopping one
  of them does not touch the other. The process is no longer started with a
  blocking `waitForStarted`, so the window stays responsive while a program
  starts.
* **Impact** — the run of a single file and the run of the project are both
  there. The reference has no separate build; the native Studio keeps the
  build, because the compiler of this repository supports it.
* **Dependencies** — finding 17 (stop) and finding 7 (a saved path is needed
  before a file can be run).
* **Size / risk** — medium / medium.
* **Test** — the cases of the run: the file that is saved before it is run,
  the wrong suffix, the missing path, the running program that is not
  replaced, the reported exit code and the separate report of a build and of
  a run.

### 16. Program input (stdin)

* **Python** — a line edit below the output pane, enabled while a program
  runs; the answer is written to the process and echoed into the log
  (`:1465-1472`, `:2018-2035`).
* **C++** — **missing**. A program that reads input cannot be used.
* **Impact** — interactive programs cannot be tried from the Studio. The
  language has no `read` builtin yet, so this is mostly a preparation item.
  The feature is **deferred** (item R14).
* **Dependencies** — finding 15.
* **Size / risk** — small / low.
* **Test** — start a program that reads a line and send input.

### 17. Stop / process management

* **Python** — `Run > Stop` (`Shift+F5`) terminates the process and kills it
  after 1.5 s if it does not stop (`:2064-2076`); the run and stop actions are
  enabled and disabled from the process state (`:1945-1958`); a failed start
  is reported through `errorOccurred` (`:2056-2062`).
* **C++** — **implemented**. `Build > Stop` with the shortcut `Shift+F5`
  terminates the running program and a single shot timer of 1500 ms kills it
  when it ignores the request (`MainWindow::stopProgram`,
  `MainWindow::killProcess`). The action is enabled only while a run is
  active, and it is disabled again as soon as the stop was requested, so a
  second click cannot start a second stop. The timer is cancelled when the
  process exits, so it can never kill a program that was started later. A
  failed start is reported through `QProcess::errorOccurred`
  (`MainWindow::processError`). The same `QProcess` serves a build and a run,
  and the kind of process is tracked separately (`MainWindow::ProcessKind`),
  so a build reports *"Build stopped."* and a run *"Program stopped."*.
  Closing the Studio while a process is active is safe as well: the close
  event is refused, the process is stopped through the same path and the
  window closes by itself once the process is gone
  (`MainWindow::closeEvent`, `MainWindow::requestStop`).
* **Impact** — a program that never returns no longer locks the Studio, and
  quitting the Studio never leaves a process behind.
* **Dependencies** — none; the `QProcess` already exists.
* **Size / risk** — small / low.
* **Test** — `stopIsDisabledWithoutARunningProgram`,
  `stopIsEnabledWhileTheProgramRuns`,
  `stoppingALongRunningProgramEndsIt`,
  `theKillFallbackEndsAProgramThatIgnoresTheStop`,
  `theKillTimerIsCancelledWhenTheProgramExits`,
  `aProgramThatFailsToStartIsReported`,
  `aProgramThatExitsNaturallyIsReported`,
  `aProgramThatFailsIsReported`,
  `theBuildResultIsReportedSeparately`,
  `closingTheStudioStopsTheRunningProgram`.

### 18. Diagnostics panel

* **Python** — **no** diagnostics panel; diagnostics are only part of the
  output text.
* **C++** — **implemented**. The compiler prints its diagnostics as
  `path:line:column: severity: message` (`src/main.cpp`, `printDiagnostics`),
  and the Studio turns exactly that into the entries of the diagnostics list
  (`parseDiagnosticLine`, `MainWindow::addDiagnostic`). The three defects of
  the previous pass are gone:
  * the `itemDoubleClicked` handler is connected **once**, in the constructor
    (`MainWindow::navigateDiagnostic`), so a double click always navigates
    exactly once, no matter how many diagnostics a build produced;
  * a relative path is resolved against the directory the compiler ran in,
    which is the project directory (`MainWindow::resolveDiagnosticPath`), so
    the `./src/main.peb` of the compiler now opens the file of the project
    and not a file of the same name somewhere else;
  * the stderr is collected in a buffer and parsed **line by line**
    (`MainWindow::feedCompilerOutput`), so a diagnostic that arrives in two
    reads is still found. The line that is still buffered is parsed when the
    process ends (`MainWindow::flushCompilerOutput`).
* **Impact** — the errors of a failed build are listed one by one, and a
  double click opens the file of the error on the line and the column that
  the compiler reported. A line without a usable source location is left in
  the output pane and does not become an entry, and a diagnostic whose file
  no longer exists is reported instead of opening an unrelated file.
* **Dependencies** — needs the project root (finding 8).
* **Size / risk** — small / low.
* **Test** — `aCompilerDiagnosticIsParsed`,
  `aMalformedCompilerDiagnosticIsRejected`,
  `aDiagnosticIsCollectedFromTheCompilerOutput`,
  `severalDiagnosticsAreCollectedInOrder`,
  `aDiagnosticThatIsSplitOverTwoReadsIsStillFound`,
  `aDiagnosticWithoutALineBreakIsFound`,
  `aDiagnosticWithSpacesInThePathIsFound`,
  `aDiagnosticResolvesItsPathAgainstTheProject`,
  `aDiagnosticNavigatesToItsSourceLocation`,
  `aDoubleClickOnADiagnosticOpensItsSource`,
  `aDiagnosticForAMissingFileIsReported`,
  `theDiagnosticsAreConnectedOnce`,
  `aBuildReportsItsDiagnostics`,
  `theDiagnosticsOfThePreviousBuildAreCleared`.

### 19. Tree context menu

* **Python** — **no** context menu anywhere.
* **C++** — a tree context menu with *Open*, *New File*, *New Directory*,
  *Rename* and *Delete*. Renaming updates the open tabs, deleting closes the
  affected tabs after asking about unsaved work. The native Studio is **ahead**
  of the reference here. The dialogs were split from the actions in this pass,
  so the actions are covered by tests.
* **Impact** — none; this is native functionality that must be preserved.
* **Dependencies** — the tab bookkeeping follows the new document model
  (finding 7).
* **Size / risk** — small / low.
* **Test** — `renamingKeepsTheOpenDocumentInSync`,
  `deletingClosesTheOpenDocument`, `deletingIsStoppedByAnUnsavedDocument`.

### 20. Menus and shortcuts

* **Python** — `File`, `Edit` (Undo, Redo, Select All, Find, Replace, Find
  Next, Find Previous), `Run`, `View` (docks, Clear Output, Appearance),
  `Help` (`:1349-1395`); shortcuts `Ctrl+N`, `Ctrl+O`, `Ctrl+S`,
  `Ctrl+Shift+S`, `Ctrl+W`, `F5`, `Shift+F5`, `Ctrl+F`, `Ctrl+H`, `F3`,
  `Shift+F3`, `Ctrl+PgDown`, `Ctrl+PgUp` (`:1300-1314`).
* **C++** — `File` (*New File*, *Open File…*, *Open Project…*, *Save*,
  *Save As…*, *Quit*), `Edit` (*Undo*, *Redo*, *Select All*, *Find*,
  *Replace*, *Find Next*, *Find Previous*) and `Build` (*Build*, *Run*,
  *Stop*); shortcuts `Ctrl+N`, `Ctrl+O`, `Ctrl+S`, `Ctrl+Shift+S`, `Ctrl+W`,
  `Shift+F5`, `Ctrl+F`, `Ctrl+H`, `F3`, `Shift+F3`, `Ctrl+Z`, `Ctrl+Shift+Z`
  (also `Ctrl+Y`), `Ctrl+A`, `Ctrl+PgDown`, `Ctrl+PgUp`. The entries of the
  *Edit* menu work on the document that is active
  (`MainWindow::undoActiveDocument`, `MainWindow::redoActiveDocument`,
  `MainWindow::selectAllActiveDocument`), and their enabled state follows
  that document: *Undo* and *Redo* are only enabled while the undo and the
  redo stack of the document allow it, *Select All* is only enabled while a
  document is open, and the state is settled when the tab changes and when a
  document is opened or closed. The shortcuts of the *Edit* menu belong to the
  tab widget, so they only fire while the editor has the focus and never take
  a key away from the find panel. `Ctrl+PgDown` and `Ctrl+PgUp` switch to the
  next and to the previous document (`MainWindow::nextDocument`,
  `MainWindow::previousDocument`) and wrap around at the last and at the
  first tab. There is still no *Run*, *View* or *Help* menu: the run is an
  entry of the *Build* menu.
* **Impact** — undo, redo and select all are reachable with the mouse and
  with the keyboard, and the documents can be walked with the keyboard. The
  menu structure still differs from the reference, which has a *Run* menu of
  its own and a *View* and a *Help* menu.
* **Dependencies** — the individual entries depend on findings 4, 5, 9, 10,
  15 and 17.
* **Size / risk** — small / low once the features exist.
* **Test** — `tests/test_studio.cpp` reaches every entry of the *Edit* menu
  and every shortcut: the menu sits between the *File* and the *Build* menu
  and holds *Undo*, *Redo* and *Select All*, the actions carry `Ctrl+Z`,
  `Ctrl+Shift+Z` and `Ctrl+A`, the actions work on the document that is
  active, the keys reach the editor, the enabled state follows the undo and
  the redo stack and the documents that are open, the keys stay out of the
  find panel, the *Find* and *Replace* entries reach the panel and
  `Ctrl+PgDown` and `Ctrl+PgUp` switch the document and wrap around.

### 21. Recent files

* **Python** — the eight most recent files, de-duplicated, pruned when the
  file no longer exists, recorded when a file is opened or saved
  (`:1571-1611`).
* **C++** — **implemented** in this pass: the *File* menu holds a *Recent
  Files* submenu with the eight files that were opened or saved last, the
  newest one first (`MainWindow::recordRecentFile`,
  `MainWindow::refreshRecentFilesMenu`). A file is in the list only once:
  it is taken out of the list and put at the top, so the list never holds a
  duplicate and never grows beyond eight entries. The list is stored under
  the `recentFiles` key of the settings of the Studio and is read again when
  the window starts (`MainWindow::loadRecentFiles`), so it survives a
  restart. A file that was deleted in the meantime leaves the list when the
  Studio starts (`MainWindow::pruneRecentFiles`), and the cleaned list is
  written back; a file that the Studio itself deletes leaves the list right
  away. A file that the Studio renames keeps its place in the list under its
  new name (`MainWindow::renameRecentFile`). An entry of the menu opens the
  file; an entry whose file cannot be opened any more reports the failure
  and leaves the list. The restore of the last session is not a new action of
  the user, so it does not move a file to the top and the order of the list
  is the order that the user left it in.
* **Impact** — the files that were worked on are one click away, also after
  a restart.
* **Dependencies** — finding 7 (the document model records the path).
* **Size / risk** — small / low.
* **Test** — `tests/test_studio.cpp` opens the files and checks the list: the
  file that was opened is in the list, the newest file is the first one, no
  file is listed twice, the list holds eight files at most, the list
  survives a restart, the restore of the session keeps the order, a file that
  was deleted leaves the list, a file that the Studio deleted or renamed
  leaves the list or keeps its place, an entry of the menu opens the file, an
  entry whose file is gone is dropped, a file that was saved is in the list.

### 22. Welcome page

* **Python** — a welcome tab with *New File*, *Open File*, *Open Example* and
  the recent files (`:1613-1713`); it is removed as soon as a document is
  opened.
* **C++** — **missing**. The feature is **deferred** (item R12): the *File*
  menu and the *Recent Files* submenu already give the three ways into the
  work, so the welcome page is a comfort, not a need.
* **Impact** — a blank window with no starting point.
* **Dependencies** — findings 4, 5 and 21.
* **Size / risk** — medium / low.
* **Test** — the welcome tab appears on an empty session and disappears when
  a document is opened.

### 23. Open example

* **Python** — *Open Example…* lists the examples found by
  `pebble_studio_common.resolve_examples_dir` (`:1872-1889`).
* **C++** — **missing**, and the helper module does not exist in this
  repository and there is no `examples/` directory, so the feature has no
  content to show.
* **Impact** — none until examples exist.
* **Dependencies** — an examples directory must be created first.
* **Size / risk** — n/a until the content exists.
* **Test** — n/a.

### 24. Help (reference, about)

* **Python** — *Pebble Language Ref* and *About* (`:1926-1943`). The
  reference text describes the **old dialect** (`rock start`, `say`, `ask`,
  `fun`, `give`) and would be wrong if it were copied verbatim.
* **C++** — **missing**. The feature is **deferred** (item R21): the
  language is documented in `docs/language.md` and the dialog would have to
  be written from that document, not from the reference, whose text still
  describes the old dialect.
* **Impact** — small; the language is documented in `docs/language.md`.
* **Dependencies** — none, but the text must be rewritten from
  `docs/language.md`.
* **Size / risk** — small / low.
* **Test** — the dialog opens and names the current dialect.

### 25. Appearance: syntax colours

* **Python** — `View > Appearance > Syntax Colours…` opens a dialog with one
  colour button per category, a live preview and *Reset to Defaults*
  (`:484-564`, `:1188-1202`); the colours are stored under `syntax/<key>`.
* **C++** — **missing**. The feature is **deferred** (item R20): the
  colours of the code belong to the visual system of the Studio
  (`src/studio/theme.hpp`), and a dialog that changes them would need a
  theme of its own.
* **Impact** — the colours cannot be changed.
* **Dependencies** — finding 12 supplies the categories.
* **Size / risk** — medium / low.
* **Test** — change a colour, restart, check that it was kept and that the
  preview matches the editor.

### 26. Appearance: wallpaper

* **Python** — a background image with a darkness overlay, configurable from
  the *Appearance* menu (`:1204-1269`).
* **C++** — **missing**. The default wallpaper `pebble_wallpaper.svg` does not
  exist in this repository, so the feature has no default asset.
* **Impact** — cosmetic.
* **Dependencies** — an asset has to be added first.
* **Size / risk** — n/a until the asset exists.
* **Test** — n/a.

### 27. Activity bar and dock layout

* **Python** — a vertical activity bar that toggles the Explorer and the
  Output dock (`:1327-1347`); the docks can be moved and hidden.
* **C++** — a vertical activity bar of 44 px on the left of the workspace
  with the switches of the two panels (`explorer_button` and
  `output_button`). The switch that belongs to the panel that is on the
  screen is marked, and the icon of a panel is brighter while its panel is
  shown. The panels are switched from the bar, from the *View* menu
  (`Ctrl+B`, `Ctrl+J`) and by the state of the workspace, which is stored
  and read again after a restart (`MainWindow::setExplorerVisible`,
  `MainWindow::setOutputPaneVisible`). The panels themselves are the panes
  of two splitters, so they can be resized by the user.
* **Impact** — the output pane no longer steals space from the editor, and
  the panels stay reachable.
* **Dependencies** — none.
* **Size / risk** — medium / low.
* **Test** — `theActivityBarHoldsTheSwitchesOfThePanels`,
  `theExplorerCanBeHiddenAndShown`, `theOutputPaneCanBeHiddenAndShown`,
  `theWorkspaceStateIsStoredWhenTheWindowCloses` and
  `theWorkspaceStateSurvivesARestart`.

### 28. Settings UI

* **Python** — there is **no** general settings dialog either; the reference
  only offers the appearance dialogs of findings 25 and 26.
* **C++** — **missing**, and the settings UI is **deferred** for this project.
* **Impact** — the appearance options are the only settings that would live
  in such a dialog.
* **Dependencies** — findings 25 and 26 first, because a settings dialog
  without configurable values is empty.
* **Size / risk** — medium / low, but out of scope until it is un-deferred.
* **Test** — deferred.

### 29. Settings persistence keys

* **Python** — `syntax/<key>` (eight colours), `background_path`,
  `background_dim`, `recent_files` (`:1118-1133`, `:1595-1611`).
* **C++** — `size`, `projectDir`, `openFiles` (`MainWindow` constructor,
  `MainWindow::persistSettings`) and `recentFiles`
  (`MainWindow::loadRecentFiles`, `MainWindow::saveRecentFiles`). The
  reference persists the recent files under `recent_files` and the native
  Studio under `recentFiles`, so the two now agree on that feature. The sets
  still do not overlap completely: **the reference persists no window size,
  no last project and no open tabs**, and the native Studio persists no
  colours and no background. The previous summary of this report claimed the
  opposite on both sides.
* **Impact** — the native keys are the ones the native Studio needs; the
  reference keys belong to features that the native Studio does not have.
* **Dependencies** — finding 8 for the restore, finding 25 for the missing
  keys.
* **Size / risk** — small / low.
* **Test** — write a key, close, reopen, check the value.

### 30. Output pane and log levels

* **Python** — one `QTextEdit` with coloured levels (`normal`, `info`,
  `warning`, `error`), the prefixes `warning: ` and `error: `, a block
  limit of 20 000 and a separate program input line (`:1288-1297`,
  `:1447-1486`).
* **C++** — one output pane (`QPlainTextEdit`) with the four levels of the
  reference: the output of the program is written as it is (`normal`), the
  messages of the Studio carry the prefix `info: `, a warning carries
  `warning: ` and an error carries `error: `. The `OutputHighlighter` gives
  every line the colour of its level, and a line that holds a diagnostic of
  the compiler is read in the colour of the severity that the compiler
  named. The pane keeps the last 5 000 lines
  (`MainWindow::outputMaximumBlockCount`), so that a long run does not eat
  the memory of the Studio, and it can be emptied with the *Clear* button
  of its head or with `View > Clear Output`. The diagnostics of the
  compiler are listed in their own list, whose head carries the number of
  the entries (`Diagnostics (3)`).
* **Impact** — the pane and the diagnostics list are one experience, and
  the levels of the messages are visible at a glance.
* **Dependencies** — finding 18 for the diagnostics side.
* **Size / risk** — medium / low.
* **Test** — `theOutputPaneHasABound`, `theOutputPaneDropsTheOldestLines`,
  `theClearButtonEmptiesTheOutput`, `theErrorOfTheProgramIsMarked`,
  `theOutputLinesAreColouredByTheirLevel`,
  `aDiagnosticLineIsColouredByItsSeverity`,
  `theDiagnosticsTitleCarriesTheNumber` and
  `theOutputCanBeEmptiedFromTheMenu`.

## Known defects in the current native Studio

| # | Severity | Defect | Source | State |
| --- | --- | --- | --- | --- |
| D1 | **Blocker** | Crash on startup: the stored files were opened before the tab widget existed | the `MainWindow` constructor, before this pass | **fixed** |
| D2 | High | The project directory was restored but the tree stayed empty | the `MainWindow` constructor | **fixed** |
| D3 | High | Unsaved work is lost when a save fails during close | `MainWindow::closeDocument` | **fixed** |
| D4 | High | No way to stop a running program | `MainWindow::stopProgram` | **fixed** |
| D5 | High | Diagnostics navigation uses the wrong base directory | `MainWindow::navigateDiagnostic` | **fixed** |
| D6 | Medium | The diagnostics double-click handler was connected once per diagnostic, and after the rewrite it was not connected at all | the `MainWindow` constructor | **fixed** |
| D7 | Medium | The persisted window size was overwritten by `main()` | `src/studio/main.cpp` | **fixed** |
| D8 | Medium | Build and run shared one process and one exit message | `MainWindow::processFinished` | **fixed** (the separate processes are R17) |
| D9 | Medium | `pebble` is looked up in `PATH` only | `MainWindow::build` | **fixed** (`MainWindow::resolvePebbleExecutable`, item R5) |
| D10 | Medium | stderr is parsed per chunk, so split diagnostics are lost | `MainWindow::readStderr` | **fixed** |
| D11 | Medium | The line number gutter paints with the wrong coordinates | `LineNumberArea::paintEvent` | **fixed** (item R19) |
| D12 | Low | The dirty marker lived in the tab title text | `MainWindow::refreshDocumentLabel` | **fixed** |
| D13 | High | Closing the Studio left the running program behind | `MainWindow::closeEvent` | **fixed** |
| D14 | Medium | The diagnostics of an earlier build stayed in the list and were resolved against the wrong project | `MainWindow::startProcess` | **fixed** |

The fixed defects are covered by `tests/test_studio.cpp`; the open ones are
items of the roadmap in [parity-backlog.md](parity-backlog.md).

## Conclusion

The native Studio is a usable skeleton, not a finished IDE. It has a project
tree, tabs, an editor with a gutter, a build and run command and a
diagnostics list.

The document lifecycle was completed in the first pass: a document can now be
created, opened, saved, saved under a new name and closed without losing
work, and the session is restored without crashing. The project tree of the
last session is now restored as well, so the Explorer is not empty after a
restart. A running program can be stopped, the Studio does not leave a
process behind when it is closed, and the result of a build or a run is
reported with the right wording. The diagnostics of a failed build are now
listed one by one, a double click opens the file of the error on the line and
the column that the compiler reported, and the errors of an earlier build are
not presented as the result of a newer one. The Studio can also find and
replace text now: a find and replace panel sits above the editor, it is
opened with `Ctrl+F` and `Ctrl+H`, the search moves through the matches with
`F3` and `Shift+F3`, and `Esc` closes the panel and gives the focus back to
the editor. The editor itself is edited with the keyboard now: `Tab` and
`Shift+Tab` indent and outdent, `Enter` keeps the indentation of the line
and `Ctrl+/` toggles the `#` comment of the selected lines. The Studio also
remembers the files that were opened or saved last: the *File* menu holds a
*Recent Files* submenu with the eight newest files, the list survives a
restart and the files that were deleted in the meantime leave it. The code is
coloured now (keywords, strings, numbers, comments, literals, calls and
operators, in the dialect that the compiler understands), the lines are
numbered, the status bar names the document that is active, the place of the
cursor and the state of the document, and the title of the window follows the
document. A single file can be run with `F5`, a build and a run are two
processes of their own, and their results are reported apart. The workspace
has an activity bar that switches the panels of the Explorer and of the
output, and the output itself carries the levels of its messages, is bounded
and can be emptied. The Studio has a visual system of its own: a palette, a
style sheet, drawn icons, a window icon and a code font that is comfortable
for a long session.

The features that are still open are the welcome page, the program input,
the help dialog, the dialogue of the syntax colours, the tab polish and the
general settings dialog; they are **deferred** in
[parity-backlog.md](parity-backlog.md) and none of them blocks the day-to-day
work of the editor.

The features the reference has and the native Studio lacks are recorded above
with their impact, dependencies, size, risk and test plan, and are turned
into an ordered plan in [parity-backlog.md](parity-backlog.md). The Studio is
**not** finished and is **not** ready for a handoff; the outstanding items of
the roadmap have to be implemented or explicitly deferred, the key workflows
have to be verified by hand, and the handoff package has to be prepared
first.

## Verification of this pass

Automated (`ctest`, 22 tests, all passing):

* `pebble_studio` — 137 cases in `tests/test_studio.cpp`. Sixteen of them
  cover the document lifecycle: new file, open file, save, save as, the `.peb`
  suffix rule, the duplicate target rule, the dirty flag, undo, the three
  answers to the unsaved prompt, a failing save, the tree rename and delete
  reconciliation, the menu wiring, the registered shortcuts, the restored
  documents and the restored window size. Eleven cover the project tree: a
  valid stored project, the directory structure, a session without a
  project, a stored directory that was deleted, a stored directory that is
  not a project, a repeated restoration, the documents next to the tree and
  the selection of the explorer after a refresh. Ten cover the processes:
  the state of the stop action, stopping a long running program, the kill
  fallback, the cancellation of the fallback timer, a program that fails to
  start, a program that ends normally, a program that fails, the separate
  report of a build and the shutdown that stops a running program. Fourteen
  cover the diagnostics: the parsing of a compiler line, the rejection of a
  malformed line, the collection of one and of several entries, a line that
  arrives in two reads, a line without a line break, a path with a space, the
  resolution against the project, the navigation to the file, the line and
  the column, a real double click on the list, a file that no longer exists,
  the single connection of the handler, the diagnostics of a build and the
  removal of the diagnostics of an earlier build. Nineteen cover the find and
  replace panel: the panel that `Ctrl+F` opens, the replacement row that
  `Ctrl+H` opens, the registered shortcuts, the search that selects the
  match, *Next* and *Previous*, `F3` and `Shift+F3`, the wrap-around, the
  *Match Case* checkbox that applies right away, *Replace* on the current
  match, *Replace* on a selection of some other text, *Replace All* with its
  count, *Replace All* with a replacement that holds the searched text, the
  panel that follows the active tab, the status label, `Esc`, *Close*, the
  place of the panel above the editor and the find field that is seeded with
  the selection. Eleven cover the keyboard editing of the editor: `Tab` that
  indents and keeps the focus, `Shift+Tab` that removes the indentation, an
  outdent that removes nothing but the indentation, a selection that covers
  several lines that is indented and outdented as a whole, `Enter` that
  keeps the indentation, an empty line, a line that holds nothing but
  spaces, `Ctrl+/` on a single line, `Ctrl+/` on a selection of several
  lines, and the keys of the editor next to the find and replace panel.
  Eleven cover the resolver of the compiler binary: the binary next to the
  Studio, `$PEBBLE`, `PATH`, an invalid value of `$PEBBLE`, a path with
  spaces and the binary of the working directory that does not belong to the
  Studio. Eighteen cover the *Edit* menu and the tab navigation: the
  entries of the menu, their shortcuts, the actions that follow the active
  document and the undo stack, the state of the actions without a document
  and the navigation that wraps around. Ten cover the tree of the project:
  the sources that are listed, the files that are no sources, the
  directories, the nested sources and the files that change while the tree
  is closed. Fifteen cover the recent files: the submenu of the *File*
  menu, the file that was opened, the order of the list, the absence of a
  duplicate, the eight entries at most, the list after a restart, the order
  after the restore of the session, a file that was deleted, a file that the
  Studio deleted itself, a file that was renamed, the opening of an entry of
  the menu, an entry whose file is gone, the file that was saved and the list
  in the settings. Fourteen cover the run of a single file and the two
  processes of a build and a run: the file that is saved before it is run,
  the wrong suffix, the path that is not there, the document that is not
  open, the program that is already running and is not replaced, the exit
  code that is reported and the result of a build and of a run that stay
  apart. Twelve cover the highlighting and the gutter: the keywords, the
  literals, the comments, the strings, the numbers, the calls and the
  operators, a word in a string and a word in a comment that are no
  keyword, the highlighter that follows the edits, the width of the gutter
  that grows with the document and the line that the cursor is on. Five
  cover the activity bar and the state of the workspace: the switches of
  the panels, the Explorer that is hidden and shown, the output that is
  hidden and shown and the state that survives a restart. Seven cover the
  output pane and the status bar: the bound of the pane, the oldest lines
  that are dropped, the button that empties the pane, the error of a program
  that is marked, the colour of the levels, the colour of a diagnostic and
  the number in the title of the diagnostics. Nine cover the visual system:
  the numbers of the lines that are on the screen, the gutter that sits on
  the left of the text, the icon of the window, the icon of a panel that
  follows its state, the mark of a changed document in the status bar, the
  font of the code, the style sheet of the workspace, the button of a tab
  that closes the document and the output that is emptied from the menu.

Manual:

* The Studio was started with a stored session (`openFiles`, `projectDir`,
  `size`); it opened the window, restored the document and stayed alive.
  Before the fix the same configuration ended the process with a segmentation
  fault, which was reproduced separately (`tabs->count()` on a null
  `QTabWidget` exits with 139).
* The startup restoration was exercised outside the test harness with a
  small program that links `pebble_studio_lib` and prints the Explorer:
  a stored project gives the full hierarchy (`main.peb`, `pebble.toml`, `src`,
  `src/lib.peb`, `src/nested`, `src/nested/deep.peb`), a stored directory that
  was deleted reports *"The saved project ... is no longer available."* and an
  empty Explorer, and a directory without `pebble.toml` reports *"The saved
  project ... is not a Pebble project."* and an empty Explorer.
* The Studio itself was started twice, once with a stored project and once
  without any stored project; both runs stayed alive.
* The process handling was exercised outside the test harness with a program
  that links `pebble_studio_lib`: the stop action is disabled at startup,
  enabled while a program runs and disabled again after the stop; a program
  that ignores `SIGTERM` is ended by the fallback with *"The program did not
  stop, killing it."*; a program that ends normally reports *"Program
  finished successfully."*, a program that fails reports *"Program exited
  with code 3."*, a program that cannot be started reports *"Failed to start
  the Pebble CLI: ..."* and leaves the run action usable, and a build reports
  *"Build finished with exit code 0."*
* The shutdown was exercised outside the test harness with a program that
  links `pebble_studio_lib`: a running program makes the first close request
  fail, the program is stopped with *"Program stopped."* and the window
  closes once the process is gone.
* The diagnostics were exercised outside the test harness against the **real**
  `pebble` compiler, in a project whose directory holds a space
  (`/tmp/kilo/r4/manual project`). A build of a file with a type error
  produced the compiler line `./src/main.peb:2:3: error: Cannot initialise
  'score' of type 'Int' with a value of type 'String'.`; the Studio listed
  it as *"error: Cannot initialise 'score' of type 'Int' with a value of
  type 'String'. (./src/main.peb:2)"*, and a double click on that entry
  opened `/tmp/kilo/r4/manual project/src/main.peb` and put the cursor on
  line 2, column 3. A build with two errors produced two entries in the
  order of the compiler output, and the double click opened the first one.
  The manual check drove the list with a real mouse double click
  (`QTest::mouseDClick`), not with a direct signal.
* The find and replace panel was exercised outside the test harness with a
  program that links `pebble_studio_lib`: the panel opens above the editor,
  its lower edge is the upper edge of the tab widget, the replacement row
  appears with `MainWindow::openFindBar(true)`, and the project tree and
  the tab widget keep their place in the splitter.
* The keyboard editing was exercised outside the test harness with a program
  that links `pebble_studio_lib` and presses the real keys
  (`QTest::keyClick`): `Tab` turned `fn main() {` into `    fn main() {` and
  the focus stayed in the editor, `Enter` at the end of that line opened a
  new line with the same four spaces and left the cursor behind them,
  `Ctrl+/` turned the selected lines into `    // fn main() {` and
  `// let x = 1` (the line that holds nothing but spaces stayed as it is) and
  removed the comments again, `Ctrl+F` still opened the find panel, and `Tab`
  in the find field kept its meaning as a line edit: the document was not
  touched. The comment of the toggle is `//`, the syntax that the lexer of
  the compiler understands (`src/lexer.cpp:286`); the earlier check of this
  record found `//` as well.
* The window of the visual pass was started outside the test harness with a
  program that links `pebble_studio_lib`, in the offscreen platform of Qt:
  the window was rendered at 900x600, at 1280x820 and at 1600x1000, the
  editor kept a usable size at every size, and the rendered pixels were
  read: the surface of the editor carries `#1b211e`, the gutter `#1e2420`,
  the tree and the output `#202522`, the chrome of the menu bar, of the
  activity bar and of the status bar `#1e2420`, the numbers of the lines
  are painted in the gutter, and the colours of the code reach a contrast of
  at least 4.5 against the background of the editor.
* The window was **not** looked at by eye. The environment has no display
  and the agent that wrote this record cannot see images, so the visual
  pass is a check of the rendered pixels and of the measured contrast, not
  a look at the window. The checklist that a human should walk through is
  in the handoff document
  ([claude-handoff.md](claude-handoff.md)).
