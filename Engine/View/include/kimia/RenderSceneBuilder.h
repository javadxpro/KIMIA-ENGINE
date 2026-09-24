#pragma once

#include <kimia/AssetManager.h>
#include <kimia/Mesh.h>
#include <kimia/Renderer.h>
#include <kimia/Skeleton.h>
#include <kimia/Types.h>
#include <kimia/World.h>

#include <map>
#include <string>
#include <vector>

namespace kimia {

// What building one frame cost. `missing` is the count the asset manager is
// holding on to, so a frame that quietly stopped drawing a model shows up
// here instead of only in a screenshot.
struct SceneBuildReport {
  usize objects = 0U;         // draw calls pushed into the scene
  usize posedEntities = 0U;   // entities re-posed from an animation this frame
  usize posedCharacters = 0U; // match characters drawn from a gait-clip pose
  usize missingAssets = 0U;
  AssetManager::Stats assets{};
};

// Turns the editor's state into one frame's draw list.
//
// This is the ~600 lines that used to sit inside Examples/WorldEditorApp.cpp:
// the ground and the placed objects, models with their materials and
// textures, the posed characters (squad members, limbs, custom rigs), the
// ball, particles, the placement ghost, selection markers and the shot-mode
// aim chain. Living in the engine instead of the example means:
//
//   * the one place that touches assets is the AssetManager (the frame loop
//     never opens a file),
//   * the scene a frame draws is testable without a window, a GL context or
//     the software rasteriser,
//   * the Android build and the desktop build cannot drift apart again.
//
// The builder owns the primitives every scene uses and the per-frame posed
// meshes; the AssetManager owns everything that came from a file. Mesh and
// image pointers handed to the scene stay valid for as long as the scene is
// drawn (both containers are node-based and only grow).
class RenderSceneBuilder {
public:
  explicit RenderSceneBuilder(AssetManager& assets);

  // Fills `scene.objects` from the editor's world (and the editor's play
  // state: ball, squads, particles, ghost, markers). The camera fields of
  // `scene` are left alone — CameraController owns those.
  //
  // Takes a mutable editor on purpose: asking for an entity's posed mesh
  // advances its animation, which is the simulation's business rather than
  // this builder's. Files are never read through the editor: every mesh,
  // material table and image here comes from the AssetManager.
  void build(WorldEditor& editor, RenderScene& scene);

  const SceneBuildReport& report() const { return report_; }
  // The primitives, exposed so an example can reuse them for its own props.
  const MeshData& cubeMesh() const { return cubeMesh_; }
  const MeshData& planeMesh() const { return planeMesh_; }
  const MeshData& sphereMesh() const { return sphereMesh_; }
  // Entities whose model file could not be read, as the asset manager
  // reported them (path -> reason). Empty when everything resolved.
  std::map<std::string, std::string> failedAssets() const;

private:
  void addGoalShape(RenderScene& scene, const EntityData& entity) const;
  void addSelectionMarkers(RenderScene& scene, const EntityData& entity) const;
  void addGhostShape(RenderScene& scene, const WorldEditor& editor) const;
  void addAimIndicator(RenderScene& scene, const WorldEditor& editor) const;
  void addSquads(RenderScene& scene, WorldEditor& editor);
  void addCurrentCupFlag(RenderScene& scene, const WorldEditor& editor) const;
  void addLimbs(RenderScene& scene, const std::vector<FigureLimb>& limbs, const Vec3& color) const;
  void addFigure(RenderScene& scene, const Skeleton& rig, const FigureMotion& motion, const Vec3& at,
                 f64 facing, const Vec3& color);
  void addCustomFigure(RenderScene& scene, const std::vector<CustomBone>& bones, const FigureMotion& motion,
                       const Vec3& at, f64 facing, const Vec3& color);
  // The bones a squad member should use, or null for the engine's figure. A
  // character's rig is read off the scene entity that represents it: the human
  // uses "Player", and everyone else falls back to a "Squad" entity if the
  // world defines one, so a whole team can be re-boned in one go.
  const std::vector<CustomBone>* customRigFor(const WorldEditor& editor, u32 id);
  // The rig every character without bones of its own uses. Shared, because
  // building it per character per frame would be waste.
  const Skeleton& figureRig() const;
  // The model's own diffuse map (first material that has one), resolved once
  // per file rather than per frame. Reads the asset manager's material table,
  // which the mesh lookup has already loaded.
  const std::string& modelTexturePath(const std::string& meshFile);

  AssetManager& assets_;
  MeshData cubeMesh_;
  MeshData planeMesh_;
  MeshData sphereMesh_;
  Skeleton figureRig_;
  // Scratch buffers, reused every frame: a frame that allocates is a frame
  // that stutters on a phone.
  std::vector<Transform3D> poseScratch_;
  std::vector<FigureLimb> limbScratch_;
  std::vector<CustomBone> boneScratch_;
  // This frame's posed meshes, keyed by entity name (map: stable addresses).
  std::map<std::string, MeshData> posedMeshes_;
  std::map<std::string, std::string> modelTexturePaths_;  // mesh file -> map_Kd
  // Models this frame could not show at all: the file, and why. Refilled
  // every build (it describes the frame, not the session).
  std::map<std::string, std::string> failedModels_;
  SceneBuildReport report_;
};

}  // namespace kimia
