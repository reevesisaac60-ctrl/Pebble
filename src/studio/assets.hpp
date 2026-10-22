#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>

namespace studio {

[[nodiscard]] QString iconPath();

[[nodiscard]] QString wallpaperPath();

[[nodiscard]] QString defaultWallpaperName();

[[nodiscard]] QIcon icon();

[[nodiscard]] QString wallpaperDirectory();

[[nodiscard]] QStringList wallpaperFileNames();

[[nodiscard]] QStringList wallpaperFileNamesIn(const QString &directory);

[[nodiscard]] QString wallpaperPathFor(const QString &name);

}
