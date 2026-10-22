#include "assets.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QPixmap>
#include <QString>
#include <QStringList>

#include <algorithm>

#ifndef PEBBLE_STUDIO_DATA_DIR
#define PEBBLE_STUDIO_DATA_DIR ""
#endif

#ifndef PEBBLE_STUDIO_DEFAULT_WALLPAPER
#define PEBBLE_STUDIO_DEFAULT_WALLPAPER "Windows XP.jpg"
#endif

namespace {

QString locateNearStudio(const QString &relative) {
    QDir directory(QCoreApplication::applicationDirPath());

    for (int level = 0; level < 4; ++level) {
        const QString candidate = directory.filePath(relative);

        if (QFileInfo::exists(candidate)) {
            return QDir::cleanPath(candidate);
        }

        if (!directory.cdUp()) {
            break;
        }
    }

    return QString();
}

QString locateNearStudioDirectory(const QString &relative) {
    QDir directory(QCoreApplication::applicationDirPath());

    for (int level = 0; level < 4; ++level) {
        const QString candidate = directory.filePath(relative);

        if (QFileInfo(candidate).isDir()) {
            return QDir::cleanPath(candidate);
        }

        if (!directory.cdUp()) {
            break;
        }
    }

    return QString();
}

QString locateAsset(const QString &relative) {
    const QString built_in = QStringLiteral(":/") + relative;

    const QStringList candidates = {
        QDir(QStringLiteral(PEBBLE_STUDIO_DATA_DIR)).filePath(relative),
        locateNearStudio(relative),
        locateNearStudio(QStringLiteral("share/pebble/") + relative),
        built_in
    };

    for (const QString &candidate : candidates) {
        if (candidate.isEmpty()) {
            continue;
        }

        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return built_in;
}

QStringList supportedImageSuffixes() {
    QStringList suffixes;

    for (const QByteArray &format : QImageReader::supportedImageFormats()) {
        suffixes << QString::fromLatin1(format).toLower();
    }

    return suffixes;
}

bool naturalLessThan(const QString &left, const QString &right) {
    int leftIndex = 0;
    int rightIndex = 0;

    while (leftIndex < left.size() && rightIndex < right.size()) {
        const QChar leftCharacter = left.at(leftIndex);
        const QChar rightCharacter = right.at(rightIndex);

        if (leftCharacter.isDigit() && rightCharacter.isDigit()) {
            int leftEnd = leftIndex;
            int rightEnd = rightIndex;

            while (leftEnd < left.size() && left.at(leftEnd).isDigit()) {
                ++leftEnd;
            }

            while (rightEnd < right.size() && right.at(rightEnd).isDigit()) {
                ++rightEnd;
            }

            const QString leftNumber = left.mid(leftIndex, leftEnd - leftIndex);
            const QString rightNumber = right.mid(rightIndex, rightEnd - rightIndex);

            if (leftNumber.length() != rightNumber.length()) {
                return leftNumber.length() < rightNumber.length();
            }

            if (leftNumber != rightNumber) {
                return leftNumber < rightNumber;
            }

            leftIndex = leftEnd;
            rightIndex = rightEnd;
            continue;
        }

        if (leftCharacter != rightCharacter) {
            return leftCharacter < rightCharacter;
        }

        ++leftIndex;
        ++rightIndex;
    }

    return left.size() - leftIndex < right.size() - rightIndex;
}

}

namespace studio {

QString iconPath() {
    return locateAsset(QStringLiteral("assets/icon.png"));
}

QString wallpaperPath() {
    return locateAsset(
        QStringLiteral("assets/") + QStringLiteral(PEBBLE_STUDIO_DEFAULT_WALLPAPER)
    );
}

QString defaultWallpaperName() {
    return QStringLiteral(PEBBLE_STUDIO_DEFAULT_WALLPAPER);
}

QIcon icon() {
    const QString path = iconPath();

    if (path.isEmpty()) {
        return QIcon();
    }

    return QIcon(path);
}

QString wallpaperDirectory() {
    QStringList candidates;

    if (!QStringLiteral(PEBBLE_STUDIO_DATA_DIR).isEmpty()) {
        candidates << QDir(
            QStringLiteral(PEBBLE_STUDIO_DATA_DIR)
        ).filePath(QStringLiteral("assets"));
    }

    candidates << locateNearStudioDirectory(QStringLiteral("assets"));
    candidates << locateNearStudioDirectory(QStringLiteral("share/pebble/assets"));

    for (const QString &candidate : candidates) {
        if (!candidate.isEmpty() && QFileInfo(candidate).isDir()) {
            return candidate;
        }
    }

    return QString();
}

QStringList wallpaperFileNamesIn(const QString &directory) {
    QStringList names;

    if (directory.isEmpty()) {
        return names;
    }

    const QDir folder(directory);

    if (!folder.exists()) {
        return names;
    }

    const QStringList suffixes = supportedImageSuffixes();
    const QString iconName = QFileInfo(iconPath()).fileName();
    const QFileInfoList entries = folder.entryInfoList(QDir::Files, QDir::Name);

    for (const QFileInfo &entry : entries) {
        if (!suffixes.contains(entry.suffix().toLower())) {
            continue;
        }

        if (!iconName.isEmpty() && entry.fileName() == iconName) {
            continue;
        }

        QImageReader reader(entry.absoluteFilePath());

        if (!reader.canRead()) {
            continue;
        }

        names << entry.fileName();
    }

    std::sort(names.begin(), names.end(), naturalLessThan);

    return names;
}

QStringList wallpaperFileNames() {
    return wallpaperFileNamesIn(wallpaperDirectory());
}

QString wallpaperPathFor(const QString &name) {
    if (name.isEmpty() || name.contains('/') || name.contains('\\')) {
        return QString();
    }

    const QString directory = wallpaperDirectory();

    if (directory.isEmpty()) {
        return QString();
    }

    const QString iconName = QFileInfo(iconPath()).fileName();

    if (!iconName.isEmpty() && name == iconName) {
        return QString();
    }

    const QString path = QDir(directory).filePath(name);

    if (!QFileInfo::exists(path)) {
        return QString();
    }

    QImageReader reader(path);

    if (!reader.canRead()) {
        return QString();
    }

    return QDir::cleanPath(path);
}

}
