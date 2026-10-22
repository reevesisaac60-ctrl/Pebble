# Pebble Studio Parity Backlog

This is the implementation roadmap that follows from
[python-cpp-parity.md](python-cpp-parity.md). Every item names the parity
finding it closes, the work it depends on, the size and the risk, and how the
result is verified.

## How the order was chosen

The bands below are the requested priority order. They are a guideline, not a
permission to invent work: an item only exists here when the audit found the
corresponding gap in the reference or a defect in the native Studio.

| Band | Meaning |
| --- | --- |
| 1 | Core usability and correctness gaps that block ordinary Studio workflows. |
| 2 | Missing editor and project-management functionality. |
| 3 | Diagnostics, build and run, and workflow improvements. |
| 4 | Remaining useful Python feature parity. |
| 5 | Settings UI and other explicitly deferred features. |
| 6 | Final polish and handoff preparation. |

States: **open**, **in progress**, **done**, **deferred**.

## Band 1 — Core usability and correctness

| ID | Item | Closes | Depends on | Size | Risk | Verification | State |
| --- | --- | --- | --- | --- | --- | --- | --- |
| R1 | Document lifecycle: New File, Open File, untitled documents, a real dirty flag, and a safe session restore | Findings 4, 5, 6, 7, 8; defects D1, D3, D7, D12 | — | medium | medium | `pebble_studio` covers new, open, save, save-as, dirty, undo, close, rename and delete | **done** |
| R2 | Restore the project tree at startup | Finding 8; defect D2 | R1 (restore order) | small | low | `pebble_studio` covers a valid project, the directory structure, a session without a project, a deleted directory, a directory that is not a project, a repeated restoration and the selection after a refresh | **done** |
| R3 | Stop a running program, report a failed start, and stop the program when the Studio is closed | Finding 17; defects D4, D13 | — | small | low | `pebble_studio` covers the state of the stop action, the stop itself, the kill fallback, the timer cleanup, a failed start, the exit reports and the shutdown that stops a running program | **done** |
| R4 | Diagnostics: connect the handler once, resolve paths against the project root, parse stderr line by line | Finding 18; defects D5, D6, D10, D14 | R2 (project root) | small | low | `pebble_studio` covers the parsing, the malformed lines, the line split over two reads, the path with a space, the resolution, the navigation, a real double click, the missing file, the single connection and the removal of the old diagnostics | **done** |
| R5 | Find the `pebble` binary next to the Studio or through `$PEBBLE` before falling back to `PATH` | Finding 14; defect D9 | — | small | low | A build works when only the Studio and the compiler binary sit in one directory | **done** |

## Band 2 — Editor and project management

| ID | Item | Closes | Depends on | Size | Risk | Verification | State |
| --- | --- | --- | --- | --- | --- | --- | --- |
| R6 | Find and replace bar: find, match case, next/previous with wrap, replace, replace all | Finding 9 | R1 | medium | low | `pebble_studio` opens the real panel and drives it with the real shortcuts: it finds, wraps, replaces one and replaces all, reports the count and follows the active tab | **done** |
| R7 | Editor keyboard editing: Tab/Shift+Tab indent and outdent, Enter keeps the indent, `Ctrl+/` toggles `//` comments | Finding 10 | R1 | medium | medium | `pebble_studio` presses the real keys in the real editor: indent, outdent, comment toggle on a selection | **done** |
| R8 | An Edit menu with Undo, Redo and Select All, plus the missing `Ctrl+PgUp`/`Ctrl+PgDown` tab navigation | Finding 20 | — | small | low | `pebble_studio` reaches every entry of the *Edit* menu and every shortcut: the actions work on the active document, their enabled state follows the undo and the redo stack, and the tab navigation wraps around | **done** |
| R9 | Project tree: show only `.peb` files, keep directories, refresh when the tree is reopened | Finding 2 | R2 | small | low | `pebble_studio` shows the sources of a project, hides the files that are no sources, keeps the directories and follows the files that change while the tree is closed | **done** |
| R10 | Recent files (eight entries, de-duplicated, pruned) | Finding 21 | R1 | small | low | `pebble_studio` opens the files, keeps the list free of duplicates, holds eight entries at most, keeps the list across a restart, drops the files that were deleted and opens a file from the menu | **done** |
| R11 | Tab polish: movable tabs, elided names, a dirty close button, `Untitled.peb` for new documents | Finding 3 | R1 | small | low | The tab bar shows the dirty state without parsing the title text | **deferred** |
| R12 | Welcome page with *New File*, *Open File* and the recent files | Finding 22 | R1, R10 | medium | low | The welcome tab appears on an empty session and disappears on the first document | **deferred** |

## Band 3 — Diagnostics, build and run, workflow

| ID | Item | Closes | Depends on | Size | Risk | Verification | State |
| --- | --- | --- | --- | --- | --- | --- | --- |
| R13 | Run the current file: save it first, require `.peb`, report the exit code, and stop the project-wide run | Finding 15; defect D8 | R1, R3 | medium | medium | `pebble_studio` saves the document, runs it, refuses a wrong suffix and a path that is not there, and reports the exit code | **done** |
| R14 | Program input: send a line to the running program and echo it | Finding 16 | R13 | small | low | A program that reads a line receives the answer | **deferred** |
| R15 | One output pane with log levels (`normal`, `info`, `warning`, `error`) and a block limit | Finding 30 | R4 | medium | low | `pebble_studio` shows a warning and an error with their level, drops the oldest lines past the bound, empties the pane with the button and counts the diagnostics | **done** |
| R16 | Status bar: flash message, document name, cursor line and column, and a state word; the window title follows the document | Finding 13 | R1 | small | low | `pebble_studio` follows the active tab, the cursor and the dirty state in the status bar and in the title of the window | **done** |
| R17 | Build and run as separate processes with their own exit messages | Finding 15 | R5 | small | low | `pebble_studio` reports a build and a run apart, and stopping one of them does not touch the other | **done** |

## Band 4 — Remaining useful Python parity

| ID | Item | Closes | Depends on | Size | Risk | Verification | State |
| --- | --- | --- | --- | --- | --- | --- | --- |
| R18 | Syntax highlighting for comments, booleans, call names and operators, using the **implemented** dialect | Finding 12 | — | small | low | `pebble_studio` formats a sample line in every category and follows the edits | **done** |
| R19 | A correct line number gutter and a current line highlight | Finding 11; defect D11 | — | small | low | `pebble_studio` finds the numbers of the lines in the rendered gutter and checks that the gutter sits on the left of the text | **done** |
| R20 | Syntax colour dialog with a live preview and a reset, stored under `syntax/<key>` | Finding 25 | R18 | medium | low | A colour is kept across a restart and the preview matches the editor | **deferred** |
| R21 | Help: a language reference written from `docs/language.md` and an about box | Finding 24 | — | small | low | The dialog names the current dialect (`fn`, `let`, `print`) | **deferred** |
| R22 | Activity bar and dock layout, so the output pane can be hidden | Finding 27 | — | medium | low | `pebble_studio` toggles both panels from the activity bar and keeps the state of the workspace across a restart | **done** |
| R23 | An `Open Example…` entry, once an examples directory exists | Finding 23 | — | small | low | The dialog lists the examples | **deferred** |

## Band 5 — Deferred

| ID | Item | Closes | Depends on | Size | Risk | Verification | State |
| --- | --- | --- | --- | --- | --- | --- | --- |
| R24 | Settings UI (a general dialog that groups the appearance and behaviour options) | Finding 28 | R20, R21 | medium | low | Deferred | **deferred** |
| R25 | Wallpaper and background darkness | Finding 26 | — | small | low | `pebble_studio` checks that the wallpaper of the user is the default background and that the window carries the icon of the user; the files are found in a development build, in an installation and in the copy that is built into the program | **done** |

## Band 6 — Polish and handoff

| ID | Item | Closes | Depends on | Size | Risk | Verification | State |
| --- | --- | --- | --- | --- | --- | --- | --- |
| R26 | Visual polish: theme, activity bar icons, window icon | Finding 1, 27 | R22 | medium | low | `pebble_studio` checks the style sheet of the workspace, the icon of the window, the icon of a panel and the font of the code; the rendered pixels were read, the window was not looked at by eye | **done** |
| R27 | Handoff package: the parity report, this roadmap, the build and test commands, the known limitations, and the manual verification checklist | — | R1..R26 | small | low | `docs/studio/claude-handoff.md` holds the package and the checklist; a human walk through of the checklist is still open | **done** |

## Critical path

```
R1 ──┬─ R2 ── R4 ── R15
     ├─ R6
     ├─ R7
     ├─ R10
     ├─ R11  (deferred)
     ├─ R12  (deferred)
     └─ R16
R3 ── R13 ── R14  (deferred)
R5 ── R17
R18 ── R20 ── R24  (deferred)
R22 ── R26 ── R27
```

`R1` to `R10` were the first ten items and are done. `R4` was unblocked by
`R2`: the diagnostics are resolved against the restored project root. `R5`
was independent of the other work and unblocks `R17`. `R6` needed the
document model of `R1` and is done. `R7` sits in the same editor as `R6` and
is done. `R8` needed the document model of `R1` and is done as well. `R9`
sits on the project root of `R2` and is done. `R10` sits on the document
model of `R1` and is done.

The second pass took the items that carry the day-to-day work of the editor:
`R13` (run the file that is being edited), `R15` (one output pane with
levels), `R16` (the status bar and the title of the window), `R17` (a build
and a run that are two processes of their own), `R18` and `R19` (the colours
of the code and the gutter), `R22` (the activity bar and the panels of the
workspace), `R26` (the visual system of the Studio) and `R27` (the handoff
package). All of them are done.

The items that are left are **deferred**: `R11` (the polish of the tabs),
`R12` (the welcome page), `R14` (the input of a program), `R20` (the dialog
of the syntax colours), `R21` (the help and the about box), `R23` (the entry
of the example project) and `R24` (the general settings dialog). None of them
blocks the work of the editor, and the language has no `read` builtin yet, so
`R14` would have nothing to feed. They stay in this file as a record of what
the reference has and what the Studio does not.

`R1` unblocks the largest number of later items and removes the only crash in
the Studio, so it was the first item of the roadmap. It is done: the studio
is now built from `src/studio/studio.hpp` and `src/studio/studio.cpp` and is
covered by `tests/test_studio.cpp` (186 cases, CTest target `pebble_studio`).

R3 is done as well: `MainWindow::stopProgram` terminates the running program
and a single shot timer of 1500 ms kills it when the program ignores the
request. The timer is cancelled when the process exits, so it can never kill
a program that was started later, and the stop action is disabled as soon as
the stop was requested. The process is no longer started with a blocking
`waitForStarted`, and the result of a build and of a run are reported with
their own wording. The Studio is closed safely as well: `MainWindow::closeEvent`
refuses the close while a process is active, stops it through the same path
and closes the window once the process is gone, so quitting never leaves a
process behind. Ten of the 186 cases in `tests/test_studio.cpp` cover the
process handling.

R2 is done as well: `MainWindow::restoreProject` rebuilds the Explorer from
the stored directory, reusing `MainWindow::populateTree`, and the tree is
restored after the documents so that the restored project keeps the message
in the status bar. A stored directory that was deleted, that cannot be read
or that holds no `pebble.toml` is reported and dropped; the Studio never
falls back to another directory. Eight of the 186 cases in
`tests/test_studio.cpp` cover the restoration, and the menu and the
restored session are covered by three more.

R4 is done as well. The compiler prints its diagnostics as
`path:line:column: severity: message`, and the Studio turns exactly that
into the entries of the diagnostics list (`parseDiagnosticLine`). The three
defects are gone: the double click handler is connected once in the
constructor, a relative path is resolved against the project directory
(`MainWindow::resolveDiagnosticPath`), and the stderr is collected in a
buffer and parsed line by line (`MainWindow::feedCompilerOutput`), so a
diagnostic that arrives in two reads is still found. The line that is still
buffered is parsed when the process ends. A line without a usable source
location stays in the output pane and does not become an entry; a
diagnostic whose file no longer exists is reported instead of opening an
unrelated file; and the list is emptied when a new process starts or when
another project is opened, so the errors of an earlier build are never
presented as the result of a newer one. Fourteen of the 186 cases in
`tests/test_studio.cpp` cover the diagnostics, among them a real double
click on the list.

R5 is done as well: `resolvePebbleExecutable` looks for the compiler next
to the Studio binary, then in `$PEBBLE` and only then in `PATH`, and the
resolution is cached so that a stub compiler that a test injects is not
replaced by a real `pebble` from the environment. Eleven of the 186 cases in
`tests/test_studio.cpp` cover the resolver.

R6 is done as well. The find and replace panel (`FindReplaceBar`) sits above
the tab widget, so it is directly over the text of the active document, and
it always works on the document that is active: a search follows the tab
that is switched to. The panel holds the find field, the replace field, the
*Match Case* checkbox, *Previous*, *Next*, *Replace*, *Replace All*, the
status label and *Close*. `Ctrl+F` opens it for a search, `Ctrl+H` opens it
with the replacement row, `F3` and `Shift+F3` walk the matches and `Esc`
closes the panel and gives the focus back to the editor. The escape key is
only handled while the panel is open, so it never takes the key away from
the editor. The *Match Case* checkbox applies right away: the search runs
again with the new setting, so the match on the screen is the one that the
setting describes. Two defects of the helper methods are gone as well: a
single *Replace* only replaces a selection that really is the text that is
searched for, so a selection of some other text is never overwritten, and
*Replace All* continues its search behind the inserted text, so a
replacement that holds the searched text can never be found a second time
and the replacement always ends. Nineteen of the 186 cases in
`tests/test_studio.cpp` cover the panel, the shortcuts and the search; they
open the real panel and drive it with the real shortcuts.

R7 is done as well. The editor (`CodeEditor::keyPressEvent`) reads the keys
that the reference reads: `Tab` indents the lines of a selection that
covers more than one line and inserts the spaces up to the next tab stop
otherwise, `Shift+Tab` removes one unit of indentation, `Enter` keeps the
indentation of the line and `Ctrl+/` toggles the `//` comment of the
selected lines, the comment syntax of the implemented dialect. The keys are only read when the editor is not read only,
so the other widgets of the Studio keep their own keys, and the find and
replace panel is not disturbed. The selection is kept when the lines are
indented, outdented or commented: every line of the selection keeps its
place and the cursor stays behind the same text. A line without text is not
indented and not commented, and an outdent removes nothing but the
indentation. Eleven of the 186 cases in `tests/test_studio.cpp` cover the
keyboard editing; they press the real keys in the real editor.

R8 is done as well. The Studio has an *Edit* menu between the *File* and the
*Build* menu, with *Undo*, *Redo* and *Select All* and with the entries of
the search that the reference keeps in the same menu (*Find*, *Replace*,
*Find Next*, *Find Previous*). The three editing entries work on the
document that is active, so a search or an undo follows the tab that the
user is in. Their enabled state follows the document as well: *Undo* and
*Redo* are only enabled while the undo and the redo stack of the active
document allow it, *Select All* is only enabled while a document is open,
and the state is settled when the tab changes and when a document is opened
or closed. `Ctrl+Z` undoes, `Ctrl+Shift+Z` (and `Ctrl+Y`) redoes and
`Ctrl+A` selects all; the shortcuts belong to the tab widget, so they only
fire while the editor has the focus and never take a key away from the find
panel, which keeps the keys of its own fields. `Ctrl+PgDown` and `Ctrl+PgUp`
switch to the next and to the previous document and wrap around at the last
and at the first tab. Eighteen of the 186 cases in `tests/test_studio.cpp`
cover the menu, the shortcuts and the tab navigation.

R9 is done as well. The tree of the Studio lists the sources of the project:
`MainWindow::populateTree` only adds a file when its suffix is `peb`, so the
manifest of the project, the assets, the lockfiles and the other files that
are no sources stay out of the tree and can no longer be opened as if they
were source. The directories stay visible, also when they hold no source at
all, so that the sources of a nested folder can be reached; the hidden
folders and the `build` folder stay hidden as well. The tree follows the
files that change on disk: `ProjectTree::showEvent` asks the window for a
refresh every time the tree becomes visible again, so a source that was
added, removed or renamed while the tree was closed shows up right away. The
refresh keeps the selection of the explorer, never lists an entry twice and
leaves the tree alone when the project directory is gone or cannot be read,
so a directory that is only temporarily unavailable does not empty the
explorer. Ten of the 186 cases in `tests/test_studio.cpp` cover the tree.

R10 is done as well. The Studio remembers the files that were opened or saved
last and offers them in the *File* menu, under *Recent Files*. The list holds
the eight newest files, the newest one first, and every file is in the list
only once: `MainWindow::recordRecentFile` takes the file out of the list and
puts it at the top, so a file that is opened again is not listed twice and
the list never holds more than eight files. The list is stored under the
`recentFiles` key of the settings of the Studio and is read again when the
window starts, so it survives a restart. A file that was deleted in the
meantime leaves the list when the Studio starts
(`MainWindow::pruneRecentFiles`), and the cleaned list is written back, so the
stored list never holds a file that is gone. A file that the Studio deletes
itself leaves the list right away, and a file that the Studio renames keeps
its place under its new name. The entries of the menu open the file; an entry
whose file cannot be opened any more reports the failure and leaves the list.
The restore of the last session is not a new action of the user, so it does
not move a file to the top and the order of the list is the order that the
user left it in. Fifteen of the 186 cases in `tests/test_studio.cpp` cover
the recent files.

R13 is done as well. `Build > Run File` (`F5`) runs the document that is
active: the document is saved before it is run, so the program never runs a
stale copy of the file, the file must end in `.peb`, the path must exist, and
a program that is already running is never replaced by a second one. A
document that is not open, a document that was never saved and a file with
the wrong suffix are refused with a message that says why. The exit code of
the program is reported with its own wording, and the result of a build
stays apart from the result of a run. Eleven of the 186 cases in
`tests/test_studio.cpp` cover the run of the current file, the build of the
current file and the two results.

R15 is done as well. The output is one pane with the four levels of the
reference: the output of the program is written as it is, the messages of the
Studio carry the prefix `info: `, a warning carries `warning: ` and an error
carries `error: `. The `OutputHighlighter` gives every line the colour of its
level, and a line that holds a diagnostic of the compiler is read in the
colour of the severity that the compiler named. The pane keeps the last 5 000
lines, so that a long run does not eat the memory of the Studio, and it can
be emptied with the *Clear* button of its head or with `View > Clear Output`.
The diagnostics of the compiler keep their own list, whose head carries the
number of the entries. Eight of the 186 cases in `tests/test_studio.cpp`
cover the pane.

R16 is done as well. The status bar shows the name of the document that is
active, the line and the column of the cursor and a state word; the state
word is coloured when the document is changed. The title of the window names
the document that is active and carries a `*` when the document is changed.
The status follows the change of the active document, the move of the cursor,
the change of the document and the state of the process. Five of the 186
cases in `tests/test_studio.cpp` cover it.

R17 is done as well. A build and a run are two processes of their own
(`MainWindow::buildProcess` and `MainWindow::runProcess`), so they never
share their state, their output or their exit code, and stopping one of them
does not touch the other.

R18 and R19 are done as well. The highlighter knows the eight categories of
the reference and follows the rules of the lexer of the compiler, so that the
code of the dialect that is implemented is the code that is coloured. The
gutter follows the number of the lines and marks the line of the cursor. The
gutter was in name only before: the area was placed at a negative
coordinate, outside the editor, so the numbers never appeared. The area now
sits in the margin that the editor keeps on the left, and two of the 186
cases read the rendered pixels to prove that the numbers are on the screen.

R22 is done as well. The workspace has an activity bar of 44 px on the left
that switches the Explorer and the output, and the switch of the panel that
is on the screen is marked. The state of the workspace is stored and read
again after a restart. Five of the 186 cases in `tests/test_studio.cpp`
cover it.

R26 is done as well. The visual system of the Studio lives in
`src/studio/theme.hpp` and `src/studio/theme.cpp`: the colours of the
surfaces, the palette, the style sheet, the fonts and the drawn icons,
including the icon of the window. The application is put into the Fusion
style before the window is built, so that the look does not depend on the
desktop of the machine. The layout was corrected as well: the output pane
took most of the window before, the tree and the editor now share the
workspace in the ratio that the work needs. Seven of the 186 cases in
`tests/test_studio.cpp` cover the parts of the system that a user sees. The
window was **not** looked at by eye; the checklist of
[claude-handoff.md](claude-handoff.md) is still open.

R27 is done as well. `docs/studio/claude-handoff.md` describes the purpose
of the project, the architecture, the directories, the build, the tests, the
implemented functions, the limits, the deferred items and the next steps.

## Next item

The roadmap is **not** a list that has to be walked to the end. The items
that carry the work of the editor are done, and the rest is deferred on
purpose. The next steps are therefore not the items of this file, but the
three lines below.

1. **Look at the window.** The visual pass was checked by reading the
   rendered pixels and the measured contrast, not by eye. A human should
   walk through the checklist of
   [claude-handoff.md](claude-handoff.md) once and report what looks wrong.
2. **Take the deferred items one at a time**, when a real need appears.
   `R11` (the polish of the tabs) is the smallest of them and the one that
   touches the editor the most; `R14` is the one that waits for the language
   to be able to read a line.
3. **Keep the reference in its place.** It is a design reference for a
   dialect that the compiler of this repository does not implement. An
   editor feature that needs language syntax is written against
   `docs/language.md` and against the lexer of the compiler, never against
   the keywords of the reference.

## Deliberately not on the roadmap

* **The old dialect of the reference** (`rock`, `start`, `say`, `ask`,
  `otherwise`, `repeat`, `fun`, `give`). The compiler that ships with this
  repository implements `fn`, `let`, `var`, `import package`, `export`, `if`,
  `else`, `while`, `return` and `print` (`docs/language.md`). The reference is
  a design reference, not a language specification; no editor feature may
  reproduce the old keywords.
* **A copy of the reference's wallpaper and icon.** The reference expects
  `pebble_wallpaper.svg` and `assets/icons/pebble.png`, which do not exist
  in this repository. The Studio carries the artwork of the user instead
  (`assets/icon.png` and `assets/Windows XP.jpg`), which is described in
  `docs/studio/branding.md`.
* **A general settings dialog** (R24) is deferred, as agreed for this
  project; the reference does not have one either.
