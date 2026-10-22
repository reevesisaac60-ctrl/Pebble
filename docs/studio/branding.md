# The branding of Pebble Studio

The Studio carries the artwork of the user: the icon of the program and the
wallpaper of the window.

## The files

| File | What it is |
| --- | --- |
| `assets/icon.png` | The artwork of the user. It keeps its own size and its own shape. |
| `assets/Windows XP.jpg` | The wallpaper of the Studio. It is the background of the window. |
| `assets/*.jpg` | The other wallpapers of the user. Every image of the `assets` directory is offered by the wallpaper picker. |
| `assets/icons/pebble-*.png` | The square icon files that the artwork produces. |

The artwork is never stretched and never cut. The square icon files hold the
whole artwork on a sheet of transparent paper, so that a desktop and a
taskbar can show it.

The square files are written by a script, so that they can be rebuilt at any
time:

```sh
python3 tools/generate_icons.py
```

The script needs Pillow. The files it writes are part of the repository, so
the build itself does not need Pillow.

## Where the files are found

The Studio looks for its files in this order:

1. The data of the installation, `<prefix>/share/pebble/assets`.
2. The project next to the program and the directories above it, so that the
   Studio finds its files when it runs from the build directory of the
   project.
3. The installation next to the program, `<prefix>/share/pebble/assets`,
   which is found from `<prefix>/bin`.
4. The copy that is built into the program, which is always there.

The program therefore finds its icon and its wallpaper in a development
build, in an installation and when it is moved to another place. The working
directory of the program is never asked.

## What the files are used for

| Where | The file |
| --- | --- |
| The icon of the window | `assets/icon.png` |
| The icon of the program in the taskbar | `assets/icon.png` |
| The mark of the Studio on the welcome page | `assets/icon.png` |
| The background of the window | `assets/Windows XP.jpg` |

The wallpaper is the background of a window that has no other background. The
menu `View → Appearance` keeps the entries of the reference:

| Entry | What it does |
| --- | --- |
| Wallpaper… | Opens the picker of the wallpapers, with a picture of every image of the `assets` directory. |
| Choose Background Image… | Takes an image of the user as the background. |
| Background Darkness… | Sets the darkness of the layer that lies over the background. |
| Remove Background Image | Takes the background away. |
| Restore Default Background | Puts the wallpaper of the user back. |

The picker offers every image of the `assets` directory that Qt can read, and
it begins with the wallpaper of the Studio itself. The icon of the program is
not a wallpaper and is never offered. A picture that cannot be read is passed
over, so that a broken file does not stop the Studio.

A wallpaper of the `assets` directory is remembered by its file name, so that
the choice of the user survives a move of the installation. An image that was
taken from somewhere else is remembered by its path. A wallpaper that is no
longer there is replaced by the wallpaper of the Studio.

A window that has never seen a background shows the wallpaper of the user. A
window whose background was taken away keeps it away, because the choice of
the user is remembered.

## The installation

```sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

The installation writes:

| Path | What it is |
| --- | --- |
| `<prefix>/bin/pebble_studio` | The program. |
| `<prefix>/share/pebble/assets/icon.png` | The artwork of the user. |
| `<prefix>/share/pebble/assets/*.jpg` | The wallpapers of the user. |
| `<prefix>/share/icons/hicolor/<size>x<size>/apps/pebble-studio.png` | The icon of the launcher, in the sizes a desktop asks for. |
| `<prefix>/share/applications/pebble-studio.desktop` | The launcher of the desktop. |

The launcher carries the icon of the user, so that the program shows it in
the menu of the desktop and in the taskbar.

## The launcher of VS Codium

The launcher of VS Codium belongs to the package that carries VS Codium, so
it is not written to. A launcher in the data of the user takes its place
instead: a desktop reads the launchers of the home directory first, and the
launcher of the package stays as it is.

```sh
tools/install_codium_icon.sh           # put the icon on the launcher
tools/install_codium_icon.sh --remove  # bring the original launcher back
```

The script writes `~/.local/share/applications/codium.desktop`, a copy of
the launcher of the package in which only the icon is changed, and the icon
itself in `~/.local/share/icons/pebble/codium.png`. The name of the program,
the arguments, the way the window is opened and the behaviour of VS Codium
stay the same.

To bring the original launcher back, remove the file of the user:

```sh
rm -f ~/.local/share/applications/codium.desktop
rm -f ~/.local/share/icons/pebble/codium.png
```

The launcher of the package in `/usr/share/applications/codium.desktop` is
never touched.
