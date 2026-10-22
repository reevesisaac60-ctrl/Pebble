# Project and Asset Pipeline — Design Proposal

Status: design only. No implementation exists yet beyond the package/manifest
layer described below.

## Existing capabilities (starting point)

- Package manifests (`pebble.json`) and lockfiles (`pebble.lock`) with dependency
  resolution, including local path dependencies and `..` traversal.
- CLI commands: `init`, `build`, `run`, `add`, `remove`, `install`, `update`,
  `tree`, `package`.
- Source compilation through C and C++ frontends with code generation.
- Module/import system for packages.

## Proposed architecture

### Project discovery and configuration

- A project root is identified by a `pebble.project.json` (new) or the existing
  manifest. Configuration lives in version-controlled source files; derived data
  lives under `.pebble/` (build, cache, index) and must be gitignored.
- Discovery walks upward from the current directory until a project marker is
  found, mirroring how package manifests are currently located.

### Stable resource identifiers

- Every asset receives a stable, project-relative URI such as
  `asset://<project>/<path>` plus a content hash. Source references use these
  URIs rather than raw filesystem paths, so renaming a file does not break
  references if the index is updated.
- Project-relative references (`@asset/...`) are resolved at build time.

### Resource loading and caching

- A resource index maps URIs to on-disk locations and content hashes.
- Loaders are type-specific (text, binary, shader, scene, map). Results are
  cached by content hash in `.pebble/cache/`.
- Hot-reload (Studio/Engine) invalidates by hash change, not by mtime alone.

### Dependency tracking

- A build graph records edges between source files, packages, shaders,
  materials, scenes and maps. Each node carries input hashes.
- Incremental invalidation recompiles only nodes whose inputs (or transitive
  inputs) changed.

### Asset validation and diagnostics

- Validation runs during indexing: missing files, malformed assets, broken
  references. Diagnostics are source-linked (file:line) and consistent across
  C/C++ frontends.

### Reproducible builds and packaging

- Lockfiles pin exact dependency versions and content hashes.
- Standalone packaging bundles resolved assets with a manifest of hashes; the
  runtime verifies hashes on load.

### Separation of source and derived data

- Source tree contains authored files only. `.pebble/` contains generated
  indices, caches and build artifacts and is never committed.

### Future compatibility

- Pebble Studio and Pebble Engine consume the same project index and resource
  URIs. Neither is required for the core build pipeline; the pipeline exposes a
  stable API so Studio/Engine can attach later.

## Key decisions

1. Content hashing over mtime for cache keys.
2. Project-relative URIs as the canonical reference form.
3. Derived state isolated under `.pebble/`.
4. Build graph is the single source of truth for invalidation.
5. No graphics backend or editor in core; pipeline stays headless.

## Not in scope for this milestone

Asset database implementation, shader compiler, scene system, graphics backend,
and editor are deferred to a separate task.

## Implemented in this milestone

- Project discovery via existing pebble_project_load.
- Resource index: pebble_resource_scan.
- Build graph: pebble_build_graph_*.
