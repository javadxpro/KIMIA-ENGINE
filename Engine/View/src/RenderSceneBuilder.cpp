// The frame's draw list, built from the editor's state.
//
// This is the scene-building half of what used to live in
// Examples/WorldEditorApp.cpp: the ground and the placed objects, models with
// their materials and textures, the posed characters, the ball, particles, the
// placement ghost, selection markers and the aim chain. Nothing here is new
// behaviour — it is the same draws in the same order — but it now reads its
// files through the AssetManager instead of keeping its own maps, and it can
// be called from a test without a window, a GL context or a rasteriser.
#include <kimia/RenderSceneBuilder.h>

#include <kimia/MathUtils.h>
#include <kimia/Particles.h>
#include <kimia/Skeleton.h>

#include <cmath>
#include <utility>

namespace kimia {

namespace {

// The two colours the editor itself uses for things that are not in the world
// yet: the object about to be placed, and the object being moved or deleted.
const Vec3 kGhostColor{1.0, 0.85, 0.2};
const Vec3 kSelectionColor{1.0, 0.9, 0.25};

}  // namespace

RenderSceneBuilder::RenderSceneBuilder(AssetManager& assets) : assets_(assets) {
  // The primitives every scene needs. Built once, at startup: they are the
  // same cube, plane and sphere for every frame and every entity.
  cubeMesh_ = makeCube(1.0);
  planeMesh_ = makePlane(1.0, 1.0);
  sphereMesh_ = makeSphere(16, 8);
  figureRig_ = makeFigureRig(1.7);
}

// A single-entity goal (scale.x = width, scale.y = height) drawn as two
// posts and a crossbar.
void RenderSceneBuilder::addGoalShape(RenderScene& scene, const EntityData& entity) const {
  const f64 width = entity.transform.scale.x;
  const f64 half = entity.transform.scale.y * 0.5;
  const Vec3 at = entity.transform.position;
  const Vec3 color = entity.color;
  const Mat4 spin = Mat4::translation(at) * entity.transform.rotation.toMat4() *
                    Mat4::translation(Vec3{-at.x, -at.y, -at.z});
  scene.objects.push_back(
      {&cubeMesh_, spin * Mat4::translation(Vec3{at.x - width * 0.5 + 0.06, at.y, at.z}) *
                       Mat4::scaling(Vec3{0.12, entity.transform.scale.y, 0.12}),
       color, entity.roughness});
  scene.objects.push_back(
      {&cubeMesh_, spin * Mat4::translation(Vec3{at.x + width * 0.5 - 0.06, at.y, at.z}) *
                       Mat4::scaling(Vec3{0.12, entity.transform.scale.y, 0.12}),
       color, entity.roughness});
  scene.objects.push_back(
      {&cubeMesh_, spin * Mat4::translation(Vec3{at.x, at.y + half, at.z}) *
                       Mat4::scaling(Vec3{width + 0.12, 0.12, 0.12}),
       color, entity.roughness});
}

// The eight corner marks around whichever object is selected, so "delete" and
// "move" have something visible pointing at their target.
void RenderSceneBuilder::addSelectionMarkers(RenderScene& scene, const EntityData& entity) const {
  const Vec3 half = entity.transform.scale * 0.5;
  const Vec3 at = entity.transform.position;
  const f64 marker = 0.06;
  for (i32 sx = -1; sx <= 1; sx += 2) {
    for (i32 sy = -1; sy <= 1; sy += 2) {
      for (i32 sz = -1; sz <= 1; sz += 2) {
        const Vec3 corner{at.x + half.x * static_cast<f64>(sx), at.y + half.y * static_cast<f64>(sy),
                          at.z + half.z * static_cast<f64>(sz)};
        scene.objects.push_back(
            {&cubeMesh_, Mat4::translation(corner) * Mat4::scaling(Vec3{marker, marker, marker}),
             kSelectionColor, 0.9});
      }
    }
  }
}

// The object about to be placed, drawn as a see-through shape of the right
// kind so the user can see where it will land before committing to it.
void RenderSceneBuilder::addGhostShape(RenderScene& scene, const WorldEditor& editor) const {
  const Vec3 ghost = editor.ghostPosition();
  const f64 size = editor.ghostSize();
  switch (editor.ghostKind()) {
    case ObjectKind::Player: {
      scene.objects.push_back({&cubeMesh_, Mat4::translation(Vec3{ghost.x, 0.5, ghost.z}) *
                                              Mat4::scaling(Vec3{0.6, 1.0, 0.6}),
                               kGhostColor, 0.9});
      scene.objects.push_back({&cubeMesh_, Mat4::translation(Vec3{ghost.x, 1.15, ghost.z}) *
                                              Mat4::scaling(Vec3{0.3, 0.3, 0.3}),
                               kGhostColor, 0.9});
      break;
    }
    case ObjectKind::Ball: {
      const f64 radius = editor.world().ball.radius;
      scene.objects.push_back({&sphereMesh_, Mat4::translation(Vec3{ghost.x, radius, ghost.z}) *
                                                 Mat4::scaling(Vec3{radius, radius, radius}),
                               kGhostColor, 0.9});
      break;
    }
    case ObjectKind::Block:
    case ObjectKind::Crate:
    case ObjectKind::Model:
      scene.objects.push_back({&cubeMesh_, Mat4::translation(Vec3{ghost.x, size * 0.5, ghost.z}) *
                                              Mat4::scaling(Vec3{size, size, size}),
                               kGhostColor, 0.9});
      break;
    case ObjectKind::Wall:
      scene.objects.push_back(
          {&cubeMesh_, Mat4::translation(Vec3{ghost.x, 0.5, ghost.z}) *
                           Mat4::scaling(editor.ghostAxisZ() ? Vec3{0.5, 1.0, size}
                                                            : Vec3{size, 1.0, 0.5}),
           kGhostColor, 0.9});
      break;
    case ObjectKind::Goal: {
      EntityData preview;
      preview.transform.position = Vec3{ghost.x, kWorldGoalHeight * 0.5, ghost.z};
      preview.transform.scale = Vec3{size, kWorldGoalHeight, 0.12};
      preview.color = kGhostColor;
      preview.roughness = 0.9;
      addGoalShape(scene, preview);
      break;
    }
    case ObjectKind::Hole: {
      // The cup preview: a flat disc (a squashed sphere), drawn slightly
      // above the ground so it is visible while placing.
      const f64 radius = kWorldHoleRadius;
      scene.objects.push_back({&sphereMesh_, Mat4::translation(Vec3{ghost.x, 0.03, ghost.z}) *
                                                 Mat4::scaling(Vec3{radius, 0.03, radius}),
                               kGhostColor, 0.9});
      break;
    }
    default:
      break;
  }
}

// Shot mode: a chain of small markers along the aim direction on the ground.
// The chain grows with the charge, like the reference golf's indicator.
void RenderSceneBuilder::addAimIndicator(RenderScene& scene, const WorldEditor& editor) const {
  if (!editor.shotMode() || !editor.playing() || !editor.ballAtRest()) return;
  const Vec3 from = editor.ballPosition();
  const Vec3 direction = editor.aimDirection();
  const f64 reach = 1.0 + (editor.charging() ? editor.power() : 0.0) * 4.0;
  const i32 count = 6;
  for (i32 i = 1; i <= count; ++i) {
    const f64 t = static_cast<f64>(i) / static_cast<f64>(count);
    const Vec3 at = from + direction * (reach * t);
    const f64 marker = 0.05 + 0.02 * (1.0 - t);
    scene.objects.push_back({&cubeMesh_, Mat4::translation(Vec3{at.x, marker * 0.5, at.z}) *
                                             Mat4::scaling(Vec3{marker, marker, marker}),
                             kGhostColor, 0.9});
  }
}

// Lays a set of limb segments into the scene as stretched cubes: a cube
// rotated onto the segment's axis, which is all the software rasteriser needs
// and reads far better than the single box characters used to be.
void RenderSceneBuilder::addLimbs(RenderScene& scene, const std::vector<FigureLimb>& limbs,
                                  const Vec3& color) const {
  for (const FigureLimb& limb : limbs) {
    const Vec3 along = limb.to - limb.from;
    const f64 length = along.length();
    if (length < 1e-4) continue;
    const Vec3 middle = limb.from + along * 0.5;
    const Vec3 up{0.0, 1.0, 0.0};
    const Vec3 dir = along * (1.0 / length);
    const f64 dot = up.x * dir.x + up.y * dir.y + up.z * dir.z;
    Mat4 orient;
    if (dot < 0.9999) {
      if (dot < -0.9999) {
        orient = Mat4::rotationX(kPi);
      } else {
        const Vec3 axis{up.y * dir.z - up.z * dir.y, up.z * dir.x - up.x * dir.z,
                        up.x * dir.y - up.y * dir.x};
        orient = Quat::fromAxisAngle(axis, std::acos(dot)).toMat4();
      }
    }
    scene.objects.push_back({&cubeMesh_,
                             Mat4::translation(middle) * orient *
                                 Mat4::scaling(Vec3{limb.thickness, length, limb.thickness}),
                             color, 1.0, 0.0, nullptr});
  }
}

// Draws one posed figure. The scratch buffers are members (not function-local
// statics) so a frame that draws twenty characters still allocates nothing.
void RenderSceneBuilder::addFigure(RenderScene& scene, const Skeleton& rig,
                                   const FigureMotion& motion, const Vec3& at, f64 facing,
                                   const Vec3& color) {
  poseFigure(rig, motion, poseScratch_);
  figureLimbs(rig, poseScratch_, at, facing, limbScratch_);
  addLimbs(scene, limbScratch_, color);
}

// Draws a character using bones the user drew themselves.
void RenderSceneBuilder::addCustomFigure(RenderScene& scene, const std::vector<CustomBone>& bones,
                                         const FigureMotion& motion, const Vec3& at, f64 facing,
                                         const Vec3& color) {
  customFigureLimbs(bones, motion, at, facing, limbScratch_);
  addLimbs(scene, limbScratch_, color);
}

// The bones a squad member should use, or null for the engine's figure. A
// character's rig is read off the scene entity that represents it: the human
// uses "Player", and everyone else falls back to a "Squad" entity if the world
// defines one, so a whole team can be re-boned in one go.
const std::vector<CustomBone>* RenderSceneBuilder::customRigFor(const WorldEditor& editor, u32 id) {
  const char* wanted = id == kPrimaryCharacter ? "Player" : "Squad";
  const EntityData* entity = editor.entity(wanted);
  if (entity == nullptr && id == kPrimaryCharacter) return nullptr;
  if (entity == nullptr || entity->rig.empty()) {
    entity = editor.entity("Player");
    if (entity == nullptr || entity->rig.empty()) return nullptr;
  }
  boneScratch_.clear();
  boneScratch_.reserve(entity->rig.size());
  for (const RigBone& bone : entity->rig) {
    CustomBone out;
    out.name = bone.name;
    out.parent = bone.parent;
    out.from = bone.from;
    out.to = bone.to;
    out.thickness = bone.thickness;
    out.swing = bone.swing;
    boneScratch_.push_back(out);
  }
  return &boneScratch_;
}

// The squads (stage 21) were spawned in physics but never actually DRAWN,
// which nobody noticed while they stood still. Now that stage 27 has them
// running about, an invisible opposition makes a match unplayable. Each
// character is a jointed figure: our side in blue, theirs in red.
void RenderSceneBuilder::addSquads(RenderScene& scene, const WorldEditor& editor) {
  if (!editor.playing() || editor.squadCount() <= 1U) return;
  const Vec3 ourColor{0.25, 0.45, 0.95};
  const Vec3 theirColor{0.90, 0.25, 0.25};
  const Vec3 keeperColor{0.95, 0.85, 0.20};  // the keeper stands out
  for (const u32 id : editor.squadIds()) {
    // The human is already drawn as the Player entity in the scene.
    if (id == kPrimaryCharacter) continue;
    const Vec3 at = editor.squadPosition(id);
    const u32 team = editor.squadTeam(id);
    Vec3 color = team == 1U ? ourColor : theirColor;
    const bool down = editor.arenaMode() && editor.downed(id);
    if (down) {
      color = Vec3{0.45, 0.45, 0.45};
    } else if (!editor.arenaMode() && id == editor.aiKeeper(team)) {
      color = keeperColor;
    }
    // A jointed figure, not a sliding box (stage 33).
    FigureMotion motion;
    motion.speed = editor.squadSpeed(id);
    motion.time = editor.figureClock();
    motion.airborne = editor.squadAirborne(id);
    motion.downed = down;
    // The feet belong on the floor: the body position is its centre.
    const Vec3 feet{at.x, at.y - kWorldPlayerRadius - 0.15, at.z};
    // A character with bones of its own uses them (stage 35). The engine's
    // figure is only the fallback for anyone who has not drawn one.
    const std::vector<CustomBone>* own = customRigFor(editor, id);
    if (own != nullptr) {
      addCustomFigure(scene, *own, motion, feet, editor.squadFacing(id), color);
    } else {
      addFigure(scene, figureRig(), motion, feet, editor.squadFacing(id), color);
    }
  }
}

// Course: a small flag pole on the cup being played, so the player can see
// which cup is next (the others are plain discs). Nothing on a finished round.
void RenderSceneBuilder::addCurrentCupFlag(RenderScene& scene, const WorldEditor& editor) const {
  if (!editor.holeScoring() || !editor.playing() || editor.roundOver()) return;
  const EntityData* cup =
      editor.world().scene.get(editor.world().scene.find(editor.currentHoleName()));
  if (cup == nullptr) return;
  const Vec3 base = cup->transform.position;
  const f64 poleHeight = 1.2;
  scene.objects.push_back({&cubeMesh_, Mat4::translation(Vec3{base.x, poleHeight * 0.5, base.z}) *
                                           Mat4::scaling(Vec3{0.04, poleHeight, 0.04}),
                           Vec3{0.92, 0.92, 0.92}, 0.6});
  scene.objects.push_back({&cubeMesh_,
                           Mat4::translation(Vec3{base.x + 0.16, poleHeight - 0.12, base.z}) *
                               Mat4::scaling(Vec3{0.3, 0.2, 0.02}),
                           Vec3{0.9, 0.15, 0.1}, 0.8});
}

const Skeleton& RenderSceneBuilder::figureRig() const { return figureRig_; }

// The diffuse map the model file itself carries (the first material that has
// one). Resolved once per file: walking the material table every frame for
// every model is exactly the kind of per-frame work a frame loop must not do.
const std::string& RenderSceneBuilder::modelTexturePath(const std::string& meshFile) {
  const auto found = modelTexturePaths_.find(meshFile);
  if (found != modelTexturePaths_.end()) return found->second;
  std::string path;
  // The asset manager's material table is the same parse the editor's
  // material view and the tints come from, and it is already in memory by the
  // time a model is drawn (its mesh was asked for first), so this is a cache
  // hit rather than a second read of the file.
  const assets::MeshAsset* asset = assets_.meshAsset(meshFile);
  if (asset != nullptr) {
    for (const MaterialData& material : asset->materials) {
      if (material.texturePath.empty()) continue;
      path = material.texturePath;
      break;
    }
  }
  return modelTexturePaths_.emplace(meshFile, std::move(path)).first->second;
}

std::map<std::string, std::string> RenderSceneBuilder::failedAssets() const {
  return failedModels_;
}

// --- The frame --------------------------------------------------------------

void RenderSceneBuilder::build(WorldEditor& editor, RenderScene& scene) {
  report_ = SceneBuildReport{};
  scene.objects.clear();
  // Last frame's posed meshes: the world may have changed, and a stale pose
  // must never be drawn for an entity that stopped animating.
  posedMeshes_.clear();
  failedModels_.clear();

  // The scene itself: the ground and everything the user placed. A goal is
  // drawn as its posts and crossbar rather than as the one box the world
  // stores; everything else is a primitive, a model or a character.
  editor.world().scene.forEach([&](EntityHandle, const EntityData& entity) {
    const ObjectKind kind = objectKindForName(entity.name);
    if (kind == ObjectKind::Goal && !isLegacyGoalPart(entity.name)) {
      addGoalShape(scene, entity);
      return;
    }
    const MeshData* mesh = &cubeMesh_;
    if (entity.mesh == MeshKind::plane) mesh = &planeMesh_;
    if (entity.mesh == MeshKind::sphere) mesh = &sphereMesh_;
    const Image* texture = nullptr;
    bool isPosed = false;
    // Model entity: the asset manager resolves the stored path, reads the
    // OBJ/FBX once and answers from memory after that. A file it cannot read
    // is remembered as such, so this costs nothing per frame and the reason is
    // in lastError().
    //
    // Everything that came out of that file — the merged mesh, the material
    // table, the per-material sub-meshes, the diffuse map — hangs off this one
    // pointer, so a model is parsed once for the whole frame path instead of
    // once per user of it.
    const assets::MeshAsset* asset =
        entity.meshFile.empty() ? nullptr : assets_.meshAsset(entity.meshFile);
    if (!entity.meshFile.empty()) {
      if (asset != nullptr) {
        mesh = &asset->mesh;
        // A playing clip re-poses the mesh every frame; otherwise the bind
        // pose draws, exactly as before.
        MeshData posed;
        if (editor.posedMesh(entity.name, posed)) {
          mesh = &posedMeshes_.insert_or_assign(entity.name, std::move(posed)).first->second;
          isPosed = true;
          ++report_.posedEntities;
        }
      } else {
        // No mesh: a bare rig (an animation-only FBX) still has a live stick
        // figure, so the entity shows up and can be played.
        MeshData stick;
        if (editor.posedStickMesh(entity.name, stick)) {
          mesh = &posedMeshes_.insert_or_assign(entity.name, std::move(stick)).first->second;
        } else {
          // Unreadable and unposable: the entity is skipped (as it always was)
          // but the reason is kept, so a caller can say which file failed and
          // why instead of only showing a smaller scene.
          failedModels_[entity.meshFile] = assets_.lastError();
          return;
        }
      }

      // Its texture, once (stage 34). The importer has always pulled the
      // diffuse map's path out of the .mtl or the FBX materials, but nothing
      // ever loaded the image — so every model rendered as a flat colour
      // however carefully it was textured.
      const std::string& modelTexture = modelTexturePath(entity.meshFile);
      if (!modelTexture.empty()) {
        const AssetManager::Texture skin = assets_.texture(modelTexture);
        if (!skin.empty()) texture = skin.image;
      }
    }
    // A texture the user put on this object beats the model's own, so a
    // picture chosen from the file list actually shows up.
    if (!entity.texture.empty()) {
      const AssetManager::Texture chosen = assets_.texture(entity.texture);
      if (!chosen.empty()) texture = chosen.image;
    }
    // Crates follow the physics bodies while playing, and the player entity
    // follows the character controller so the play character is actually
    // visible where the physics puts it (including mid-jump).
    const bool playCharacter = kind == ObjectKind::Player && editor.playing();
    Vec3 position = kind == ObjectKind::Crate
                        ? editor.cratePosition(entity.name)
                        : (playCharacter ? editor.playerPosition() : entity.transform.position);
    // The character controller reports the body's CENTER, but a model file
    // stands on its own feet (local y=0): sink a modeled player by the
    // character's half height (0.5, see CharacterBody) so its feet touch the
    // ground instead of floating.
    if (playCharacter && !entity.meshFile.empty()) position.y -= 0.5;
    const Vec3 scale =
        entity.mesh == MeshKind::sphere ? entity.transform.scale * 0.5 : entity.transform.scale;
    const Mat4 model =
        Mat4::translation(position) * entity.transform.rotation.toMat4() * Mat4::scaling(scale);
    // A model whose file brings its own materials draws one tinted piece per
    // material; anything posed (or without materials) draws whole in the
    // entity color, exactly as before.
    struct DrawCall {
      const MeshData* mesh = nullptr;
      Vec3 color{1.0, 1.0, 1.0};
      const Image* texture = nullptr;
    };
    std::vector<DrawCall> draws;
    if (asset != nullptr && !isPosed && assets::splitsByMaterial(*asset)) {
      // The tints are the editor's answer (each piece carries the entity
      // colour times the material colour) and are the same length as the
      // table's sub-meshes because both come from the same file.
      const std::vector<Vec3> tints = editor.modelTints(entity.name);
      if (tints.size() == asset->subMeshes.size()) {
        for (usize i = 0; i < asset->subMeshes.size(); ++i) {
          const Image* materialTexture = texture;
          // Each MTL/FBX material owns its own map_Kd. An explicit image
          // painted on the entity remains the override for every slot.
          if (entity.texture.empty()) {
            for (const MaterialData& material : asset->materials) {
              if (material.name != asset->subMeshes[i].materialName ||
                  material.texturePath.empty()) {
                continue;
              }
              const AssetManager::Texture loaded = assets_.texture(material.texturePath);
              if (!loaded.empty()) materialTexture = loaded.image;
              break;
            }
          }
          draws.push_back(DrawCall{&asset->subMeshes[i], tints[i], materialTexture});
        }
      }
    }
    if (draws.empty()) draws.push_back(DrawCall{mesh, entity.color, texture});
    for (const DrawCall& draw : draws) {
      scene.objects.push_back({draw.mesh, model, draw.color, entity.roughness, entity.metallic,
                               draw.texture, entity.emissive, entity.alpha});
    }
    // A full-body model needs no extra block head; the bare cube does.
    if (kind == ObjectKind::Player && entity.meshFile.empty()) {
      // A little head so the player reads as a character.
      scene.objects.push_back({&cubeMesh_, Mat4::translation(position + Vec3{0.0, 0.65, 0.0}) *
                                               Mat4::scaling(Vec3{0.3, 0.3, 0.3}),
                               entity.color, entity.roughness});
    }
  });

  // The ball follows the physics body — but only when there is one. A fresh
  // world is an EMPTY stage (an empty Unity scene: a floor and nothing else),
  // so no ball is drawn until the user adds a "Ball" object (or until PLAY,
  // where the game needs one to run).
  if (editor.world().scene.find("Ball") != 0 || editor.playing()) {
    const f64 ballRadius = editor.world().ball.radius;
    scene.objects.push_back({&sphereMesh_,
                             Mat4::translation(editor.ballPosition()) *
                                 Mat4::scaling(Vec3{ballRadius, ballRadius, ballRadius}),
                             editor.world().ball.color, 0.3});
  }
  // Ghost preview while placing, selection markers while managing.
  if (editor.placing()) addGhostShape(scene, editor);
  // Particles: small camera-facing cubes. A cube rather than a sprite because
  // the software rasteriser has no billboard path, and at this size the
  // difference is invisible.
  for (const Particle& particle : editor.particles().particles()) {
    const f64 size = particle.sizeNow();
    if (size <= 0.001) continue;
    scene.objects.push_back({&cubeMesh_,
                             Mat4::translation(particle.position) *
                                 Mat4::scaling(Vec3{size, size, size}),
                             particle.colorNow(), 1.0, 0.0, nullptr});
  }

  addSquads(scene, editor);
  // Arena tracer: a thin line along the last shot, so a firefight is readable
  // instead of invisible.
  if (editor.arenaMode() && editor.playing()) {
    const Vec3 from = editor.lastShotFrom();
    const Vec3 to = editor.lastShotTo();
    const Vec3 along = to - from;
    const f64 length = along.length();
    if (length > 0.01) {
      const i32 beads = 12;
      for (i32 i = 1; i <= beads; ++i) {
        const f64 t = static_cast<f64>(i) / static_cast<f64>(beads + 1);
        const Vec3 at = from + along * t;
        scene.objects.push_back({&cubeMesh_,
                                 Mat4::translation(at) * Mat4::scaling(Vec3{0.05, 0.05, 0.05}),
                                 Vec3{1.0, 0.9, 0.4}, 0.9});
      }
    }
  }
  addAimIndicator(scene, editor);
  addCurrentCupFlag(scene, editor);
  if (editor.selectingObject() && editor.selectedEntity() != nullptr) {
    addSelectionMarkers(scene, *editor.selectedEntity());
  }

  report_.objects = scene.objects.size();
  report_.missingAssets = assets_.missingAssets().size();
  report_.assets = assets_.stats();
}

}  // namespace kimia
