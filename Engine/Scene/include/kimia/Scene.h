#pragma once

#include <kimia/Entity.h>
#include <kimia/Quat.h>
#include <kimia/Types.h>
#include <kimia/Vec.h>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kimia {

struct Transform {
  Vec3 position{0.0, 0.0, 0.0};
  Vec3 scale{1.0, 1.0, 1.0};
  Quat rotation{};  // identity
};

// Mesh reference: primitive kind (scene v1) — later stages extend this with
// real mesh assets.
enum class MeshKind { cube, plane, sphere };

// --- Components and tags (stage 31) ---
//
// Until now an entity was a shape and a colour, and everything it could DO
// was decided by its name: "Ball" bounced, "Goal*" scored, "Crate_*" was
// pushable. That works for four built-in games and for nothing else — you
// could not import a model and make it a solid, animated, noisy thing.
//
// An entity now carries optional components instead. The name still works
// (every existing world loads unchanged), but a component overrides it.

// How an entity takes part in the simulation.
enum class BodyKind {
  None,     // decoration: drawn, never collided with
  Static,   // immovable solid: walls, cover, scenery
  Dynamic,  // pushed around: crates, props
  Sphere,   // rolling body: balls
};

// Physics component. Attached to anything that should be solid.
struct BodyComponent {
  BodyKind kind = BodyKind::None;
  f64 mass = 1.0;
  f64 friction = 0.4;
  f64 restitution = 0.3;   // bounciness, 0 = dead, 1 = perfectly elastic
  f64 radius = 0.0;        // Sphere only; 0 = derive from the transform
};

// Animation component: a clip from the entity's model, and the input that
// plays it. `trigger` is a key name ("j", "space") or one of the built-in
// events ("walk", "idle", "kick") the game drives itself.
struct AnimationComponent {
  std::string clip;     // clip name inside the FBX
  std::string trigger;  // key name or built-in event
  bool loop = true;
  f64 speed = 1.0;
};

// --- Rig component (stage 35) ---
//
// One bone of a character, as the editor edits it: a named segment with a
// start and an end, in the character's own space with the feet at y = 0.
// This is deliberately "from here to there" rather than a rotation and a
// length, because that is how a person describes a limb and how the
// Workbench lets you drag one.
struct RigBone {
  std::string name;    // "LeftLeg", "Tail", whatever the character needs
  std::string parent;  // parent bone's name ("" = a root)
  Vec3 from{0.0, 0.0, 0.0};
  Vec3 to{0.0, 0.0, 0.0};
  f64 thickness = 0.08;
  // Which way the bone swings when the character walks. 0 = still (a head
  // or a torso), 1 = a full stride, negative = opposite phase, which is
  // how the arms are made to swing against the legs.
  f64 swing = 0.0;
};

// Dialogue component: one spoken line and when it plays.
//
// Phase 8 needs a data-driven story ("at least ten lines, not hard-coded"),
// and this is the piece that makes a line DATA: it is authored in the editor,
// saved in the .kimia file with everything else, reached by the same trigger
// names animations and sounds use, and therefore testable and replayable
// without a dialogue system existing yet.
//
// The speaker is deliberately not a field: the component belongs to the
// entity that speaks, so a line can never be attributed to the wrong actor by
// a typo — and an entity that is deleted takes its dialogue with it.
struct DialogueComponent {
  std::string line;     // the text, in whatever language the game is in
  std::string trigger;  // key name or built-in event, like AnimationComponent
  f64 volume = 1.0;     // how loud, when the line is voiced
  f64 holdSeconds = 3.0;  // how long a caption stays on screen
};

// Camera target component: this entity is what the camera should look at.
//
// The editor has always decided the camera's subject by rules inside
// WorldEditor::cameraTarget() (the ball while playing, the selected object
// while editing). This component lets a WORLD overrule that: the entity
// carrying it wins, with a weight, so an author can say "watch the keeper" or
// "this is the hero of this scene" and mean it.
struct CameraTargetComponent {
  f64 weight = 1.0;    // higher wins when several entities carry it
  bool whilePlaying = true;  // false = only while editing
  Vec3 offset{0.0, 0.0, 0.0};  // look slightly above/beside the entity
};

// Sound component: a registered sound name, played on the same kind of
// trigger as an animation.
struct SoundComponent {
  std::string sound;    // name registered with the server (/sfx/<name>)
  std::string trigger;
  f64 volume = 1.0;
};

struct EntityData {
  std::string name;
  Transform transform;
  MeshKind mesh = MeshKind::cube;
  // Model entities: a mesh asset file (OBJ/FBX) placed in the scene. Empty
  // for primitive entities. Rendering resolves this path at run time; the
  // primitive mesh acts as the fallback shape.
  std::string meshFile;
  // An image painted onto this object, chosen from the asset folder.
  // Empty means the model's own texture (or a flat colour).
  std::string texture;
  Vec3 color{1.0, 1.0, 1.0};
  f64 roughness = 0.5;
  // Cook-Torrance metalness, 0..1 (0 = dielectric, 1 = full metal). See
  // Pbr.h. Defaults to 0 so every scene saved before PBR keeps its look.
  f64 metallic = 0.0;
  // Self-illumination (LINEAR, HDR), added on top of the lights.
  Vec3 emissive{0.0, 0.0, 0.0};
  // Opacity: 1.0 = opaque; below blends over the frame (back-to-front).
  f64 alpha = 1.0;

  // --- Components (stage 31) ---
  // Free-form labels. A tag is how one object refers to a GROUP of others
  // without knowing their names: "goal", "cover", "enemy". The editor and
  // the game rules both look things up this way.
  std::vector<std::string> tags;
  // Optional components. Absent means "this entity does not do that".
  std::optional<BodyComponent> body;
  std::optional<CameraTargetComponent> cameraTarget;
  std::vector<AnimationComponent> animations;
  std::vector<SoundComponent> sounds;
  std::vector<DialogueComponent> dialogue;
  // A character's own bones (stage 35). Empty means "use the engine's
  // default figure", so nothing that worked before needs changing.
  std::vector<RigBone> rig;

  bool hasTag(const std::string& tag) const {
    for (const std::string& own : tags) {
      if (own == tag) return true;
    }
    return false;
  }
  void addTag(const std::string& tag) {
    if (!tag.empty() && !hasTag(tag)) tags.push_back(tag);
  }
  bool removeTag(const std::string& tag) {
    for (usize i = 0; i < tags.size(); ++i) {
      if (tags[i] != tag) continue;
      tags.erase(tags.begin() + static_cast<std::ptrdiff_t>(i));
      return true;
    }
    return false;
  }
};

// Player-authored demo shot stored in scene files as "# demo <aim> <power>".
struct DemoShot {
  f64 aim = 0.0;
  f64 power = 0.0;
};

// Entity container.
//
// Identity (see Documentation/Scene.md for the reasoning):
//
//   * Handles are 1-based, zero is null, and an id is issued ONCE — a
//     destroyed handle is never handed out again, so a stale handle can never
//     address a newer entity. That is why there is no separate generation
//     counter: the id itself is the generation.
//   * Names are the working key of the editor and the games. Duplicates are
//     allowed (old files and tools must keep working) and find() always
//     answers with the LOWEST live handle for a name, which makes the answer
//     independent of insert history.
//   * An empty name is not indexed: unnamed means unfindable.
//   * Iteration is in handle order (deterministic, the base of stable
//     serialization).
//
// Lookup goes through a name index, because find() is called from loops
// (physics rebuild, sound lookup, object naming) and used to walk the whole
// scene every time.
class Scene {
public:
  Scene() = default;
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = default;
  Scene& operator=(Scene&&) = default;

  // Copying a scene is deliberately not silent: scenes are large and an
  // accidental copy would be an expensive surprise. Switching stages
  // genuinely needs one, so it asks for it by name.
  //
  // A clone keeps the handles of the original: once ids are written to files
  // (scene v2), an entity's id is part of what it is, and a stage switch must
  // not renumber it.
  Scene clone() const;

  EntityHandle create(const std::string& name = "Entity");
  EntityHandle create(const EntityData& data);
  // Puts an entity at a specific id, for a loader restoring a file's own ids.
  // False when the id is null or already taken. The next created entity gets
  // an id above the highest restored one.
  bool restore(EntityHandle handle, const EntityData& data);
  bool destroy(EntityHandle handle);
  // Renames through the index. The blessed path — writing entity->name
  // directly still works (find() notices and repairs the index), but this one
  // leaves the index exact without a rebuild.
  bool rename(EntityHandle handle, const std::string& name);

  EntityData* get(EntityHandle handle);
  const EntityData* get(EntityHandle handle) const;
  bool alive(EntityHandle handle) const;
  usize count() const { return entities_.size(); }

  // Lowest live handle with this name, or kNullEntity.
  EntityHandle find(const std::string& name) const;
  // A name nobody is using: "Block" when it is free, else "Block_2",
  // "Block_3", … The editor's spelling of "make me another one".
  std::string uniqueName(const std::string& wanted) const;

  void forEach(const std::function<void(EntityHandle, const EntityData&)>& callback) const;
  void clear();

  // How many times the name index had to be rebuilt because an entity was
  // renamed behind its back (see find()). Zero in a well-behaved frame loop;
  // a rising number is a caller that should use rename().
  usize nameIndexRebuilds() const { return nameIndexRebuilds_; }
  // Live names in the index; for tests and diagnostics.
  usize indexedNameCount() const { return names_.size(); }

  std::optional<DemoShot> demoShot;

private:
  void indexName(const EntityData& entity, EntityHandle handle) const;
  void reindex() const;

  std::map<EntityHandle, EntityData> entities_;
  // Mutable because find() is const and repairs the index (a cache, not
  // state: it can always be rebuilt from entities_).
  mutable std::unordered_map<std::string, EntityHandle> names_;
  mutable usize nameIndexRebuilds_ = 0U;
  u32 nextHandle_ = 1U;
};

}  // namespace kimia
