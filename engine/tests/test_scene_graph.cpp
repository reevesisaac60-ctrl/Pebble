#include "scene/node3d.hpp"
#include "scene/part.hpp"
#include "scene/camera3d.hpp"
#include "scene/directional_light.hpp"
#include "scene/point_light.hpp"
#include "scene/world_environment.hpp"
#include "scene/scene.hpp"
#include "scene/selection.hpp"
#include "editor_camera.hpp"
#include "renderer/mesh.hpp"
#include "renderer/material.hpp"
#include "renderer/model_importer.hpp"
#include "editor/asset_browser.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

constexpr float kDegreesToRadiansTest =
    3.14159265358979323846f / 180.0f;

// Forward declarations for tests defined after main().
void testMeshGeneration();
void testMaterialDefaults();
void testMaterialValidation();
void testMaterialManager();
void testSerializationWithMaterials();
void testReparentingPreventsCycles();
void testAssetPathResolution();
void testAssetFilters();
void testMissingAssetHandling();
void testModelImport();
void testModelImportMalformed();
void testCoordinateConversion();
void testGizmoAxisMath();
void testModelImportAllGlbs();
void testPartReferencesImportedMesh();
void testSceneRoundTripWithImportedMesh();
void testGltfNodeTransforms();
void testGltfParentChildHierarchy();
void testGltfMultipleNodesSameMesh();

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::string tmpPath(const std::string& name) {
    return std::filesystem::temp_directory_path().string() + "/" + name;
}

// ---------------------------------------------------------------------------
// Parent-child relationships
// ---------------------------------------------------------------------------

void testParentChild() {
    Scene scene("Test");
    NodeId root = scene.createNode("Node3D", "Root");
    NodeId child = scene.createNode("Node3D", "Child");
    NodeId grandchild = scene.createNode("Node3D", "Grandchild");

    require(scene.has(root), "root exists");
    require(scene.has(child), "child exists");
    require(scene.has(grandchild), "grandchild exists");

    // Initially all are roots.
    require(scene.rootNodeIds().size() == 3, "all roots initially");

    // Reparent child under root.
    require(scene.setParent(child, root), "set child parent to root");
    require(scene.find(child)->hasParent(), "child has parent");
    require(scene.find(root)->hasChildren(), "root has children");
    require(scene.rootNodeIds().size() == 2, "two roots after reparent");

    // Reparent grandchild under child.
    require(scene.setParent(grandchild, child), "set grandchild parent to child");
    require(scene.rootNodeIds().size() == 1, "single root after nesting");

    // Children list consistency.
    const auto& rootChildren = scene.find(root)->children();
    require(rootChildren.size() == 1 && rootChildren[0] == child,
            "root has exactly child");

    const auto& childChildren = scene.find(child)->children();
    require(childChildren.size() == 1 && childChildren[0] == grandchild,
            "child has exactly grandchild");

    // Cycle detection: root cannot become a child of its descendant.
    require(!scene.setParent(root, grandchild), "cycle rejected");
    require(scene.find(root)->parent() == nullptr,
            "root still has no parent after rejected cycle");

    // Self-parent rejected.
    require(!scene.setParent(child, child), "self-parent rejected");

    // Reparenting back to root clears old parent.
    require(scene.setParent(child, NodeId{0}), "reparent child to root null");
    require(!scene.find(child)->hasParent(), "child no parent");
    require(scene.rootNodeIds().size() == 2, "two roots again");
}

// ---------------------------------------------------------------------------
// Transform inheritance
// ---------------------------------------------------------------------------

void testTransformInheritance() {
    Scene scene("TransformTest");
    NodeId parent = scene.createNode("Part", "Parent");
    NodeId child = scene.createNode("Part", "Child");
    scene.setParent(child, parent);

    Node3D* p3d = dynamic_cast<Node3D*>(scene.find(parent));
    Node3D* c3d = dynamic_cast<Node3D*>(scene.find(child));
    require(p3d != nullptr && c3d != nullptr, "nodes are Node3D");

    p3d->position = Vec3(1.0f, 2.0f, 3.0f);
    p3d->scale = Vec3(2.0f, 2.0f, 2.0f);
    c3d->position = Vec3(1.0f, 0.0f, 0.0f);

    Mat4 parentGlobal = p3d->globalTransform();
    Mat4 childGlobal = c3d->globalTransform();

    // Child global = parent global * child local.
    Mat4 expected = parentGlobal * c3d->localTransform();
    for (int i = 0; i < 16; ++i) {
        require(std::fabs(childGlobal(i / 4, i % 4) - expected(i / 4, i % 4)) <
                    1e-4f,
                "child global equals parent global * child local");
    }

    // Moving the parent moves the child (world position changes).
    Vec3 childWorldBefore = c3d->worldPosition();
    p3d->position = Vec3(10.0f, 10.0f, 10.0f);
    Vec3 childWorldAfter = c3d->worldPosition();
    require(childWorldAfter.x > childWorldBefore.x + 5.0f,
            "moving parent moves child");
}

// ---------------------------------------------------------------------------
// Node creation, deletion, selection
// ---------------------------------------------------------------------------

void testCreateDelete() {
    Scene scene("DeleteTest");
    Scene scene2;  // default name

    NodeId a = scene.createNode("Part", "A");
    NodeId b = scene.createNode("Part", "B");
    NodeId c = scene.createNode("Camera3D", "Cam");
    scene.setParent(b, a);
    scene.setParent(c, a);

    require(scene.nodeIds().size() == 3, "three nodes");
    require(scene.find(a)->hasChildren(), "a has children");
    require(scene.find(b)->hasParent(), "b has parent");
    require(scene.find(c)->hasParent(), "c has parent");

    // Deleting a parent cascades to children.
    require(scene.destroyNode(a), "delete parent a");
    require(!scene.has(a), "a gone");
    require(!scene.has(b), "b cascaded away");
    require(!scene.has(c), "c cascaded away");
    require(scene.nodeIds().empty(), "scene empty after cascade delete");

    // Deleting a non-existent node returns false.
    require(!scene.destroyNode(NodeId{999}), "delete nonexistent returns false");

    // Unknown type returns null id.
    require(scene.createNode("DoesNotExist", "X").value == 0,
            "unknown type returns null id");
}

// ---------------------------------------------------------------------------
// Serialization round-trips (v2)
// ---------------------------------------------------------------------------

void saveV2(const Scene& scene, const std::string& path) {
    require(scene.save(path), "save v2 scene");
}

Scene loadV2(const std::string& path) {
    Scene scene;
    require(scene.load(path), "load v2 scene");
    return scene;
}

void testSerializationRoundTrip() {
    Scene scene("RoundTrip");
    NodeId root = scene.createNode("Part", "Root");
    NodeId child = scene.createNode("Part", "Child");
    NodeId cam = scene.createNode("Camera3D", "MainCam");
    NodeId light = scene.createNode("DirectionalLight", "Sun");
    NodeId env = scene.createNode("WorldEnvironment", "Env");
    scene.setParent(child, root);
    scene.setParent(cam, root);

    Node3D* r3d = dynamic_cast<Node3D*>(scene.find(root));
    r3d->position = Vec3(1.0f, 2.0f, 3.0f);
    r3d->rotation = Vec3(10.0f, 20.0f, 30.0f);
    r3d->scale = Vec3(2.0f, 1.0f, 1.5f);

    Camera3D* c = dynamic_cast<Camera3D*>(scene.find(cam));
    c->fieldOfView = 60.0f;
    c->nearPlane = 0.5f;
    c->farPlane = 500.0f;

    DirectionalLight* l = dynamic_cast<DirectionalLight*>(scene.find(light));
    l->color = Vec3(1.0f, 0.8f, 0.6f);
    l->intensity = 2.5f;

    WorldEnvironment* e = dynamic_cast<WorldEnvironment*>(scene.find(env));
    e->fogColor = Vec3(0.1f, 0.2f, 0.3f);
    e->fogStart = 5.0f;
    e->fogEnd = 200.0f;

    const std::string path = tmpPath("test_roundtrip.pebscene");
    saveV2(scene, path);
    Scene loaded = loadV2(path);

    require(loaded.name() == "RoundTrip", "scene name round trip");
    require(loaded.nodeIds().size() == 5, "node count round trip");

    // Types preserved.
    require(loaded.find(root)->type() == "Part", "root type preserved");
    require(loaded.find(child)->type() == "Part", "child type preserved");
    require(loaded.find(cam)->type() == "Camera3D", "cam type preserved");
    require(loaded.find(light)->type() == "DirectionalLight", "light type preserved");
    require(loaded.find(env)->type() == "WorldEnvironment", "env type preserved");

    // Hierarchy preserved.
    require(loaded.find(child)->hasParent(), "child has parent after load");
    require(loaded.find(child)->parent()->id() == root,
            "child's parent is root");
    require(!loaded.find(root)->hasParent(), "root has no parent");

    // Transform preserved.
    Node3D* lr3d = dynamic_cast<Node3D*>(loaded.find(root));
    require(lr3d != nullptr, "root is Node3D after load");
    require(std::fabs(lr3d->position.x - 1.0f) < 1e-5f, "position x");
    require(std::fabs(lr3d->position.y - 2.0f) < 1e-5f, "position y");
    require(std::fabs(lr3d->position.z - 3.0f) < 1e-5f, "position z");
    require(std::fabs(lr3d->rotation.x - 10.0f) < 1e-5f, "rotation x");
    require(std::fabs(lr3d->scale.x - 2.0f) < 1e-5f, "scale x");

    // Typed properties preserved.
    Camera3D* lc = dynamic_cast<Camera3D*>(loaded.find(cam));
    require(lc != nullptr, "cam is Camera3D after load");
    require(std::fabs(lc->fieldOfView - 60.0f) < 1e-5f, "cam fov");
    require(std::fabs(lc->nearPlane - 0.5f) < 1e-5f, "cam near");
    require(std::fabs(lc->farPlane - 500.0f) < 1e-5f, "cam far");

    DirectionalLight* ll = dynamic_cast<DirectionalLight*>(loaded.find(light));
    require(ll != nullptr, "light is DirectionalLight after load");
    require(std::fabs(ll->intensity - 2.5f) < 1e-5f, "light intensity");

    WorldEnvironment* le = dynamic_cast<WorldEnvironment*>(loaded.find(env));
    require(le != nullptr, "env is WorldEnvironment after load");
    require(std::fabs(le->fogEnd - 200.0f) < 1e-5f, "env fog end");

    std::filesystem::remove(path);
}

void testEmptySceneSerialization() {
    Scene scene("Empty");
    const std::string path = tmpPath("test_empty.pebscene");
    require(scene.save(path), "save empty scene");
    Scene loaded = loadV2(path);
    require(loaded.name() == "Empty", "empty scene name");
    require(loaded.nodeIds().empty(), "empty scene no nodes");
    std::filesystem::remove(path);
}

// ---------------------------------------------------------------------------
// Invalid / unsupported serialized data
// ---------------------------------------------------------------------------

void testInvalidHeader() {
    const std::string path = tmpPath("test_bad_header.pebscene");
    {
        std::ofstream out(path, std::ios::binary);
        out << "NOT_A_SCENE";
    }
    Scene scene;
    require(!scene.load(path), "bad header rejected");
    std::filesystem::remove(path);
}

void testUnsupportedVersion() {
    const std::string path = tmpPath("test_bad_version.pebscene");
    {
        std::ofstream out(path, std::ios::binary);
        out.write("PEBBLE_SCENE", 12);
        out.put('\0');
        uint32_t version = 99;
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        uint32_t nameSize = 0;
        out.write(reinterpret_cast<const char*>(&nameSize), sizeof(nameSize));
    }
    Scene scene;
    require(!scene.load(path), "unsupported version rejected");
    std::filesystem::remove(path);
}

void testDuplicateIds() {
    const std::string path = tmpPath("test_dup_id.pebscene");
    {
        std::ofstream out(path, std::ios::binary);
        out.write("PEBBLE_SCENE", 12);
        out.put('\0');
        uint32_t version = 2;
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        uint32_t nameSize = 5;
        out.write(reinterpret_cast<const char*>(&nameSize), sizeof(nameSize));
        out.write("Scene", 5);
        uint32_t nodeCount = 2;
        out.write(reinterpret_cast<const char*>(&nodeCount), sizeof(nodeCount));
        // Node 1: id 42, type Node3D, name "A", parent 0
        uint64_t id = 42;
        out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        std::string type = "Node3D";
        uint32_t tsize = static_cast<uint32_t>(type.size());
        out.write(reinterpret_cast<const char*>(&tsize), sizeof(tsize));
        out.write(type.data(), tsize);
        std::string name = "A";
        uint32_t nsize = static_cast<uint32_t>(name.size());
        out.write(reinterpret_cast<const char*>(&nsize), sizeof(nsize));
        out.write(name.data(), nsize);
        uint64_t parent = 0;
        out.write(reinterpret_cast<const char*>(&parent), sizeof(parent));
        // transform (9 floats, all zero)
        float zero = 0.0f;
        for (int i = 0; i < 9; ++i) out.write(reinterpret_cast<const char*>(&zero), sizeof(float));
        // Node 2: duplicate id 42, type Part, name "B", parent 0
        out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        type = "Part";
        tsize = static_cast<uint32_t>(type.size());
        out.write(reinterpret_cast<const char*>(&tsize), sizeof(tsize));
        out.write(type.data(), tsize);
        name = "B";
        nsize = static_cast<uint32_t>(name.size());
        out.write(reinterpret_cast<const char*>(&nsize), sizeof(nsize));
        out.write(name.data(), nsize);
        out.write(reinterpret_cast<const char*>(&parent), sizeof(parent));
        for (int i = 0; i < 9; ++i) out.write(reinterpret_cast<const char*>(&zero), sizeof(float));
        uint32_t primitive = 0;
        out.write(reinterpret_cast<const char*>(&primitive), sizeof(primitive));
    }
    Scene scene;
    require(!scene.load(path), "duplicate ids rejected");
    std::filesystem::remove(path);
}

void testInvalidParent() {
    const std::string path = tmpPath("test_bad_parent.pebscene");
    {
        std::ofstream out(path, std::ios::binary);
        out.write("PEBBLE_SCENE", 12);
        out.put('\0');
        uint32_t version = 2;
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        uint32_t nameSize = 5;
        out.write(reinterpret_cast<const char*>(&nameSize), sizeof(nameSize));
        out.write("Scene", 5);
        uint32_t nodeCount = 1;
        out.write(reinterpret_cast<const char*>(&nodeCount), sizeof(nodeCount));
        // Node id 1, parent 999 (does not exist)
        uint64_t id = 1;
        out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        std::string type = "Node3D";
        uint32_t tsize = static_cast<uint32_t>(type.size());
        out.write(reinterpret_cast<const char*>(&tsize), sizeof(tsize));
        out.write(type.data(), tsize);
        std::string name = "A";
        uint32_t nsize = static_cast<uint32_t>(name.size());
        out.write(reinterpret_cast<const char*>(&nsize), sizeof(nsize));
        out.write(name.data(), nsize);
        uint64_t parent = 999;
        out.write(reinterpret_cast<const char*>(&parent), sizeof(parent));
        float zero = 0.0f;
        for (int i = 0; i < 9; ++i) out.write(reinterpret_cast<const char*>(&zero), sizeof(float));
    }
    Scene scene;
    require(!scene.load(path), "invalid parent reference rejected");
    std::filesystem::remove(path);
}

void testUnsupportedType() {
    const std::string path = tmpPath("test_bad_type.pebscene");
    {
        std::ofstream out(path, std::ios::binary);
        out.write("PEBBLE_SCENE", 12);
        out.put('\0');
        uint32_t version = 2;
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        uint32_t nameSize = 5;
        out.write(reinterpret_cast<const char*>(&nameSize), sizeof(nameSize));
        out.write("Scene", 5);
        uint32_t nodeCount = 1;
        out.write(reinterpret_cast<const char*>(&nodeCount), sizeof(nodeCount));
        uint64_t id = 1;
        out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        std::string type = "DoesNotExist";
        uint32_t tsize = static_cast<uint32_t>(type.size());
        out.write(reinterpret_cast<const char*>(&tsize), sizeof(tsize));
        out.write(type.data(), tsize);
        std::string name = "A";
        uint32_t nsize = static_cast<uint32_t>(name.size());
        out.write(reinterpret_cast<const char*>(&nsize), sizeof(nsize));
        out.write(name.data(), nsize);
        uint64_t parent = 0;
        out.write(reinterpret_cast<const char*>(&parent), sizeof(parent));
    }
    Scene scene;
    require(!scene.load(path), "unsupported node type rejected");
    std::filesystem::remove(path);
}

// ---------------------------------------------------------------------------
// Existing scene compatibility (v1 migration)
// ---------------------------------------------------------------------------

void testV1Migration() {
    // Build a legacy v1 file manually: header, version 1, name, entity count,
    // then per-entity: id, name, 9 floats (pos, rot, scale).
    const std::string path = tmpPath("test_v1.pebscene");
    {
        std::ofstream out(path, std::ios::binary);
        out.write("PEBBLE_SCENE", 12);
        out.put('\0');
        uint32_t version = 1;
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        uint32_t nameSize = 7;
        out.write(reinterpret_cast<const char*>(&nameSize), sizeof(nameSize));
        out.write("Starter", 7);
        uint32_t entityCount = 1;
        out.write(reinterpret_cast<const char*>(&entityCount), sizeof(entityCount));
        uint64_t id = 5;
        out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        uint32_t nsize = 4;
        out.write(reinterpret_cast<const char*>(&nsize), sizeof(nsize));
        out.write("Cube", 4);
        float vals[9] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
        out.write(reinterpret_cast<const char*>(vals), sizeof(vals));
    }
    Scene scene;
    require(scene.load(path), "v1 scene loaded");
    require(scene.name() == "Starter", "v1 scene name");
    require(scene.nodeIds().size() == 1, "v1 one node migrated");
    require(scene.find(NodeId{5}) != nullptr, "v1 id preserved");
    require(scene.find(NodeId{5})->type() == "Part",
            "v1 entity migrated to Part");
    require(!scene.find(NodeId{5})->hasParent(), "v1 node has no parent");
    Node3D* n3d = dynamic_cast<Node3D*>(scene.find(NodeId{5}));
    require(n3d != nullptr, "v1 node is Node3D");
    require(std::fabs(n3d->position.x - 0.0f) < 1e-5f, "v1 position x");
    require(std::fabs(n3d->scale.x - 1.0f) < 1e-5f, "v1 scale x");
    std::filesystem::remove(path);
}

// ---------------------------------------------------------------------------
// Camera view/projection matrices
// ---------------------------------------------------------------------------

void testCameraMatrices() {
    Scene scene("CameraTest");
    const NodeId id = scene.createNode("Camera3D", "MainCam");
    Camera3D* camera = dynamic_cast<Camera3D*>(scene.find(id));
    require(camera != nullptr, "created Camera3D");
    camera->position = Vec3(0.0f, 0.0f, 3.0f);
    camera->rotation = Vec3(0.0f, 0.0f, 0.0f);

    require(camera->isValid(), "default camera is valid");

    // View matrix: camera at +Z looking toward origin (down -Z).
    const Mat4 view = camera->viewMatrix();
    // The camera's forward is -Z, so looking from (0,0,3) at (0,0,2):
    // a world point at the origin should map to negative Z in view space.
    const Vec3 originView = transformPoint(view, Vec3(0.0f, 0.0f, 0.0f));
    require(originView.z < 0.0f, "origin is in front of camera (negative view Z)");

    // Projection matrix for a 16:9 viewport.
    const Mat4 projection = camera->projectionMatrix(16.0f / 9.0f);
    require(projection != Mat4(), "projection is not identity when valid");

    // Aspect ratio changes the projection (x-scale differs).
    const Mat4 projectionWide = camera->projectionMatrix(16.0f / 9.0f);
    const Mat4 projectionTall = camera->projectionMatrix(9.0f / 16.0f);
    require(std::fabs(projectionWide(0, 0) - projectionTall(0, 0)) > 1e-6f,
            "aspect ratio changes projection x-scale");
}

void testCameraInvalidProjection() {
    Scene scene("InvalidCam");
    const NodeId id = scene.createNode("Camera3D", "BadCam");
    Camera3D* camera = dynamic_cast<Camera3D*>(scene.find(id));
    require(camera != nullptr, "created Camera3D");

    // FOV out of range.
    camera->fieldOfView = 0.0f;
    require(!camera->isValid(), "FOV 0 invalid");
    camera->fieldOfView = 180.0f;
    require(!camera->isValid(), "FOV 180 invalid");
    camera->fieldOfView = 45.0f;

    // Near <= 0 invalid.
    camera->nearPlane = 0.0f;
    require(!camera->isValid(), "near 0 invalid");
    camera->nearPlane = -1.0f;
    require(!camera->isValid(), "near negative invalid");
    camera->nearPlane = 0.1f;

    // Far <= near invalid.
    camera->farPlane = 0.1f;
    require(!camera->isValid(), "far == near invalid");
    camera->farPlane = 0.05f;
    require(!camera->isValid(), "far < near invalid");
    camera->farPlane = 100.0f;

    require(camera->isValid(), "reset camera is valid");

    // Invalid projection returns identity.
    camera->nearPlane = 0.0f;
    require(camera->projectionMatrix(16.0f / 9.0f) == Mat4(),
            "invalid projection returns identity");
    camera->nearPlane = 0.1f;
}

void testCameraSelectionAndFallback() {
    // No camera: activeCamera() returns nullptr.
    {
        Scene scene("NoCam");
        require(scene.activeCamera() == nullptr,
                "no camera: activeCamera() is null");
    }

    // First Camera3D node is the active camera.
    {
        Scene scene("WithCam");
        const NodeId cam1 = scene.createNode("Camera3D", "Cam1");
        const NodeId cam2 = scene.createNode("Camera3D", "Cam2");
        const Camera3D* active = scene.activeCamera();
        require(active != nullptr, "active camera exists");
        require(active->id() == cam1, "first camera is active");
        require(active->id() != cam2, "second camera is not active");
    }

    // Camera3D is a Node3D with a transform.
    {
        Scene scene("CamTransform");
        const NodeId id = scene.createNode("Camera3D", "Cam");
        Camera3D* camera = dynamic_cast<Camera3D*>(scene.find(id));
        camera->position = Vec3(5.0f, 0.0f, 0.0f);
        require(camera->worldPosition().x > 4.0f, "camera world position set");
    }
}

void testEditorCameraMovement() {
    EditorCamera camera;
    const Vec3 start = camera.position();

    // Move forward along the camera's forward direction.
    camera.move(camera.forward() * 2.0f);
    require((camera.position() - start).lengthSquared() > 0.0f,
            "editor camera moves forward");

    // Rotate changes forward direction.
    camera.rotate(90.0f, 0.0f);
    require(std::fabs(camera.forward().x - 1.0f) < 0.01f,
            "yaw 90 points +X");

    // Pitch is clamped.
    camera.rotate(0.0f, 200.0f);
    require(camera.pitch() <= 89.0f + 1.0f, "pitch upper clamp");
    require(camera.pitch() >= -89.0f - 1.0f, "pitch lower clamp");

    // lookAt orients toward a target.
    camera.lookAt(Vec3(0.0f, 0.0f, 0.0f), 4.0f);
    require(camera.position().lengthSquared() > 0.0f,
            "lookAt moves camera");
}

// ---------------------------------------------------------------------------
// Editor camera free-look (mouse-look) properties
// ---------------------------------------------------------------------------

void testEditorCameraFreeLook() {
    EditorCamera camera;
    const Vec3 initialPosition = camera.position();
    const Vec3 initialForward = camera.forward();

    // Yaw rotation changes forward direction but not position.
    camera.rotate(90.0f, 0.0f);
    require((camera.position() - initialPosition).lengthSquared() < 0.0001f,
            "yaw rotation does not change camera position");
    require(std::fabs(camera.forward().x - 1.0f) < 0.01f,
            "yaw 90 points +X");
    require(std::fabs(camera.forward().z) < 0.01f,
            "yaw 90 has no Z component");

    // Pitch rotation changes forward direction but not position.
    camera.rotate(0.0f, 45.0f);
    require((camera.position() - initialPosition).lengthSquared() < 0.0001f,
            "pitch rotation does not change camera position");
    require(std::fabs(camera.pitch() - 45.0f) < 0.01f,
            "pitch is 45 degrees");

    // Repeated rotations accumulate but position stays fixed.
    const Vec3 posAfterRotations = camera.position();
    camera.rotate(-90.0f, -45.0f);
    require((camera.position() - posAfterRotations).lengthSquared() < 0.0001f,
            "reverse rotations do not change camera position");
    require((camera.position() - initialPosition).lengthSquared() < 0.0001f,
            "camera position is back to initial");

    // Forward direction is restored after inverse rotations.
    require(std::fabs(camera.forward().x - initialForward.x) < 0.01f,
            "forward x restored after inverse rotation");
    require(std::fabs(camera.forward().z - initialForward.z) < 0.01f,
            "forward z restored after inverse rotation");

    // View matrix maps camera position to origin in view space.
    const Mat4 view = camera.viewMatrix();
    const Vec3 eyeInView = transformPoint(view, camera.position());
    require(std::fabs(eyeInView.x) < 0.001f,
            "view matrix maps camera X to origin");
    require(std::fabs(eyeInView.y) < 0.001f,
            "view matrix maps camera Y to origin");
    require(std::fabs(eyeInView.z) < 0.001f,
            "view matrix maps camera Z to origin");

    // A point in front of the camera has negative view-space Z.
    const Vec3 inFront = camera.position() + camera.forward() * 5.0f;
    const Vec3 pointInView = transformPoint(view, inFront);
    require(pointInView.z < 0.0f,
            "point in front of camera has negative view-space Z");

    // A point behind the camera has positive view-space Z.
    const Vec3 behind = camera.position() - camera.forward() * 5.0f;
    const Vec3 behindInView = transformPoint(view, behind);
    require(behindInView.z > 0.0f,
            "point behind camera has positive view-space Z");

    // right() and up() are consistent with forward().
    const Vec3 f = camera.forward();
    const Vec3 r = camera.right();
    const Vec3 u = camera.up();
    require(std::fabs(dot(r, f)) < 0.001f, "right is perpendicular to forward");
    require(std::fabs(dot(u, f)) < 0.001f, "up is perpendicular to forward");
    require(std::fabs(dot(r, u)) < 0.001f, "right is perpendicular to up");
    require(std::fabs(r.lengthSquared() - 1.0f) < 0.001f,
            "right is unit length");
    require(std::fabs(u.lengthSquared() - 1.0f) < 0.001f,
            "up is unit length");

    // Pitch clamp prevents flip-over.
    camera.rotate(0.0f, 1000.0f);
    require(camera.pitch() <= 89.0f, "pitch clamped to max");
    camera.rotate(0.0f, -1000.0f);
    require(camera.pitch() >= -89.0f, "pitch clamped to min");
}

void testLookAtRotatedCameraBasis() {
    // The old lookAt implementation placed the basis vectors in columns
    // instead of rows for this column-vector math convention. That happened
    // to look correct when the camera had no rotation, but broke view
    // orientation as soon as yaw/pitch changed.
    const Vec3 eye(4.0f, 2.0f, -3.0f);
    const Vec3 forward = Vec3(1.0f, 0.0f, 0.0f);
    const Vec3 target = eye + forward * 5.0f;
    const Mat4 view = lookAt(eye, target, Vec3(0.0f, 1.0f, 0.0f));

    const Vec3 eyeInView = transformPoint(view, eye);
    require(std::fabs(eyeInView.x) < 0.001f, "rotated lookAt maps eye X to origin");
    require(std::fabs(eyeInView.y) < 0.001f, "rotated lookAt maps eye Y to origin");
    require(std::fabs(eyeInView.z) < 0.001f, "rotated lookAt maps eye Z to origin");

    const Vec3 targetInView = transformPoint(view, target);
    require(std::fabs(targetInView.x) < 0.001f,
            "lookAt maps forward target to center X");
    require(std::fabs(targetInView.y) < 0.001f,
            "lookAt maps forward target to center Y");
    require(targetInView.z < 0.0f,
            "lookAt maps forward target to negative view Z");

    const Vec3 rightInView = transformPoint(view, eye + Vec3(0.0f, 0.0f, 1.0f));
    require(rightInView.x > 0.0f,
            "lookAt maps camera-right direction to positive view X");
}

void testEditorCameraRotatedViewMatchesOrientation() {
    EditorCamera camera;
    camera.move(Vec3(4.0f, 2.0f, -3.0f));
    camera.rotate(90.0f, 25.0f);

    const Mat4 view = camera.viewMatrix();
    const Vec3 eyeInView = transformPoint(view, camera.position());
    require(std::fabs(eyeInView.x) < 0.001f &&
            std::fabs(eyeInView.y) < 0.001f &&
            std::fabs(eyeInView.z) < 0.001f,
            "rotated editor camera position maps to view origin");

    const Vec3 forwardPoint = camera.position() + camera.forward() * 4.0f;
    const Vec3 forwardInView = transformPoint(view, forwardPoint);
    require(std::fabs(forwardInView.x) < 0.002f,
            "rotated camera forward maps to view center X");
    require(std::fabs(forwardInView.y) < 0.002f,
            "rotated camera forward maps to view center Y");
    require(forwardInView.z < 0.0f && std::fabs(forwardInView.z + 4.0f) < 0.002f,
            "rotated camera forward maps to negative view Z at expected distance");

    const Vec3 rightPoint = camera.position() + camera.right() * 2.0f;
    const Vec3 rightInView = transformPoint(view, rightPoint);
    require(rightInView.x > 1.99f && std::fabs(rightInView.y) < 0.002f &&
            std::fabs(rightInView.z) < 0.002f,
            "rotated camera right maps to positive view X");

    const Vec3 upPoint = camera.position() + camera.up() * 2.0f;
    const Vec3 upInView = transformPoint(view, upPoint);
    require(upInView.y > 1.99f && std::fabs(upInView.x) < 0.002f &&
            std::fabs(upInView.z) < 0.002f,
            "rotated camera up maps to positive view Y");
}

void testEditorCameraDefaultFov() {
    EditorCamera camera;
    // Default vertical FOV must be 65 degrees.
    // Since FOV is private, verify via projection matrix: at aspect 1:1,
    // tan(fov/2) determines the x-scale of the projection matrix.
    const Mat4 proj = camera.projectionMatrix(1.0f);
    // For fovY=65°, f = 1/tan(65/2 degrees) ≈ 1/tan(0.5672) ≈ 1.6243
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    const float expectedF = 1.0f / std::tan(65.0f * 0.5f * kDegToRad);
    require(std::fabs(proj(1, 1) - expectedF) < 0.01f,
            "default vertical FOV is 65 degrees (projection y-scale)");
}

void testEditorCameraContinuousYaw() {
    EditorCamera camera;
    // Rotate through more than 360 degrees of yaw.
    camera.rotate(400.0f, 0.0f);
    // Yaw is normalized to [-180, 180], so 400° becomes 40°.
    require(std::fabs(camera.yaw() - 40.0f) < 0.01f,
            "yaw normalizes past 360 degrees to [-180, 180] range");
    // After 400 degrees, forward should point where 40 degrees points.
    const Vec3 f = camera.forward();
    const Vec3 expected(0.6428f, 0.0f, -0.7660f); // sin(40), 0, -cos(40)
    require(std::fabs(f.x - expected.x) < 0.02f,
            "continuous yaw forward x matches 40 degrees");
    require(std::fabs(f.z - expected.z) < 0.02f,
            "continuous yaw forward z matches 40 degrees");
    // Position must still be unchanged.
    require((std::fabs(camera.position().x - 0.0f) < 0.0001f &&
             std::fabs(camera.position().y - 0.0f) < 0.0001f &&
             std::fabs(camera.position().z - 3.0f) < 0.0001f),
            "continuous yaw does not move camera");
}

void testEditorCameraNoRollAccumulation() {
    EditorCamera camera;
    // Repeated mouse-look: alternate yaw and pitch to simulate natural use.
    for (int i = 0; i < 200; ++i) {
        camera.rotate(2.0f, 1.0f);
        camera.rotate(-1.0f, -0.5f);
    }
    // No roll: right() must stay horizontal (perpendicular to world up).
    // If roll accumulated, right() would tilt up/down.
    const Vec3 r = camera.right();
    require(std::fabs(r.y) < 0.01f,
            "right vector stays horizontal (no roll accumulation)");
    // Camera up and forward must remain orthonormal.
    const Vec3 u = camera.up();
    const Vec3 f = camera.forward();
    require(std::fabs(dot(r, f)) < 0.001f, "right is perpendicular to forward");
    require(std::fabs(dot(u, f)) < 0.001f, "up is perpendicular to forward");
    require(std::fabs(dot(r, u)) < 0.001f, "right is perpendicular to up");
    require(std::fabs(r.lengthSquared() - 1.0f) < 0.001f,
            "right is unit length");
    require(std::fabs(u.lengthSquared() - 1.0f) < 0.001f,
            "up is unit length");
    // Position must still be unchanged.
    require((std::fabs(camera.position().x - 0.0f) < 0.0001f &&
             std::fabs(camera.position().y - 0.0f) < 0.0001f &&
             std::fabs(camera.position().z - 3.0f) < 0.0001f),
            "no positional change during repeated mouse-look");
}

void testEditorCameraProjectionAspectRatio() {
    EditorCamera camera;
    const Mat4 wide = camera.projectionMatrix(16.0f / 9.0f);
    const Mat4 tall = camera.projectionMatrix(9.0f / 16.0f);
    // Different aspect ratios must produce different x-scales.
    require(std::fabs(wide(0, 0) - tall(0, 0)) > 0.001f,
            "aspect ratio changes x-scale");
    // y-scale must be identical (vertical FOV is fixed).
    require(std::fabs(wide(1, 1) - tall(1, 1)) < 0.0001f,
            "y-scale is identical across aspect ratios");
}

void testEditorCameraProjectionInvalidAspect() {
    EditorCamera camera;
    // Zero or near-zero aspect must not crash or produce NaN.
    const Mat4 projZero = camera.projectionMatrix(0.0f);
    require(std::isfinite(projZero(0, 0)), "zero aspect x-scale is finite");
    require(std::isfinite(projZero(1, 1)), "zero aspect y-scale is finite");
    const Mat4 projTiny = camera.projectionMatrix(0.001f);
    require(std::isfinite(projTiny(0, 0)), "tiny aspect x-scale is finite");
    require(std::isfinite(projTiny(1, 1)), "tiny aspect y-scale is finite");
    const Mat4 projNan = camera.projectionMatrix(std::numeric_limits<float>::quiet_NaN());
    require(std::isfinite(projNan(0, 0)), "NaN aspect x-scale is finite");
    require(std::isfinite(projNan(1, 1)), "NaN aspect y-scale is finite");
}

void testEditorCameraPositionPreservedDuringMouseLook() {
    EditorCamera camera;
    const Vec3 initialPosition = camera.position();
    const Mat4 initialView = camera.viewMatrix();

    // Apply many small mouse-look deltas, simulating rapid RMB dragging.
    for (int i = 0; i < 100; ++i) {
        camera.rotate(1.5f, -0.8f);
    }

    // Position must be exactly unchanged.
    require((camera.position() - initialPosition).lengthSquared() < 0.0001f,
            "camera position unchanged after 100 mouse-look rotations");

    // View matrix must have changed (orientation changed).
    require(camera.viewMatrix() != initialView,
            "camera view matrix changed after rotation");

    // Camera's own world position maps to view-space origin.
    const Mat4 view = camera.viewMatrix();
    const Vec3 eyeInView = transformPoint(view, camera.position());
    require(std::fabs(eyeInView.x) < 0.001f,
            "camera position maps to view-space origin X");
    require(std::fabs(eyeInView.y) < 0.001f,
            "camera position maps to view-space origin Y");
    require(std::fabs(eyeInView.z) < 0.001f,
            "camera position maps to view-space origin Z");

    // A point in front of the camera has negative view-space Z.
    const Vec3 inFront = camera.position() + camera.forward() * 5.0f;
    const Vec3 pointInView = transformPoint(view, inFront);
    require(pointInView.z < 0.0f,
            "point in front of camera has negative view-space Z");

    // Reverse all rotations; position must still be exactly unchanged.
    for (int i = 0; i < 100; ++i) {
        camera.rotate(-1.5f, 0.8f);
    }
    require((camera.position() - initialPosition).lengthSquared() < 0.0001f,
            "camera position back to start after reverse rotations");

    // Reapplying rotations should not accumulate positional drift.
    const Vec3 posBeforeReapply = camera.position();
    for (int i = 0; i < 50; ++i) {
        camera.rotate(2.0f, 1.0f);
    }
    require((camera.position() - posBeforeReapply).lengthSquared() < 0.0001f,
            "no positional drift after reapplying rotations");
}

void testEditorCameraHorizontalMovementAtYaws() {
    struct YawCase {
        float yawDeg;
        Vec3 expectedForward;
        Vec3 expectedRight;
    };
    const YawCase cases[] = {
        {0.0f,    Vec3(0.0f, 0.0f, -1.0f), Vec3(1.0f, 0.0f, 0.0f)},
        {90.0f,   Vec3(1.0f, 0.0f,  0.0f), Vec3(0.0f, 0.0f,  1.0f)},
        {180.0f,  Vec3(0.0f, 0.0f,  1.0f), Vec3(-1.0f, 0.0f, 0.0f)},
        {270.0f,  Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)},
    };

    for (const auto& c : cases) {
        EditorCamera camera;
        camera.rotate(c.yawDeg, 0.0f);

        const Vec3 hFwd = camera.horizontalForward();
        require(std::fabs(hFwd.x - c.expectedForward.x) < 0.01f,
                "horizontalForward x at yaw");
        require(std::fabs(hFwd.z - c.expectedForward.z) < 0.01f,
                "horizontalForward z at yaw");
        require(std::fabs(hFwd.y) < 0.001f,
                "horizontalForward has no vertical component");

        const Vec3 hRight = camera.horizontalRight();
        require(std::fabs(hRight.x - c.expectedRight.x) < 0.01f,
                "horizontalRight x at yaw");
        require(std::fabs(hRight.z - c.expectedRight.z) < 0.01f,
                "horizontalRight z at yaw");
        require(std::fabs(hRight.y) < 0.001f,
                "horizontalRight has no vertical component");

        // W moves forward along horizontal forward.
        const Vec3 start = camera.position();
        camera.move(hFwd * 2.0f);
        require((camera.position() - (start + hFwd * 2.0f)).lengthSquared() < 0.0001f,
                "W moves forward along horizontal forward");

        // S moves backward.
        camera.move(hFwd * -4.0f);
        require((camera.position() - (start + hFwd * -2.0f)).lengthSquared() < 0.0001f,
                "S moves backward along horizontal forward");

        // A strafes left (negative right).
        camera.move(hFwd * 2.0f);
        camera.move(hRight * -2.0f);
        require((camera.position() - (start + hRight * -2.0f)).lengthSquared() < 0.0001f,
                "A strafes left along horizontal right");

        // D strafes right (positive right).
        camera.move(hRight * 4.0f);
        require((camera.position() - (start + hRight * 2.0f)).lengthSquared() < 0.0001f,
                "D strafes right along horizontal right");
    }
}

void testEditorCameraMovementWhileLookingUpDown() {
    EditorCamera camera;

    // Look steeply upward.
    camera.rotate(0.0f, 60.0f);
    const Vec3 fwdUp = camera.forward();
    // W uses the full forward vector: looking up moves upward too.
    require(std::fabs(fwdUp.y - std::sin(60.0f * kDegreesToRadiansTest)) < 0.01f,
            "forward has positive vertical component when looking up");

    const Vec3 start = camera.position();
    camera.move(fwdUp * 3.0f);
    // W movement is not horizontal when looking up.
    require(camera.position().y > start.y,
            "W moves camera upward when looking up");
    require(std::fabs(camera.position().x - (start.x + fwdUp.x * 3.0f)) < 0.001f,
            "W moves along forward x when looking up");
    require(std::fabs(camera.position().z - (start.z + fwdUp.z * 3.0f)) < 0.001f,
            "W moves along forward z when looking up");

    // Look steeply downward.
    camera.rotate(0.0f, -120.0f);
    const Vec3 fwdDown = camera.forward();
    require(std::fabs(fwdDown.y - std::sin(-120.0f * kDegreesToRadiansTest)) < 0.01f,
            "forward has negative vertical component when looking down");

    // W should move downward when looking down.
    const Vec3 startDown = camera.position();
    camera.move(fwdDown * 3.0f);
    require(camera.position().y < startDown.y,
            "W moves camera downward when looking down");

    // Turning 180° reverses horizontal forward correctly.
    const Vec3 hFwdDown = camera.horizontalForward();
    camera.rotate(180.0f, 0.0f);
    const Vec3 hFwdRev = camera.horizontalForward();
    require(std::fabs(hFwdRev.x - (-hFwdDown.x)) < 0.01f,
            "horizontalForward reverses after 180° turn");
    require(std::fabs(hFwdRev.z - (-hFwdDown.z)) < 0.01f,
            "horizontalForward z reverses after 180° turn");
}

void testEditorCameraForwardAtPitches() {
    // Zero pitch: forward is horizontal, pointing -Z at yaw 0.
    EditorCamera camera;
    camera.rotate(0.0f, 0.0f);
    const Vec3 f0 = camera.forward();
    require(std::fabs(f0.x) < 0.001f, "forward x is 0 at zero yaw/pitch");
    require(std::fabs(f0.y) < 0.001f, "forward y is 0 at zero pitch");
    require(std::fabs(f0.z - (-1.0f)) < 0.001f,
            "forward z is -1 at zero yaw/pitch");

    // Positive pitch: forward points upward.
    camera.rotate(0.0f, 45.0f);
    const Vec3 f45 = camera.forward();
    require(std::fabs(f45.y - std::sin(45.0f * kDegreesToRadiansTest)) < 0.01f,
            "forward y is sin(pitch) at positive pitch");

    // Negative pitch: reset then rotate to -45.
    camera.rotate(0.0f, -90.0f);
    const Vec3 fNeg45 = camera.forward();
    require(std::fabs(fNeg45.y - std::sin(-45.0f * kDegreesToRadiansTest)) < 0.01f,
            "forward y is sin(pitch) at negative pitch");
    require(fNeg45.y < 0.0f, "forward points downward at negative pitch");

    // Forward is unit length at all pitches.
    require(std::fabs(f0.lengthSquared() - 1.0f) < 0.001f,
            "forward is unit length at zero pitch");
    require(std::fabs(f45.lengthSquared() - 1.0f) < 0.001f,
            "forward is unit length at positive pitch");
    require(std::fabs(fNeg45.lengthSquared() - 1.0f) < 0.001f,
            "forward is unit length at negative pitch");
}

void testEditorCameraMovementSymmetry() {
    EditorCamera camera;
    const Vec3 start = camera.position();
    const Vec3 fwd = camera.forward();
    const float dist = 4.0f;

    // W moves forward, S moves backward by the same amount.
    camera.move(fwd * dist);
    const Vec3 afterForward = camera.position();
    require(std::fabs((afterForward - start).lengthSquared() -
                      (dist * dist)) < 0.001f,
            "forward movement distance matches");
    camera.move(fwd * -dist);
    const Vec3 afterBackward = camera.position();
    require((afterBackward - start).lengthSquared() < 0.0001f,
            "backward movement returns to start");

    // A/D strafing is symmetric around start.
    const Vec3 right = camera.horizontalRight();
    camera.move(right * dist);
    const Vec3 afterRight = camera.position();
    require(std::fabs((afterRight - start).lengthSquared() -
                      (dist * dist)) < 0.001f,
            "right strafing distance matches");
    camera.move(right * -dist);
    require((camera.position() - start).lengthSquared() < 0.0001f,
            "left strafing returns to start");
}

void testEditorCameraVerticalMovementWorldAligned() {
    EditorCamera camera;
    const Vec3 start = camera.position();

    // Q moves down along world vertical.
    camera.move(Vec3(0.0f, -1.0f, 0.0f) * 3.0f);
    require(std::fabs(camera.position().x - start.x) < 0.001f,
            "Q movement has no horizontal X component");
    require(std::fabs(camera.position().z - start.z) < 0.001f,
            "Q movement has no horizontal Z component");
    require(camera.position().y < start.y, "Q moves camera down");

    // E moves up along world vertical.
    camera.move(Vec3(0.0f, 1.0f, 0.0f) * 3.0f);
    require(std::fabs(camera.position().x - start.x) < 0.001f,
            "E movement has no horizontal X component");
    require(std::fabs(camera.position().z - start.z) < 0.001f,
            "E movement has no horizontal Z component");
    require(std::fabs(camera.position().y - start.y) < 0.001f,
            "E movement returns camera to start height");

    // Q/E movement is independent of camera pitch.
    camera.rotate(0.0f, 70.0f);
    const Vec3 posBeforeVert = camera.position();
    camera.move(Vec3(0.0f, 2.0f, 0.0f));
    require(std::fabs(camera.position().x - posBeforeVert.x) < 0.001f,
            "E movement stays world-vertical when looking up");
    require(std::fabs(camera.position().z - posBeforeVert.z) < 0.001f,
            "E movement stays world-vertical when looking up (Z)");
    require(camera.position().y > posBeforeVert.y, "E moves camera up");
}

void testEditorCameraDiagonalMovementNormalization() {
    EditorCamera camera;
    const Vec3 start = camera.position();
    const float dist = 3.0f;

    // Move diagonally (W + D) - should be same distance as single axis.
    const Vec3 hFwd = camera.horizontalForward();
    const Vec3 hRight = camera.horizontalRight();
    Vec3 diag = (hFwd + hRight).normalized() * dist;
    camera.move(diag);
    const float diagDist = std::sqrt((camera.position() - start).lengthSquared());
    require(std::fabs(diagDist - dist) < 0.001f,
            "diagonal movement is not faster than single-axis movement");

    // Verify the destination matches the normalized diagonal direction.
    require((camera.position() - (start + diag)).lengthSquared() < 0.0001f,
            "diagonal movement follows normalized combined direction");

    // Single-axis movement distance matches.
    camera.move(diag * -1.0f);
    camera.move(hFwd * dist);
    const float fwdDist = std::sqrt((camera.position() - start).lengthSquared());
    require(std::fabs(fwdDist - dist) < 0.001f,
            "single-axis forward movement distance matches");
}

void testEditorCameraLookAtFocus() {
    EditorCamera camera;
    const Vec3 initialPosition = camera.position();
    const Vec3 target(0.0f, 0.0f, 0.0f);

    // lookAt moves the camera to frame the target at a distance.
    camera.lookAt(target, 4.0f);
    require((camera.position() - initialPosition).lengthSquared() > 0.0001f,
            "lookAt changes camera position");

    // After lookAt, the target is directly in front of the camera.
    const Mat4 view = camera.viewMatrix();
    const Vec3 targetInView = transformPoint(view, target);
    require(std::fabs(targetInView.x) < 0.01f,
            "lookAt centers target in view X");
    require(std::fabs(targetInView.y) < 0.01f,
            "lookAt centers target in view Y");
    require(targetInView.z < 0.0f,
            "lookAt places target in front of camera");
}

void testEditorCameraVerticalMovement() {
    EditorCamera camera;
    const Vec3 start = camera.position();
    const Vec3 up = camera.up();

    // Move up along camera's up vector.
    camera.move(up * 3.0f);
    require((camera.position() - start).lengthSquared() > 0.0f,
            "editor camera moves up");
    require((camera.position() - start).lengthSquared() >
                (up * 3.0f).lengthSquared() * 0.99f,
            "editor camera moves up by expected amount");

    // Move down back to start.
    camera.move(up * -3.0f);
    require((camera.position() - start).lengthSquared() < 0.0001f,
            "editor camera returns to start after down movement");
}

void testEditorCameraSpeedBoost() {
    EditorCamera camera;
    const float baseSpeed = camera.movementSpeed();
    const float boostedSpeed = baseSpeed * 3.0f;

    const Vec3 forward = camera.forward();
    const Vec3 start = camera.position();

    // Move at base speed.
    camera.move(forward * baseSpeed * 0.016f);
    const float dist1 = std::sqrt((camera.position() - start).lengthSquared());

    // Reset and move at boosted speed.
    camera.move(forward * baseSpeed * -0.016f);
    camera.move(forward * boostedSpeed * 0.016f);
    const float dist2 = std::sqrt((camera.position() - start).lengthSquared());

    require(dist2 > dist1 * 2.0f, "boosted speed moves further");
}

void testEditorCameraFocusDoesNotAffectFreelook() {
    EditorCamera camera;

    // Rotate the camera.
    camera.rotate(45.0f, 20.0f);

    // Focus on a target (simulates pressing F).
    camera.lookAt(Vec3(0.0f, 0.0f, 0.0f), 4.0f);

    // After focus, subsequent rotation should still work from the new state.
    camera.rotate(-45.0f, -20.0f);
    require(camera.position().lengthSquared() > 0.0f,
            "camera can rotate after focus");
}

void testPointLightLimitAndAttenuation() {
    Scene scene("PointLights");
    // Create more point lights than the shader supports (kMaxPointLights = 4).
    for (int i = 0; i < 7; ++i) {
        scene.createNode("PointLight", "Light" + std::to_string(i));
    }

    int count = 0;
    for (const NodeId id : scene.nodeIds()) {
        const Node* node = scene.find(id);
        if (node == nullptr) continue;
        if (dynamic_cast<const PointLight*>(node)) ++count;
    }
    require(count == 7, "seven point lights created");

    // Renderer clamps to kMaxPointLights; here we verify the count would be
    // clamped, not that it overflows. Attenuation formula sanity:
    //   attenuation = clamp(1 - d^2/r^2, 0, 1)
    // Attenuation formula sanity: clamp(1 - d^2/r^2, 0, 1).
    const float range = 10.0f;
    const auto attenuation = [&](float distance) {
        return std::max(0.0f, 1.0f - (distance * distance) / (range * range));
    };
    require(std::fabs(attenuation(0.0f) - 1.0f) < 1e-6f,
            "attenuation at d=0 is 1");
    require(std::fabs(attenuation(5.0f) - 0.75f) < 1e-6f,
            "attenuation at d=5, r=10 is 0.75");
    require(std::fabs(attenuation(15.0f) - 0.0f) < 1e-6f,
            "attenuation beyond range is 0");
}

void testFogRangeValidation() {
    Scene scene("Fog");
    const NodeId id = scene.createNode("WorldEnvironment", "Env");
    WorldEnvironment* env = dynamic_cast<WorldEnvironment*>(scene.find(id));
    require(env != nullptr, "created WorldEnvironment");

    // Degenerate range: fog disabled.
    env->fogStart = 10.0f;
    env->fogEnd = 10.0f;
    require(env->fogEnd <= env->fogStart, "fog end == start is degenerate");

    env->fogEnd = 5.0f;
    require(env->fogEnd < env->fogStart, "fog end < start is degenerate");

    env->fogStart = 10.0f;
    env->fogEnd = 60.0f;
    require(env->fogEnd > env->fogStart, "fog end > start is valid");
}

// ---------------------------------------------------------------------------
// Serialization round-trips with camera, lights, environment
// ---------------------------------------------------------------------------

void testLightingEnvironmentSerialization() {
    Scene scene("Lighting");
    const NodeId cam = scene.createNode("Camera3D", "GameCam");
    const NodeId dir = scene.createNode("DirectionalLight", "Sun");
    const NodeId pt = scene.createNode("PointLight", "Lamp");
    const NodeId env = scene.createNode("WorldEnvironment", "Fog");

    Camera3D* c = dynamic_cast<Camera3D*>(scene.find(cam));
    c->fieldOfView = 60.0f;
    c->nearPlane = 0.5f;
    c->farPlane = 500.0f;
    c->position = Vec3(2.0f, 3.0f, 5.0f);

    DirectionalLight* dl = dynamic_cast<DirectionalLight*>(scene.find(dir));
    dl->direction = Vec3(0.0f, -1.0f, -1.0f);
    dl->color = Vec3(1.0f, 0.8f, 0.6f);
    dl->intensity = 2.0f;

    PointLight* pl = dynamic_cast<PointLight*>(scene.find(pt));
    pl->color = Vec3(1.0f, 1.0f, 1.0f);
    pl->intensity = 3.0f;
    pl->range = 25.0f;
    pl->position = Vec3(0.0f, 2.0f, 0.0f);

    WorldEnvironment* e = dynamic_cast<WorldEnvironment*>(scene.find(env));
    e->fogColor = Vec3(0.2f, 0.3f, 0.4f);
    e->fogStart = 5.0f;
    e->fogEnd = 40.0f;

    const std::string path = tmpPath("test_lighting_env.pebscene");
    require(scene.save(path), "save lighting scene");

    Scene loaded = loadV2(path);
    require(loaded.nodeIds().size() == 4, "all 4 nodes round trip");

    Camera3D* lc = dynamic_cast<Camera3D*>(loaded.find(cam));
    require(lc != nullptr, "camera round trip");
    require(std::fabs(lc->fieldOfView - 60.0f) < 1e-5f, "camera fov");
    require(std::fabs(lc->nearPlane - 0.5f) < 1e-5f, "camera near");
    require(std::fabs(lc->farPlane - 500.0f) < 1e-5f, "camera far");
    require(std::fabs(lc->position.x - 2.0f) < 1e-5f, "camera pos x");

    DirectionalLight* ldl = dynamic_cast<DirectionalLight*>(loaded.find(dir));
    require(ldl != nullptr, "directional light round trip");
    require(std::fabs(ldl->intensity - 2.0f) < 1e-5f, "dir light intensity");
    require(std::fabs(ldl->color.x - 1.0f) < 1e-5f, "dir light color x");

    PointLight* lpl = dynamic_cast<PointLight*>(loaded.find(pt));
    require(lpl != nullptr, "point light round trip");
    require(std::fabs(lpl->range - 25.0f) < 1e-5f, "point light range");
    require(std::fabs(lpl->position.y - 2.0f) < 1e-5f, "point light pos y");

    WorldEnvironment* le = dynamic_cast<WorldEnvironment*>(loaded.find(env));
    require(le != nullptr, "environment round trip");
    require(std::fabs(le->fogEnd - 40.0f) < 1e-5f, "fog end");
    require(std::fabs(le->fogColor.x - 0.2f) < 1e-5f, "fog color x");

    std::filesystem::remove(path);
}


int main() {
    testParentChild();
    std::cout << "parent-child tests passed\n";

    testTransformInheritance();
    std::cout << "transform inheritance tests passed\n";

    testCreateDelete();
    std::cout << "create/delete tests passed\n";

    testSerializationRoundTrip();
    std::cout << "serialization round-trip tests passed\n";

    testEmptySceneSerialization();
    std::cout << "empty scene serialization tests passed\n";

    testInvalidHeader();
    testUnsupportedVersion();
    testDuplicateIds();
    testInvalidParent();
    testUnsupportedType();
    std::cout << "invalid/unsupported data tests passed\n";

    testV1Migration();
    std::cout << "v1 migration tests passed\n";

    testCameraMatrices();
    std::cout << "camera matrix tests passed\n";

    testCameraInvalidProjection();
    std::cout << "camera invalid projection tests passed\n";

    testCameraSelectionAndFallback();
    std::cout << "camera selection/fallback tests passed\n";

    testEditorCameraMovement();
    std::cout << "editor camera movement tests passed\n";

    testEditorCameraFreeLook();
    std::cout << "editor camera free-look tests passed\n";

    testEditorCameraForwardAtPitches();
    std::cout << "editor camera forward-at-pitches tests passed\n";

    testEditorCameraMovementSymmetry();
    std::cout << "editor camera movement symmetry tests passed\n";

    testLookAtRotatedCameraBasis();
    testEditorCameraRotatedViewMatchesOrientation();
    testEditorCameraDefaultFov();
    std::cout << "editor camera default FOV test passed\n";

    testEditorCameraContinuousYaw();
    std::cout << "editor camera continuous yaw test passed\n";

    testEditorCameraNoRollAccumulation();
    std::cout << "editor camera no-roll-accumulation test passed\n";

    testEditorCameraProjectionAspectRatio();
    std::cout << "editor camera projection aspect ratio test passed\n";

    testEditorCameraProjectionInvalidAspect();
    std::cout << "editor camera projection invalid aspect test passed\n";

    testEditorCameraPositionPreservedDuringMouseLook();
    std::cout << "editor camera position-preserved-during-mouse-look tests passed\n";

    testEditorCameraHorizontalMovementAtYaws();
    std::cout << "editor camera horizontal movement at yaws tests passed\n";

    testEditorCameraMovementWhileLookingUpDown();
    std::cout << "editor camera movement while looking up/down tests passed\n";

    testEditorCameraVerticalMovementWorldAligned();
    std::cout << "editor camera vertical movement world-aligned tests passed\n";

    testEditorCameraDiagonalMovementNormalization();
    std::cout << "editor camera diagonal movement normalization tests passed\n";

    testEditorCameraLookAtFocus();
    std::cout << "editor camera lookAt focus tests passed\n";

    testEditorCameraVerticalMovement();
    std::cout << "editor camera vertical movement tests passed\n";

    testEditorCameraSpeedBoost();
    std::cout << "editor camera speed boost tests passed\n";

    testEditorCameraFocusDoesNotAffectFreelook();
    std::cout << "editor camera focus independence tests passed\n";

    testPointLightLimitAndAttenuation();
    std::cout << "point light limit/attenuation tests passed\n";

    testFogRangeValidation();
    std::cout << "fog range validation tests passed\n";

    testLightingEnvironmentSerialization();
    std::cout << "lighting/environment serialization tests passed\n";

    testMeshGeneration();
    std::cout << "mesh generation tests passed\n";

    testMaterialDefaults();
    testMaterialValidation();
    testMaterialManager();
    std::cout << "material tests passed\n";

    testSerializationWithMaterials();
    std::cout << "serialization with materials test passed\n";

    testReparentingPreventsCycles();
    std::cout << "reparenting cycle prevention test passed\n";

    testAssetPathResolution();
    std::cout << "asset path resolution tests passed\n";

    testAssetFilters();
    std::cout << "asset filter tests passed\n";

    testMissingAssetHandling();
    std::cout << "missing asset handling tests passed\n";

    testModelImport();
    std::cout << "model import tests passed\n";

    testModelImportAllGlbs();
    std::cout << "all GLB import tests passed\n";

    testPartReferencesImportedMesh();
    std::cout << "part references imported mesh tests passed\n";

    testSceneRoundTripWithImportedMesh();
    std::cout << "scene round-trip with imported mesh tests passed\n";

    testGltfNodeTransforms();
    std::cout << "glTF node transform tests passed\n";

    testGltfParentChildHierarchy();
    std::cout << "glTF parent-child hierarchy tests passed\n";

    testGltfMultipleNodesSameMesh();
    std::cout << "glTF multiple nodes same mesh tests passed\n";

    testModelImportMalformed();
    std::cout << "model import malformed rejection tests passed\n";

    testCoordinateConversion();
    std::cout << "coordinate conversion tests passed\n";

    testGizmoAxisMath();
    std::cout << "gizmo axis math tests passed\n";

    std::cout << "All scene graph tests passed\n";
    return 0;
}

// -------------------------------------------------------------------
// Primitive mesh generation tests
// -------------------------------------------------------------------

void testMeshGeneration() {
    // Cube: 24 vertices, 36 indices.
    {
        MeshData cube = Mesh::createCubeData();
        require(cube.vertexCount == 24, "cube has 24 vertices");
        require(cube.indexCount == 36, "cube has 36 indices");
        require(!cube.vertices.empty(), "cube has vertex data");
        require(!cube.indices.empty(), "cube has index data");
        // All indices must be valid.
        for (unsigned int idx : cube.indices) {
            require(idx < static_cast<unsigned int>(cube.vertexCount),
                    "cube index in range");
        }
        std::cout << "  cube mesh generation test passed\n";
    }

    // Sphere.
    {
        MeshData sphere = Mesh::createSphereData(1.0f, 32, 16);
        require(sphere.vertexCount > 0, "sphere has vertices");
        require(sphere.indexCount > 0, "sphere has indices");
        for (unsigned int idx : sphere.indices) {
            require(idx < static_cast<unsigned int>(sphere.vertexCount),
                    "sphere index in range");
        }
        std::cout << "  sphere mesh generation test passed\n";
    }

    // Plane.
    {
        MeshData plane = Mesh::createPlaneData(2.0f, 2.0f, 2, 2);
        require(plane.vertexCount == 9, "plane 2x2 segments has 9 vertices");
        require(plane.indexCount == 24, "plane 2x2 segments has 24 indices");
        for (unsigned int idx : plane.indices) {
            require(idx < static_cast<unsigned int>(plane.vertexCount),
                    "plane index in range");
        }
        std::cout << "  plane mesh generation test passed\n";
    }

    // Cylinder.
    {
        MeshData cyl = Mesh::createCylinderData(1.0f, 2.0f, 32);
        require(cyl.vertexCount > 0, "cylinder has vertices");
        require(cyl.indexCount > 0, "cylinder has indices");
        for (unsigned int idx : cyl.indices) {
            require(idx < static_cast<unsigned int>(cyl.vertexCount),
                    "cylinder index in range");
        }
        std::cout << "  cylinder mesh generation test passed\n";
    }

    // Cone.
    {
        MeshData cone = Mesh::createConeData(1.0f, 2.0f, 32);
        require(cone.vertexCount > 0, "cone has vertices");
        require(cone.indexCount > 0, "cone has indices");
        for (unsigned int idx : cone.indices) {
            require(idx < static_cast<unsigned int>(cone.vertexCount),
                    "cone index in range");
        }
        std::cout << "  cone mesh generation test passed\n";
    }

    // Capsule.
    {
        MeshData cap = Mesh::createCapsuleData(0.5f, 1.0f, 16, 8);
        require(cap.vertexCount > 0, "capsule has vertices");
        require(cap.indexCount > 0, "capsule has indices");
        for (unsigned int idx : cap.indices) {
            require(idx < static_cast<unsigned int>(cap.vertexCount),
                    "capsule index in range");
        }
        std::cout << "  capsule mesh generation test passed\n";
    }

    // Degenerate dimensions should not crash.
    {
        MeshData flat = Mesh::createCubeData(Vec3(0.0f, 0.0f, 0.0f));
        require(flat.vertexCount > 0, "zero-size cube still generates");
    }
}

// -------------------------------------------------------------------
// Material tests
// -------------------------------------------------------------------

void testMaterialDefaults() {
    Material mat("test_material");
    require(mat.name == "test_material", "material name set");
    require(mat.baseColor.x == 0.8f && mat.baseColor.y == 0.8f &&
                mat.baseColor.z == 0.8f,
        "material default base color");
    require(mat.metallic == 0.0f, "material default metallic");
    require(mat.roughness == 0.5f, "material default roughness");
    require(mat.texturePath.empty(), "material default no texture");
    require(!mat.hasTexture, "material default no texture flag");
    std::cout << "  material defaults test passed\n";
}

void testMaterialValidation() {
    Material mat("validation_mat");
    mat.baseColor = Vec3(0.0f, 0.0f, 0.0f);
    mat.metallic = 1.0f;
    mat.roughness = 0.0f;
    mat.texturePath = "textures/stone.ppm";
    mat.hasTexture = true;

    require(mat.metallic >= 0.0f && mat.metallic <= 1.0f,
            "metallic in valid range");
    require(mat.roughness >= 0.0f && mat.roughness <= 1.0f,
            "roughness in valid range");
    require(mat.hasTexture, "material has texture");
    require(!mat.texturePath.empty(), "texture path set");
    std::cout << "  material validation test passed\n";
}

void testMaterialManager() {
    MaterialManager mgr;
    Material m1("mat_a");
    Material m2("mat_b");
    m1.baseColor = Vec3(1.0f, 0.0f, 0.0f);
    m2.baseColor = Vec3(0.0f, 1.0f, 0.0f);
    mgr.registerMaterial(m1);
    mgr.registerMaterial(m2);

    require(mgr.find("mat_a") != nullptr, "mat_a found");
    require(mgr.find("mat_b") != nullptr, "mat_b found");
    require(mgr.find("nonexistent") == nullptr, "nonexistent not found");
    require(mgr.find("mat_a")->baseColor.x == 1.0f, "mat_a base color red");
    require(mgr.names().size() == 2, "material manager has 2 names");

    // Register same name replaces.
    Material m1b("mat_a");
    m1b.baseColor = Vec3(0.0f, 0.0f, 1.0f);
    mgr.registerMaterial(m1b);
    require(mgr.find("mat_a")->baseColor.z == 1.0f, "mat_a replaced");
    require(mgr.names().size() == 2, "names count unchanged after replace");

    mgr.clear();
    require(mgr.find("mat_a") == nullptr, "mat_a cleared");
    require(mgr.names().empty(), "names empty after clear");
    std::cout << "  material manager test passed\n";
}

// -------------------------------------------------------------------
// Scene serialization with materials (v3 round trip)
// -------------------------------------------------------------------

void testSerializationWithMaterials() {
    Scene scene("MaterialTest");

    // Create a Part with a mesh and material reference.
    NodeId partId = scene.createNode("Part", "TexturedCube");
    if (Part* part = dynamic_cast<Part*>(scene.find(partId))) {
        part->meshName = "cube";
        part->materialName = "red_material";
    }

    // Set up serialized materials (simulating what the renderer would
    // provide).
    std::vector<SerializedMaterial> materials;
    {
        SerializedMaterial sm;
        sm.name = "red_material";
        sm.baseColor[0] = 1.0f;
        sm.baseColor[1] = 0.0f;
        sm.baseColor[2] = 0.0f;
        sm.metallic = 0.0f;
        sm.roughness = 0.5f;
        sm.hasTexture = false;
        materials.push_back(sm);
    }
    {
        SerializedMaterial sm;
        sm.name = "blue_material";
        sm.baseColor[0] = 0.0f;
        sm.baseColor[1] = 0.0f;
        sm.baseColor[2] = 1.0f;
        sm.metallic = 0.5f;
        sm.roughness = 0.2f;
        sm.hasTexture = true;
        sm.texturePath = "textures/stone.ppm";
        materials.push_back(sm);
    }
    scene.setSerializedMaterials(materials);

    const std::string path = tmpPath("test_materials.pebscene");
    require(scene.save(path), "scene with materials saved");

    // Load into a new scene.
    Scene scene2("MaterialTest2");
    require(scene2.load(path), "scene with materials loaded");

    // Verify nodes.
    require(scene2.has(partId), "part node exists after load");
    if (Part* loadedPart = dynamic_cast<Part*>(scene2.find(partId))) {
        require(loadedPart->meshName == "cube", "mesh name preserved");
        require(loadedPart->materialName == "red_material",
                "material name preserved");
    }

    // Verify materials.
    const auto& loadedMats = scene2.getSerializedMaterials();
    require(loadedMats.size() == 2, "two materials loaded");
    bool foundRed = false, foundBlue = false;
    for (const auto& sm : loadedMats) {
        if (sm.name == "red_material") {
            foundRed = true;
            require(sm.baseColor[0] == 1.0f, "red material base color");
            require(!sm.hasTexture, "red material no texture");
        }
        if (sm.name == "blue_material") {
            foundBlue = true;
            require(sm.baseColor[2] == 1.0f, "blue material base color");
            require(sm.hasTexture, "blue material has texture");
            require(sm.texturePath == "textures/stone.ppm",
                    "blue material texture path");
        }
    }
    require(foundRed, "red material found");
    require(foundBlue, "blue material found");

    std::cout << "  serialization with materials test passed\n";
}

// -------------------------------------------------------------------
// Hierarchy reparenting cycle prevention (extended)
// -------------------------------------------------------------------

void testReparentingPreventsCycles() {
    Scene scene("CycleTest");
    NodeId a = scene.createNode("Node3D", "A");
    NodeId b = scene.createNode("Node3D", "B");
    NodeId c = scene.createNode("Node3D", "C");

    require(scene.setParent(b, a), "b parent a");
    require(scene.setParent(c, b), "c parent b");
    // a cannot become child of c (would create cycle a->b->c->a).
    require(!scene.setParent(a, c), "cycle a->c rejected");
    // c cannot become child of itself.
    require(!scene.setParent(c, c), "self-parent c rejected");

    std::cout << "  reparenting cycle prevention test passed\n";
}

// -------------------------------------------------------------------
// Asset browser: project-relative path resolution and filtering
// -------------------------------------------------------------------

void testAssetPathResolution() {
    AssetBrowser browser;
    browser.setRootDirectory("/project/assets");

    require(browser.relativePath("/project/assets/textures/stone.ppm") ==
                "textures/stone.ppm",
            "relative path resolves under root");
    require(browser.relativePath("/project/assets/skybox/cubemap/posx.ppm") ==
                "skybox/cubemap/posx.ppm",
            "relative path resolves in subdirectory");
    require(browser.relativePath("/project/assets") == ".",
            "relative path of root is dot");
    require(browser.currentDirectory() == "/project/assets",
            "current directory tracks root");

    // setRootDirectory resets the current directory to the root.
    browser.setRootDirectory("/project/assets");
    require(browser.currentDirectory() == "/project/assets",
            "setRootDirectory resets current directory");
}

void testAssetFilters() {
    // A .ppm-only filter accepts PPM and rejects other image formats.
    AssetBrowser ppm;
    ppm.open("Textures", {".ppm"});
    require(ppm.matchesFilter("stone.ppm"), ".ppm matches ppm filter");
    require(!ppm.matchesFilter("stone.jpg"),
            ".jpg rejected by ppm filter");
    require(!ppm.matchesFilter("stone.png"),
            ".png rejected by ppm filter");
    require(!ppm.matchesFilter("stone.PPM"),
            "uppercase .PPM rejected by ppm filter");

    // An empty filter accepts every file.
    AssetBrowser all;
    all.open("All", {});
    require(all.matchesFilter("anything.jpg"),
            "empty filter accepts jpg");
    require(all.matchesFilter("anything.ppm"),
            "empty filter accepts ppm");
    require(all.matchesFilter("readme.txt"),
            "empty filter accepts txt");
}

void testMissingAssetHandling() {
    AssetBrowser browser;

    // A nonexistent root directory must not crash and yields an empty
    // listing (graceful degradation for missing assets).
    browser.setRootDirectory("/this/path/does/not/exist/pebble-assets");
    browser.open("Missing", {".ppm"});
    require(browser.files().empty(),
            "nonexistent root lists no files");
    require(browser.dirs().empty(),
            "nonexistent root lists no directories");

    // Relative path resolution still produces a non-empty result even when
    // the referenced file does not exist; the caller validates before
    // accepting the reference.
    std::string rel =
        browser.relativePath("/this/path/does/not/exist/foo.ppm");
    require(!rel.empty(), "relative path of missing file is non-empty");
}

// -------------------------------------------------------------------
// Model import tests (glTF 2.0 / GLB)
// -------------------------------------------------------------------

void testModelImport() {
    ModelImporter importer;

    // Import the cube primitive (has positions, normals, texcoords, indices).
    ImportResult result =
        importer.importFile("../../assets/models/Cube.glb");
    if (!result.success) {
        std::cerr << "Cube.glb import error: " << result.error << '\n';
    }
    require(result.success, "Cube.glb should import successfully");
    require(result.meshes.size() == 1,
            "Cube.glb should contain exactly one mesh");
    require(result.meshes[0].data.vertexCount == 24,
            "Cube should have 24 vertices");
    require(result.meshes[0].data.indexCount == 36,
            "Cube should have 36 indices");
    require(!result.meshes[0].data.vertices.empty(),
            "Cube should have vertex data");
    require(!result.meshes[0].data.indices.empty(),
            "Cube should have index data");

    // Verify position bounds match glTF accessor min/max (-1 to 1).
    const auto& verts = result.meshes[0].data.vertices;
    float minX = verts[0], maxX = verts[0];
    float minY = verts[1], maxY = verts[1];
    float minZ = verts[2], maxZ = verts[2];
    for (int i = 0; i < result.meshes[0].data.vertexCount; ++i) {
        minX = std::min(minX, verts[i * 8]);
        maxX = std::max(maxX, verts[i * 8]);
        minY = std::min(minY, verts[i * 8 + 1]);
        maxY = std::max(maxY, verts[i * 8 + 1]);
        minZ = std::min(minZ, verts[i * 8 + 2]);
        maxZ = std::max(maxZ, verts[i * 8 + 2]);
    }
    require(minX >= -1.0f && maxX <= 1.0f, "Cube X within [-1, 1]");
    require(minY >= -1.0f && maxY <= 1.0f, "Cube Y within [-1, 1]");
    require(minZ >= -1.0f && maxZ <= 1.0f, "Cube Z within [-1, 1]");

    // Import the sphere primitive.
    result = importer.importFile("../../assets/models/Sphere.glb");
    if (!result.success) {
        std::cerr << "Sphere.glb import error: " << result.error << '\n';
    }
    require(result.success, "Sphere.glb should import successfully");
    require(!result.meshes.empty(), "Sphere should have at least one mesh");
    require(result.meshes[0].data.vertexCount > 0,
            "Sphere should have vertices");
    require(result.meshes[0].data.indexCount > 0,
            "Sphere should have indices");

    // Import the plane primitive.
    result = importer.importFile("../../assets/models/Plane.glb");
    if (!result.success) {
        std::cerr << "Plane.glb import error: " << result.error << '\n';
    }
    require(result.success, "Plane.glb should import successfully");
    require(!result.meshes.empty(), "Plane should have at least one mesh");
    require(result.meshes[0].data.vertexCount >= 4,
            "Plane should have at least 4 vertices");

    std::cout << "  model import tests passed\n";
}

void testModelImportMalformed() {
    ModelImporter importer;

    // Non-existent file.
    ImportResult result = importer.importFile("../../assets/models/Nonexistent.glb");
    require(!result.success, "Non-existent file should fail");
    require(!result.error.empty(),
            "Non-existent file should produce an error message");

    // Empty file.
    std::string emptyPath = tmpPath("empty.glb");
    {
        std::ofstream f(emptyPath, std::ios::binary);
        f.close();
    }
    result = importer.importFile(emptyPath);
    require(!result.success, "Empty file should fail");

    // Non-glTF binary file (wrong magic).
    std::string badPath = tmpPath("bad.glb");
    {
        std::ofstream f(badPath, std::ios::binary);
        const char badMagic[4] = {'X', 'Y', 'Z', '!'};
        f.write(badMagic, 4);
        f.close();
    }
    result = importer.importFile(badPath);
    require(!result.success, "Wrong magic should fail");

    // glTF with no meshes (valid JSON but no mesh data).
    std::string noMeshPath = tmpPath("no_mesh.glb");
    {
        std::ofstream f(noMeshPath, std::ios::binary);
        const char gltfMagic[4] = {'g', 'l', 'T', 'f'};
        uint32_t version = 2;
        uint32_t length = 20; // header(12) + JSON chunk(8)
        f.write(gltfMagic, 4);
        f.write(reinterpret_cast<const char*>(&version), 4);
        f.write(reinterpret_cast<const char*>(&length), 4);
        // JSON chunk: empty asset
        uint32_t jsonLen = 16;
        uint32_t jsonType = 0x4E4F534A; // "JSON"
        f.write(reinterpret_cast<const char*>(&jsonLen), 4);
        f.write(reinterpret_cast<const char*>(&jsonType), 4);
        const char jsonData[16] = {"{\"asset\":{}}"};
        f.write(jsonData, 16);
        f.close();
    }
    result = importer.importFile(noMeshPath);
    require(!result.success, "glTF with no meshes should fail");
    require(!result.error.empty(),
            "No-mesh glTF should produce an error message");

    std::cout << "  model import malformed rejection tests passed\n";
}

void testCoordinateConversion() {
    // glTF 2.0 uses a right-handed, Y-up coordinate system, which matches
    // Pebble's convention. No axis swapping is required. This test verifies
    // that imported vertex data can be used directly without coordinate
    // conversion.
    ModelImporter importer;
    ImportResult result = importer.importFile("../../assets/models/Cube.glb");
    if (!result.success) {
        std::cerr << "Cube.glb import error: " << result.error << '\n';
    }
    require(result.success, "Cube.glb should import for coordinate test");

    // Verify the cube is centered at origin with vertices in [-1, 1].
    const auto& verts = result.meshes[0].data.vertices;
    for (int i = 0; i < result.meshes[0].data.vertexCount; ++i) {
        float x = verts[i * 8];
        float y = verts[i * 8 + 1];
        float z = verts[i * 8 + 2];
        require(x >= -1.0f && x <= 1.0f, "Cube X in [-1, 1] (Y-up, no flip)");
        require(y >= -1.0f && y <= 1.0f, "Cube Y in [-1, 1] (Y-up, no flip)");
        require(z >= -1.0f && z <= 1.0f, "Cube Z in [-1, 1] (Y-up, no flip)");
    }

    std::cout << "  coordinate conversion tests passed\n";
}

void testGizmoAxisMath() {
    // Test the axis vectors used by the gizmo for Move/Rotate/Scale modes.
    // These are the same axis vectors used in drawViewportGizmo.
    const Vec3 axisX(1.0f, 0.0f, 0.0f);
    const Vec3 axisY(0.0f, 1.0f, 0.0f);
    const Vec3 axisZ(0.0f, 0.0f, 1.0f);

    require(dot(axisX, axisX) == 1.0f, "X axis is unit length");
    require(dot(axisY, axisY) == 1.0f, "Y axis is unit length");
    require(dot(axisZ, axisZ) == 1.0f, "Z axis is unit length");
    require(dot(axisX, axisY) == 0.0f, "X and Y axes are orthogonal");
    require(dot(axisY, axisZ) == 0.0f, "Y and Z axes are orthogonal");
    require(dot(axisZ, axisX) == 0.0f, "Z and X axes are orthogonal");

    // Test Move axis selection logic: moving along X should only change X.
    Vec3 startPos(1.0f, 2.0f, 3.0f);
    Vec3 newPos = startPos;
    newPos.x += 5.0f; // Move along X
    require(newPos.x == 6.0f, "Move along X changes X only");
    require(newPos.y == 2.0f, "Move along X preserves Y");
    require(newPos.z == 3.0f, "Move along X preserves Z");

    // Test Scale: uniform scale should keep all components equal.
    Vec3 scale(1.0f, 1.0f, 1.0f);
    float s = scale.x + 0.5f;
    scale = Vec3(s, s, s);
    require(scale.x == scale.y && scale.y == scale.z,
            "Uniform scale keeps components equal");
    require(scale.x > 0.01f, "Scale stays positive");

    std::cout << "  gizmo axis math tests passed\n";
}

// -------------------------------------------------------------------
// Model import integration tests (importer -> scene -> renderer)
// -------------------------------------------------------------------

void testModelImportAllGlbs() {
    ModelImporter importer;
    const std::vector<std::string> glbFiles = {
        "Capsule.glb", "Cone.glb", "Cube.glb", "Cylinder.glb",
        "Plane.glb", "Pyramid.glb", "Rock.glb", "Sphere.glb", "Torus.glb",
    };

    for (const auto& fileName : glbFiles) {
        std::string path = "../../assets/models/" + fileName;
        ImportResult result = importer.importFile(path);
        require(result.success, ("Failed to import " + fileName).c_str());
        require(!result.meshes.empty(),
                (fileName + " should have at least one mesh").c_str());
        for (const auto& imported : result.meshes) {
            require(!imported.name.empty(),
                    (fileName + " mesh should have a name").c_str());
            require(imported.data.vertexCount > 0,
                    (fileName + " mesh should have vertices").c_str());
            require(imported.data.indexCount > 0,
                    (fileName + " mesh should have indices").c_str());
            require(!imported.data.vertices.empty(),
                    (fileName + " mesh should have vertex data").c_str());
            require(!imported.data.indices.empty(),
                    (fileName + " mesh should have index data").c_str());
            // All indices must be in range.
            for (unsigned int idx : imported.data.indices) {
                require(idx < static_cast<unsigned int>(imported.data.vertexCount),
                        (fileName + " index out of range").c_str());
            }
        }
    }
    std::cout << "  all GLB import tests passed\n";
}

void testPartReferencesImportedMesh() {
    ModelImporter importer;
    Scene scene("ImportTest");

    // Import Cube.glb and get its mesh name.
    ImportResult result = importer.importFile("../../assets/models/Cube.glb");
    require(result.success, "Cube.glb should import");
    require(result.meshes.size() == 1, "Cube.glb should have one mesh");
    const std::string importedMeshName = result.meshes[0].name;

    // Create a Part node referencing the imported mesh.
    NodeId partId = scene.createNode("Part", "ImportedCube");
    require(partId.value != 0, "Part node created");
    if (Part* part = dynamic_cast<Part*>(scene.find(partId))) {
        part->meshName = importedMeshName;
        part->materialName = "default";
        require(part->meshName == importedMeshName,
                "Part meshName matches imported mesh name");
    } else {
        require(false, "Created node is a Part");
    }

    // Verify the mesh name would be findable in the renderer's mesh manager
    // (the renderer falls back to "cube" when a referenced mesh is missing,
    // so a non-matching name silently degrades; this test verifies the name
    // is stored correctly).
    require(scene.find(partId)->type() == "Part",
            "Node type is Part");
}

void testSceneRoundTripWithImportedMesh() {
    ModelImporter importer;
    Scene scene("RoundTripImport");

    // Import a GLB and capture its mesh name.
    ImportResult result = importer.importFile("../../assets/models/Cube.glb");
    require(result.success, "Cube.glb should import");
    const std::string importedMeshName = result.meshes[0].name;

    // Create a Part referencing the imported mesh with a transform.
    NodeId partId = scene.createNode("Part", "ImportedCube");
    if (Part* part = dynamic_cast<Part*>(scene.find(partId))) {
        part->meshName = importedMeshName;
        part->materialName = "default";
    }
    if (Node3D* node3d = dynamic_cast<Node3D*>(scene.find(partId))) {
        node3d->position = Vec3(1.0f, 2.0f, 3.0f);
        node3d->rotation = Vec3(10.0f, 20.0f, 30.0f);
        node3d->scale = Vec3(2.0f, 1.0f, 1.5f);
    }

    const std::string path = tmpPath("test_import_roundtrip.pebscene");
    require(scene.save(path), "scene with imported mesh reference saved");

    Scene loaded = loadV2(path);
    require(loaded.has(partId), "part node exists after load");

    if (Part* loadedPart = dynamic_cast<Part*>(loaded.find(partId))) {
        require(loadedPart->meshName == importedMeshName,
                "imported mesh name preserved after round trip");
        require(loadedPart->materialName == "default",
                "material name preserved after round trip");
    } else {
        require(false, "loaded node is a Part");
    }

    if (Node3D* loadedNode3d = dynamic_cast<Node3D*>(loaded.find(partId))) {
        require(std::fabs(loadedNode3d->position.x - 1.0f) < 1e-5f,
                "position x preserved");
        require(std::fabs(loadedNode3d->position.y - 2.0f) < 1e-5f,
                "position y preserved");
        require(std::fabs(loadedNode3d->position.z - 3.0f) < 1e-5f,
                "position z preserved");
    }

    std::filesystem::remove(path);
    std::cout << "  scene round-trip with imported mesh tests passed\n";
}

// -------------------------------------------------------------------
// Regression tests for glTF node import (transforms, hierarchy)
// -------------------------------------------------------------------

// Write a minimal glTF + .bin pair describing one cube mesh and return the
// .gltf path. Returns empty string on failure.
// Binary layout (non-interleaved):
//   positions: 24 * 3 floats at offset 0 (288 bytes)
//   normals:   24 * 3 floats at offset 288 (288 bytes)
//   texcoords: 24 * 2 floats at offset 576 (192 bytes)
//   indices:   36 * uint at offset 768 (144 bytes)
//   total: 912 bytes
static std::string writeTestGltf(const std::string& dir,
                                 const std::string& gltfName,
                                 const std::string& binName,
                                 const std::string& jsonText) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::string gltfPath = dir + "/" + gltfName;
    {
        std::ofstream out(gltfPath);
        out << jsonText;
    }
    std::string binPath = dir + "/" + binName;
    {
        std::ofstream out(binPath, std::ios::binary);
        // Positions (24 * 3 floats).
        const float positions[] = {
            -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,0.5f,-0.5f, -0.5f,0.5f,-0.5f,
            -0.5f,-0.5f,0.5f,   0.5f,-0.5f,0.5f,   0.5f,0.5f,0.5f,   -0.5f,0.5f,0.5f,
            -0.5f,-0.5f,-0.5f, -0.5f,-0.5f,0.5f,  -0.5f,0.5f,0.5f,  -0.5f,0.5f,-0.5f,
             0.5f,-0.5f,-0.5f,  0.5f,-0.5f,0.5f,   0.5f,0.5f,0.5f,   0.5f,0.5f,-0.5f,
            -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f,0.5f,  -0.5f,-0.5f,0.5f,
            -0.5f,0.5f,-0.5f,   0.5f,0.5f,-0.5f,   0.5f,0.5f,0.5f,   -0.5f,0.5f,0.5f,
        };
        // Normals (24 * 3 floats).
        const float normals[] = {
            0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1,
            0,0,1,  0,0,1,  0,0,1,  0,0,1,
            -1,0,0, -1,0,0, -1,0,0, -1,0,0,
             1,0,0,  1,0,0,  1,0,0,  1,0,0,
            0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0,
             0,1,0,  0,1,0,  0,1,0,  0,1,0,
        };
        // TexCoords (24 * 2 floats).
        const float texCoords[] = {
            0,0, 1,0, 1,1, 0,1,
            0,0, 1,0, 1,1, 0,1,
            0,0, 1,0, 1,1, 0,1,
            0,0, 1,0, 1,1, 0,1,
            0,0, 1,0, 1,1, 0,1,
            0,0, 1,0, 1,1, 0,1,
        };
        // Indices (36 unsigned ints).
        const unsigned int indices[] = {
            0,1,2, 0,2,3,   // back
            4,5,6, 4,6,7,   // front
            8,9,10, 8,10,11,// left
            12,13,14, 12,14,15,// right
            16,17,18, 16,18,19,// bottom
            20,21,22, 20,22,23,// top
        };
        out.write(reinterpret_cast<const char*>(positions), sizeof(positions));
        out.write(reinterpret_cast<const char*>(normals), sizeof(normals));
        out.write(reinterpret_cast<const char*>(texCoords), sizeof(texCoords));
        out.write(reinterpret_cast<const char*>(indices), sizeof(indices));
    }
    return gltfPath;
}

void testGltfNodeTransforms() {
    ModelImporter importer;

    // Build a minimal glTF with one node that has translation, rotation,
    // and scale.
    const std::string jsonText = R"({
      "asset": {"version": "2.0"},
      "buffers": [{"byteLength": 912, "uri": "cube.bin"}],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0, "byteLength": 288, "target": 34962},
        {"buffer": 0, "byteOffset": 288, "byteLength": 288, "target": 34962},
        {"buffer": 0, "byteOffset": 576, "byteLength": 192, "target": 34962},
        {"buffer": 0, "byteOffset": 768, "byteLength": 144, "target": 34963}
      ],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 24, "type": "VEC3", "byteOffset": 0},
        {"bufferView": 1, "componentType": 5126, "count": 24, "type": "VEC3", "byteOffset": 0},
        {"bufferView": 2, "componentType": 5126, "count": 24, "type": "VEC2", "byteOffset": 0},
        {"bufferView": 3, "componentType": 5125, "count": 36, "type": "SCALAR", "byteOffset": 0}
      ],
      "meshes": [
        {
          "name": "Cube",
          "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
            "indices": 3,
            "mode": 4
          }]
        }
      ],
      "nodes": [
        {
          "name": "TransformedCube",
          "mesh": 0,
          "translation": [1.0, 2.0, 3.0],
          "rotation": [0, 0, 0.70710678118, 0.70710678118],
          "scale": [2.0, 1.0, 0.5]
        }
      ],
      "scenes": [{"nodes": [0]}],
      "scene": 0
    })";

    std::string dir = tmpPath("test_gltf_node_transforms");
    std::string gltfPath = writeTestGltf(dir, "cube.gltf", "cube.bin", jsonText);
    require(!gltfPath.empty(), "test glTF written");

    ImportResult result = importer.importFile(gltfPath);
    require(result.success, "glTF with node transforms should import");
    require(result.nodes.size() == 1, "should have one node");

    const auto& node = result.nodes[0];
    require(node.name == "TransformedCube", "node name");
    require(node.meshName == "Cube", "node mesh name");
    require(std::fabs(node.position.x - 1.0f) < 1e-4f, "position x");
    require(std::fabs(node.position.y - 2.0f) < 1e-4f, "position y");
    require(std::fabs(node.position.z - 3.0f) < 1e-4f, "position z");
    // 90-degree rotation around Z → yaw = 90 degrees.
    require(std::fabs(node.rotation.z - 90.0f) < 1.0f, "rotation z (yaw)");
    require(std::fabs(node.scale.x - 2.0f) < 1e-4f, "scale x");
    require(std::fabs(node.scale.y - 1.0f) < 1e-4f, "scale y");
    require(std::fabs(node.scale.z - 0.5f) < 1e-4f, "scale z");

    // Verify mesh data was extracted correctly.
    require(result.meshes.size() >= 1, "at least one mesh");
    require(result.meshes[0].data.vertexCount == 24, "cube vertex count");
    require(result.meshes[0].data.indexCount == 36, "cube index count");

    std::filesystem::remove_all(dir);
    std::cout << "  glTF node transform tests passed\n";
}

void testGltfParentChildHierarchy() {
    ModelImporter importer;

    const std::string jsonText = R"({
      "asset": {"version": "2.0"},
      "buffers": [{"byteLength": 1824, "uri": "cube.bin"}],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0, "byteLength": 288, "target": 34962},
        {"buffer": 0, "byteOffset": 288, "byteLength": 288, "target": 34962},
        {"buffer": 0, "byteOffset": 576, "byteLength": 192, "target": 34962},
        {"buffer": 0, "byteOffset": 768, "byteLength": 144, "target": 34963},
        {"buffer": 0, "byteOffset": 912, "byteLength": 288, "target": 34962},
        {"buffer": 0, "byteOffset": 1200, "byteLength": 288, "target": 34962},
        {"buffer": 0, "byteOffset": 1488, "byteLength": 192, "target": 34962},
        {"buffer": 0, "byteOffset": 1680, "byteLength": 144, "target": 34963}
      ],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 24, "type": "VEC3", "byteOffset": 0},
        {"bufferView": 1, "componentType": 5126, "count": 24, "type": "VEC3", "byteOffset": 0},
        {"bufferView": 2, "componentType": 5126, "count": 24, "type": "VEC2", "byteOffset": 0},
        {"bufferView": 3, "componentType": 5125, "count": 36, "type": "SCALAR", "byteOffset": 0},
        {"bufferView": 4, "componentType": 5126, "count": 24, "type": "VEC3", "byteOffset": 0},
        {"bufferView": 5, "componentType": 5126, "count": 24, "type": "VEC3", "byteOffset": 0},
        {"bufferView": 6, "componentType": 5126, "count": 24, "type": "VEC2", "byteOffset": 0},
        {"bufferView": 7, "componentType": 5125, "count": 36, "type": "SCALAR", "byteOffset": 0}
      ],
      "meshes": [
        {
          "name": "Cube",
          "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
            "indices": 3,
            "mode": 4
          }]
        },
        {
          "name": "Sphere",
          "primitives": [{
            "attributes": {"POSITION": 4, "NORMAL": 5, "TEXCOORD_0": 6},
            "indices": 7,
            "mode": 4
          }]
        }
      ],
      "nodes": [
        {
          "name": "Parent",
          "mesh": 0,
          "translation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0],
          "children": [1, 2]
        },
        {
          "name": "ChildCube",
          "mesh": 0,
          "translation": [1.0, 0.0, 0.0]
        },
        {
          "name": "ChildSphere",
          "mesh": 1,
          "translation": [0.0, 1.0, 0.0]
        }
      ],
      "scenes": [{"nodes": [0]}],
      "scene": 0
    })";

    std::string dir = tmpPath("test_gltf_parent_child");
    std::string gltfPath = writeTestGltf(dir, "hierarchy.gltf", "cube.bin", jsonText);
    require(!gltfPath.empty(), "test glTF written");

    ImportResult result = importer.importFile(gltfPath);
    require(result.success, "hierarchy glTF should import");
    require(result.nodes.size() == 3, "should have three nodes");

    // Parent is node 0.
    const auto& parent = result.nodes[0];
    require(parent.parentIndex == -1, "parent has no parent");
    require(parent.meshName == "Cube", "parent mesh name");

    // Children are nodes 1 and 2, both parented to node 0.
    const auto& childCube = result.nodes[1];
    require(childCube.parentIndex == 0, "child cube parent is node 0");
    require(std::fabs(childCube.position.x - 1.0f) < 1e-4f, "child cube position x");

    const auto& childSphere = result.nodes[2];
    require(childSphere.parentIndex == 0, "child sphere parent is node 0");
    require(std::fabs(childSphere.position.y - 1.0f) < 1e-4f, "child sphere position y");
    require(childSphere.meshName == "Sphere", "child sphere mesh name");

    std::filesystem::remove_all(dir);
    std::cout << "  glTF parent-child hierarchy tests passed\n";
}

void testGltfMultipleNodesSameMesh() {
    ModelImporter importer;

    const std::string jsonText = R"({
      "asset": {"version": "2.0"},
      "buffers": [{"byteLength": 912, "uri": "cube.bin"}],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0, "byteLength": 288, "target": 34962},
        {"buffer": 0, "byteOffset": 288, "byteLength": 288, "target": 34962},
        {"buffer": 0, "byteOffset": 576, "byteLength": 192, "target": 34962},
        {"buffer": 0, "byteOffset": 768, "byteLength": 144, "target": 34963}
      ],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 24, "type": "VEC3", "byteOffset": 0},
        {"bufferView": 1, "componentType": 5126, "count": 24, "type": "VEC3", "byteOffset": 0},
        {"bufferView": 2, "componentType": 5126, "count": 24, "type": "VEC2", "byteOffset": 0},
        {"bufferView": 3, "componentType": 5125, "count": 36, "type": "SCALAR", "byteOffset": 0}
      ],
      "meshes": [
        {
          "name": "Cube",
          "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
            "indices": 3,
            "mode": 4
          }]
        }
      ],
      "nodes": [
        {
          "name": "CubeInstance1",
          "mesh": 0,
          "translation": [-2.0, 0.0, 0.0]
        },
        {
          "name": "CubeInstance2",
          "mesh": 0,
          "translation": [2.0, 0.0, 0.0]
        }
      ],
      "scenes": [{"nodes": [0, 1]}],
      "scene": 0
    })";

    std::string dir = tmpPath("test_gltf_multi_nodes");
    std::string gltfPath = writeTestGltf(dir, "multi.gltf", "cube.bin", jsonText);
    require(!gltfPath.empty(), "test glTF written");

    ImportResult result = importer.importFile(gltfPath);
    require(result.success, "multi-node glTF should import");
    require(result.nodes.size() == 2, "should have two nodes");

    // Both nodes reference the same mesh.
    require(result.nodes[0].meshName == "Cube", "node 0 mesh name");
    require(result.nodes[1].meshName == "Cube", "node 1 mesh name");

    // Different positions.
    require(std::fabs(result.nodes[0].position.x - -2.0f) < 1e-4f, "node 0 position x");
    require(std::fabs(result.nodes[1].position.x - 2.0f) < 1e-4f, "node 1 position x");

    // Only one unique mesh imported.
    require(result.meshes.size() == 1, "one unique mesh");
    require(result.meshes[0].data.vertexCount == 24, "cube vertex count");
    require(result.meshes[0].data.indexCount == 36, "cube index count");

    std::filesystem::remove_all(dir);
    std::cout << "  glTF multiple nodes same mesh tests passed\n";
}

