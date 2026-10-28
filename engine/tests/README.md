
## Scene tests

The scene graph tests (`test_scene_graph.cpp`) validate node hierarchy,
transforms, serialization, materials, editor camera behavior, lighting,
environment fog, reparenting cycle prevention, and asset browser behavior.
They compile against the engine sources directly.

Run with:

```sh
cd engine/build
ctest --output-on-failure
```

Test groups:

- **Scene graph**: `testParentChild`, `testTransformInheritance`, `testCreateDelete`
- **Serialization**: `testSerializationRoundTrip`, `testEmptySceneSerialization`,
  `testInvalidHeader`, `testUnsupportedVersion`, `testDuplicateIds`,
  `testInvalidParent`, `testUnsupportedType`, `testV1Migration`,
  `testLightingEnvironmentSerialization`, `testSerializationWithMaterials`
- **Cameras**: `testCameraMatrices`, `testCameraInvalidProjection`,
  `testCameraSelectionAndFallback`
- **Editor camera**: `testEditorCameraMovement`, `testEditorCameraFreeLook`,
  `testLookAtRotatedCameraBasis`, `testEditorCameraRotatedViewMatchesOrientation`,
  `testEditorCameraDefaultFov`, `testEditorCameraContinuousYaw`,
  `testEditorCameraNoRollAccumulation`, `testEditorCameraProjectionAspectRatio`,
  `testEditorCameraProjectionInvalidAspect`,
  `testEditorCameraPositionPreservedDuringMouseLook`,
  `testEditorCameraHorizontalMovementAtYaws`,
  `testEditorCameraMovementWhileLookingUpDown`, `testEditorCameraForwardAtPitches`,
  `testEditorCameraMovementSymmetry`, `testEditorCameraVerticalMovementWorldAligned`,
  `testEditorCameraDiagonalMovementNormalization`, `testEditorCameraLookAtFocus`,
  `testEditorCameraVerticalMovement`, `testEditorCameraSpeedBoost`,
  `testEditorCameraFocusDoesNotAffectFreelook`
- **Lighting / environment**: `testPointLightLimitAndAttenuation`,
  `testFogRangeValidation`
- **Mesh generation**: `testMeshGeneration`
- **Materials**: `testMaterialDefaults`, `testMaterialValidation`,
  `testMaterialManager`, `testSerializationWithMaterials`
- **Hierarchy integrity**: `testReparentingPreventsCycles`
- **Asset browser**: `testAssetPathResolution`, `testAssetFilters`,
  `testMissingAssetHandling`
