#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

struct SDL_Window;
struct SDL_GLContextState;
struct ImGuiContext;

#include "editor/asset_browser.hpp"
#include "editor_camera.hpp"
#include "renderer/renderer.hpp"
#include "scene/scene.hpp"
#include "scene/selection.hpp"

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool init(const std::vector<std::string>& commandLine);
    bool initSDLOnly();

    void run();
    void shutdown();

    bool sdlInitialized() const { return m_sdlInitialized; }

private:
    bool processEvents();
    void buildEditorUI(float deltaTime);
    void animateScene(float deltaTime);
    bool startPlaySession();
    void stopPlaySession();
    void drawHierarchyNode(class Node* node);
    void drawViewportGizmo(class Node3D* node3d, const RendererCamera& camera,
                           int viewportWidth, int viewportHeight,
                           ImVec2 viewportImageOrigin);
    void updateEditorCameraInput(float deltaTime);
    void focusSelectedNode();
    RendererCamera computeRendererCamera(float aspect) const;
    RendererLighting computeRendererLighting() const;
    RendererEnvironment computeRendererEnvironment() const;
    std::string resolveAssetsDirectory(const std::vector<std::string>& commandLine) const;
    std::string resolveScenesDirectory() const;
    std::string resolveModelsDirectory() const;
    void loadStarterScene();
    void saveScene(const std::string& path);
    void loadScene(const std::string& path);
    void deleteSelectedNode();
    void duplicateSelectedNode();
    void openAssetBrowser(const std::string& target, NodeId nodeId,
                          const std::vector<std::string>& extensions);
    void applyAssetBrowserSelection();
    void logConsole(const std::string& message);

    SDL_Window* m_window = nullptr;
    SDL_GLContextState* m_glContext = nullptr;
    ImGuiContext* m_imguiContext = nullptr;

    Renderer m_renderer;
    Scene m_scene{"Untitled"};
    Selection m_selection;
    AssetBrowser m_assetBrowser;
    EditorCamera m_editorCamera;

    bool m_running = true;
    int m_drawableWidth = 1280;
    int m_drawableHeight = 720;

    uint64_t m_lastTicks = 0;
    float m_deltaTime = 0.0f;

    std::string m_assetsDirectory;
    std::string m_scenesDirectory;
    std::string m_projectBrowserDirectory;
    std::string m_projectBrowserSelectedFile;
    std::vector<std::string> m_console;

    bool m_showDemoWindow = false;
    enum class PlayState { Stopped, Playing, Paused };
    PlayState m_playState = PlayState::Stopped;
    std::string m_playSnapshotPath;
    NodeId m_selectionBeforePlay{0};
    float m_playElapsed = 0.0f;
    bool m_animate = false;
    bool m_viewportHovered = false;
    bool m_viewportImageBoundsValid = false;
    float m_viewportImageLeft = 0.0f;
    float m_viewportImageTop = 0.0f;
    float m_viewportImageRight = 0.0f;
    float m_viewportImageBottom = 0.0f;
    bool m_mouseCaptured = false;
    bool m_invertMouseY = false;
    float m_cameraSpeed = 5.0f;
    float m_mouseSensitivity = 0.3f;
    bool m_showSaveModal = false;
    bool m_showLoadModal = false;
    bool m_showRenameModal = false;
    bool m_showAddNodeModal = false;
    bool m_showAssetBrowser = false;
    bool m_showCreateMaterialModal = false;
    char m_fileNameBuffer[256] = "";
    char m_renameBuffer[256] = "";
    char m_newNodeNameBuffer[256] = "";
    char m_newMaterialNameBuffer[256] = "";
    char m_skyboxPathBuffer[256] = "";
    NodeId m_renameTarget{0};
    NodeId m_assetBrowserTargetNode{0};
    NodeId m_createMaterialTarget{0};
    int m_addNodeTypeIndex = 0;
    std::string m_assetBrowserTarget;
    Vec3 m_newMaterialBaseColor = Vec3(0.8f, 0.8f, 0.8f);

    enum class GizmoMode { None, Move, Rotate, Scale };
    GizmoMode m_gizmoMode = GizmoMode::Move;
    bool m_gizmoActive = false;
    int m_gizmoAxis = -1;
    Vec3 m_gizmoStartPosition;
    Vec3 m_gizmoStartRotation;
    Vec3 m_gizmoStartScale = Vec3(1.0f, 1.0f, 1.0f);
    float m_imguiDisplayScale = 1.0f;
    float m_gizmoSnapTranslation = 0.0f;
    float m_gizmoSnapRotation = 0.0f;
    float m_gizmoSnapScale = 0.0f;
    bool m_sdlInitialized = false;
    bool m_windowCreated = false;
    bool m_glContextCreated = false;
    bool m_imguiCreated = false;
    bool m_layoutBuilt = false;
    bool m_imguiSDL3Initialized = false;
    bool m_imguiOpenGL3Initialized = false;
    bool m_rendererInitialized = false;
};
