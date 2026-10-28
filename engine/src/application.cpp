#include "application.hpp"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <epoxy/gl.h>

#ifdef PEBBLE_ENGINE_HAS_IMGUI
#include <imgui.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include <imgui_internal.h>
#pragma GCC diagnostic pop
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#endif

#include "scene/node3d.hpp"
#include "scene/part.hpp"
#include "scene/camera3d.hpp"
#include "scene/directional_light.hpp"
#include "scene/point_light.hpp"
#include "scene/world_environment.hpp"
#include "renderer/material.hpp"
#include "editor_camera.hpp"

namespace {

constexpr int kDefaultWindowWidth = 1280;
constexpr int kDefaultWindowHeight = 720;
constexpr float kMaxDeltaTime = 0.25f;
constexpr char kGlslVersion[] = "#version 330 core";

std::string sdlError(const char* operation) {
    const char* err = SDL_GetError();
    std::string result;
    result += operation;
    result += " failed";
    if (err != nullptr && err[0] != '\0') {
        result += ": ";
        result += err;
    } else {
        result += " (no SDL error string available)";
    }
    return result;
}

}

namespace {

namespace fs = std::filesystem;

bool validateTexturePath(const std::string& assetsDir,
                         const std::string& relativePath) {
    if (relativePath.empty()) {
        return true;
    }
    std::error_code ec;
    const fs::path full = fs::path(assetsDir) / relativePath;
    if (!fs::exists(full, ec) || !fs::is_regular_file(full, ec)) {
        return false;
    }
    return full.extension() == ".ppm";
}

bool validateSkyboxPath(const std::string& assetsDir,
                        const std::string& relativePath) {
    if (relativePath.empty()) {
        return true;
    }
    std::error_code ec;
    const fs::path dir = fs::path(assetsDir) / relativePath;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        return false;
    }
    const char* faces[6] = {
        "posx.ppm", "negx.ppm",
        "posy.ppm", "negy.ppm",
        "posz.ppm", "negz.ppm",
    };
    for (const char* face : faces) {
        const fs::path facePath = dir / face;
        if (!fs::exists(facePath, ec) || !fs::is_regular_file(facePath, ec)) {
            return false;
        }
    }
    return true;
}

}

Application::Application() = default;

Application::~Application() {
    shutdown();
}

std::string Application::resolveAssetsDirectory(
    const std::vector<std::string>& ) const {
    if (const char* envOverride = std::getenv("PEBBLE_ENGINE_ASSETS")) {
        return envOverride;
    }

    const std::vector<std::string> candidates = [] {
        std::vector<std::string> result;
        if (const char* basePath = SDL_GetBasePath()) {
            result.emplace_back(std::string(basePath) + "assets/shaders");
            result.emplace_back(std::string(basePath) + "../assets/shaders");
            result.emplace_back(std::string(basePath) + "../share/pebble/assets/shaders");
            result.emplace_back(std::string(basePath) + "assets");
            result.emplace_back(std::string(basePath) + "../assets");
            result.emplace_back(std::string(basePath) + "../share/pebble/assets");
        }
        result.emplace_back("engine/assets/shaders");
        result.emplace_back("assets/shaders");
        result.emplace_back("engine/assets");
        result.emplace_back("assets");
        return result;
    }();

    std::error_code errorCode;
    for (const std::string& candidate : candidates) {
        if (std::filesystem::is_directory(candidate, errorCode)) {
            return candidate;
        }
    }

    return "";
}

std::string Application::resolveScenesDirectory() const {
    std::vector<std::string> candidates;
    if (const char* basePath = SDL_GetBasePath()) {
        candidates.emplace_back(std::string(basePath) + "scenes");
        candidates.emplace_back(std::string(basePath) + "../scenes");
        candidates.emplace_back(std::string(basePath) + "../share/pebble/scenes");
    }
    candidates.emplace_back("scenes");
    candidates.emplace_back("engine/scenes");
    return candidates.empty() ? "scenes" : candidates[0];
}

std::string Application::resolveModelsDirectory() const {
    std::vector<std::string> candidates;
    if (const char* basePath = SDL_GetBasePath()) {
        candidates.emplace_back(std::string(basePath) + "models");
        candidates.emplace_back(std::string(basePath) + "../assets/models");
        candidates.emplace_back(std::string(basePath) + "../../assets/models");
        candidates.emplace_back(std::string(basePath) + "../share/pebble/assets/models");
    }
    candidates.emplace_back("assets/models");
    candidates.emplace_back("engine/assets/models");
    candidates.emplace_back("../assets/models");
    candidates.emplace_back("../../assets/models");

    std::error_code errorCode;
    for (const std::string& candidate : candidates) {
        if (std::filesystem::is_directory(candidate, errorCode)) {
            return candidate;
        }
    }

    return candidates.empty() ? "assets/models" : candidates[0];
}

void Application::logConsole(const std::string& message) {
    m_console.push_back(message);
    while (m_console.size() > 200) {
        m_console.erase(m_console.begin());
    }
}

void Application::loadStarterScene() {
    m_scene.clear();
    m_scene.setName("Starter");
    const NodeId id = m_scene.createNode("Part", "Cube");
    if (Node3D* node = dynamic_cast<Node3D*>(m_scene.find(id))) {
        node->position = Vec3(0.0f, 0.0f, 0.0f);
        node->rotation = Vec3(0.0f, 0.0f, 0.0f);
        node->scale = Vec3(1.0f, 1.0f, 1.0f);
    }
    m_selection.select(id);
    logConsole("Loaded starter scene with cube part");
}

void Application::saveScene(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code errorCode;
    const fs::path filePath = path;
    if (filePath.has_parent_path()) {
        fs::create_directories(filePath.parent_path(), errorCode);
    }

    std::vector<SerializedMaterial> serialized;
    const auto& materialNames = m_renderer.materialManager().names();
    for (const auto& name : materialNames) {
        if (const Material* mat = m_renderer.materialManager().find(name)) {
            SerializedMaterial sm;
            sm.name = mat->name;
            sm.baseColor[0] = mat->baseColor.x;
            sm.baseColor[1] = mat->baseColor.y;
            sm.baseColor[2] = mat->baseColor.z;
            sm.metallic = mat->metallic;
            sm.roughness = mat->roughness;
            sm.hasTexture = mat->hasTexture;
            sm.texturePath = mat->texturePath;
            serialized.push_back(sm);
        }
    }
    m_scene.setSerializedMaterials(serialized);

    if (!m_scene.save(path)) {
        logConsole("Failed to save scene: " + path);
        return;
    }
    logConsole("Saved scene to " + path);
}

void Application::loadScene(const std::string& path) {
    if (!m_scene.load(path)) {
        logConsole("Failed to load scene: " + path);
        return;
    }
    m_selection.clear();

    m_renderer.materialManager().clear();
    for (const auto& sm : m_scene.getSerializedMaterials()) {
        Material mat(sm.name);
        mat.baseColor = Vec3(sm.baseColor[0], sm.baseColor[1], sm.baseColor[2]);
        mat.metallic = sm.metallic;
        mat.roughness = sm.roughness;
        mat.hasTexture = sm.hasTexture;
        mat.texturePath = sm.texturePath;
        m_renderer.materialManager().registerMaterial(mat);
    }

    logConsole("Loaded scene from " + path);
}

bool Application::startPlaySession() {
    if (m_playState != PlayState::Stopped) {
        return false;
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    m_playSnapshotPath = (std::filesystem::temp_directory_path() /
        ("pebble-play-snapshot-" + std::to_string(stamp) + ".pebscene")).string();
    m_selectionBeforePlay = m_selection.selected().value_or(NodeId{0});
    saveScene(m_playSnapshotPath);

    std::error_code ec;
    if (!std::filesystem::is_regular_file(m_playSnapshotPath, ec)) {
        m_playSnapshotPath.clear();
        logConsole("Play failed: could not create a scene snapshot");
        return false;
    }

    m_playState = PlayState::Playing;
    m_playElapsed = 0.0f;
    m_animate = true;
    logConsole("Play session started (scene snapshot saved)");
    return true;
}

void Application::stopPlaySession() {
    if (m_playState == PlayState::Stopped) {
        return;
    }

    const std::string snapshot = m_playSnapshotPath;
    m_playState = PlayState::Stopped;
    m_animate = false;
    m_playElapsed = 0.0f;

    if (!snapshot.empty()) {
        loadScene(snapshot);
        std::error_code ec;
        std::filesystem::remove(snapshot, ec);
        m_playSnapshotPath.clear();
        if (m_selectionBeforePlay.value != 0 && m_scene.has(m_selectionBeforePlay)) {
            m_selection.select(m_selectionBeforePlay);
        }
    }
    m_selectionBeforePlay = NodeId{0};
    logConsole("Play session stopped; editor scene restored");
}

RendererCamera Application::computeRendererCamera(float aspect) const {
    RendererCamera result;
    result.valid = false;

    if (m_playState != PlayState::Stopped) {
        if (const Camera3D* camera = m_scene.activeCamera()) {
            if (camera->isValid()) {
                result.view = camera->viewMatrix();
                result.projection = camera->projectionMatrix(aspect);
                result.position = camera->worldPosition();
                result.valid = true;
                return result;
            }
            std::fprintf(stderr,
                         "Renderer: active camera has invalid projection "
                         "parameters (fov=%.1f, near=%.3f, far=%.3f); "
                         "using editor camera fallback\n",
                         camera->fieldOfView, camera->nearPlane, camera->farPlane);
        }
    }

    result.view = m_editorCamera.viewMatrix();
    result.projection = m_editorCamera.projectionMatrix(aspect);
    result.position = m_editorCamera.position();
    result.valid = true;
    return result;
}

RendererLighting Application::computeRendererLighting() const {
    RendererLighting lighting;

    bool hasDirLight = false;
    for (const NodeId id : m_scene.nodeIds()) {
        const Node* node = m_scene.find(id);
        if (node == nullptr) {
            continue;
        }
        if (const DirectionalLight* light =
                dynamic_cast<const DirectionalLight*>(node)) {
            lighting.direction = light->direction;
            lighting.color = light->color;
            lighting.intensity = light->intensity;
            hasDirLight = true;
            break;
        }
    }
    if (!hasDirLight) {
        lighting.direction = Vec3(0.0f, 0.0f, -1.0f);
        lighting.color = Vec3(1.0f, 1.0f, 1.0f);
        lighting.intensity = 1.0f;
    }

    int index = 0;
    for (const NodeId id : m_scene.nodeIds()) {
        if (index >= kMaxPointLights) {
            break;
        }
        const Node* node = m_scene.find(id);
        if (node == nullptr) {
            continue;
        }
        if (const PointLight* light = dynamic_cast<const PointLight*>(node)) {
            lighting.position[index] = light->worldPosition();
            lighting.pointColor[index] = light->color;
            lighting.pointIntensity[index] = light->intensity;
            lighting.pointRange[index] = light->range;
            ++index;
        }
    }
    lighting.count = index;
    return lighting;
}

RendererEnvironment Application::computeRendererEnvironment() const {
    RendererEnvironment environment;
    bool found = false;
    for (const NodeId id : m_scene.nodeIds()) {
        const Node* node = m_scene.find(id);
        if (node == nullptr) {
            continue;
        }
        if (const WorldEnvironment* env =
                dynamic_cast<const WorldEnvironment*>(node)) {
            environment.fogColor = env->fogColor;
            environment.fogStart = env->fogStart;
            environment.fogEnd = env->fogEnd;
            environment.fogEnabled = (env->fogEnd > env->fogStart);
            environment.skyMode = env->skyMode;
            environment.skyColor = env->skyColor;
            environment.horizonColor = env->horizonColor;
            environment.skyIntensity = env->skyIntensity;
            environment.environmentRotation = env->environmentRotation;
            environment.skyboxTexture = 0;
            environment.skyboxPath = env->skyboxPath;
            found = true;
            break;
        }
    }
    if (!found) {

        environment.fogColor = Vec3(0.04f, 0.045f, 0.06f);
        environment.fogStart = 2.0f;
        environment.fogEnd = 15.0f;
        environment.fogEnabled = true;
        environment.skyMode = 0;
        environment.skyColor = Vec3(0.4f, 0.6f, 0.85f);
        environment.horizonColor = Vec3(0.8f, 0.85f, 0.9f);
        environment.skyIntensity = 1.0f;
        environment.environmentRotation = 0.0f;
        environment.skyboxTexture = 0;
        environment.skyboxPath = "";
    }

    if (environment.skyMode == 1 && environment.skyboxTexture == 0 &&
        !environment.skyboxPath.empty()) {
        std::string skyboxPath = m_assetsDirectory + "/" +
                                 environment.skyboxPath;
        environment.skyboxTexture =
            m_renderer.loadSkyboxCubemap(skyboxPath);
        if (environment.skyboxTexture == 0) {
            std::fprintf(stderr,
                         "Sky: failed to load skybox from '%s', falling "
                         "back to procedural sky\n",
                         skyboxPath.c_str());
            environment.skyMode = 0;
        }
    }

    return environment;
}

void Application::updateEditorCameraInput(float deltaTime) {
    const auto* keyboardState = SDL_GetKeyboardState(nullptr);
    const ImGuiIO& io = ImGui::GetIO();
    if (m_mouseCaptured || (m_viewportHovered && !io.WantCaptureKeyboard)) {
        const Vec3 forward = m_editorCamera.forward();
        const Vec3 hRight = m_editorCamera.horizontalRight();
        const Vec3 worldUp(0.0f, 1.0f, 0.0f);
        Vec3 move(0.0f, 0.0f, 0.0f);
        if (m_mouseCaptured) {
            if (keyboardState[SDL_SCANCODE_W]) {
                move = move + forward;
            }
            if (keyboardState[SDL_SCANCODE_S]) {
                move = move - forward;
            }
            if (keyboardState[SDL_SCANCODE_A]) {
                move = move - hRight;
            }
            if (keyboardState[SDL_SCANCODE_D]) {
                move = move + hRight;
            }
            if (keyboardState[SDL_SCANCODE_Q]) {
                move = move - worldUp;
            }
            if (keyboardState[SDL_SCANCODE_E]) {
                move = move + worldUp;
            }
        } else if (m_viewportHovered && !io.WantCaptureKeyboard) {
            if (keyboardState[SDL_SCANCODE_W]) {
                move = move + forward;
            }
            if (keyboardState[SDL_SCANCODE_S]) {
                move = move - forward;
            }
            if (keyboardState[SDL_SCANCODE_A]) {
                move = move - hRight;
            }
            if (keyboardState[SDL_SCANCODE_D]) {
                move = move + hRight;
            }
        }
        if (move.lengthSquared() > 0.0f) {
            move = move.normalized();
            const float speed = m_cameraSpeed *
                                (keyboardState[SDL_SCANCODE_LSHIFT] ||
                                 keyboardState[SDL_SCANCODE_RSHIFT] ? 3.0f : 1.0f);
            m_editorCamera.move(move * speed * deltaTime);
        }
    }
}

static Vec3 projectToScreen(const Vec3& worldPos, const Mat4& view,
                            const Mat4& projection, int width, int height) {

    const float ex =
        view(0, 0) * worldPos.x + view(0, 1) * worldPos.y +
        view(0, 2) * worldPos.z + view(0, 3);
    const float ey =
        view(1, 0) * worldPos.x + view(1, 1) * worldPos.y +
        view(1, 2) * worldPos.z + view(1, 3);
    const float ez =
        view(2, 0) * worldPos.x + view(2, 1) * worldPos.y +
        view(2, 2) * worldPos.z + view(2, 3);
    const float ew =
        view(3, 0) * worldPos.x + view(3, 1) * worldPos.y +
        view(3, 2) * worldPos.z + view(3, 3);

    const float cx =
        projection(0, 0) * ex + projection(0, 1) * ey +
        projection(0, 2) * ez + projection(0, 3) * ew;
    const float cy =
        projection(1, 0) * ex + projection(1, 1) * ey +
        projection(1, 2) * ez + projection(1, 3) * ew;
    const float cz =
        projection(2, 0) * ex + projection(2, 1) * ey +
        projection(2, 2) * ez + projection(2, 3) * ew;
    const float cw =
        projection(3, 0) * ex + projection(3, 1) * ey +
        projection(3, 2) * ez + projection(3, 3) * ew;

    if (!std::isfinite(cw) || cw <= 1.0e-6f) {
        const float invalid = std::numeric_limits<float>::quiet_NaN();
        return Vec3(invalid, invalid, invalid);
    }

    const float ndcX = cx / cw;
    const float ndcY = cy / cw;
    const float ndcZ = cz / cw;
    return Vec3(
        (ndcX * 0.5f + 0.5f) * static_cast<float>(width),
        (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height),
        ndcZ);
}

void Application::drawViewportGizmo(Node3D* node3d, const RendererCamera& camera,
                                    int viewportWidth, int viewportHeight,
                                    ImVec2 viewportImageOrigin) {
#ifdef PEBBLE_ENGINE_HAS_IMGUI
    if (node3d == nullptr || m_gizmoMode == GizmoMode::None ||
        viewportWidth <= 0 || viewportHeight <= 0) {
        return;
    }

    const Vec3 worldPos = node3d->worldPosition();
    const Vec3 projectedOrigin = projectToScreen(
        worldPos, camera.view, camera.projection, viewportWidth, viewportHeight);
    if (!std::isfinite(projectedOrigin.x) ||
        !std::isfinite(projectedOrigin.y) ||
        !std::isfinite(projectedOrigin.z) || projectedOrigin.z < -1.0f ||
        projectedOrigin.z > 1.0f) {
        m_gizmoActive = false;
        m_gizmoAxis = -1;
        return;
    }

    const ImVec2 origin(viewportImageOrigin.x + projectedOrigin.x,
                        viewportImageOrigin.y + projectedOrigin.y);
    const Vec3 axes[] = {Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1)};
    ImVec2 directions[3] = {ImVec2(1, 0), ImVec2(0, -1), ImVec2(0.7071f, -0.7071f)};
    for (int i = 0; i < 3; ++i) {
        const Vec3 p = projectToScreen(worldPos + axes[i], camera.view,
                                       camera.projection, viewportWidth,
                                       viewportHeight);
        if (std::isfinite(p.x) && std::isfinite(p.y)) {
            float dx = p.x - projectedOrigin.x;
            float dy = p.y - projectedOrigin.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.001f) {
                directions[i] = ImVec2(dx / len, dy / len);
            }
        }
    }

    constexpr float kAxisLength = 78.0f;
    constexpr float kHitRadius = 10.0f;
    const ImU32 colors[3] = {IM_COL32(244, 88, 88, 255),
                             IM_COL32(105, 220, 135, 255),
                             IM_COL32(100, 165, 255, 255)};
    const ImU32 activeColor = IM_COL32(255, 224, 105, 255);
    ImVec2 ends[3];
    for (int i = 0; i < 3; ++i) {
        ends[i] = ImVec2(origin.x + directions[i].x * kAxisLength,
                         origin.y + directions[i].y * kAxisLength);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 clipMin = viewportImageOrigin;
    const ImVec2 clipMax(viewportImageOrigin.x + static_cast<float>(viewportWidth),
                         viewportImageOrigin.y + static_cast<float>(viewportHeight));
    drawList->PushClipRect(clipMin, clipMax, true);

    if (m_gizmoMode == GizmoMode::Rotate) {

        const float radii[3] = {32.0f, 45.0f, 58.0f};
        for (int i = 0; i < 3; ++i) {
            const ImU32 color = m_gizmoAxis == i ? activeColor : colors[i];
            drawList->AddCircle(origin, radii[i], color, 64, 2.5f);
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            const ImU32 color = m_gizmoAxis == i ? activeColor : colors[i];
            drawList->AddLine(origin, ends[i], color, 2.5f);
            if (m_gizmoMode == GizmoMode::Scale) {

                drawList->AddRectFilled(ImVec2(ends[i].x - 5, ends[i].y - 5),
                                         ImVec2(ends[i].x + 5, ends[i].y + 5), color);
            } else {
                drawList->AddCircleFilled(ends[i], 5.5f, color, 20);
            }
        }
        drawList->AddCircleFilled(origin, 4.0f, IM_COL32(245, 245, 250, 255));
    }
    drawList->PopClipRect();

    const ImVec2 mouse = ImGui::GetMousePos();
    const bool mouseInViewport = mouse.x >= clipMin.x && mouse.x <= clipMax.x &&
                                 mouse.y >= clipMin.y && mouse.y <= clipMax.y;
    int hoveredAxis = -1;
    if (mouseInViewport) {
        if (m_gizmoMode == GizmoMode::Rotate) {
            const float dx = mouse.x - origin.x;
            const float dy = mouse.y - origin.y;
            const float radius = std::sqrt(dx * dx + dy * dy);
            const float radii[3] = {32.0f, 45.0f, 58.0f};
            float best = 9.0f;
            for (int i = 0; i < 3; ++i) {
                const float error = std::abs(radius - radii[i]);
                if (error < best) { best = error; hoveredAxis = i; }
            }
        } else {
            float best = kHitRadius;
            for (int i = 0; i < 3; ++i) {
                const float vx = ends[i].x - origin.x;
                const float vy = ends[i].y - origin.y;
                const float wx = mouse.x - origin.x;
                const float wy = mouse.y - origin.y;
                const float denom = vx * vx + vy * vy;
                const float t = denom > 0.0f
                    ? std::clamp((wx * vx + wy * vy) / denom, 0.0f, 1.0f) : 0.0f;
                const float dx = wx - t * vx;
                const float dy = wy - t * vy;
                const float distance = std::sqrt(dx * dx + dy * dy);
                if (distance < best) { best = distance; hoveredAxis = i; }
            }
        }
    }

    if (!m_gizmoActive && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        mouseInViewport && hoveredAxis >= 0) {
        m_gizmoActive = true;
        m_gizmoAxis = hoveredAxis;
        m_gizmoStartPosition = node3d->position;
        m_gizmoStartRotation = node3d->rotation;
        m_gizmoStartScale = node3d->scale;
    }
    if (m_gizmoActive) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            m_gizmoActive = false;
            m_gizmoAxis = -1;
        } else {
            const ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            float amount = 0.0f;
            if (m_gizmoMode == GizmoMode::Move) {
                const ImVec2 dir = directions[m_gizmoAxis];
                const float len2 = dir.x * dir.x + dir.y * dir.y;
                amount = len2 > 0.000001f ? (delta.x * dir.x + delta.y * dir.y) / len2 : 0.0f;
                amount *= kAxisLength * 0.01f;
                if (m_gizmoSnapTranslation > 0.0f)
                    amount = std::round(amount / m_gizmoSnapTranslation) * m_gizmoSnapTranslation;
                node3d->position = m_gizmoStartPosition;
                if (m_gizmoAxis == 0) node3d->position.x += amount;
                else if (m_gizmoAxis == 1) node3d->position.y += amount;
                else node3d->position.z += amount;
            } else if (m_gizmoMode == GizmoMode::Rotate) {
                amount = delta.x * 0.5f;
                if (m_gizmoSnapRotation > 0.0f)
                    amount = std::round(amount / m_gizmoSnapRotation) * m_gizmoSnapRotation;
                node3d->rotation = m_gizmoStartRotation;
                if (m_gizmoAxis == 0) node3d->rotation.x += amount;
                else if (m_gizmoAxis == 1) node3d->rotation.y += amount;
                else node3d->rotation.z += amount;
            } else if (m_gizmoMode == GizmoMode::Scale) {
                amount = delta.x * 0.005f;
                if (m_gizmoSnapScale > 0.0f)
                    amount = std::round(amount / m_gizmoSnapScale) * m_gizmoSnapScale;
                node3d->scale = m_gizmoStartScale;
                const float startValue = m_gizmoAxis == 0 ? m_gizmoStartScale.x : (m_gizmoAxis == 1 ? m_gizmoStartScale.y : m_gizmoStartScale.z);
                const float value = std::max(0.01f, startValue + amount);
                if (m_gizmoAxis == 0) node3d->scale.x = value;
                else if (m_gizmoAxis == 1) node3d->scale.y = value;
                else node3d->scale.z = value;
            }
        }
    }
#else
    (void)node3d; (void)camera; (void)viewportWidth; (void)viewportHeight;
    (void)viewportImageOrigin;
#endif
}

void Application::focusSelectedNode() {
    if (!m_selection.selected().has_value()) {
        logConsole("Focus: no node selected");
        return;
    }
    const Node* node = m_scene.find(m_selection.selected().value());
    if (node == nullptr) {
        logConsole("Focus: selected node not found");
        return;
    }

    Vec3 target(0.0f, 0.0f, 0.0f);
    if (const Node3D* node3d = dynamic_cast<const Node3D*>(node)) {
        target = node3d->worldPosition();
    }
    m_editorCamera.lookAt(target, 6.0f);
    logConsole("Focused camera on node " + node->name());
}

void Application::openAssetBrowser(const std::string& target,
                                   NodeId nodeId,
                                   const std::vector<std::string>& extensions) {
    m_assetBrowserTarget = target;
    m_assetBrowserTargetNode = nodeId;
    m_assetBrowser.setRootDirectory(m_assetsDirectory);
    std::vector<std::string> exts = extensions;
    if (exts.empty()) {
        exts = {".ppm"};
    }
    m_assetBrowser.open("Select Asset", exts);
    m_showAssetBrowser = true;
}

void Application::applyAssetBrowserSelection() {

}

void Application::deleteSelectedNode() {
    if (!m_selection.selected().has_value()) {
        return;
    }
    NodeId id = m_selection.selected().value();
    if (!m_scene.has(id)) {
        m_selection.clear();
        return;
    }
    const Node* node = m_scene.find(id);
    if (node == nullptr) {
        m_selection.clear();
        return;
    }

    if (node->name().empty()) {
        logConsole("Cannot delete protected node");
        return;
    }
    if (m_scene.destroyNode(id)) {
        m_selection.clear();
        logConsole("Deleted node: " + node->name());
    }
}

void Application::duplicateSelectedNode() {
    if (!m_selection.selected().has_value()) {
        return;
    }
    NodeId id = m_selection.selected().value();
    if (!m_scene.has(id)) {
        m_selection.clear();
        return;
    }
    const Node* src = m_scene.find(id);
    if (src == nullptr) {
        m_selection.clear();
        return;
    }

    NodeId newId = m_scene.createNode(src->type(), src->name() + " (copy)");
    if (newId.value == 0) {
        logConsole("Duplicate failed: unsupported node type");
        return;
    }
    Node* dst = m_scene.find(newId);
    if (dst == nullptr) {
        return;
    }
    if (const Node3D* src3d = dynamic_cast<const Node3D*>(src)) {
        if (Node3D* dst3d = dynamic_cast<Node3D*>(dst)) {
            dst3d->position = src3d->position;
            dst3d->rotation = src3d->rotation;
            dst3d->scale = src3d->scale;
        }
    }
    if (const Camera3D* srcCam = dynamic_cast<const Camera3D*>(src)) {
        if (Camera3D* dstCam = dynamic_cast<Camera3D*>(dst)) {
            dstCam->fieldOfView = srcCam->fieldOfView;
            dstCam->nearPlane = srcCam->nearPlane;
            dstCam->farPlane = srcCam->farPlane;
        }
    }
    if (const DirectionalLight* srcL = dynamic_cast<const DirectionalLight*>(src)) {
        if (DirectionalLight* dstL = dynamic_cast<DirectionalLight*>(dst)) {
            dstL->direction = srcL->direction;
            dstL->color = srcL->color;
            dstL->intensity = srcL->intensity;
        }
    }
    if (const PointLight* srcL = dynamic_cast<const PointLight*>(src)) {
        if (PointLight* dstL = dynamic_cast<PointLight*>(dst)) {
            dstL->position = srcL->position;
            dstL->color = srcL->color;
            dstL->intensity = srcL->intensity;
            dstL->range = srcL->range;
        }
    }
    if (const WorldEnvironment* srcE = dynamic_cast<const WorldEnvironment*>(src)) {
        if (WorldEnvironment* dstE = dynamic_cast<WorldEnvironment*>(dst)) {
            dstE->skyMode = srcE->skyMode;
            dstE->skyColor = srcE->skyColor;
            dstE->horizonColor = srcE->horizonColor;
            dstE->skyIntensity = srcE->skyIntensity;
            dstE->environmentRotation = srcE->environmentRotation;
            dstE->skyboxPath = srcE->skyboxPath;
            dstE->fogColor = srcE->fogColor;
            dstE->fogStart = srcE->fogStart;
            dstE->fogEnd = srcE->fogEnd;
        }
    }
    if (const Part* srcP = dynamic_cast<const Part*>(src)) {
        if (Part* dstP = dynamic_cast<Part*>(dst)) {
            dstP->meshName = srcP->meshName;
            dstP->materialName = srcP->materialName;
        }
    }

    if (src->hasParent()) {
        m_scene.setParent(newId, src->parent()->id());
    }
    m_selection.select(newId);
    logConsole("Duplicated node: " + src->name());
}

bool Application::initSDLOnly() {
    if (m_sdlInitialized) {
        return true;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "Application: %s\n", sdlError("SDL_Init(SDL_INIT_VIDEO)").c_str());
        return false;
    }

    m_sdlInitialized = true;
    int version = SDL_GetVersion();
    std::fprintf(stdout, "Application: SDL %d.%d.%d initialized (SDL_INIT_VIDEO)\n",
                 SDL_VERSIONNUM_MAJOR(version), SDL_VERSIONNUM_MINOR(version),
                 SDL_VERSIONNUM_MICRO(version));

    int numDrivers = SDL_GetNumVideoDrivers();
    std::fprintf(stdout, "Application: %d video driver(s) available:\n", numDrivers);
    for (int i = 0; i < numDrivers; ++i) {
        const char* driver = SDL_GetVideoDriver(i);
        std::fprintf(stdout, "  [%d] %s\n", i, driver ? driver : "(null)");
    }

    return true;
}

bool Application::init(const std::vector<std::string>& commandLine) {

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "Application: %s\n",
                     sdlError("SDL_Init(SDL_INIT_VIDEO)").c_str());
        return false;
    }
    m_sdlInitialized = true;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_window = SDL_CreateWindow("Pebble Engine",
                                kDefaultWindowWidth, kDefaultWindowHeight,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (m_window == nullptr) {
        std::fprintf(stderr, "Application: %s\n",
                     sdlError("SDL_CreateWindow").c_str());
        shutdown();
        return false;
    }
    m_windowCreated = true;

    m_glContext = SDL_GL_CreateContext(m_window);
    if (m_glContext == nullptr) {
        std::fprintf(stderr, "Application: %s\n",
                     sdlError("SDL_GL_CreateContext").c_str());
        shutdown();
        return false;
    }
    m_glContextCreated = true;
    SDL_GL_MakeCurrent(m_window, m_glContext);

    int major = 0;
    int minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    const char* versionString =
        reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* rendererString =
        reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    std::fprintf(stdout, "Application: OpenGL %d.%d (%s, %s)\n", major, minor,
                 versionString ? versionString : "(unknown)",
                 rendererString ? rendererString : "(unknown)");
    std::fprintf(stderr, "Application: SDL video driver: %s\n",
                 SDL_GetCurrentVideoDriver());
    logConsole(std::string("OpenGL ") + (versionString ? versionString : "(unknown)")
               + " (" + (rendererString ? rendererString : "(unknown)") + ")");

    if (major < 3 || (major == 3 && minor < 3)) {
        std::fprintf(stderr,
                     "Application: OpenGL 3.3 Core is required, context is %d.%d\n",
                     major, minor);
        shutdown();
        return false;
    }

    SDL_GL_SetSwapInterval(1);
    SDL_GetWindowSize(m_window, &m_drawableWidth, &m_drawableHeight);

#ifdef PEBBLE_ENGINE_HAS_IMGUI

    IMGUI_CHECKVERSION();
    m_imguiContext = ImGui::CreateContext();
    if (m_imguiContext == nullptr) {
        std::fprintf(stderr, "Application: ImGui::CreateContext() returned null\n");
        shutdown();
        return false;
    }
    m_imguiCreated = true;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    ImGui::StyleColorsDark();

    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 7.0f;
        style.ChildRounding = 5.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 5.0f;
        style.ScrollbarRounding = 7.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.WindowPadding = ImVec2(10.0f, 9.0f);
        style.FramePadding = ImVec2(9.0f, 5.0f);
        style.CellPadding = ImVec2(7.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 7.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
        style.IndentSpacing = 16.0f;
        style.ScrollbarSize = 13.0f;
        ImVec4* c = style.Colors;
        c[ImGuiCol_Text]                  = ImVec4(0.88f, 0.88f, 0.91f, 1.00f);
        c[ImGuiCol_TextDisabled]          = ImVec4(0.48f, 0.49f, 0.55f, 1.00f);
        c[ImGuiCol_WindowBg]              = ImVec4(0.055f, 0.058f, 0.070f, 1.00f);
        c[ImGuiCol_ChildBg]               = ImVec4(0.060f, 0.063f, 0.076f, 1.00f);
        c[ImGuiCol_PopupBg]               = ImVec4(0.075f, 0.078f, 0.092f, 0.98f);
        c[ImGuiCol_Border]                = ImVec4(0.16f, 0.17f, 0.20f, 0.85f);
        c[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_FrameBg]               = ImVec4(0.105f, 0.108f, 0.128f, 1.00f);
        c[ImGuiCol_FrameBgHovered]        = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
        c[ImGuiCol_FrameBgActive]         = ImVec4(0.20f, 0.19f, 0.25f, 1.00f);
        c[ImGuiCol_TitleBg]               = ImVec4(0.065f, 0.067f, 0.080f, 1.00f);
        c[ImGuiCol_TitleBgActive]         = ImVec4(0.085f, 0.086f, 0.105f, 1.00f);
        c[ImGuiCol_MenuBarBg]             = ImVec4(0.070f, 0.072f, 0.086f, 1.00f);
        c[ImGuiCol_ScrollbarBg]           = ImVec4(0.045f, 0.047f, 0.058f, 0.70f);
        c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.31f, 0.30f, 0.38f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.43f, 0.39f, 0.55f, 1.00f);
        c[ImGuiCol_CheckMark]             = ImVec4(0.73f, 0.66f, 0.95f, 1.00f);
        c[ImGuiCol_SliderGrab]            = ImVec4(0.61f, 0.54f, 0.83f, 1.00f);
        c[ImGuiCol_SliderGrabActive]      = ImVec4(0.76f, 0.69f, 0.98f, 1.00f);
        c[ImGuiCol_Button]                = ImVec4(0.14f, 0.13f, 0.18f, 1.00f);
        c[ImGuiCol_ButtonHovered]         = ImVec4(0.23f, 0.21f, 0.30f, 1.00f);
        c[ImGuiCol_ButtonActive]          = ImVec4(0.31f, 0.27f, 0.42f, 1.00f);
        c[ImGuiCol_Header]                = ImVec4(0.16f, 0.15f, 0.21f, 1.00f);
        c[ImGuiCol_HeaderHovered]         = ImVec4(0.22f, 0.20f, 0.29f, 1.00f);
        c[ImGuiCol_HeaderActive]          = ImVec4(0.29f, 0.25f, 0.39f, 1.00f);
        c[ImGuiCol_Separator]             = ImVec4(0.16f, 0.17f, 0.20f, 1.00f);
        c[ImGuiCol_SeparatorHovered]      = ImVec4(0.49f, 0.43f, 0.67f, 0.78f);
        c[ImGuiCol_SeparatorActive]       = ImVec4(0.65f, 0.57f, 0.88f, 1.00f);
        c[ImGuiCol_ResizeGrip]            = ImVec4(0.43f, 0.36f, 0.62f, 0.22f);
        c[ImGuiCol_ResizeGripHovered]     = ImVec4(0.57f, 0.48f, 0.79f, 0.67f);
        c[ImGuiCol_ResizeGripActive]      = ImVec4(0.70f, 0.61f, 0.94f, 0.95f);
        c[ImGuiCol_Tab]                   = ImVec4(0.095f, 0.096f, 0.115f, 1.00f);
        c[ImGuiCol_TabHovered]            = ImVec4(0.25f, 0.22f, 0.34f, 1.00f);
        c[ImGuiCol_TabSelected]           = ImVec4(0.16f, 0.14f, 0.22f, 1.00f);
        c[ImGuiCol_TabSelectedOverline]   = ImVec4(0.69f, 0.61f, 0.91f, 1.00f);
        c[ImGuiCol_DockingPreview]        = ImVec4(0.60f, 0.52f, 0.82f, 0.45f);
        c[ImGuiCol_DockingEmptyBg]        = ImVec4(0.045f, 0.047f, 0.058f, 1.00f);
        c[ImGuiCol_TableHeaderBg]         = ImVec4(0.11f, 0.11f, 0.14f, 1.00f);
        c[ImGuiCol_TableBorderStrong]     = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
        c[ImGuiCol_TableBorderLight]      = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
        c[ImGuiCol_TextSelectedBg]        = ImVec4(0.39f, 0.33f, 0.55f, 0.45f);
        c[ImGuiCol_NavHighlight]          = ImVec4(0.68f, 0.60f, 0.90f, 1.00f);
    }

    {
        const float displayScale = SDL_GetWindowDisplayScale(m_window);
        if (displayScale > 0.0f) {
            m_imguiDisplayScale = displayScale;
            io.FontGlobalScale = displayScale;
            ImGui::GetStyle().ScaleAllSizes(displayScale);
        }
    }

    if (!ImGui_ImplSDL3_InitForOpenGL(m_window, m_glContext)) {
        std::fprintf(stderr, "Application: ImGui_ImplSDL3_InitForOpenGL() returned false\n");
        shutdown();
        return false;
    }
    m_imguiSDL3Initialized = true;

    if (!ImGui_ImplOpenGL3_Init(kGlslVersion)) {
        std::fprintf(stderr, "Application: ImGui_ImplOpenGL3_Init() returned false\n");
        shutdown();
        return false;
    }
    m_imguiOpenGL3Initialized = true;

    logConsole("Dear ImGui docking shell initialized");
#endif

    m_assetsDirectory = resolveAssetsDirectory(commandLine);
    if (m_assetsDirectory.empty()) {
        std::fprintf(stderr,
                     "Application: could not locate the assets/shaders directory\n");
        shutdown();
        return false;
    }
    logConsole("Assets: " + m_assetsDirectory);

    m_scenesDirectory = resolveScenesDirectory();
    std::error_code ec;
    std::filesystem::create_directories(m_scenesDirectory, ec);

    loadStarterScene();

    if (!m_renderer.init(m_assetsDirectory)) {
        std::fprintf(stderr, "Application: renderer initialization failed\n");
        shutdown();
        return false;
    }
    m_rendererInitialized = true;
    m_renderer.resize(m_drawableWidth, m_drawableHeight);

    std::string modelsDir = resolveModelsDirectory();
    std::vector<ImportedNode> importedNodes;
    int modelCount = m_renderer.loadGlbModels(modelsDir, importedNodes);
    if (modelCount > 0) {
        logConsole("Loaded " + std::to_string(modelCount) +
                   " model asset(s) from " + modelsDir);

        std::vector<NodeId> nodeIds;
        nodeIds.reserve(importedNodes.size());
        for (const auto& nodeData : importedNodes) {
            if (nodeData.meshName.empty()) {
                continue;
            }
            const NodeId id = m_scene.createNode("Part", nodeData.name);
            if (id.value == 0) {
                continue;
            }
            if (Part* part = dynamic_cast<Part*>(m_scene.find(id))) {
                part->meshName = nodeData.meshName;
                part->position = nodeData.position;
                part->rotation = nodeData.rotation;
                part->scale = nodeData.scale;
            }
            nodeIds.push_back(id);
        }

        for (size_t i = 0; i < importedNodes.size(); ++i) {
            const auto& nodeData = importedNodes[i];
            if (nodeData.meshName.empty()) {
                continue;
            }
            if (nodeData.parentIndex >= 0 &&
                nodeData.parentIndex < static_cast<int>(nodeIds.size())) {
                NodeId childId = nodeIds[i];
                NodeId parentId = nodeIds[static_cast<size_t>(nodeData.parentIndex)];

                if (parentId.value != 0 &&
                    m_scene.find(parentId) != nullptr) {
                    m_scene.setParent(childId, parentId);
                }
            }
        }
        logConsole("Created " + std::to_string(nodeIds.size()) +
                   " part node(s) from imported models");
    } else {
        logConsole("No GLB model assets found in " + modelsDir);
    }

    logConsole("Application initialized");
    return true;
}

void Application::shutdown() {
    if (m_playState != PlayState::Stopped) {
        stopPlaySession();
    }

    if (m_rendererInitialized) {
        m_renderer.shutdown();
        m_rendererInitialized = false;
    }

#ifdef PEBBLE_ENGINE_HAS_IMGUI
    if (m_imguiOpenGL3Initialized) {
        ImGui_ImplOpenGL3_Shutdown();
        m_imguiOpenGL3Initialized = false;
    }
    if (m_imguiSDL3Initialized) {
        ImGui_ImplSDL3_Shutdown();
        m_imguiSDL3Initialized = false;
    }
    if (m_imguiCreated) {
        ImGui::DestroyContext();
        m_imguiCreated = false;
        m_imguiContext = nullptr;
    }
#endif

    if (m_glContextCreated) {
        SDL_GL_DestroyContext(m_glContext);
        m_glContextCreated = false;
        m_glContext = nullptr;
    }
    if (m_windowCreated) {
        SDL_DestroyWindow(m_window);
        m_windowCreated = false;
        m_window = nullptr;
    }

    if (m_sdlInitialized) {
        SDL_Quit();
        m_sdlInitialized = false;
    }
}

bool Application::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
#ifdef PEBBLE_ENGINE_HAS_IMGUI
        ImGui_ImplSDL3_ProcessEvent(&event);
#endif
        ImGuiIO& io = ImGui::GetIO();
        const bool wantCaptureMouse = io.WantCaptureMouse;

        switch (event.type) {
        case SDL_EVENT_QUIT:
            m_running = false;
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            m_running = false;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (m_mouseCaptured) {
                m_mouseCaptured = false;
                SDL_SetWindowRelativeMouseMode(m_window, false);
            }
            break;
        case SDL_EVENT_KEY_DOWN: {
            if (event.key.key == SDLK_ESCAPE) {
                if (m_gizmoActive) {
                    m_gizmoActive = false;
                    m_gizmoAxis = -1;
                } else if (m_selection.selected().has_value()) {
                    m_selection.clear();
                    logConsole("Selection cleared");
                } else {
                    m_running = false;
                }
            }

            {
                const bool textEntryActive = io.WantTextInput || ImGui::IsAnyItemActive();
                if (!textEntryActive && m_playState == PlayState::Stopped && !m_mouseCaptured) {
                    const bool ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
                    if (event.key.key == SDLK_DELETE ||
                        event.key.key == SDLK_BACKSPACE) {
                        deleteSelectedNode();
                    } else if (ctrl && event.key.key == SDLK_D) {
                        duplicateSelectedNode();
                    } else if (event.key.key == SDLK_F) {
                        focusSelectedNode();
                    } else if (event.key.key == SDLK_Q || event.key.scancode == SDL_SCANCODE_Q) {
                        m_gizmoMode = GizmoMode::None;
                        m_gizmoActive = false;
                        m_gizmoAxis = -1;
                    } else if (event.key.key == SDLK_W || event.key.scancode == SDL_SCANCODE_W) {
                        m_gizmoMode = GizmoMode::Move;
                        m_gizmoActive = false;
                        m_gizmoAxis = -1;
                    } else if (event.key.key == SDLK_E || event.key.scancode == SDL_SCANCODE_E) {
                        m_gizmoMode = GizmoMode::Rotate;
                        m_gizmoActive = false;
                        m_gizmoAxis = -1;
                    } else if (event.key.key == SDLK_R || event.key.scancode == SDL_SCANCODE_R) {
                        m_gizmoMode = GizmoMode::Scale;
                        m_gizmoActive = false;
                        m_gizmoAxis = -1;
                    }
                }
            }
            break;
        }
        case SDL_EVENT_WINDOW_RESIZED:
            SDL_GetWindowSize(m_window, &m_drawableWidth, &m_drawableHeight);
            break;
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:

            if (m_imguiCreated) {
                const float displayScale = SDL_GetWindowDisplayScale(m_window);
                if (displayScale > 0.0f && std::isfinite(displayScale)) {
                    const float ratio = displayScale / std::max(0.25f, m_imguiDisplayScale);
                    ImGui::GetStyle().ScaleAllSizes(ratio);
                    m_imguiDisplayScale = displayScale;
                    ImGui::GetIO().FontGlobalScale = displayScale;
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {

            const bool pointerOverSceneImage = m_viewportImageBoundsValid &&
                event.button.x >= m_viewportImageLeft &&
                event.button.x <= m_viewportImageRight &&
                event.button.y >= m_viewportImageTop &&
                event.button.y <= m_viewportImageBottom;
            if (event.button.button == SDL_BUTTON_RIGHT && pointerOverSceneImage) {
                m_mouseCaptured = true;
                if (!SDL_SetWindowRelativeMouseMode(m_window, true)) {
                    std::fprintf(stderr,
                                 "Application: failed to enable relative mouse "
                                 "mode: %s\n",
                                 SDL_GetError());
                    m_mouseCaptured = false;
                }
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_RIGHT && m_mouseCaptured) {
                m_mouseCaptured = false;
                SDL_SetWindowRelativeMouseMode(m_window, false);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (m_mouseCaptured) {
                const float pitchSign = m_invertMouseY ? -1.0f : 1.0f;

                m_editorCamera.rotate(
                    event.motion.xrel * m_mouseSensitivity,
                    -event.motion.yrel * m_mouseSensitivity * pitchSign);
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (!wantCaptureMouse && m_viewportHovered) {

                constexpr float kWheelSpeed = 0.5f;
                const float scroll =
                    static_cast<float>(event.wheel.y) * kWheelSpeed;
                m_editorCamera.move(m_editorCamera.forward() * scroll);
            }
            break;
        default:
            break;
        }
    }
    return true;
}

void Application::drawHierarchyNode(Node* node) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (m_selection.selected().has_value() &&
        m_selection.selected().value() == node->id()) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool open = ImGui::TreeNodeEx(
        reinterpret_cast<void*>(static_cast<uintptr_t>(node->id().value)),
        flags, "%s", node->name().c_str());

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        NodeId dragId = node->id();
        ImGui::SetDragDropPayload("REPARENT_NODE", &dragId, sizeof(NodeId));
        ImGui::Text("Reparent: %s", node->name().c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::IsItemClicked()) {
        m_selection.select(node->id());
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("REPARENT_NODE")) {
            NodeId draggedId =
                *static_cast<const NodeId*>(payload->Data);
            if (draggedId != node->id() &&
                m_scene.setParent(draggedId, node->id())) {
                logConsole("Reparented node under " + node->name());

                if (m_selection.selected().has_value() &&
                    m_selection.selected().value() == draggedId) {
                    m_selection.select(draggedId);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
            if (m_scene.destroyNode(node->id())) {
                if (m_selection.selected().has_value() &&
                    m_selection.selected().value() == node->id()) {
                    m_selection.clear();
                }
                logConsole("Deleted node and its children");
            }
        }
        if (ImGui::MenuItem("Rename")) {
            m_renameTarget = node->id();
            std::snprintf(m_renameBuffer, sizeof(m_renameBuffer), "%s",
                          node->name().c_str());
            m_showRenameModal = true;
        }
        ImGui::EndPopup();
    }

    if (open) {
        for (const NodeId childId : node->children()) {
            if (Node* child = m_scene.find(childId)) {
                drawHierarchyNode(child);
            }
        }
        ImGui::TreePop();
    }
}

void Application::buildEditorUI(float deltaTime) {
#ifdef PEBBLE_ENGINE_HAS_IMGUI

    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    const ImGuiID dockspaceId = ImGui::GetID("PebbleMainDockSpace");

    if (!m_layoutBuilt) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspaceId, mainViewport->WorkSize);

        ImGuiID workspace = dockspaceId;
        ImGuiID toolbar = ImGui::DockBuilderSplitNode(workspace, ImGuiDir_Up, 0.06f, nullptr, &workspace);
        ImGuiID left = ImGui::DockBuilderSplitNode(workspace, ImGuiDir_Left, 0.18f, nullptr, &workspace);
        ImGuiID right = ImGui::DockBuilderSplitNode(workspace, ImGuiDir_Right, 0.22f, nullptr, &workspace);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(workspace, ImGuiDir_Down, 0.22f, nullptr, &workspace);
        ImGuiID leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.4f, nullptr, &left);
        ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.28f, nullptr, &right);

        ImGui::DockBuilderDockWindow("Toolbar", toolbar);
        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Project", leftBottom);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Console", bottom);
        ImGui::DockBuilderDockWindow("Viewport", workspace);
        ImGui::DockBuilderDockWindow("Play Controls", rightBottom);
        ImGui::DockBuilderFinish(dockspaceId);
        m_layoutBuilt = true;
    }

    ImGui::DockSpaceOverViewport(dockspaceId, mainViewport, ImGuiDockNodeFlags_PassthruCentralNode, nullptr);

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            const bool authoringEnabled = m_playState == PlayState::Stopped;
            if (ImGui::MenuItem("New Scene", nullptr, false, authoringEnabled)) {
                loadStarterScene();
            }
            if (ImGui::MenuItem("Save Scene...", nullptr, false, authoringEnabled)) {
                m_showSaveModal = true;
                std::snprintf(m_fileNameBuffer, sizeof(m_fileNameBuffer), "scene.peb");
            }
            if (ImGui::MenuItem("Load Scene...", nullptr, false, authoringEnabled)) {
                m_showLoadModal = true;
                std::snprintf(m_fileNameBuffer, sizeof(m_fileNameBuffer), "scene.peb");
            }
            if (ImGui::MenuItem("Exit")) {
                m_running = false;
            }
            ImGui::EndMenu();
        }

        const bool authoringEnabled = m_playState == PlayState::Stopped;
        if (ImGui::BeginMenu("Home")) {
            if (ImGui::MenuItem("Select", "Q")) m_gizmoMode = GizmoMode::None;
            if (ImGui::MenuItem("Move", "W")) m_gizmoMode = GizmoMode::Move;
            if (ImGui::MenuItem("Rotate", "E")) m_gizmoMode = GizmoMode::Rotate;
            if (ImGui::MenuItem("Scale", "R")) m_gizmoMode = GizmoMode::Scale;
            ImGui::Separator();
            if (ImGui::MenuItem("Focus Selection", "F", false,
                                m_selection.selected().has_value())) {
                focusSelectedNode();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Scene")) {
            if (ImGui::MenuItem("Add Node...", nullptr, false, authoringEnabled)) {
                m_showAddNodeModal = true;
                m_addNodeTypeIndex = 0;
                m_newNodeNameBuffer[0] = '\0';
            }
            if (ImGui::MenuItem("Rename Selected...", nullptr, false,
                                authoringEnabled && m_selection.selected().has_value())) {
                if (Node* node = m_scene.find(m_selection.selected().value_or(NodeId{}))) {
                    std::snprintf(m_renameBuffer, sizeof(m_renameBuffer), "%s",
                                  node->name().c_str());
                    m_renameTarget = node->id();
                    m_showRenameModal = true;
                }
            }
            if (ImGui::MenuItem("Delete Selected", "Delete", false,
                                authoringEnabled && m_selection.selected().has_value())) {
                deleteSelectedNode();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Model")) {
            if (ImGui::MenuItem("Create Cube", nullptr, false, authoringEnabled)) {
                const NodeId id = m_scene.createNode("Part", "Cube");
                m_selection.select(id);
                logConsole("Created cube");
            }
            if (ImGui::MenuItem("Create Sphere", nullptr, false, authoringEnabled)) {
                const NodeId id = m_scene.createNode("Part", "Sphere");
                if (Part* part = dynamic_cast<Part*>(m_scene.find(id))) part->meshName = "sphere";
                m_selection.select(id);
                logConsole("Created sphere");
            }
            if (ImGui::MenuItem("Create Plane", nullptr, false, authoringEnabled)) {
                const NodeId id = m_scene.createNode("Part", "Plane");
                if (Part* part = dynamic_cast<Part*>(m_scene.find(id))) part->meshName = "plane";
                m_selection.select(id);
                logConsole("Created plane");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Test")) {
            if (ImGui::MenuItem("Play", nullptr, false,
                                m_playState == PlayState::Stopped)) startPlaySession();
            if (ImGui::MenuItem("Pause / Resume", nullptr, false,
                                m_playState != PlayState::Stopped)) {
                if (m_playState == PlayState::Playing) {
                    m_playState = PlayState::Paused;
                    m_animate = false;
                } else {
                    m_playState = PlayState::Playing;
                    m_animate = true;
                }
            }
            if (ImGui::MenuItem("Stop", nullptr, false,
                                m_playState != PlayState::Stopped)) stopPlaySession();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Demo window", nullptr, &m_showDemoWindow);
            if (ImGui::MenuItem("Reset Layout")) {
                ImGui::DockBuilderRemoveNode(dockspaceId);
                ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
                const ImGuiViewport* vp = ImGui::GetMainViewport();
                ImGui::DockBuilderSetNodeSize(dockspaceId, vp->WorkSize);
                ImGuiID workspace = dockspaceId;
                ImGuiID toolbar = ImGui::DockBuilderSplitNode(workspace, ImGuiDir_Up, 0.06f, nullptr, &workspace);
                ImGuiID left = ImGui::DockBuilderSplitNode(workspace, ImGuiDir_Left, 0.18f, nullptr, &workspace);
                ImGuiID right = ImGui::DockBuilderSplitNode(workspace, ImGuiDir_Right, 0.22f, nullptr, &workspace);
                ImGuiID bottom = ImGui::DockBuilderSplitNode(workspace, ImGuiDir_Down, 0.22f, nullptr, &workspace);
                ImGuiID leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.4f, nullptr, &left);
                ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.28f, nullptr, &right);
                ImGui::DockBuilderDockWindow("Toolbar", toolbar);
                ImGui::DockBuilderDockWindow("Hierarchy", left);
                ImGui::DockBuilderDockWindow("Project", leftBottom);
                ImGui::DockBuilderDockWindow("Inspector", right);
                ImGui::DockBuilderDockWindow("Console", bottom);
                ImGui::DockBuilderDockWindow("Viewport", workspace);
                ImGui::DockBuilderDockWindow("Play Controls", rightBottom);
                ImGui::DockBuilderFinish(dockspaceId);
                m_layoutBuilt = true;
                logConsole("Editor layout reset to defaults");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("Toolbar", nullptr, toolbarFlags);
    const bool authoringAllowed = m_playState == PlayState::Stopped;
    ImGui::TextColored(ImVec4(0.75f, 0.68f, 0.96f, 1.0f), "PEBBLE");
    ImGui::SameLine(0.0f, 18.0f);
    if (ImGui::Button("Select [Q]")) m_gizmoMode = GizmoMode::None;
    ImGui::SameLine();
    if (ImGui::Button("Move [W]")) m_gizmoMode = GizmoMode::Move;
    ImGui::SameLine();
    if (ImGui::Button("Rotate [E]")) m_gizmoMode = GizmoMode::Rotate;
    ImGui::SameLine();
    if (ImGui::Button("Scale [R]")) m_gizmoMode = GizmoMode::Scale;
    ImGui::SameLine(0.0f, 14.0f);
    ImGui::BeginDisabled(!authoringAllowed);
    if (ImGui::Button("+ Add Node")) {
        m_showAddNodeModal = true;
        m_addNodeTypeIndex = 0;
        m_newNodeNameBuffer[0] = '\0';
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, 14.0f);
    const bool toolbarPlaying = m_playState == PlayState::Playing;
    if (m_playState == PlayState::Stopped) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.30f, 0.22f, 1.0f));
        if (ImGui::Button("Play")) startPlaySession();
        ImGui::PopStyleColor();
    } else if (toolbarPlaying) {
        if (ImGui::Button("Pause")) { m_playState = PlayState::Paused; m_animate = false; logConsole("Play session paused"); }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) stopPlaySession();
    } else {
        if (ImGui::Button("Resume")) { m_playState = PlayState::Playing; m_animate = true; logConsole("Play session resumed"); }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) stopPlaySession();
    }
    ImGui::SameLine(0.0f, 16.0f);
    ImGui::TextDisabled("%s  /  %s", m_scene.name().c_str(),
        m_playState == PlayState::Stopped ? "EDITING" : (toolbarPlaying ? "PLAYING" : "PAUSED"));
    ImGui::End();

    ImGui::Begin("Play Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    const bool stopped = m_playState == PlayState::Stopped;
    const bool playing = m_playState == PlayState::Playing;
    if (stopped) {
        if (ImGui::Button("Play")) {
            startPlaySession();
        }
    } else {
        if (playing) {
            if (ImGui::Button("Pause")) {
                m_playState = PlayState::Paused;
                m_animate = false;
                logConsole("Play session paused");
            }
        } else {
            if (ImGui::Button("Resume")) {
                m_playState = PlayState::Playing;
                m_animate = true;
                logConsole("Play session resumed");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart")) {
            stopPlaySession();
            startPlaySession();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            stopPlaySession();
        }
    }
    ImGui::SameLine();
    const char* playLabel = stopped ? "Stopped" : (playing ? "Playing" : "Paused");
    ImGui::Text("State: %s", playLabel);
    ImGui::End();

    if (m_showSaveModal) {
        ImGui::OpenPopup("Save Scene");
        m_showSaveModal = false;
    }
    if (ImGui::BeginPopupModal("Save Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("File name", m_fileNameBuffer, sizeof(m_fileNameBuffer));
        if (ImGui::Button("Save")) {
            std::string path = m_scenesDirectory + "/" + std::string(m_fileNameBuffer);
            saveScene(path);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (m_showLoadModal) {
        ImGui::OpenPopup("Load Scene");
        m_showLoadModal = false;
    }
    if (ImGui::BeginPopupModal("Load Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("File name", m_fileNameBuffer, sizeof(m_fileNameBuffer));
        if (ImGui::Button("Load")) {
            std::string path = m_scenesDirectory + "/" + std::string(m_fileNameBuffer);
            loadScene(path);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (m_showRenameModal && m_renameTarget.value != 0) {
        ImGui::OpenPopup("Rename Entity");
        m_showRenameModal = false;
    }
    if (ImGui::BeginPopupModal("Rename Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", m_renameBuffer, sizeof(m_renameBuffer));
        if (ImGui::Button("Rename")) {
            if (Node* node = m_scene.find(m_renameTarget)) {
                node->setName(std::string(m_renameBuffer));
                logConsole("Renamed node to " + node->name());
            }
            m_renameTarget = NodeId{0};
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_renameTarget = NodeId{0};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_None);
    {

        ImGui::Text("Editor camera speed: %.1f units/s", m_cameraSpeed);
        if (ImGui::DragFloat("##speed", &m_cameraSpeed, 0.1f, 0.1f, 100.0f)) {
            m_editorCamera.setMovementSpeed(m_cameraSpeed);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset camera")) {
            m_editorCamera = EditorCamera();
            logConsole("Editor camera reset");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("RMB drag: look  |  WASD: move  |  Wheel: zoom  |  F: focus");
        ImGui::Separator();

        ImGui::Text("Camera controls");
        if (ImGui::DragFloat("Mouse sensitivity", &m_mouseSensitivity, 0.01f, 0.01f, 5.0f)) {}
        ImGui::SameLine();
        if (ImGui::Checkbox("Invert Y", &m_invertMouseY)) {}
        ImGui::Separator();

        ImGui::Text("Gizmo");
        ImGui::TextUnformatted("Transform");
        ImGui::SameLine();
        const auto chooseGizmo = [this](GizmoMode mode) {
            if (m_gizmoMode != mode) {
                m_gizmoMode = mode;
                m_gizmoActive = false;
                m_gizmoAxis = -1;
            }
        };
        if (ImGui::Button(m_gizmoMode == GizmoMode::Move ? "Move [W] *" : "Move [W]"))
            chooseGizmo(GizmoMode::Move);
        ImGui::SameLine();
        if (ImGui::Button(m_gizmoMode == GizmoMode::Rotate ? "Rotate [E] *" : "Rotate [E]"))
            chooseGizmo(GizmoMode::Rotate);
        ImGui::SameLine();
        if (ImGui::Button(m_gizmoMode == GizmoMode::Scale ? "Scale [R] *" : "Scale [R]"))
            chooseGizmo(GizmoMode::Scale);
        ImGui::SameLine();
        ImGui::TextDisabled("X=red  Y=green  Z=blue");
        ImGui::DragFloat("Snap trans.", &m_gizmoSnapTranslation, 0.01f, 0.0f, 10.0f);
        ImGui::SameLine();
        ImGui::DragFloat("Snap rot.", &m_gizmoSnapRotation, 0.5f, 0.0f, 180.0f);
        ImGui::SameLine();
        ImGui::DragFloat("Snap scale", &m_gizmoSnapScale, 0.01f, 0.0f, 10.0f);
        ImGui::Separator();
        ImGui::Separator();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 imageMin = ImGui::GetCursorScreenPos();
        const ImVec2 imageMax(imageMin.x + avail.x, imageMin.y + avail.y);
        m_viewportImageLeft = imageMin.x;
        m_viewportImageTop = imageMin.y;
        m_viewportImageRight = imageMax.x;
        m_viewportImageBottom = imageMax.y;
        m_viewportImageBoundsValid = avail.x > 0.0f && avail.y > 0.0f;
        m_viewportHovered = m_viewportImageBoundsValid &&
            ImGui::IsMouseHoveringRect(imageMin, imageMax, true);
        const int width = static_cast<int>(avail.x);
        const int height = static_cast<int>(avail.y);

        if (width > 0 && height > 0) {
            m_renderer.resize(width, height);
            if (m_playState == PlayState::Stopped) {
                updateEditorCameraInput(m_deltaTime);
            }

            if (m_playState == PlayState::Playing && m_animate && !m_scene.empty()) {

                m_playElapsed += deltaTime;
                const float time = m_playElapsed;
                for (const NodeId id : m_scene.nodeIds()) {
                    Node* node = m_scene.find(id);
                    if (dynamic_cast<Part*>(node) == nullptr) {
                        continue;
                    }
                    if (Node3D* node3d = dynamic_cast<Node3D*>(node)) {
                        node3d->rotation.y = time * 45.0f;
                        node3d->rotation.x = time * 30.0f;
                    }
                }
            }

            const float aspect =
                static_cast<float>(width) / static_cast<float>(height);
            const RendererCamera camera = computeRendererCamera(aspect);
            const RendererLighting lighting = computeRendererLighting();
            const RendererEnvironment environment =
                computeRendererEnvironment();
            m_renderer.render(m_scene, camera, lighting, environment);

            ImGui::Image(static_cast<ImTextureID>(
                             static_cast<ImU64>(m_renderer.colorTexture())),
                         avail, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

            if (m_selection.selected().has_value()) {
                Node* selected = m_scene.find(m_selection.selected().value());
                static NodeId s_gizmoLastSelectedId{};
                const bool selectionChanged = s_gizmoLastSelectedId.value !=
                    m_selection.selected().value().value;
                if (selectionChanged) {
                    std::fprintf(stderr,
                        "[GIZMO] selection active id=%llu name='%s' type='%s' isNode3D=%d mode=%d\n",
                        (unsigned long long)m_selection.selected().value().value,
                        selected ? selected->name().c_str() : "(null)",
                        selected ? selected->type().c_str() : "(null)",
                        selected ? dynamic_cast<Node3D*>(selected) != nullptr : 0,
                        (int)m_gizmoMode);
                }
                s_gizmoLastSelectedId = m_selection.selected().value();
                if (Node3D* node3d = dynamic_cast<Node3D*>(selected)) {
                    drawViewportGizmo(node3d, camera, width, height, imageMin);
                }
            } else {
                static bool s_gizmoWasSelected = false;
                if (s_gizmoWasSelected) {
                    std::fprintf(stderr, "[GIZMO] selection cleared\n");
                    s_gizmoWasSelected = false;
                }
            }
        } else {
            ImGui::Text("Viewport area unavailable");
        }
    }
    ImGui::End();

    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_None);
    ImGui::Text("Scene: %s", m_scene.name().c_str());
    ImGui::Separator();
    if (ImGui::Button("Add Node")) {
        m_showAddNodeModal = true;
        m_addNodeTypeIndex = 0;
        m_newNodeNameBuffer[0] = '\0';
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected")) {
        if (m_selection.selected().has_value()) {
            if (m_scene.destroyNode(m_selection.selected().value())) {
                m_selection.clear();
                logConsole("Deleted node and its children");
            }
        }
    }
    ImGui::Separator();

    if (ImGui::Button("Create Cube")) {
        const NodeId id = m_scene.createNode("Part", "Cube");
        if (Node3D* node3d = dynamic_cast<Node3D*>(m_scene.find(id))) {
            node3d->position = Vec3(0.0f, 0.0f, 0.0f);
            node3d->scale = Vec3(1.0f, 1.0f, 1.0f);
        }
        m_selection.select(id);
        logConsole("Created cube");
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Sphere")) {
        const NodeId id = m_scene.createNode("Part", "Sphere");
        if (Part* part = dynamic_cast<Part*>(m_scene.find(id))) {
            part->meshName = "sphere";
        }
        if (Node3D* node3d = dynamic_cast<Node3D*>(m_scene.find(id))) {
            node3d->position = Vec3(0.0f, 0.0f, 0.0f);
            node3d->scale = Vec3(1.0f, 1.0f, 1.0f);
        }
        m_selection.select(id);
        logConsole("Created sphere");
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Plane")) {
        const NodeId id = m_scene.createNode("Part", "Plane");
        if (Part* part = dynamic_cast<Part*>(m_scene.find(id))) {
            part->meshName = "plane";
        }
        if (Node3D* node3d = dynamic_cast<Node3D*>(m_scene.find(id))) {
            node3d->position = Vec3(0.0f, 0.0f, 0.0f);
            node3d->scale = Vec3(1.0f, 1.0f, 1.0f);
        }
        m_selection.select(id);
        logConsole("Created plane");
    }
    if (ImGui::Button("Create Cylinder")) {
        const NodeId id = m_scene.createNode("Part", "Cylinder");
        if (Part* part = dynamic_cast<Part*>(m_scene.find(id))) {
            part->meshName = "cylinder";
        }
        if (Node3D* node3d = dynamic_cast<Node3D*>(m_scene.find(id))) {
            node3d->position = Vec3(0.0f, 0.0f, 0.0f);
            node3d->scale = Vec3(1.0f, 1.0f, 1.0f);
        }
        m_selection.select(id);
        logConsole("Created cylinder");
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Cone")) {
        const NodeId id = m_scene.createNode("Part", "Cone");
        if (Part* part = dynamic_cast<Part*>(m_scene.find(id))) {
            part->meshName = "cone";
        }
        if (Node3D* node3d = dynamic_cast<Node3D*>(m_scene.find(id))) {
            node3d->position = Vec3(0.0f, 0.0f, 0.0f);
            node3d->scale = Vec3(1.0f, 1.0f, 1.0f);
        }
        m_selection.select(id);
        logConsole("Created cone");
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Capsule")) {
        const NodeId capsuleId = m_scene.createNode("Part", "Capsule");
        if (Part* part = dynamic_cast<Part*>(m_scene.find(capsuleId))) {
            part->meshName = "capsule";
        }
        if (Node3D* node3d = dynamic_cast<Node3D*>(m_scene.find(capsuleId))) {
            node3d->position = Vec3(0.0f, 0.0f, 0.0f);
            node3d->scale = Vec3(1.0f, 1.0f, 1.0f);
        }
        m_selection.select(capsuleId);
        logConsole("Created capsule");
    }
    ImGui::Separator();

    const bool empty = m_scene.rootNodeIds().empty();
    if (empty) {
        ImGui::TextDisabled("No nodes");
    }
    for (const NodeId rootId : m_scene.rootNodeIds()) {
        if (Node* node = m_scene.find(rootId)) {
            drawHierarchyNode(node);
        }
    }

    if (m_selection.selected().has_value()) {
        if (ImGui::Button("Rename Selected")) {
            if (Node* node = m_scene.find(m_selection.selected().value())) {
                std::snprintf(m_renameBuffer, sizeof(m_renameBuffer), "%s", node->name().c_str());
                m_renameTarget = m_selection.selected().value();
            }
            m_showRenameModal = true;
        }
    }
    ImGui::End();

    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_None);
    {
        if (Node* node = m_scene.find(m_selection.selected().value_or(NodeId{}))) {
            ImGui::Text("Name: %s", node->name().c_str());
            ImGui::Text("Type: %s", node->type().c_str());
            ImGui::Text("ID: %llu", static_cast<unsigned long long>(node->id().value));
            ImGui::Text("Children: %zu", node->children().size());
            ImGui::Separator();

            if (Node3D* node3d = dynamic_cast<Node3D*>(node)) {
                ImGui::Text("Transform (local, parent-relative)");
                if (ImGui::DragFloat3("Position", &node3d->position.x, 0.01f)) {}
                if (ImGui::DragFloat3("Rotation", &node3d->rotation.x, 1.0f)) {}
                if (ImGui::DragFloat3("Scale", &node3d->scale.x, 0.01f)) {}
                ImGui::Separator();
            }

            if (Part* part = dynamic_cast<Part*>(node)) {
                ImGui::Text("Part: renderable mesh");
                ImGui::Separator();

                const Mesh* currentMesh = m_renderer.meshManager().find(part->meshName);
                std::string meshLabel = currentMesh ? part->meshName : part->meshName + " (missing)";
                if (ImGui::BeginCombo("Mesh", meshLabel.c_str())) {
                    const auto& meshNames = m_renderer.meshManager().names();
                    for (const auto& meshName : meshNames) {
                        bool selected = (part->meshName == meshName);
                        if (ImGui::Selectable(meshName.c_str(), selected)) {
                            part->meshName = meshName;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse...")) {
                    m_assetBrowserTarget = "part:mesh";
                    m_assetBrowserTargetNode = part->id();
                    m_assetBrowser.setRootDirectory(m_assetsDirectory);
                    m_assetBrowser.open("Select Model", {".glb", ".gltf"});
                    m_showAssetBrowser = true;
                }

                const Material* currentMaterial =
                    m_renderer.materialManager().find(part->materialName);
                std::string materialLabel = currentMaterial ? part->materialName
                                                            : part->materialName + " (missing)";
                if (ImGui::BeginCombo("Material", materialLabel.c_str())) {
                    const auto& materialNames = m_renderer.materialManager().names();
                    for (const auto& materialName : materialNames) {
                        bool selected = (part->materialName == materialName);
                        if (ImGui::Selectable(materialName.c_str(), selected)) {
                            part->materialName = materialName;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::Button("Create Material...")) {
                    m_createMaterialTarget = part->id();
                    m_newMaterialNameBuffer[0] = '\0';
                    m_newMaterialBaseColor = Vec3(0.8f, 0.8f, 0.8f);
                    m_showCreateMaterialModal = true;
                }

                Material* mutableMaterial =
                    m_renderer.materialManager().find(part->materialName);
                if (mutableMaterial != nullptr) {
                    ImGui::Separator();
                    ImGui::Text("Material: %s", mutableMaterial->name.c_str());
                    if (ImGui::ColorEdit3("Base Color",
                                          &mutableMaterial->baseColor.x)) {}
                    if (ImGui::DragFloat("Metallic", &mutableMaterial->metallic,
                                         0.01f, 0.0f, 1.0f)) {}
                    if (ImGui::DragFloat("Roughness",
                                         &mutableMaterial->roughness,
                                         0.01f, 0.0f, 1.0f)) {}

                    std::snprintf(m_skyboxPathBuffer,
                                  sizeof(m_skyboxPathBuffer), "%s",
                                  mutableMaterial->texturePath.c_str());
                    if (ImGui::InputText("Texture Path", m_skyboxPathBuffer,
                                         sizeof(m_skyboxPathBuffer))) {
                        const std::string candidate = m_skyboxPathBuffer;
                        if (candidate.empty() ||
                            validateTexturePath(m_assetsDirectory, candidate)) {
                            mutableMaterial->texturePath = candidate;
                            mutableMaterial->hasTexture =
                                !candidate.empty();
                        } else {

                            std::snprintf(m_skyboxPathBuffer,
                                          sizeof(m_skyboxPathBuffer), "%s",
                                          mutableMaterial->texturePath.c_str());
                        }
                    }
                    if (ImGui::Button("Browse...")) {
                        m_assetBrowserTarget = "material:texture";
                        m_assetBrowserTargetNode = part->id();
                        m_assetBrowser.setRootDirectory(m_assetsDirectory);
                        m_assetBrowser.open("Select Texture", {".ppm"});
                        m_showAssetBrowser = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear Texture")) {
                        mutableMaterial->texturePath.clear();
                        mutableMaterial->hasTexture = false;
                    }
                    if (!mutableMaterial->texturePath.empty()) {
                        ImGui::Text("Has texture: %s",
                                    mutableMaterial->texturePath.c_str());
                    }
                }
                ImGui::Separator();
            }
            if (Camera3D* camera = dynamic_cast<Camera3D*>(node)) {
                ImGui::Text("Camera (operational)");
                if (ImGui::DragFloat("Field of view (deg)", &camera->fieldOfView, 0.5f, 1.0f, 179.0f)) {
                    if (!camera->isValid()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                           "Invalid: near must be < far > 0");
                    }
                }
                if (ImGui::DragFloat("Near plane", &camera->nearPlane, 0.001f, 0.001f, 1000.0f)) {
                    if (!camera->isValid()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                           "Invalid: near must be < far > 0");
                    }
                }
                if (ImGui::DragFloat("Far plane", &camera->farPlane, 0.1f, 0.001f, 10000.0f)) {
                    if (!camera->isValid()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                           "Invalid: far must be > near > 0");
                    }
                }
                ImGui::Text("Valid: %s", camera->isValid() ? "yes" : "no");
                ImGui::Separator();
            }
            if (DirectionalLight* light = dynamic_cast<DirectionalLight*>(node)) {
                ImGui::Text("Directional Light (operational)");
                ImGui::DragFloat3("Direction", &light->direction.x, 0.01f);
                ImGui::ColorEdit3("Color", &light->color.x);
                ImGui::DragFloat("Intensity", &light->intensity, 0.01f, 0.0f, 10.0f);
                ImGui::Separator();
            }
            if (PointLight* light = dynamic_cast<PointLight*>(node)) {
                ImGui::Text("Point Light (operational)");
                ImGui::DragFloat3("Position", &light->position.x, 0.01f);
                ImGui::ColorEdit3("Color", &light->color.x);
                ImGui::DragFloat("Intensity", &light->intensity, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Range", &light->range, 0.1f, 0.0f, 1000.0f);
                ImGui::Separator();
            }
            if (WorldEnvironment* env = dynamic_cast<WorldEnvironment*>(node)) {
                ImGui::Text("World Environment (operational)");
                static const char* kSkyModes[] = {"Procedural", "Textured"};
                ImGui::Combo("Sky mode", &env->skyMode, kSkyModes,
                             static_cast<int>(sizeof(kSkyModes) /
                                              sizeof(kSkyModes[0])));
                ImGui::ColorEdit3("Sky color", &env->skyColor.x);
                ImGui::ColorEdit3("Horizon color", &env->horizonColor.x);
                ImGui::DragFloat("Sky intensity", &env->skyIntensity, 0.01f,
                                 0.0f, 10.0f);
                ImGui::DragFloat("Environment rotation (deg)",
                                 &env->environmentRotation, 1.0f, 0.0f, 360.0f);
                if (env->skyMode == 1) {

                    std::snprintf(m_skyboxPathBuffer, sizeof(m_skyboxPathBuffer),
                                  "%s", env->skyboxPath.c_str());
                    if (ImGui::InputText("Skybox path", m_skyboxPathBuffer,
                                         sizeof(m_skyboxPathBuffer))) {
                        const std::string candidate = m_skyboxPathBuffer;
                        if (candidate.empty() ||
                            validateSkyboxPath(m_assetsDirectory, candidate)) {
                            env->skyboxPath = candidate;
                        } else {
                            std::snprintf(m_skyboxPathBuffer,
                                          sizeof(m_skyboxPathBuffer), "%s",
                                          env->skyboxPath.c_str());
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Browse...")) {
                        m_assetBrowserTarget = "skybox";
                        m_assetBrowserTargetNode = env->id();
                        m_assetBrowser.setRootDirectory(m_assetsDirectory);
                        m_assetBrowser.open("Select Skybox Directory", {});
                        m_assetBrowser.setSelectDirectory(true);
                        m_showAssetBrowser = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Procedural")) {
                        env->skyMode = 0;
                        env->skyboxPath.clear();
                    }
                    if (env->skyboxPath.empty()) {
                        ImGui::TextColored(
                            ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                            "Enter a skybox path (relative to assets)");
                    }
                }
                ImGui::Separator();
                ImGui::Text("Fog (operational)");
                ImGui::ColorEdit3("Fog color", &env->fogColor.x);
                ImGui::DragFloat("Fog start", &env->fogStart, 0.1f, 0.0f,
                                 10000.0f);
                ImGui::DragFloat("Fog end", &env->fogEnd, 0.1f, 0.0f, 10000.0f);
                if (env->fogEnd <= env->fogStart) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                       "Fog disabled: end must be > start");
                }
                ImGui::Separator();
            }

            ImGui::Text("Frame time: %.3f ms", deltaTime * 1000.0f);
        } else {
            ImGui::TextDisabled("No node selected");
            ImGui::Separator();
            ImGui::Text("Frame time: %.3f ms", deltaTime * 1000.0f);
        }
    }
    ImGui::End();

    ImGui::Begin("Console", nullptr, ImGuiWindowFlags_None);
    for (const std::string& line : m_console) {
        ImGui::TextUnformatted(line.c_str());
    }
    ImGui::End();

    ImGui::Begin("Project", nullptr, ImGuiWindowFlags_None);
    if (m_projectBrowserDirectory.empty())
        m_projectBrowserDirectory = m_assetsDirectory;
    const std::filesystem::path assetsRoot = std::filesystem::weakly_canonical(m_assetsDirectory);
    std::error_code projectEc;
    std::filesystem::path currentDir = std::filesystem::weakly_canonical(m_projectBrowserDirectory, projectEc);
    if (projectEc || currentDir.string().compare(0, assetsRoot.string().size(), assetsRoot.string()) != 0) {
        currentDir = assetsRoot;
        m_projectBrowserDirectory = currentDir.string();
    }
    ImGui::TextWrapped("%s", currentDir.string().c_str());
    if (ImGui::Button("Assets root")) {
        m_projectBrowserDirectory = assetsRoot.string();
        m_projectBrowserSelectedFile.clear();
        currentDir = assetsRoot;
    }
    ImGui::SameLine();
    if (currentDir != assetsRoot && ImGui::Button("Up")) {
        const auto parent = currentDir.parent_path();
        if (parent.string().size() >= assetsRoot.string().size())
            m_projectBrowserDirectory = parent.string();
        m_projectBrowserSelectedFile.clear();
    }
    ImGui::Separator();
    const bool projectEntriesVisible = ImGui::BeginChild("##projectBrowserEntries", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
    if (projectEntriesVisible) {
        std::vector<std::filesystem::directory_entry> directories;
        std::vector<std::filesystem::directory_entry> files;
        std::error_code listEc;
        for (std::filesystem::directory_iterator it(currentDir, listEc), finish;
             it != finish && !listEc; it.increment(listEc)) {
            std::error_code typeEc;
            if (it->is_directory(typeEc)) directories.push_back(*it);
            else if (it->is_regular_file(typeEc)) files.push_back(*it);
        }
        const auto byName = [](const auto& a, const auto& b) {
            return a.path().filename().string() < b.path().filename().string();
        };
        std::sort(directories.begin(), directories.end(), byName);
        std::sort(files.begin(), files.end(), byName);
        for (const auto& entry : directories) {
            const std::string name = "[DIR] " + entry.path().filename().string();
            if (ImGui::Selectable(name.c_str(), false)) {
                m_projectBrowserDirectory = entry.path().string();
                m_projectBrowserSelectedFile.clear();
            }
        }
        for (const auto& entry : files) {
            const std::string full = entry.path().string();
            const std::string name = entry.path().filename().string();
            if (ImGui::Selectable(name.c_str(), m_projectBrowserSelectedFile == full))
                m_projectBrowserSelectedFile = full;
            if (ImGui::BeginPopupContextItem(("project-file-" + full).c_str())) {
                if (ImGui::MenuItem("Copy full path")) {
                    ImGui::SetClipboardText(full.c_str());
                }
                ImGui::EndPopup();
            }
        }
        if (directories.empty() && files.empty()) ImGui::TextDisabled("This folder is empty.");
    }
    ImGui::EndChild();
    if (!m_projectBrowserSelectedFile.empty())
        ImGui::TextWrapped("Selected: %s", std::filesystem::path(m_projectBrowserSelectedFile).filename().string().c_str());
    ImGui::End();

    if (m_showDemoWindow) {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }

    if (m_showAddNodeModal) {
        ImGui::OpenPopup("Add Node");
        m_showAddNodeModal = false;
    }
    if (ImGui::BeginPopupModal("Add Node", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static const char* kNodeTypes[] = {"Node3D", "Part", "Camera3D",
                                           "DirectionalLight", "PointLight",
                                           "WorldEnvironment"};
        ImGui::Combo("Type", &m_addNodeTypeIndex, kNodeTypes,
                     static_cast<int>(sizeof(kNodeTypes) / sizeof(kNodeTypes[0])));
        ImGui::InputText("Name", m_newNodeNameBuffer, sizeof(m_newNodeNameBuffer));
        if (ImGui::Button("Create")) {
            const std::string type = kNodeTypes[m_addNodeTypeIndex];
            std::string name = m_newNodeNameBuffer[0] == '\0' ? type : std::string(m_newNodeNameBuffer);
            const NodeId createdId = m_scene.createNode(type, name);
            if (createdId.value != 0) {
                m_selection.select(createdId);
                logConsole("Added " + type + " node");
            } else {
                logConsole("Failed to add node of type " + type);
            }
            m_newNodeNameBuffer[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_newNodeNameBuffer[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (m_showAssetBrowser) {
        ImGui::OpenPopup("Asset Browser");
        m_showAssetBrowser = false;
    }
    if (ImGui::BeginPopupModal("Asset Browser", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string selectedPath;
        if (m_assetBrowser.draw("Asset Browser##modal",
                                &m_showAssetBrowser, &selectedPath)) {

            if (m_assetBrowserTargetNode.value != 0) {
                Node* node = m_scene.find(m_assetBrowserTargetNode);
                if (node != nullptr) {
                    if (m_assetBrowserTarget == "skybox") {
                        if (WorldEnvironment* env =
                                dynamic_cast<WorldEnvironment*>(node)) {
                            if (!validateSkyboxPath(m_assetsDirectory,
                                                    selectedPath)) {
                                logConsole(
                                    "Skybox rejected: '" + selectedPath +
                                    "' is not a valid cubemap directory "
                                    "(needs posx/negx/posy/negy/posz/negz "
                                    "PPM faces)");
                            } else {
                                env->skyboxPath = selectedPath;
                                logConsole("Skybox set to: " + selectedPath);
                            }
                        }
                    } else if (m_assetBrowserTarget == "material:texture") {
                        if (Part* part = dynamic_cast<Part*>(node)) {
                            if (Material* material =
                                    m_renderer.materialManager().find(
                                        part->materialName)) {
                                if (!validateTexturePath(m_assetsDirectory,
                                                         selectedPath)) {
                                    logConsole(
                                        "Texture rejected: '" + selectedPath +
                                        "' does not exist or is not a "
                                        "supported .ppm image");
                                } else {
                                    material->texturePath = selectedPath;
                                    material->hasTexture =
                                        !selectedPath.empty();
                                    logConsole("Material texture set to: " +
                                               selectedPath);
                                }
                            }
                        }
                    } else if (m_assetBrowserTarget == "part:mesh") {
                        if (Part* part = dynamic_cast<Part*>(node)) {
                            const std::size_t dot = selectedPath.find_last_of('.');
                            std::string ext = dot == std::string::npos
                                ? std::string{} : selectedPath.substr(dot);
                            std::transform(ext.begin(), ext.end(), ext.begin(),
                                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                            if (ext == ".glb" || ext == ".gltf") {
                                std::string fullPath =
                                    m_assetsDirectory + "/" + selectedPath;
                                if (m_renderer.loadGlbModel(
                                        fullPath, selectedPath)) {
                                    part->meshName = selectedPath;
                                    logConsole(
                                        "Model mesh set to: " + selectedPath);
                                } else {
                                    logConsole(
                                        "Model rejected: failed to load '" +
                                        selectedPath + "'");
                                }
                            } else {
                                logConsole(
                                    "Model rejected: '" + selectedPath +
                                    "' is not a supported .glb/.gltf model");
                            }
                        }
                    }
                }
            }
            m_assetBrowserTargetNode = NodeId{0};
            m_assetBrowserTarget.clear();
        }
        ImGui::EndPopup();
    }

    if (m_showCreateMaterialModal) {
        ImGui::OpenPopup("Create Material");
        m_showCreateMaterialModal = false;
    }
    if (ImGui::BeginPopupModal("Create Material", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new reusable material and assign it to the "
                    "selected object.");
        ImGui::Separator();
        ImGui::InputText("Name", m_newMaterialNameBuffer,
                         sizeof(m_newMaterialNameBuffer));
        ImGui::ColorEdit3("Base Color", &m_newMaterialBaseColor.x);
        ImGui::Separator();

        bool nameValid = m_newMaterialNameBuffer[0] != '\0';
        bool nameUnique = true;
        if (nameValid) {
            nameUnique =
                m_renderer.materialManager().find(m_newMaterialNameBuffer) ==
                nullptr;
        }
        if (!nameUnique) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "A material with that name already exists");
        }

        if (ImGui::Button("Create", ImVec2(120, 0)) && nameValid && nameUnique) {
            Material material(m_newMaterialNameBuffer);
            material.baseColor = m_newMaterialBaseColor;
            m_renderer.materialManager().registerMaterial(material);
            if (Node* node = m_scene.find(m_createMaterialTarget)) {
                if (Part* part = dynamic_cast<Part*>(node)) {
                    part->materialName = m_newMaterialNameBuffer;
                    logConsole("Created material " +
                               std::string(m_newMaterialNameBuffer));
                }
            }
            m_newMaterialNameBuffer[0] = '\0';
            m_createMaterialTarget = NodeId{0};
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_newMaterialNameBuffer[0] = '\0';
            m_createMaterialTarget = NodeId{0};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
#else
    (void)deltaTime;
#endif
}

void Application::run() {
    m_lastTicks = SDL_GetTicks();

    while (m_running) {
        processEvents();

        const Uint64 now = SDL_GetTicks();
        m_deltaTime = static_cast<float>(now - m_lastTicks) / 1000.0f;
        m_lastTicks = now;
        if (m_deltaTime > kMaxDeltaTime) {
            m_deltaTime = kMaxDeltaTime;
        }

#ifdef PEBBLE_ENGINE_HAS_IMGUI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        buildEditorUI(m_deltaTime);

        ImGui::Render();
#endif

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(0, 0, m_drawableWidth, m_drawableHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

#ifdef PEBBLE_ENGINE_HAS_IMGUI
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

        SDL_GL_SwapWindow(m_window);
    }
}
