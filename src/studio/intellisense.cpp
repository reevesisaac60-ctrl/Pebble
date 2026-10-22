#include "intellisense.hpp"

#include "pebble/ast.hpp"
#include "pebble/lexer.hpp"
#include "pebble/module.hpp"
#include "pebble/package.hpp"
#include "pebble/parser.hpp"
#include "pebble/type_checker.hpp"

#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace {

int sourceOffset(const QByteArray &text, const pebble::SourceLocation &location) {
    int line = 1;
    int offset = 0;
    while (line < static_cast<int>(location.line) && offset < text.size()) {
        if (text.at(offset++) == '\n') {
            ++line;
        }
    }
    return static_cast<int>(qMin<qsizetype>(
        text.size(),
        static_cast<qsizetype>(offset + static_cast<int>(location.column) - 1)
    ));
}

QString fromUtf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QString nodeType(const pebble::Node &node) {
    for (const auto &child : node.children) {
        if (child->kind == pebble::NodeKind::TypeName) {
            return fromUtf8(child->text);
        }
    }
    for (const auto &child : node.children) {
        if (child->kind != pebble::NodeKind::LiteralExpression) {
            continue;
        }
        if (child->text.starts_with('"')) return QStringLiteral("String");
        if (child->text == "true" || child->text == "false") return QStringLiteral("Bool");
        return child->text.find('.') == std::string::npos
            ? QStringLiteral("Int") : QStringLiteral("Float");
    }
    return QStringLiteral("unknown");
}

QString returnType(const pebble::Node &function) {
    for (const auto &child : function.children) {
        if (child->kind == pebble::NodeKind::TypeName) return fromUtf8(child->text);
    }
    return QStringLiteral("Void");
}

QString functionSignature(const pebble::Node &function) {
    QStringList parameters;
    QString returnType = QStringLiteral("Void");
    for (const auto &child : function.children) {
        if (child->kind == pebble::NodeKind::Parameter) {
            QString parameter = fromUtf8(child->text);
            for (const auto &annotation : child->children) {
                if (annotation->kind == pebble::NodeKind::TypeName) {
                    parameter += QStringLiteral(": ") + fromUtf8(annotation->text);
                }
            }
            parameters.append(parameter);
        } else if (child->kind == pebble::NodeKind::TypeName) {
            returnType = fromUtf8(child->text);
        }
    }
    return QStringLiteral("%1(%2) -> %3")
        .arg(fromUtf8(function.text), parameters.join(QStringLiteral(", ")), returnType);
}

QString keywordDescription(pebble::TokenKind kind) {
    switch (kind) {
    case pebble::TokenKind::Fn: return QStringLiteral("Declares a named function.");
    case pebble::TokenKind::Let: return QStringLiteral("Declares an immutable local value.");
    case pebble::TokenKind::Var: return QStringLiteral("Declares a mutable local value.");
    case pebble::TokenKind::Import: return QStringLiteral("Imports a package for qualified calls.");
    case pebble::TokenKind::Package: return QStringLiteral("Introduces the package name in an import.");
    case pebble::TokenKind::Export: return QStringLiteral("Makes a package function available to importers.");
    case pebble::TokenKind::If: return QStringLiteral("Runs a block when its condition is true.");
    case pebble::TokenKind::Else: return QStringLiteral("Runs the alternate branch of an if statement.");
    case pebble::TokenKind::While: return QStringLiteral("Repeats a block while its condition is true.");
    case pebble::TokenKind::Return: return QStringLiteral("Returns a value from the current function.");
    case pebble::TokenKind::True: return QStringLiteral("Boolean literal.");
    case pebble::TokenKind::False: return QStringLiteral("Boolean literal.");
    case pebble::TokenKind::Null: return QStringLiteral("Null literal.");
    default: return {};
    }
}

}

void IntelliSenseModel::setProjectPackages(
    const QStringList &searchPaths,
    const QStringList &packageNames
) {
    if (projectSearchPaths == searchPaths && projectPackageNames == packageNames) return;
    projectSearchPaths = searchPaths;
    projectPackageNames = packageNames;
}

QStringList IntelliSenseModel::moduleSourceFiles() const {
    QStringList result;
    for (const QString &path : moduleCacheFiles) {
        if (QFileInfo::exists(path)) result.append(QFileInfo(path).absoluteFilePath());
    }
    result.removeDuplicates();
    return result;
}

QStringList IntelliSenseModel::moduleSourceDirectories() const {
    QStringList result;
    for (const QString &path : moduleCacheFiles) {
        const QString directory = QFileInfo(path).absolutePath();
        if (QFileInfo(directory).isDir()) result.append(directory);
    }
    result.removeDuplicates();
    return result;
}

QStringList IntelliSenseModel::availablePackageNames(const QString &sourcePath) const {
    QStringList names;
    for (const pebble::Package &package : pebble::builtinPackages()) {
        names.append(fromUtf8(package.name));
    }
    names.append(projectPackageNames);

    const auto appendSourcesIn = [&names](const QString &directoryPath) {
        if (directoryPath.isEmpty()) return;
        const QDir directory(directoryPath);
        const QStringList files = directory.entryList(
            {QStringLiteral("*.peb")}, QDir::Files | QDir::Readable, QDir::Name
        );
        for (const QString &file : files) names.append(QFileInfo(file).completeBaseName());
    };
    const QString sourceDirectory = QFileInfo(sourcePath).absolutePath();
    if (!sourcePath.isEmpty()) {
        appendSourcesIn(sourceDirectory);
        appendSourcesIn(QDir(sourceDirectory).filePath(QStringLiteral("packages")));
    }
    for (const QString &searchPath : projectSearchPaths) appendSourcesIn(searchPath);

    names.removeDuplicates();
    return names;
}

QString IntelliSenseModel::packageContextName(int utf16Position) const {
    const int position = qBound(0, utf16Position, static_cast<int>(source.size()));
    const QString beforePrefix = source.left(position - prefixAt(position).size());
    static const QRegularExpression memberContext(
        QStringLiteral("([A-Za-z_][A-Za-z0-9_]*)\\s*([:.])\\s*$")
    );
    const QRegularExpressionMatch match = memberContext.match(beforePrefix);
    if (!match.hasMatch()) return {};
    if (match.captured(2) == QLatin1String(":") && isTypeContext(position)) return {};
    const QString packageName = match.captured(1);
    if (!imports.contains(packageName)) return {};
    const int bytePosition = static_cast<int>(source.left(position).toUtf8().size());
    for (const Symbol &symbol : symbols) {
        if (symbol.name == packageName &&
            (symbol.kind == QLatin1String("let") || symbol.kind == QLatin1String("var") ||
             symbol.kind == QLatin1String("parameter")) &&
            bytePosition >= symbol.declaredAt && bytePosition >= symbol.scopeStart &&
            bytePosition <= symbol.scopeEnd) {
            return {};
        }
    }
    return packageName;
}

bool IntelliSenseModel::isImportContext(int utf16Position) const {
    const int position = qBound(0, utf16Position, static_cast<int>(source.size()));
    const QString before = source.left(position);
    const QString line = before.mid(before.lastIndexOf(QLatin1Char('\n')) + 1);
    static const QRegularExpression importContext(
        QStringLiteral("^\\s*import\\s+(?:package\\s+)?[A-Za-z_0-9]*$")
    );
    return importContext.match(line).hasMatch();
}

bool IntelliSenseModel::isTypeContext(int utf16Position) const {
    const int position = qBound(0, utf16Position, static_cast<int>(source.size()));
    const QString before = source.left(position - prefixAt(position).size());
    static const QRegularExpression variableTypeContext(
        QStringLiteral("\\b(?:let|var)\\s+[A-Za-z_][A-Za-z_0-9]*\\s*:\\s*$")
    );
    static const QRegularExpression parameterTypeContext(
        QStringLiteral("\\bfn\\s+[A-Za-z_][A-Za-z_0-9]*\\s*\\([^)]*"
                       "[A-Za-z_][A-Za-z_0-9]*\\s*:\\s*$")
    );
    static const QRegularExpression returnTypeContext(
        QStringLiteral("\\bfn\\s+[A-Za-z_][A-Za-z_0-9]*\\s*\\([^)]*\\)\\s*->\\s*$")
    );
    return variableTypeContext.match(before).hasMatch() ||
           parameterTypeContext.match(before).hasMatch() ||
           returnTypeContext.match(before).hasMatch();
}

bool IntelliSenseModel::hasContextWithoutPrefix(int utf16Position) const {
    return !packageContextName(utf16Position).isEmpty() ||
           isImportContext(utf16Position) || isTypeContext(utf16Position);
}

void IntelliSenseModel::rebuild(const QString &newSource, const QString &sourcePath) {
    source = newSource;
    activeSourcePath = sourcePath;
    utf8 = source.toUtf8();
    symbols.clear();
    imports.clear();
    tokens.clear();

    const pebble::Source input{
        sourcePath.toStdString(),
        std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()))
    };
    const pebble::LexResult lexed = pebble::Lexer{}.scan(input);
    for (const pebble::Token &token : lexed.tokens) {
        tokens.append(LexicalToken{
            sourceOffset(utf8, token.location),
            sourceOffset(utf8, token.location) + static_cast<int>(token.lexeme.size()),
            static_cast<int>(token.kind),
            fromUtf8(token.lexeme)
        });
    }
    activeCallAtBoundary.clear();
    braceDepthAtBoundary.clear();
    QList<int> previousCalls;
    int activeCall = -1;
    int braceDepth = 0;
    for (int index = 0; index < tokens.size(); ++index) {
        activeCallAtBoundary.append(activeCall);
        braceDepthAtBoundary.append(braceDepth);
        const LexicalToken &token = tokens.at(index);
        if (token.kind == static_cast<int>(pebble::TokenKind::LeftParen)) {
            previousCalls.append(activeCall);
            const bool followsName = index > 0 && tokens.at(index - 1).kind ==
                static_cast<int>(pebble::TokenKind::Identifier);
            if (followsName) activeCall = index;
        } else if (token.kind == static_cast<int>(pebble::TokenKind::RightParen) &&
                   !previousCalls.isEmpty()) {
            activeCall = previousCalls.takeLast();
        } else if (token.kind == static_cast<int>(pebble::TokenKind::LeftBrace)) {
            ++braceDepth;
        } else if (token.kind == static_cast<int>(pebble::TokenKind::RightBrace)) {
            braceDepth = qMax(0, braceDepth - 1);
        }
    }
    activeCallAtBoundary.append(activeCall);
    braceDepthAtBoundary.append(braceDepth);

    pebble::ParseResult parsed = pebble::Parser{}.parse(lexed);
    if (!parsed.root) {
        return;
    }

    for (const auto &node : parsed.root->children) {
        if (node->kind == pebble::NodeKind::ImportDeclaration) {
            imports.append(fromUtf8(node->text));
        }
    }

    QString earlyCacheKey = QFileInfo(sourcePath).absoluteFilePath();
    earlyCacheKey += QLatin1Char('\n') + projectSearchPaths.join(QLatin1Char('\n'));
    QStringList earlyImports = imports;
    earlyImports.sort(Qt::CaseSensitive);
    earlyCacheKey += QLatin1Char('\n') + earlyImports.join(QLatin1Char('\n'));
    const auto cacheFingerprint = [](const QStringList &paths) {
        QString result;
        for (const QString &path : paths) {
            const QFileInfo info(path);
            result += path + QLatin1Char('|') + (info.exists() ? QLatin1Char('1') : QLatin1Char('0')) +
                QLatin1Char('|') + QString::number(info.size()) + QLatin1Char('|') +
                QString::number(info.lastModified().toMSecsSinceEpoch()) + QLatin1Char('\n');
        }
        return result;
    };
    const bool moduleCacheFresh = earlyCacheKey == moduleCacheKey &&
        cacheFingerprint(moduleCacheFiles) == moduleCacheFingerprint;
    pebble::ModuleResult resolvedModules;
    if (!moduleCacheFresh && !sourcePath.isEmpty() && !imports.isEmpty()) {
        std::vector<std::string> searchPaths;
        searchPaths.reserve(static_cast<std::size_t>(projectSearchPaths.size()));
        for (const QString &path : projectSearchPaths) searchPaths.push_back(path.toStdString());
        auto resolutionProgram = pebble::cloneNode(*parsed.root);
        resolvedModules = pebble::resolveModules(
            *resolutionProgram,
            QFileInfo(sourcePath).absolutePath().toStdString(),
            searchPaths
        );
    }

    const int documentEnd = static_cast<int>(utf8.size());

    QMap<QString, QString> functionReturns;
    for (const auto &node : parsed.root->children) {
        if (node->kind == pebble::NodeKind::Function) {
            functionReturns.insert(fromUtf8(node->text), returnType(*node));
        }
    }
    if (moduleCacheFresh) {
        for (const Symbol &symbol : moduleCacheSymbols) {
            if (symbol.kind != QLatin1String("imported function")) continue;
            const qsizetype arrow = symbol.signature.lastIndexOf(QStringLiteral(" -> "));
            if (arrow >= 0) functionReturns.insert(symbol.name, symbol.signature.mid(arrow + 4));
        }
    } else {
        for (const auto &function : resolvedModules.functions) {
            if (function->exported) {
                functionReturns.insert(fromUtf8(function->text), returnType(*function));
            }
        }
    }

    struct BlockRange { int start; int end; };
    QList<BlockRange> blocks;
    QList<int> braceStack;
    QMap<int, int> matchingBraces;
    for (const LexicalToken &token : tokens) {
        if (token.kind == static_cast<int>(pebble::TokenKind::LeftBrace)) {
            braceStack.append(token.start);
        } else if (token.kind == static_cast<int>(pebble::TokenKind::RightBrace) &&
                   !braceStack.isEmpty()) {
            matchingBraces.insert(braceStack.takeLast(), token.end);
        }
    }

    std::function<QString(const pebble::Node &, int)> inferExpression;
    inferExpression = [&](const pebble::Node &expression, int at) -> QString {
        if (expression.kind == pebble::NodeKind::LiteralExpression) {
            if (expression.text.starts_with('"')) return QStringLiteral("String");
            if (expression.text == "true" || expression.text == "false") {
                return QStringLiteral("Bool");
            }
            if (expression.text == "null") return QStringLiteral("Null");
            return expression.text.find('.') == std::string::npos
                ? QStringLiteral("Int") : QStringLiteral("Float");
        }
        if (expression.kind == pebble::NodeKind::NameExpression) {
            const QString name = fromUtf8(expression.text);
            if (functionReturns.contains(name)) return QStringLiteral("Function");
            for (auto symbol = symbols.crbegin(); symbol != symbols.crend(); ++symbol) {
                if (symbol->name == name && symbol->name.indexOf(QLatin1Char('.')) < 0 &&
                    at >= symbol->declaredAt && at >= symbol->scopeStart &&
                    at <= symbol->scopeEnd &&
                    (symbol->kind == QLatin1String("let") ||
                     symbol->kind == QLatin1String("var") ||
                     symbol->kind == QLatin1String("parameter"))) {
                    return symbol->detail;
                }
            }
            return QStringLiteral("unknown");
        }
        if (expression.kind == pebble::NodeKind::CallExpression &&
            !expression.children.empty()) {
            const pebble::Node &callee = *expression.children.front();
            if (callee.kind == pebble::NodeKind::NameExpression) {
                const QString name = fromUtf8(callee.text);
                if (functionReturns.contains(name)) return functionReturns.value(name);
                if (pebble::isBuiltinFunction(callee.text)) return QStringLiteral("Void");
            } else if (callee.kind == pebble::NodeKind::MemberExpression &&
                       !callee.children.empty()) {
                const QString packageName = fromUtf8(callee.children.front()->text);
                if (imports.contains(packageName)) {
                    if (const pebble::Package *package = pebble::findBuiltinPackage(
                        packageName.toStdString()); package != nullptr) {
                        if (const pebble::PackageFunction *function = pebble::findPackageFunction(
                            *package, callee.text); function != nullptr) {
                            return fromUtf8(function->returnType);
                        }
                    }
                }
                const QString qualified = packageName + QLatin1Char('.') + fromUtf8(callee.text);
                if (functionReturns.contains(qualified)) return functionReturns.value(qualified);
                for (const Symbol &symbol : symbols) {
                    if (symbol.name == qualified &&
                        symbol.kind == QLatin1String("imported function")) {
                        const qsizetype arrow = symbol.signature.lastIndexOf(QStringLiteral(" -> "));
                        if (arrow >= 0) return symbol.signature.mid(arrow + 4);
                    }
                }
            }
            return QStringLiteral("unknown");
        }
        if (expression.kind == pebble::NodeKind::UnaryExpression &&
            !expression.children.empty()) {
            return expression.text == "!" ? QStringLiteral("Bool")
                : inferExpression(*expression.children.front(), at);
        }
        if (expression.kind == pebble::NodeKind::BinaryExpression &&
            expression.children.size() == 2) {
            const QString op = fromUtf8(expression.text);
            if (op == "==" || op == "!=" || op == "<" || op == "<=" ||
                op == ">" || op == ">=" || op == "&&" || op == "||") {
                return QStringLiteral("Bool");
            }
            const QString left = inferExpression(*expression.children[0], at);
            const QString right = inferExpression(*expression.children[1], at);
            if (op == "+" && left == QLatin1String("String") &&
                right == QLatin1String("String")) return QStringLiteral("String");
            if ((left == QLatin1String("Int") || left == QLatin1String("Float")) &&
                (right == QLatin1String("Int") || right == QLatin1String("Float"))) {
                return left == QLatin1String("Float") || right == QLatin1String("Float")
                    ? QStringLiteral("Float") : QStringLiteral("Int");
            }
        }
        return QStringLiteral("unknown");
    };

    const auto declarationType = [this, &inferExpression](const pebble::Node &node) {
        for (const auto &child : node.children) {
            if (child->kind == pebble::NodeKind::TypeName) return fromUtf8(child->text);
        }
        const int location = sourceOffset(utf8, node.location);
        for (const auto &child : node.children) {
            if (child->kind != pebble::NodeKind::TypeName) {
                return inferExpression(*child, location);
            }
        }
        return QStringLiteral("unknown");
    };

    const auto addSymbol = [this](const pebble::Node &node, const QString &kind,
                                  const QString &type, int scopeStart, int scopeEnd,
                                  bool visibleAtScopeStart) {
        const int nodeStart = sourceOffset(utf8, node.location);
        const bool importedFunction = kind == QLatin1String("imported function");
        QString declaredName = fromUtf8(node.text);
        if (importedFunction) {
            declaredName = declaredName.section(QLatin1Char('.'), -1);
        }
        const QByteArray declaredBytes = declaredName.toUtf8();
        const int found = static_cast<int>(utf8.indexOf(declaredBytes, nodeStart));
        const int declaredAt = importedFunction ? nodeStart : (found >= 0 ? found : nodeStart);
        const QString rawPath = fromUtf8(node.location.file).isEmpty()
            ? activeSourcePath : fromUtf8(node.location.file);
        const QString locationPath = rawPath.isEmpty()
            ? QString() : QFileInfo(rawPath).absoluteFilePath();
        const int line = static_cast<int>(node.location.line);
        const int lineStart = static_cast<int>(utf8.lastIndexOf('\n', declaredAt - 1) + 1);
        const int column = importedFunction ? static_cast<int>(node.location.column)
            : static_cast<int>(QString::fromUtf8(
                utf8.mid(lineStart, declaredAt - lineStart)
            ).size()) + 1;
        symbols.append(Symbol{
            importedFunction
                ? fromUtf8(node.text) : declaredName,
            kind,
            node.kind == pebble::NodeKind::Function
                ? functionSignature(node) : QString(),
            type,
            locationPath,
            line,
            column,
            importedFunction ? -1 : declaredAt,
            visibleAtScopeStart ? scopeStart : declaredAt,
            scopeStart,
            scopeEnd
        });
    };

    std::function<void(const pebble::Node &, int, int)> collectBlock;
    collectBlock = [&](const pebble::Node &node, int scopeStart, int scopeEnd) {
        if (node.kind == pebble::NodeKind::Block) {
            scopeStart = sourceOffset(utf8, node.location);
            scopeEnd = matchingBraces.value(scopeStart, documentEnd);
            blocks.append({scopeStart, scopeEnd});
        }
        if (node.kind == pebble::NodeKind::LetDeclaration ||
            node.kind == pebble::NodeKind::VarDeclaration) {
            addSymbol(node, node.kind == pebble::NodeKind::LetDeclaration
                                ? QStringLiteral("let") : QStringLiteral("var"),
                      declarationType(node), scopeStart, scopeEnd, false);
            return;
        }
        for (const auto &child : node.children) {
            collectBlock(*child, scopeStart, scopeEnd);
        }
    };

    for (const auto &node : parsed.root->children) {
        if (node->kind == pebble::NodeKind::Function) {
            addSymbol(*node, QStringLiteral("function"), functionSignature(*node),
                      0, documentEnd, true);
            const pebble::Node *body = nullptr;
            for (const auto &child : node->children) {
                if (child->kind == pebble::NodeKind::Block) body = child.get();
            }
            if (body != nullptr) {
                const int bodyStart = sourceOffset(utf8, body->location);
                const int bodyEnd = matchingBraces.value(bodyStart, documentEnd);
                for (const auto &child : node->children) {
                    if (child->kind == pebble::NodeKind::Parameter) {
                        addSymbol(*child, QStringLiteral("parameter"), nodeType(*child),
                                  bodyStart, bodyEnd, true);
                    }
                }
                collectBlock(*body, bodyStart, bodyEnd);
            }
        } else if (node->kind == pebble::NodeKind::LetDeclaration ||
                   node->kind == pebble::NodeKind::VarDeclaration) {
            addSymbol(*node, node->kind == pebble::NodeKind::LetDeclaration
                                ? QStringLiteral("let") : QStringLiteral("var"),
                      declarationType(*node), 0, documentEnd, false);
        }
    }

    QString cacheKey = QFileInfo(sourcePath).absoluteFilePath();
    cacheKey += QLatin1Char('\n') + projectSearchPaths.join(QLatin1Char('\n'));
    QStringList sortedImports = imports;
    sortedImports.sort(Qt::CaseSensitive);
    cacheKey += QLatin1Char('\n') + sortedImports.join(QLatin1Char('\n'));

    QStringList filesToCheck;
    if (!sourcePath.isEmpty()) {
        const QString baseDirectory = QFileInfo(sourcePath).absolutePath();
        for (const QString &name : imports) {
            filesToCheck.append(QDir(baseDirectory).filePath(name + QStringLiteral(".peb")));
            filesToCheck.append(QDir(baseDirectory).filePath(
                QStringLiteral("packages/") + name + QStringLiteral(".peb")
            ));
            for (const QString &searchPath : projectSearchPaths) {
                filesToCheck.append(QDir(searchPath).filePath(name + QStringLiteral(".peb")));
            }
        }
    }
    const auto fingerprint = [](const QStringList &paths) {
        QString result;
        for (const QString &path : paths) {
            const QFileInfo info(path);
            result += path + QLatin1Char('|') + (info.exists() ? QLatin1Char('1') : QLatin1Char('0')) +
                QLatin1Char('|') + QString::number(info.size()) + QLatin1Char('|') +
                QString::number(info.lastModified().toMSecsSinceEpoch()) + QLatin1Char('\n');
        }
        return result;
    };

    if (cacheKey == moduleCacheKey &&
        fingerprint(moduleCacheFiles) == moduleCacheFingerprint) {
        symbols.append(moduleCacheSymbols);
    } else {
        const qsizetype firstPackageSymbol = symbols.size();
        if (!sourcePath.isEmpty() && !imports.isEmpty()) {
            for (const std::string &moduleFile : resolvedModules.sourceFiles) {
                filesToCheck.append(fromUtf8(moduleFile));
            }
            for (const auto &function : resolvedModules.functions) {
                if (function->exported) {
                    addSymbol(*function, QStringLiteral("imported function"),
                              QStringLiteral("Exported Pebble package function"),
                              0, documentEnd, true);
                }
            }
        }
        filesToCheck.removeDuplicates();
        moduleCacheKey = cacheKey;
        moduleCacheFiles = filesToCheck;
        moduleCacheFingerprint = fingerprint(moduleCacheFiles);
        moduleCacheSymbols = symbols.mid(firstPackageSymbol);
    }
}

QString IntelliSenseModel::prefixAt(int utf16Position) const {
    int position = qBound(0, utf16Position, static_cast<int>(source.size()));
    while (position > 0) {
        const QChar character = source.at(position - 1);
        const ushort code = character.unicode();
        const bool asciiNameCharacter =
            (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z') ||
            (code >= '0' && code <= '9') || code == '_';
        if (!asciiNameCharacter) break;
        --position;
    }
    return source.mid(position, qBound(0, utf16Position, source.size()) - position);
}

bool IntelliSenseModel::suggestionsAllowedAt(int utf16Position) const {
    const int safePosition = qBound(0, utf16Position, static_cast<int>(source.size()));
    const QByteArray prefixBytes = source.left(safePosition).toUtf8();
    const int bytePosition = static_cast<int>(prefixBytes.size());
    const int lineStart = static_cast<int>(prefixBytes.lastIndexOf('\n') + 1);
    bool inString = false;
    bool escaped = false;
    for (int index = lineStart; index + 1 < bytePosition; ++index) {
        const char character = prefixBytes.at(index);
        if (inString) {
            if (escaped) escaped = false;
            else if (character == '\\') escaped = true;
            else if (character == '"') inString = false;
        } else if (character == '"') {
            inString = true;
        } else if (character == '/' && prefixBytes.at(index + 1) == '/') {
            return false;
        }
    }
    const auto containingToken = std::lower_bound(tokens.cbegin(), tokens.cend(), bytePosition,
        [](const LexicalToken &token, int position) { return token.end <= position; });
    if (containingToken != tokens.cend() &&
        containingToken->kind == static_cast<int>(pebble::TokenKind::String) &&
        bytePosition >= containingToken->start && bytePosition < containingToken->end) {
        return false;
    }
    return true;
}

QList<IntelliSenseCompletion> IntelliSenseModel::completions(
    int utf16Position,
    bool explicitRequest
) const {
    QList<IntelliSenseCompletion> result;
    if (!suggestionsAllowedAt(utf16Position)) return result;

    const int safePosition = qBound(0, utf16Position, static_cast<int>(source.size()));
    const QString prefix = prefixAt(safePosition);
    const int bytePosition = static_cast<int>(source.left(safePosition).toUtf8().size());
    if (prefix.isEmpty() && !explicitRequest && !hasContextWithoutPrefix(safePosition)) {
        return result;
    }

    QSet<QString> addedNames;
    const auto add = [&result, &addedNames, &prefix](IntelliSenseCompletion item) {
        if (!item.name.startsWith(prefix, Qt::CaseSensitive) ||
            addedNames.contains(item.name)) {
            return;
        }
        addedNames.insert(item.name);
        result.append(std::move(item));
    };

    const QString packageName = packageContextName(safePosition);
    if (!packageName.isEmpty()) {
        if (const pebble::Package *package =
                pebble::findBuiltinPackage(packageName.toStdString()); package != nullptr) {
            for (const pebble::PackageFunction &function : package->functions) {
                QStringList parameters;
                for (const std::string &parameter : function.parameters) {
                    parameters.append(fromUtf8(parameter));
                }
                const QString name = fromUtf8(function.name);
                add({name, QStringLiteral("package function"),
                     QStringLiteral("%1(%2) -> %3")
                         .arg(name, parameters.join(QStringLiteral(", ")),
                              fromUtf8(function.returnType)),
                     QStringLiteral("Built-in package %1").arg(packageName)});
            }
        }
        const QString qualifiedPrefix = packageName + QLatin1Char('.');
        for (const Symbol &symbol : symbols) {
            if (symbol.kind != QLatin1String("imported function") ||
                !symbol.name.startsWith(qualifiedPrefix, Qt::CaseSensitive)) continue;
            const QString memberName = symbol.name.mid(qualifiedPrefix.size());
            add({memberName, symbol.kind, symbol.signature,
                 QStringLiteral("Exported from package %1").arg(packageName)});
        }
        return result;
    }

    if (isImportContext(safePosition)) {
        const QString before = source.left(safePosition);
        const QString line = before.mid(before.lastIndexOf(QLatin1Char('\n')) + 1);
        static const QRegularExpression afterImport(QStringLiteral("^\\s*import\\s+([^\\s]*)$"));
        const auto match = afterImport.match(line);
        if (match.hasMatch() && QStringLiteral("package").startsWith(prefix) &&
            !match.captured(1).startsWith(QLatin1String("package"))) {
            add({QStringLiteral("package"), QStringLiteral("keyword"), {},
                 QStringLiteral("Uses the canonical package import form.")});
        }
        for (const QString &name : availablePackageNames(activeSourcePath)) {
            add({name, QStringLiteral("package"), {}, QStringLiteral("Available Pebble package")});
        }
        return result;
    }

    if (isTypeContext(safePosition)) {
        for (const std::string_view type : pebble::builtinTypeNames()) {
            const QString name = QString::fromLatin1(type.data(),
                                                      static_cast<qsizetype>(type.size()));
            add({name, QStringLiteral("type"), {}, QStringLiteral("Built-in Pebble type")});
        }
        return result;
    }

    QMap<QString, Symbol> visible;
    for (const Symbol &symbol : symbols) {
        if (bytePosition < symbol.declaredAt || bytePosition < symbol.scopeStart ||
            bytePosition > symbol.scopeEnd) continue;
        const auto existing = visible.constFind(symbol.name);
        if (existing == visible.cend() ||
            symbol.scopeEnd - symbol.scopeStart <
                existing->scopeEnd - existing->scopeStart ||
            (symbol.scopeEnd - symbol.scopeStart ==
                 existing->scopeEnd - existing->scopeStart &&
             symbol.declaredAt > existing->declaredAt)) {
            visible.insert(symbol.name, symbol);
        }
    }
    for (auto it = visible.cbegin(); it != visible.cend(); ++it) {
        const Symbol &symbol = it.value();
        if (symbol.name.contains(QLatin1Char('.'))) continue;
        add({symbol.name, symbol.kind, symbol.signature, symbol.detail});
    }

    const QString beforePrefix = source.left(safePosition - prefix.size());
    const QString linePrefix = beforePrefix.mid(beforePrefix.lastIndexOf(QLatin1Char('\n')) + 1);
    bool statementPosition = linePrefix.trimmed().isEmpty();
    if (!statementPosition) {
        qsizetype last = beforePrefix.size();
        while (last > 0 && beforePrefix.at(last - 1).isSpace()) --last;
        if (last > 0) {
            const QChar previous = beforePrefix.at(last - 1);
            statementPosition = previous == QLatin1Char('{') || previous == QLatin1Char(';') ||
                                previous == QLatin1Char('}');
        }
    }
    if (linePrefix.trimmed().isEmpty() && statementPosition) {
        const QString preceding = beforePrefix.left(beforePrefix.lastIndexOf(QLatin1Char('\n')));
        qsizetype last = preceding.size();
        while (last > 0 && preceding.at(last - 1).isSpace()) --last;
        if (last > 0 && QStringLiteral("+-*/%=<>!&|,.:\\").contains(preceding.at(last - 1))) {
            statementPosition = false;
        }
    }
    const auto tokenBoundary = std::lower_bound(tokens.cbegin(), tokens.cend(), bytePosition,
        [](const LexicalToken &token, int position) { return token.start < position; });
    const int tokenCount = static_cast<int>(std::distance(tokens.cbegin(), tokenBoundary));
    const int braceDepth = braceDepthAtBoundary.value(tokenCount, 0);
    if (statementPosition) {
        bool elsePosition = false;
        const int prefixByteStart = static_cast<int>(
            source.left(safePosition - prefix.size()).toUtf8().size()
        );
        const auto codeBoundary = std::lower_bound(tokens.cbegin(), tokens.cend(), prefixByteStart,
            [](const LexicalToken &token, int position) { return token.end <= position; });
        const int lastCodeToken = static_cast<int>(
            std::distance(tokens.cbegin(), codeBoundary)
        ) - 1;
        if (lastCodeToken >= 0 && tokens.at(lastCodeToken).kind ==
                static_cast<int>(pebble::TokenKind::RightBrace)) {
            int depth = 0;
            int openingBrace = -1;
            for (int index = lastCodeToken; index >= 0; --index) {
                const int kind = tokens.at(index).kind;
                if (kind == static_cast<int>(pebble::TokenKind::RightBrace)) ++depth;
                else if (kind == static_cast<int>(pebble::TokenKind::LeftBrace) && --depth == 0) {
                    openingBrace = tokens.at(index).start;
                    break;
                }
            }
            if (openingBrace >= 0) {
                const QString beforeBlock = QString::fromUtf8(utf8.constData(), openingBrace);
                const qsizetype boundary = std::max({
                    beforeBlock.lastIndexOf(QLatin1Char('{')),
                    beforeBlock.lastIndexOf(QLatin1Char('}')),
                    beforeBlock.lastIndexOf(QLatin1Char(';'))
                });
                const QString header = beforeBlock.mid(boundary + 1);
                static const QRegularExpression ifHeader(
                    QStringLiteral("\\bif\\b[^{};]*$")
                );
                elsePosition = ifHeader.match(header).hasMatch();
            }
        }
        for (const pebble::TokenKind kind : pebble::languageKeywordTokens()) {
            const QString name = QString::fromLatin1(pebble::tokenKindName(kind).data(),
                static_cast<qsizetype>(pebble::tokenKindName(kind).size()));
            const bool literal = kind == pebble::TokenKind::True ||
                                 kind == pebble::TokenKind::False ||
                                 kind == pebble::TokenKind::Null;
            const bool topLevelOnly = kind == pebble::TokenKind::Fn ||
                                      kind == pebble::TokenKind::Import ||
                                      kind == pebble::TokenKind::Export;
            const bool statementKeyword = !literal && !topLevelOnly &&
                kind != pebble::TokenKind::Package && kind != pebble::TokenKind::Else;
            if ((literal && braceDepth > 0) || statementKeyword ||
                (kind == pebble::TokenKind::Else && elsePosition) ||
                (topLevelOnly && braceDepth == 0)) {
                add({name, literal ? QStringLiteral("literal") : QStringLiteral("keyword"),
                     {}, QStringLiteral("Pebble language token")});
            }
        }
    } else {
        for (const pebble::TokenKind kind : pebble::languageKeywordTokens()) {
            if (kind != pebble::TokenKind::True && kind != pebble::TokenKind::False &&
                kind != pebble::TokenKind::Null) continue;
            const QString name = QString::fromLatin1(pebble::tokenKindName(kind).data(),
                static_cast<qsizetype>(pebble::tokenKindName(kind).size()));
            add({name, QStringLiteral("literal"), {}, QStringLiteral("Pebble literal")});
        }
    }
    for (const std::string_view function : pebble::builtinFunctionNames()) {
        const QString name = QString::fromLatin1(function.data(),
                                                  static_cast<qsizetype>(function.size()));
        add({name, QStringLiteral("built-in function"),
             name + QStringLiteral("(...) -> Void"),
             QStringLiteral("Writes each argument to standard output.")});
    }

    const auto rank = [](const IntelliSenseCompletion &item) {
        if (item.kind == QLatin1String("parameter") || item.kind == QLatin1String("let") ||
            item.kind == QLatin1String("var")) return 0;
        if (item.kind.contains(QLatin1String("function"))) return 1;
        if (item.kind == QLatin1String("type")) return 2;
        if (item.kind == QLatin1String("keyword")) return 3;
        return 2;
    };
    std::stable_sort(result.begin(), result.end(), [&rank](const auto &a, const auto &b) {
        if (rank(a) != rank(b)) return rank(a) < rank(b);
        return a.name < b.name;
    });
    static_cast<void>(explicitRequest);
    return result;
}

IntelliSenseHover IntelliSenseModel::hoverAt(int utf16Position) const {
    if (!suggestionsAllowedAt(utf16Position)) return {};
    const int bytePosition = static_cast<int>(source.left(qBound(
        0, utf16Position, static_cast<int>(source.size())
    )).toUtf8().size());
    const auto tokenIt = std::lower_bound(tokens.cbegin(), tokens.cend(), bytePosition,
        [](const LexicalToken &token, int position) { return token.end <= position; });
    if (tokenIt != tokens.cend() && bytePosition >= tokenIt->start &&
        bytePosition < tokenIt->end) {
        const LexicalToken &token = *tokenIt;
        const auto kind = static_cast<pebble::TokenKind>(token.kind);
        const QString keyword = keywordDescription(kind);
        if (!keyword.isEmpty()) return {token.text, keyword};
        if (kind != pebble::TokenKind::Identifier) return {};

        const Symbol *resolved = nullptr;
        for (const Symbol &symbol : symbols) {
            if (symbol.name != token.text || bytePosition < symbol.scopeStart ||
                bytePosition > symbol.scopeEnd || bytePosition < symbol.declaredAt) continue;
            if (resolved == nullptr || symbol.scopeEnd - symbol.scopeStart <
                                       resolved->scopeEnd - resolved->scopeStart ||
                (symbol.scopeEnd - symbol.scopeStart ==
                     resolved->scopeEnd - resolved->scopeStart &&
                 symbol.declaredAt > resolved->declaredAt)) {
                resolved = &symbol;
            }
        }
        if (resolved != nullptr) {
            QString detail = resolved->kind == QLatin1String("function") ||
                                     resolved->kind == QLatin1String("imported function")
                ? resolved->signature
                : QStringLiteral("%1 value — %2").arg(resolved->kind, resolved->detail);
            if (resolved->declarationOffset >= 0) {
                const qsizetype declarationLine =
                    utf8.left(resolved->declarationOffset).count('\n') + 1;
                detail += QStringLiteral("\nDeclared on line %1").arg(declarationLine);
            } else if (!resolved->detail.isEmpty()) {
                detail += QStringLiteral("\n") + resolved->detail;
            }
            return {resolved->name, detail};
        }
        if (pebble::isBuiltinFunction(token.text.toStdString())) {
            return {token.text, QStringLiteral("print(...) -> Void\nWrites each argument to standard output.")};
        }
        const QString beforeToken = source.left(qBound(0, utf16Position,
                                                        static_cast<int>(source.size())));
        const int tokenStart = static_cast<int>(QString::fromUtf8(
            utf8.constData(), token.start
        ).size());
        static const QRegularExpression packageMember(
            QStringLiteral("([A-Za-z_][A-Za-z0-9_]*)\\s*[:.]\\s*$")
        );
        const QRegularExpressionMatch packageMatch = packageMember.match(
            tokenStart >= 0 ? source.left(tokenStart) : beforeToken
        );
        if (packageMatch.hasMatch() && imports.contains(packageMatch.captured(1))) {
            const QString packageName = packageMatch.captured(1);
            const QString qualifiedName = packageName + QLatin1Char('.') + token.text;
            for (const Symbol &symbol : symbols) {
                if (symbol.kind == QLatin1String("imported function") &&
                    symbol.name == qualifiedName) {
                    return {token.text, symbol.signature};
                }
            }
            if (const pebble::Package *package = pebble::findBuiltinPackage(
                    packageName.toStdString()); package != nullptr) {
                if (const pebble::PackageFunction *function = pebble::findPackageFunction(
                        *package, token.text.toStdString()); function != nullptr) {
                    QStringList parameters;
                    for (const std::string &parameter : function->parameters) {
                        parameters.append(fromUtf8(parameter));
                    }
                    return {token.text, QStringLiteral("%1(%2) -> %3")
                        .arg(token.text, parameters.join(QStringLiteral(", ")),
                             fromUtf8(function->returnType))};
                }
            }
        }
        for (const std::string_view type : pebble::builtinTypeNames()) {
            if (token.text == QString::fromLatin1(type.data(),
                    static_cast<qsizetype>(type.size()))) {
                return {token.text, QStringLiteral("Built-in Pebble type.")};
            }
        }
        if (imports.contains(token.text)) return {token.text, QStringLiteral("Imported package.")};
        return {};
    }
    return {};
}

IntelliSenseSignature IntelliSenseModel::signatureHelpAt(int utf16Position) const {
    const int safePosition = qBound(0, utf16Position, static_cast<int>(source.size()));
    const int bytePosition = static_cast<int>(source.left(safePosition).toUtf8().size());
    const auto boundary = std::lower_bound(tokens.cbegin(), tokens.cend(), bytePosition,
        [](const LexicalToken &token, int position) { return token.start < position; });
    const int tokenCount = static_cast<int>(std::distance(tokens.cbegin(), boundary));
    const int openIndex = activeCallAtBoundary.value(tokenCount, -1);
    if (openIndex < 1) return {};
        int calleeIndex = openIndex - 1;
        if (tokens.at(calleeIndex).kind != static_cast<int>(pebble::TokenKind::Identifier)) {
            return {};
        }
        const QString callee = tokens.at(calleeIndex).text;
        QString packageName;
        if (calleeIndex >= 2 &&
            (tokens.at(calleeIndex - 1).kind == static_cast<int>(pebble::TokenKind::Colon) ||
             tokens.at(calleeIndex - 1).kind == static_cast<int>(pebble::TokenKind::Dot)) &&
            tokens.at(calleeIndex - 2).kind == static_cast<int>(pebble::TokenKind::Identifier)) {
            packageName = tokens.at(calleeIndex - 2).text;
        }

        QStringList parameters;
        QString signature;
        if (!packageName.isEmpty() && imports.contains(packageName)) {
            const QString qualifiedName = packageName + QLatin1Char('.') + callee;
            for (const Symbol &symbol : symbols) {
                if (symbol.kind == QLatin1String("imported function") &&
                    symbol.name == qualifiedName && !symbol.signature.isEmpty()) {
                    signature = symbol.signature;
                    const qsizetype left = signature.indexOf(QLatin1Char('('));
                    const qsizetype right = signature.indexOf(QLatin1Char(')'), left + 1);
                    const QString parameterText = left >= 0 && right > left
                        ? signature.mid(left + 1, right - left - 1) : QString();
                    for (const QString &parameter : parameterText.split(
                             QStringLiteral(", "), Qt::SkipEmptyParts)) {
                        parameters.append(parameter);
                    }
                    break;
                }
            }
            if (const pebble::Package *package = pebble::findBuiltinPackage(
                    packageName.toStdString()); package != nullptr) {
                if (const pebble::PackageFunction *function = pebble::findPackageFunction(
                        *package, callee.toStdString()); function != nullptr) {
                    for (const std::string &parameter : function->parameters) {
                        parameters.append(fromUtf8(parameter));
                    }
                    signature = QStringLiteral("%1(%2) -> %3")
                        .arg(qualifiedName, parameters.join(QStringLiteral(", ")),
                             fromUtf8(function->returnType));
                }
            }
        } else {
            for (const Symbol &symbol : symbols) {
                if (symbol.name != callee || symbol.kind != QLatin1String("function") ||
                    symbol.signature.isEmpty()) continue;
                const int position = bytePosition;
                if (position < symbol.scopeStart || position > symbol.scopeEnd) continue;
                signature = symbol.signature;
                const qsizetype left = signature.indexOf(QLatin1Char('('));
                const qsizetype right = signature.indexOf(QLatin1Char(')'), left + 1);
                const QString parameterText = left >= 0 && right > left
                    ? signature.mid(left + 1, right - left - 1) : QString();
                for (const QString &parameter : parameterText.split(
                         QStringLiteral(", "), Qt::SkipEmptyParts)) {
                    parameters.append(parameter);
                }
                break;
            }
            if (callee == QLatin1String("print") && pebble::isBuiltinFunction("print")) {
                signature = QStringLiteral("print(value, ...) -> Void");
                parameters = {QStringLiteral("value, ...")};
            }
        }
        if (signature.isEmpty()) return {};

        int activeParameter = 0;
        int nested = 0;
        for (int index = openIndex + 1; index < tokens.size(); ++index) {
            const LexicalToken &token = tokens.at(index);
            if (token.start >= bytePosition) break;
            if (token.kind == static_cast<int>(pebble::TokenKind::LeftParen)) ++nested;
            else if (token.kind == static_cast<int>(pebble::TokenKind::RightParen)) {
                if (nested == 0) break;
                --nested;
            } else if (nested == 0 &&
                       token.kind == static_cast<int>(pebble::TokenKind::Comma)) {
                ++activeParameter;
            }
        }
        if (!parameters.isEmpty()) {
            activeParameter = qMin(activeParameter, static_cast<int>(parameters.size()) - 1);
        }
        return {callee, signature, parameters, activeParameter};
}

IntelliSenseDefinition IntelliSenseModel::definitionAt(int utf16Position) const {
    const int safePosition = qBound(0, utf16Position, static_cast<int>(source.size()));
    const int bytePosition = static_cast<int>(source.left(safePosition).toUtf8().size());
    const auto tokenIt = std::lower_bound(tokens.cbegin(), tokens.cend(), bytePosition,
        [](const LexicalToken &token, int position) { return token.end < position; });
    if (tokenIt == tokens.cend() || tokenIt->kind !=
        static_cast<int>(pebble::TokenKind::Identifier) ||
        bytePosition < tokenIt->start || bytePosition > tokenIt->end) return {};
    const LexicalToken *identifier = &*tokenIt;
    const int identifierIndex = static_cast<int>(std::distance(tokens.cbegin(), tokenIt));

    QString targetName = identifier->text;
    if (identifierIndex >= 2 &&
        (tokens.at(identifierIndex - 1).kind == static_cast<int>(pebble::TokenKind::Dot) ||
         tokens.at(identifierIndex - 1).kind == static_cast<int>(pebble::TokenKind::Colon)) &&
        tokens.at(identifierIndex - 2).kind == static_cast<int>(pebble::TokenKind::Identifier)) {
        const QString packageName = tokens.at(identifierIndex - 2).text;
        if (!imports.contains(packageName)) return {};
        targetName = packageName + QLatin1Char('.') + identifier->text;
    }

    const Symbol *best = nullptr;
    for (const Symbol &symbol : symbols) {
        if (symbol.name != targetName || symbol.line <= 0 || symbol.column <= 0) continue;
        const bool packageSymbol = symbol.kind == QLatin1String("imported function");
        if (targetName.contains(QLatin1Char('.')) != packageSymbol) continue;
        const bool onDeclaration = identifier->start == symbol.declarationOffset;
        if (!packageSymbol && !onDeclaration && (bytePosition < symbol.declaredAt ||
            bytePosition < symbol.scopeStart || bytePosition > symbol.scopeEnd)) continue;
        if (best == nullptr || symbol.scopeEnd - symbol.scopeStart <
                               best->scopeEnd - best->scopeStart) {
            best = &symbol;
        }
    }
    return best == nullptr ? IntelliSenseDefinition{}
                           : IntelliSenseDefinition{best->path, best->line, best->column};
}
