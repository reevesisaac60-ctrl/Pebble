#include "renderer/model_importer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {

// ---------------------------------------------------------------------------
// File helpers (internal to this translation unit)
// ---------------------------------------------------------------------------

std::string readFileBytes(const std::string& path, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        if (error) *error = "Cannot open file: " + path;
        return "";
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    if (!file.good() && !file.eof()) {
        if (error) *error = "Error reading file: " + path;
        return "";
    }
    return stream.str();
}

// ---------------------------------------------------------------------------
// Quaternion -> Euler conversion (glTF quaternion w,x,y,z -> degrees X,Y,Z)
// Rotation order matches Node3D::localTransform(): rotateZ * rotateY * rotateX
// ---------------------------------------------------------------------------

// Convert glTF quaternion (w, x, y, z) to Euler angles in degrees.
// Returns (roll=X, pitch=Y, yaw=Z) matching Node3D's ZYX extrinsic order.
Vec3 quaternionToEulerDegrees(double qw, double qx, double qy, double qz) {
    const double sqx = qx * qx;
    const double sqy = qy * qy;
    const double sqz = qz * qz;

    // Rotation matrix elements needed for ZYX extraction.
    const double r20 = 2.0 * (qx * qz - qw * qy);      // row 2, col 0
    const double r21 = 2.0 * (qy * qz + qw * qx);      // row 2, col 1
    const double r22 = 1.0 - 2.0 * (sqx + sqy);        // row 2, col 2
    const double r10 = 2.0 * (qx * qy + qw * qz);      // row 1, col 0
    const double r00 = 1.0 - 2.0 * (sqy + sqz);        // row 0, col 0

    const double pitch = std::asin(std::clamp(-r20, -1.0, 1.0));
    const double cosPitch = std::cos(pitch);

    double roll = 0.0;
    double yaw = 0.0;
    if (std::fabs(cosPitch) > 1e-6) {
        roll = std::atan2(r21, r22);
        yaw = std::atan2(r10, r00);
    }

    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    return Vec3(static_cast<float>(roll * kRadToDeg),
                static_cast<float>(pitch * kRadToDeg),
                static_cast<float>(yaw * kRadToDeg));
}

} // namespace

// ===========================================================================
// JsonParser (public, defined outside the anonymous namespace)
// ===========================================================================

void JsonParser::skipWhitespace(State& s) {
    while (s.pos < s.end) {
        char c = *s.pos;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++s.pos;
        } else {
            break;
        }
    }
}

char JsonParser::nextChar(State& s) {
    if (s.pos >= s.end) {
        return '\0';
    }
    return *s.pos++;
}

bool JsonParser::peekChar(State& s, char c) {
    return s.pos < s.end && *s.pos == c;
}

JsonValue JsonParser::parseLiteral(State& s, std::string* error) {
    if (s.pos + 4 <= s.end && std::strncmp(s.pos, "true", 4) == 0) {
        s.pos += 4;
        return JsonValue(true);
    }
    if (s.pos + 5 <= s.end && std::strncmp(s.pos, "false", 5) == 0) {
        s.pos += 5;
        return JsonValue(false);
    }
    if (s.pos + 4 <= s.end && std::strncmp(s.pos, "null", 4) == 0) {
        s.pos += 4;
        return JsonValue(); // null
    }
    if (error) {
        *error = "Unexpected literal at position " +
                 std::to_string(static_cast<int>(s.pos - s.pos));
    }
    return JsonValue();
}

JsonValue JsonParser::parseString(State& s, std::string* error) {
    std::string result;
    // Expect s.pos to point at the opening quote.
    if (s.pos >= s.end || *s.pos != '"') {
        if (error) *error = "Expected string";
        return JsonValue();
    }
    ++s.pos; // consume opening quote
    while (s.pos < s.end) {
        char c = *s.pos++;
        if (c == '"') {
            return JsonValue(result);
        }
        if (c == '\\') {
            if (s.pos >= s.end) {
                if (error) *error = "Unexpected end of string";
                return JsonValue();
            }
            char esc = *s.pos++;
            switch (esc) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    if (s.pos + 4 > s.end) {
                        if (error) *error = "Invalid unicode escape";
                        return JsonValue();
                    }
                    unsigned int code = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = *s.pos++;
                        code <<= 4;
                        if (h >= '0' && h <= '9') code += h - '0';
                        else if (h >= 'a' && h <= 'f') code += h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') code += h - 'A' + 10;
                    }
                    if (code < 0x80) {
                        result += static_cast<char>(code);
                    } else if (code < 0x800) {
                        result += static_cast<char>(0xC0 | (code >> 6));
                        result += static_cast<char>(0x80 | (code & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 | (code >> 12));
                        result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (code & 0x3F));
                    }
                    break;
                }
                default:
                    if (error) *error = "Unknown escape sequence";
                    return JsonValue();
            }
        } else {
            result += c;
        }
    }
    if (error) *error = "Unterminated string";
    return JsonValue();
}

JsonValue JsonParser::parseNumber(State& s, std::string* error) {
    const char* start = s.pos;
    if (s.pos < s.end && (*s.pos == '-' || *s.pos == '+')) {
        ++s.pos;
    }
    while (s.pos < s.end &&
           ((*s.pos >= '0' && *s.pos <= '9') || *s.pos == '.' ||
            *s.pos == 'e' || *s.pos == 'E' || *s.pos == '+' || *s.pos == '-')) {
        ++s.pos;
    }
    size_t len = static_cast<size_t>(s.pos - start);
    std::string numStr(start, len);
    char* endPtr = nullptr;
    double val = std::strtod(numStr.c_str(), &endPtr);
    if (endPtr == numStr.c_str()) {
        if (error) *error = "Invalid number";
        return JsonValue();
    }
    return JsonValue(val);
}

JsonValue JsonParser::parseArray(State& s, std::string* error) {
    JsonValue::Array arr;
    ++s.pos; // consume '['
    skipWhitespace(s);
    if (peekChar(s, ']')) {
        ++s.pos;
        return JsonValue(arr);
    }
    while (s.pos < s.end) {
        skipWhitespace(s);
        arr.push_back(parseValue(s, error));
        if (error && !error->empty()) {
            return JsonValue();
        }
        skipWhitespace(s);
        if (peekChar(s, ']')) {
            ++s.pos;
            return JsonValue(arr);
        }
        if (peekChar(s, ',')) {
            ++s.pos;
            continue;
        }
        if (error) *error = "Expected ',' or ']' in array";
        return JsonValue();
    }
    if (error) *error = "Unterminated array";
    return JsonValue();
}

JsonValue JsonParser::parseObject(State& s, std::string* error) {
    JsonValue::Object obj;
    ++s.pos; // consume '{'
    skipWhitespace(s);
    if (peekChar(s, '}')) {
        ++s.pos;
        return JsonValue(obj);
    }
    while (s.pos < s.end) {
        skipWhitespace(s);
        JsonValue key = parseString(s, error);
        if (error && !error->empty()) {
            return JsonValue();
        }
        skipWhitespace(s);
        if (!peekChar(s, ':')) {
            if (error) *error = "Expected ':' after object key";
            return JsonValue();
        }
        ++s.pos;
        JsonValue val = parseValue(s, error);
        if (error && !error->empty()) {
            return JsonValue();
        }
        obj[key.asString()] = std::move(val);
        skipWhitespace(s);
        if (peekChar(s, '}')) {
            ++s.pos;
            return JsonValue(obj);
        }
        if (peekChar(s, ',')) {
            ++s.pos;
            continue;
        }
        if (error) *error = "Expected ',' or '}' in object";
        return JsonValue();
    }
    if (error) *error = "Unterminated object";
    return JsonValue();
}

JsonValue JsonParser::parseValue(State& s, std::string* error) {
    skipWhitespace(s);
    if (s.pos >= s.end) {
        if (error) *error = "Unexpected end of input";
        return JsonValue();
    }
    char c = *s.pos;
    if (c == '{') {
        return parseObject(s, error);
    }
    if (c == '[') {
        return parseArray(s, error);
    }
    if (c == '"') {
        return parseString(s, error);
    }
    if (c == 't' || c == 'f' || c == 'n') {
        return parseLiteral(s, error);
    }
    if (c == '-' || c == '+' || (c >= '0' && c <= '9')) {
        return parseNumber(s, error);
    }
    if (error) {
        *error = "Unexpected character '" + std::string(1, c) +
                 "' at position " +
                 std::to_string(static_cast<int>(s.pos - s.pos));
    }
    return JsonValue();
}

JsonValue JsonParser::parse(const std::string& text, std::string* error) {
    State s{text.c_str(), text.c_str() + text.size()};
    skipWhitespace(s);
    return parseValue(s, error);
}

// ===========================================================================
// ModelImporter
// ===========================================================================

std::string ModelImporter::readFileBytes(const std::string& path,
                                         std::string* error) {
    return ::readFileBytes(path, error);
}

std::string ModelImporter::resolvePath(const std::string& path,
                                       const std::string& baseDir) const {
    if (!path.empty() && path[0] == '/') {
        return path;
    }
    if (baseDir.empty()) {
        return path;
    }
    std::string base = baseDir;
    while (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    return base + "/" + path;
}

ImportResult ModelImporter::importFile(const std::string& filePath) {
    size_t lastSlash = filePath.find_last_of("/\\");
    std::string baseDir;
    if (lastSlash != std::string::npos) {
        baseDir = filePath.substr(0, lastSlash);
    }

    std::string error;
    std::string fileData = readFileBytes(filePath, &error);
    if (fileData.empty()) {
        ImportResult result;
        result.success = false;
        result.error = error.empty() ? "Empty file: " + filePath : error;
        return result;
    }

    if (fileData.size() >= 4 &&
        fileData[0] == 'g' && fileData[1] == 'l' &&
        fileData[2] == 'T' && fileData[3] == 'F') {
        std::vector<uint8_t> glb(fileData.begin(), fileData.end());
        return importGlb(glb, baseDir);
    }

    JsonValue json = JsonParser::parse(fileData, &error);
    if (json.isNull() && !error.empty()) {
        ImportResult result;
        result.success = false;
        result.error = "Failed to parse JSON: " + error;
        return result;
    }
    return importGltfJson(json, baseDir);
}

ImportResult ModelImporter::importFromMemory(
    const std::vector<uint8_t>& glbData) {
    if (glbData.size() >= 4 &&
        glbData[0] == 'g' && glbData[1] == 'l' &&
        glbData[2] == 'T' && glbData[3] == 'F') {
        return importGlb(glbData, "");
    }
    ImportResult result;
    result.success = false;
    result.error = "Memory buffer is not a glTF binary (.glb) file";
    return result;
}

ImportResult ModelImporter::importGlb(const std::vector<uint8_t>& data,
                                      const std::string& baseDir) {
    ImportResult result;

    if (data.size() < 12) {
        result.success = false;
        result.error = "GLB file too small for header";
        return result;
    }

    uint32_t magic = static_cast<uint32_t>(data[0]) |
                     (static_cast<uint32_t>(data[1]) << 8) |
                     (static_cast<uint32_t>(data[2]) << 16) |
                     (static_cast<uint32_t>(data[3]) << 24);
    if (magic != 0x46546C67) { // "glTF" (little-endian: g=0x67, l=0x6C, T=0x54, F=0x46)
        result.success = false;
        result.error = "Not a glTF binary file (bad magic)";
        return result;
    }

    uint32_t version =
        static_cast<uint32_t>(data[4]) |
        (static_cast<uint32_t>(data[5]) << 8) |
        (static_cast<uint32_t>(data[6]) << 16) |
        (static_cast<uint32_t>(data[7]) << 24);
    if (version != 2) {
        result.success = false;
        result.error = "Unsupported glTF version: " + std::to_string(version);
        return result;
    }

    size_t offset = 12;
    std::string jsonChunk;
    std::vector<uint8_t> binChunk;

    while (offset + 8 <= data.size()) {
        uint32_t chunkLen =
            static_cast<uint32_t>(data[offset]) |
            (static_cast<uint32_t>(data[offset + 1]) << 8) |
            (static_cast<uint32_t>(data[offset + 2]) << 16) |
            (static_cast<uint32_t>(data[offset + 3]) << 24);
        uint32_t chunkType =
            static_cast<uint32_t>(data[offset + 4]) |
            (static_cast<uint32_t>(data[offset + 5]) << 8) |
            (static_cast<uint32_t>(data[offset + 6]) << 16) |
            (static_cast<uint32_t>(data[offset + 7]) << 24);

        offset += 8;
        if (offset + chunkLen > data.size()) {
            result.success = false;
            result.error = "GLB chunk exceeds file size";
            return result;
        }

        if (chunkType == 0x4E4F534A) { // "JSON"
            jsonChunk.assign(reinterpret_cast<const char*>(data.data() + offset),
                             chunkLen);
        } else if (chunkType == 0x004E4942) { // "BIN\0"
            binChunk.assign(data.data() + offset,
                            data.data() + offset + chunkLen);
        }

        offset += chunkLen;
    }

    if (jsonChunk.empty()) {
        result.success = false;
        result.error = "GLB file contains no JSON chunk";
        return result;
    }

    std::string error;
    JsonValue json = JsonParser::parse(jsonChunk, &error);
    if (json.isNull() && !error.empty()) {
        result.success = false;
        result.error = "Failed to parse glTF JSON: " + error;
        return result;
    }

    return importGltfJson(json, baseDir, binChunk);
}

ImportResult ModelImporter::importGltfJson(const JsonValue& json,
                                           const std::string& baseDir,
                                           const std::vector<uint8_t>& binChunk) {
    ImportResult result;

    const JsonValue* asset = json.find("asset");
    if (!asset || !asset->isObject()) {
        result.success = false;
        result.error = "Missing or invalid 'asset' in glTF";
        return result;
    }

    const JsonValue* buffers = json.find("buffers");
    const JsonValue* bufferViews = json.find("bufferViews");
    const JsonValue* accessors = json.find("accessors");
    const JsonValue* meshes = json.find("meshes");

    if (!buffers || !buffers->isArray() || !bufferViews ||
        !bufferViews->isArray() || !accessors || !accessors->isArray() ||
        !meshes || !meshes->isArray()) {
        result.success = false;
        result.error = "Missing required glTF sections (buffers, "
                       "bufferViews, accessors, meshes)";
        return result;
    }

    // Load all buffers into memory.
    std::vector<std::vector<uint8_t>> bufferData;
    bufferData.reserve(buffers->asArray().size());
    for (const auto& bufferJson : buffers->asArray()) {
        std::vector<uint8_t> buf;
        if (bufferJson.has("uri")) {
            std::string uri = bufferJson.find("uri")->asString();
            std::string path = resolvePath(uri, baseDir);
            std::string error;
            std::string fileData = readFileBytes(path, &error);
            if (fileData.empty()) {
                result.success = false;
                result.error = "Failed to load buffer '" + uri + "': " +
                               (error.empty() ? "empty" : error);
                return result;
            }
            buf.assign(fileData.begin(), fileData.end());
        } else if (!binChunk.empty()) {
            buf = binChunk;
        } else {
            result.success = false;
            result.error =
                "Buffer has no URI and no embedded binary data";
            return result;
        }
        bufferData.push_back(std::move(buf));
    }

    // BufferView info for accessor lookups.
    std::vector<BufferViewInfo> bufferViewInfos;
    bufferViewInfos.reserve(bufferViews->asArray().size());
    for (const auto& bvJson : bufferViews->asArray()) {
        BufferViewInfo info;
        info.bufferIndex =
            static_cast<size_t>(bvJson.find("buffer")->asNumber());
        info.byteOffset =
            static_cast<size_t>(bvJson.find("byteOffset")->asNumber());
        info.byteLength =
            static_cast<size_t>(bvJson.find("byteLength")->asNumber());
        bufferViewInfos.push_back(info);
    }

    // Process each mesh.
    for (const auto& meshJson : meshes->asArray()) {
        std::string meshName;
        if (meshJson.has("name")) {
            meshName = meshJson.find("name")->asString();
        } else {
            meshName = "UnnamedMesh";
        }

        const JsonValue* primitives = meshJson.find("primitives");
        if (!primitives || !primitives->isArray()) {
            continue;
        }

        int primitiveIndex = 0;
        for (const auto& primJson : primitives->asArray()) {
            ImportedMesh imported;
            imported.name = meshName +
                (primitives->asArray().size() > 1
                     ? ("_" + std::to_string(primitiveIndex))
                     : "");

            bool ok = extractPrimitive(primJson, json, bufferData,
                                       bufferViewInfos, imported);
            if (!ok) {
                ++primitiveIndex;
                continue;
            }

            result.meshes.push_back(std::move(imported));
            ++primitiveIndex;
        }
    }

    result.success = !result.meshes.empty();
    if (!result.success) {
        result.error = "No importable meshes found in glTF";
    }

    // Parse glTF node hierarchy and transforms.
    importNodes(json, result.nodes);

    // Resolve "idx:N" mesh references to actual ImportedMesh names.
    // A glTF mesh may have multiple primitives, each becoming a separate
    // ImportedMesh named "BaseName" or "BaseName_N". A node referencing
    // mesh index N should use the first primitive's mesh name.
    for (auto& node : result.nodes) {
        if (node.meshName.rfind("idx:", 0) != 0) {
            continue;
        }
        try {
            int meshIndex = std::stoi(node.meshName.substr(4));
            const JsonValue* meshesJson = json.find("meshes");
            std::string resolvedName;
            if (meshesJson && meshesJson->isArray() &&
                meshIndex >= 0 &&
                meshIndex < static_cast<int>(meshesJson->asArray().size())) {
                const JsonValue& meshJson =
                    meshesJson->asArray()[static_cast<size_t>(meshIndex)];
                if (meshJson.has("name")) {
                    resolvedName = meshJson.find("name")->asString();
                } else {
                    resolvedName = "UnnamedMesh";
                }
            }
            if (!resolvedName.empty()) {
                // Prefer the exact base name, else baseName_0 (first
                // primitive of a multi-primitive mesh).
                for (const auto& imported : result.meshes) {
                    if (imported.name == resolvedName ||
                        imported.name == resolvedName + "_0") {
                        resolvedName = imported.name;
                        break;
                    }
                }
                node.meshName = resolvedName;
            } else {
                node.meshName.clear();
            }
        } catch (...) {
            node.meshName.clear();
        }
    }

    return result;
}

bool ModelImporter::importNodes(const JsonValue& gltfJson,
                                std::vector<ImportedNode>& outNodes) const {
    const JsonValue* nodesJson = gltfJson.find("nodes");
    if (!nodesJson || !nodesJson->isArray()) {
        return false;
    }

    const auto& nodeArray = nodesJson->asArray();
    outNodes.reserve(nodeArray.size());

    for (const auto& nodeJson : nodeArray) {
        ImportedNode node;

        if (nodeJson.has("name")) {
            node.name = nodeJson.find("name")->asString();
        } else {
            node.name = "UnnamedNode";
        }

        // Store glTF mesh index as a string prefixed with "idx:" so the
        // caller can resolve it to an ImportedMesh name after all meshes
        // are processed.
        if (nodeJson.has("mesh")) {
            int meshIndex =
                static_cast<int>(nodeJson.find("mesh")->asNumber());
            node.meshName = "idx:" + std::to_string(meshIndex);
        }

        // Translation (default zero).
        if (nodeJson.has("translation")) {
            const JsonValue* trans = nodeJson.find("translation");
            if (trans && trans->isArray() && trans->asArray().size() >= 3) {
                node.position = Vec3(
                    static_cast<float>(trans->asArray()[0].asNumber()),
                    static_cast<float>(trans->asArray()[1].asNumber()),
                    static_cast<float>(trans->asArray()[2].asNumber()));
            }
        }

        // Rotation as quaternion [x, y, z, w] -> convert to Euler degrees.
        if (nodeJson.has("rotation")) {
            const JsonValue* rot = nodeJson.find("rotation");
            if (rot && rot->isArray() && rot->asArray().size() >= 4) {
                double qx = rot->asArray()[0].asNumber();
                double qy = rot->asArray()[1].asNumber();
                double qz = rot->asArray()[2].asNumber();
                double qw = rot->asArray()[3].asNumber();
                node.rotation = quaternionToEulerDegrees(qw, qx, qy, qz);
            }
        }

        // Scale (default one).
        if (nodeJson.has("scale")) {
            const JsonValue* sc = nodeJson.find("scale");
            if (sc && sc->isArray() && sc->asArray().size() >= 3) {
                node.scale = Vec3(
                    static_cast<float>(sc->asArray()[0].asNumber()),
                    static_cast<float>(sc->asArray()[1].asNumber()),
                    static_cast<float>(sc->asArray()[2].asNumber()));
            }
        }

        // Matrix-based transform (overrides T/R/S when present).
        if (nodeJson.has("matrix")) {
            const JsonValue* mat = nodeJson.find("matrix");
            if (mat && mat->isArray() && mat->asArray().size() >= 16) {
                const auto& m = mat->asArray();
                // Extract translation from column 4.
                node.position = Vec3(
                    static_cast<float>(m[12].asNumber()),
                    static_cast<float>(m[13].asNumber()),
                    static_cast<float>(m[14].asNumber()));
                // Decompose scale from column lengths.
                Vec3 col0(static_cast<float>(m[0].asNumber()),
                          static_cast<float>(m[1].asNumber()),
                          static_cast<float>(m[2].asNumber()));
                Vec3 col1(static_cast<float>(m[4].asNumber()),
                          static_cast<float>(m[5].asNumber()),
                          static_cast<float>(m[6].asNumber()));
                Vec3 col2(static_cast<float>(m[8].asNumber()),
                          static_cast<float>(m[9].asNumber()),
                          static_cast<float>(m[10].asNumber()));
                node.scale = Vec3(std::sqrt(col0.lengthSquared()),
                                  std::sqrt(col1.lengthSquared()),
                                  std::sqrt(col2.lengthSquared()));
                // Rotation from matrix is complex; leave as identity when
                // matrix is used.
            }
        }

        outNodes.push_back(std::move(node));
    }

    // Second pass: set parentIndex for each node based on children arrays.
    for (size_t i = 0; i < nodeArray.size(); ++i) {
        const JsonValue* nodeJson = &nodeArray[i];
        if (!nodeJson->has("children")) continue;
        const JsonValue* children = nodeJson->find("children");
        if (!children || !children->isArray()) continue;
        for (const auto& childJson : children->asArray()) {
            if (!childJson.isNumber()) continue;
            int childIdx = static_cast<int>(childJson.asNumber());
            if (childIdx >= 0 &&
                childIdx < static_cast<int>(outNodes.size())) {
                outNodes[static_cast<size_t>(childIdx)].parentIndex =
                    static_cast<int>(i);
            }
        }
    }

    return true;
}

bool ModelImporter::extractPrimitive(
    const JsonValue& primitiveJson, const JsonValue& gltfJson,
    const std::vector<std::vector<uint8_t>>& bufferData,
    const std::vector<BufferViewInfo>& bufferViewInfos, ImportedMesh& out) {
    const JsonValue* indicesAccessor =
        primitiveJson.find("indices");
    int indicesAccessorIndex = -1;
    if (indicesAccessor && indicesAccessor->isNumber()) {
        indicesAccessorIndex = static_cast<int>(indicesAccessor->asNumber());
    }

    const JsonValue* attributes = primitiveJson.find("attributes");
    if (!attributes || !attributes->isObject()) {
        return false;
    }

    const JsonValue* posAttr = attributes->find("POSITION");
    if (!posAttr || !posAttr->isNumber()) {
        return false;
    }
    int positionAccessorIndex = static_cast<int>(posAttr->asNumber());

    const JsonValue* normalAttr = attributes->find("NORMAL");
    int normalAccessorIndex = -1;
    if (normalAttr && normalAttr->isNumber()) {
        normalAccessorIndex = static_cast<int>(normalAttr->asNumber());
    }

    const JsonValue* texCoordAttr = attributes->find("TEXCOORD_0");
    int texCoordAccessorIndex = -1;
    if (texCoordAttr && texCoordAttr->isNumber()) {
        texCoordAccessorIndex = static_cast<int>(texCoordAttr->asNumber());
    }

    // Read position data.
    std::vector<float> positions;
    if (!readAccessor(positionAccessorIndex, gltfJson, bufferData,
                      bufferViewInfos, positions)) {
        return false;
    }
    if (positions.size() % 3 != 0) {
        return false;
    }
    int vertexCount = static_cast<int>(positions.size() / 3);

    // Read normal data (optional).
    std::vector<float> normals;
    if (normalAccessorIndex >= 0) {
        if (!readAccessor(normalAccessorIndex, gltfJson, bufferData,
                          bufferViewInfos, normals)) {
            return false;
        }
        if (static_cast<int>(normals.size()) != vertexCount * 3) {
            normals.clear();
        }
    }

    // Read texcoord data (optional).
    std::vector<float> texCoords;
    if (texCoordAccessorIndex >= 0) {
        if (!readAccessor(texCoordAccessorIndex, gltfJson, bufferData,
                          bufferViewInfos, texCoords)) {
            return false;
        }
        if (static_cast<int>(texCoords.size()) != vertexCount * 2) {
            texCoords.clear();
        }
    }

    // Read index data (optional).
    std::vector<unsigned int> indices;
    if (indicesAccessorIndex >= 0) {
        if (!readAccessorIndices(indicesAccessorIndex, gltfJson, bufferData,
                                 bufferViewInfos, indices)) {
            return false;
        }
    } else {
        indices.reserve(vertexCount);
        for (int i = 0; i < vertexCount; ++i) {
            indices.push_back(static_cast<unsigned int>(i));
        }
    }

    // Build interleaved vertex data: position(3) + normal(3) + texCoord(2).
    out.data.vertices.clear();
    out.data.vertices.reserve(vertexCount * 8);
    out.data.indices = std::move(indices);
    out.data.indexCount = static_cast<int>(out.data.indices.size());
    out.data.vertexCount = vertexCount;

    for (int i = 0; i < vertexCount; ++i) {
        out.data.vertices.push_back(positions[i * 3]);
        out.data.vertices.push_back(positions[i * 3 + 1]);
        out.data.vertices.push_back(positions[i * 3 + 2]);

        if (!normals.empty()) {
            out.data.vertices.push_back(normals[i * 3]);
            out.data.vertices.push_back(normals[i * 3 + 1]);
            out.data.vertices.push_back(normals[i * 3 + 2]);
        } else {
            out.data.vertices.push_back(0.0f);
            out.data.vertices.push_back(1.0f);
            out.data.vertices.push_back(0.0f);
        }

        if (!texCoords.empty()) {
            out.data.vertices.push_back(texCoords[i * 2]);
            out.data.vertices.push_back(texCoords[i * 2 + 1]);
        } else {
            out.data.vertices.push_back(0.0f);
            out.data.vertices.push_back(0.0f);
        }
    }

    return true;
}

bool ModelImporter::readAccessor(
    int accessorIndex, const JsonValue& gltfJson,
    const std::vector<std::vector<uint8_t>>& bufferData,
    const std::vector<BufferViewInfo>& bufferViewInfos,
    std::vector<float>& out) {
    const JsonValue* accessors = gltfJson.find("accessors");
    if (!accessors || !accessors->isArray()) {
        return false;
    }
    const auto& accArray = accessors->asArray();
    if (accessorIndex < 0 ||
        accessorIndex >= static_cast<int>(accArray.size())) {
        return false;
    }

    const JsonValue& accJson = accArray[static_cast<size_t>(accessorIndex)];
    if (!accJson.isObject()) {
        return false;
    }

    int bufferViewIndex = -1;
    if (accJson.has("bufferView")) {
        bufferViewIndex =
            static_cast<int>(accJson.find("bufferView")->asNumber());
    } else {
        // No bufferView: accessor is initialized to zeros.
        int count = static_cast<int>(accJson.find("count")->asNumber());
        std::string type = accJson.find("type")->asString();
        int components = 0;
        if (type == "SCALAR") components = 1;
        else if (type == "VEC2") components = 2;
        else if (type == "VEC3") components = 3;
        else if (type == "VEC4") components = 4;
        out.assign(static_cast<size_t>(count * components), 0.0f);
        return true;
    }

    if (bufferViewIndex < 0 ||
        bufferViewIndex >= static_cast<int>(bufferViewInfos.size())) {
        return false;
    }

    const BufferViewInfo& bv =
        bufferViewInfos[static_cast<size_t>(bufferViewIndex)];
    if (bv.bufferIndex >= bufferData.size()) {
        return false;
    }

    int count = static_cast<int>(accJson.find("count")->asNumber());
    std::string type = accJson.find("type")->asString();
    int components = 0;
    if (type == "SCALAR") components = 1;
    else if (type == "VEC2") components = 2;
    else if (type == "VEC3") components = 3;
    else if (type == "VEC4") components = 4;
    else {
        return false;
    }

    int componentType = 5126; // default FLOAT
    if (accJson.has("componentType")) {
        componentType =
            static_cast<int>(accJson.find("componentType")->asNumber());
    }

    size_t byteOffset = 0;
    if (accJson.has("byteOffset")) {
        byteOffset =
            static_cast<size_t>(accJson.find("byteOffset")->asNumber());
    }

    const std::vector<uint8_t>& buf = bufferData[bv.bufferIndex];
    size_t dataStart = bv.byteOffset + byteOffset;

    size_t compSize = 0;
    switch (componentType) {
        case 5120: compSize = 1; break; // BYTE
        case 5121: compSize = 1; break; // UNSIGNED_BYTE
        case 5122: compSize = 2; break; // SHORT
        case 5123: compSize = 2; break; // UNSIGNED_SHORT
        case 5125: compSize = 4; break; // UNSIGNED_INT
        case 5126: compSize = 4; break; // FLOAT
        default: return false;
    }

    size_t stride = compSize * components;
    size_t totalBytes = static_cast<size_t>(count) * stride;
    if (dataStart + totalBytes > buf.size()) {
        return false;
    }

    out.clear();
    out.reserve(static_cast<size_t>(count) * components);

    for (int i = 0; i < count; ++i) {
        size_t offset = dataStart + static_cast<size_t>(i) * stride;
        for (int c = 0; c < components; ++c) {
            size_t compOffset = offset + static_cast<size_t>(c) * compSize;
            float value = 0.0f;
            switch (componentType) {
                case 5120: // BYTE
                    value =
                        static_cast<float>(static_cast<int8_t>(buf[compOffset]));
                    break;
                case 5121: // UNSIGNED_BYTE
                    value = static_cast<float>(buf[compOffset]);
                    break;
                case 5122: { // SHORT
                    int16_t v;
                    std::memcpy(&v, &buf[compOffset], 2);
                    value = static_cast<float>(v);
                    break;
                }
                case 5123: { // UNSIGNED_SHORT
                    uint16_t v;
                    std::memcpy(&v, &buf[compOffset], 2);
                    value = static_cast<float>(v);
                    break;
                }
                case 5125: { // UNSIGNED_INT
                    uint32_t v;
                    std::memcpy(&v, &buf[compOffset], 4);
                    value = static_cast<float>(v);
                    break;
                }
                case 5126: { // FLOAT
                    float v;
                    std::memcpy(&v, &buf[compOffset], 4);
                    value = v;
                    break;
                }
            }
            out.push_back(value);
        }
    }
    return true;
}

bool ModelImporter::readAccessorIndices(
    int accessorIndex, const JsonValue& gltfJson,
    const std::vector<std::vector<uint8_t>>& bufferData,
    const std::vector<BufferViewInfo>& bufferViewInfos,
    std::vector<unsigned int>& out) {
    const JsonValue* accessors = gltfJson.find("accessors");
    if (!accessors || !accessors->isArray()) {
        return false;
    }
    const auto& accArray = accessors->asArray();
    if (accessorIndex < 0 ||
        accessorIndex >= static_cast<int>(accArray.size())) {
        return false;
    }

    const JsonValue& accJson = accArray[static_cast<size_t>(accessorIndex)];
    if (!accJson.isObject()) {
        return false;
    }

    int bufferViewIndex = -1;
    if (accJson.has("bufferView")) {
        bufferViewIndex =
            static_cast<int>(accJson.find("bufferView")->asNumber());
    } else {
        return false;
    }

    if (bufferViewIndex < 0 ||
        bufferViewIndex >= static_cast<int>(bufferViewInfos.size())) {
        return false;
    }

    const BufferViewInfo& bv =
        bufferViewInfos[static_cast<size_t>(bufferViewIndex)];
    if (bv.bufferIndex >= bufferData.size()) {
        return false;
    }

    int count = static_cast<int>(accJson.find("count")->asNumber());
    int componentType = 5126;
    if (accJson.has("componentType")) {
        componentType =
            static_cast<int>(accJson.find("componentType")->asNumber());
    }

    size_t byteOffset = 0;
    if (accJson.has("byteOffset")) {
        byteOffset =
            static_cast<size_t>(accJson.find("byteOffset")->asNumber());
    }

    const std::vector<uint8_t>& buf = bufferData[bv.bufferIndex];
    size_t dataStart = bv.byteOffset + byteOffset;

    size_t compSize = 0;
    switch (componentType) {
        case 5120: compSize = 1; break;
        case 5121: compSize = 1; break;
        case 5122: compSize = 2; break;
        case 5123: compSize = 2; break;
        case 5125: compSize = 4; break;
        default: return false;
    }

    size_t totalBytes = static_cast<size_t>(count) * compSize;
    if (dataStart + totalBytes > buf.size()) {
        return false;
    }

    out.clear();
    out.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        size_t offset = dataStart + static_cast<size_t>(i) * compSize;
        unsigned int value = 0;
        switch (componentType) {
            case 5120: // BYTE
                value =
                    static_cast<unsigned int>(static_cast<int8_t>(buf[offset]));
                break;
            case 5121: // UNSIGNED_BYTE
                value = static_cast<unsigned int>(buf[offset]);
                break;
            case 5122: { // SHORT
                int16_t v;
                std::memcpy(&v, &buf[offset], 2);
                value = static_cast<unsigned int>(v);
                break;
            }
            case 5123: { // UNSIGNED_SHORT
                uint16_t v;
                std::memcpy(&v, &buf[offset], 2);
                value = v;
                break;
            }
            case 5125: { // UNSIGNED_INT
                uint32_t v;
                std::memcpy(&v, &buf[offset], 4);
                value = v;
                break;
            }
        }
        out.push_back(value);
    }
    return true;
}
