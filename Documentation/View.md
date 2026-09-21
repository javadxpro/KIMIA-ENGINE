# Engine/View — the frame loop's engine half

Phase 2 of the engine plan asks for `Examples/WorldEditorApp.cpp` to stop being
the place where the engine lives. Before this module existed the example owned
the scene builder, the camera rig, the whole input mapping and the gameplay
event table, in about 900 lines of the 1282-line file. Nothing could be tested
without opening a window, and the same logic would have had to be written a
second time by any other front end (the Android runner, a tool, a test).

`Engine/View` is that code, moved into the engine, split by what it answers:

| File | Answers |
| --- | --- |
| `RenderSceneBuilder.{h,cpp}` | "what does this frame draw?" — the draw list, built from the editor's state |
| `CameraController.{h,cpp}` | "where is the camera, and what does it see?" — the rig, its smoothing and the view/projection it hands to the scene |
| `InputRouter.{h,cpp}` | "what did the player just ask for?" — raw device state to editor calls, and the camera's share of it |
| `GameplayEvents.{h,cpp}` | "what happened, and what does it sound like?" — the event table, the trigger names and the sound bank |

`AssetManager` (see `Documentation/AssetManager.md`) is the fifth piece of the
same phase: it is the only thing in the frame loop that opens a file.

## The contracts

**No window, no GL, no file read of its own.** Every piece here is a plain
function over engine types. `RenderSceneBuilder` reads models, material tables
and images through the `AssetManager` it is constructed with; it never calls a
loader and never resolves a path. `CameraController` and `InputRouter` touch no
assets at all. That is why `Tests/src/ViewTests.cpp` can check all of them
against the real editor and the real test assets.

**The editor is the source of truth.** These are not a second world state.
`RenderSceneBuilder::build(WorldEditor&, RenderScene&)` reads the editor — which
screen it is on, where the ball is, which entity is selected — and writes only
into the caller's `RenderScene`. `InputRouter::routeEditorInput(WorldEditor&,
const InputState&, dt)` writes into the editor exactly where the inline version
did, so key bindings have not moved; it returns what it did as a
`RoutedInput` so a caller (or a test) does not have to re-derive it.

**Pointers handed to the scene stay valid for the frame.** The builder owns the
primitives every scene uses (cube, plane, sphere, the figure rig) and this
frame's posed meshes; the asset manager owns everything that came from a file.
Both are node-based and only grow, so a `RenderObject::mesh` pointer is stable
across frames for unchanged geometry, which is the property the renderers'
caches rely on. `SceneBuildReport` says what a frame cost: objects, posed
entities, missing assets and the manager's counters.

**One place per decision.** The rule "does this model draw in several tinted
pieces?" lives in `assets::splitsByMaterial()`, not in the editor and the
renderer separately; the event-to-trigger and event-to-sound tables live in
`GameplayEvents.h`, not in the frame loop. When two callers have to agree, the
answer is one function.

## What the example keeps

`Examples/WorldEditorApp.cpp` (639 lines, was 1282) still owns everything that
is genuinely the *application*: the window and the web server, the D3D11/GL/
software render path, the on-frame HUD, the intro film, the menu plumbing over
the engine's screens, and the runtime loop that calls the pieces above. Its
frame reads, in order:

```cpp
const kimia::RoutedInput routed = kimia::routeEditorInput(editor, input, dt);
cameraController.applyInput(routed.camera);
cameraController.update(editor, dt);
editor.setViewport(cameraController.viewport(width, height));
runtimeLoop.tick(dt, [&](f64 step) { editor.update(step); });
for (const char* cue : kimia::pumpGameplayEvents(editor)) server.playSound(cue);
sceneBuilder.build(editor, scene);
cameraController.applyTo(scene, width, height);
```

## What is still to do in this phase

* `WorldRuntime` — the fixed-step world update is still `Engine/Runtime`'s
  `RuntimeLoop` plus two call sites in the example. Splitting it out is worth
  doing together with the replay work (phase 7), because a replay is exactly
  "the runtime, re-run from recorded input".
* `CinematicCamera` and `ReplaySystem` — the camera modes (broadcast, goal
  replay, skill replay, intro) and the replay stream are phase 7 features with
  their own acceptance criteria; building the names now would leave two empty
  systems next to each other, which the plan explicitly forbids.
* `World.h` (1145 lines) and `World.cpp` (3818 lines) still hold the editor,
  the builder screens, the physics bridge and the animation bridge in one
  translation unit. Phase 3 (scene/entity, serialization) is the right moment
  to split them, because that is when the component list changes.

## Verification

`Tests/src/ViewTests.cpp` (11 tests) covers the event table and its pump, the
input mapping (keys, stick precedence, held vs pressed, transport and ball
control), the camera contract, the orbit/zoom/resting distance behaviour, the
chase easing, and the scene builder against the real test assets: one read per
model, no per-frame loads, stable pointers, a missing model reported once with
its reason while the rest of the frame still draws.
