#pragma once

#include <QList>
#include <QByteArray>
#include <QString>
#include <QStringList>

struct IntelliSenseCompletion {
    QString name;
    QString kind;
    QString signature;
    QString detail;
};

struct IntelliSenseHover {
    QString title;
    QString detail;

    bool isValid() const { return !title.isEmpty(); }
};

struct IntelliSenseSignature {
    QString name;
    QString signature;
    QStringList parameters;
    int activeParameter = 0;

    bool isValid() const { return !name.isEmpty() && !signature.isEmpty(); }
};

struct IntelliSenseDefinition {
    QString path;
    int line = 0;
    int column = 0;

    bool isValid() const { return line > 0 && column > 0; }
};

class IntelliSenseModel {
public:
    void rebuild(const QString &source, const QString &sourcePath = {});
    void setProjectPackages(const QStringList &searchPaths, const QStringList &packageNames);
    void invalidateModuleCache() { moduleCacheKey.clear(); }
    QStringList moduleSourceFiles() const;
    QStringList moduleSourceDirectories() const;

    QString prefixAt(int utf16Position) const;
    QList<IntelliSenseCompletion> completions(
        int utf16Position,
        bool explicitRequest = false
    ) const;
    IntelliSenseHover hoverAt(int utf16Position) const;
    IntelliSenseSignature signatureHelpAt(int utf16Position) const;
    IntelliSenseDefinition definitionAt(int utf16Position) const;
    bool suggestionsAllowedAt(int utf16Position) const;
    bool hasContextWithoutPrefix(int utf16Position) const;

private:
    struct Symbol {
        QString name;
        QString kind;
        QString signature;
        QString detail;
        QString path;
        int line = 0;
        int column = 0;
        int declarationOffset = -1;
        int declaredAt = 0;
        int scopeStart = 0;
        int scopeEnd = 0;
    };
    QString source;
    QString activeSourcePath;
    QByteArray utf8;
    QList<Symbol> symbols;
    QStringList imports;
    QStringList projectSearchPaths;
    QStringList projectPackageNames;
    QString moduleCacheKey;
    QStringList moduleCacheFiles;
    QString moduleCacheFingerprint;
    QList<Symbol> moduleCacheSymbols;
    struct LexicalToken {
        int start = 0;
        int end = 0;
        int kind = 0;
        QString text;
    };
    QList<LexicalToken> tokens;
    QList<int> activeCallAtBoundary;
    QList<int> braceDepthAtBoundary;

    QStringList availablePackageNames(const QString &sourcePath) const;
    QString packageContextName(int utf16Position) const;
    bool isImportContext(int utf16Position) const;
    bool isTypeContext(int utf16Position) const;
};
