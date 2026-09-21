// Playing clips, and the pose that comes out.
//
// Split out of World.cpp with no behaviour change. Two halves, and the seam
// between them is the point:
//
//   * SKELETON-FREE authors: a character with a hand-made rig (entity.rig) and
//     no FBX skeleton gets its figure drawn from bones the editor moved
//     (posedStickMesh, skinnedForEntity, characterBoneMarkers).
//   * FBX clips: an imported model with a real skeleton gets a clip played
//     through Engine/Animation's Animator (playClip, startClipFrom,
//     posedMesh, animationClips).
//
// Assets are read through the editor's one AssetManager
// (Documentation/AssetManager.md), so a model imported in the editor and a
// model drawn in a frame are the same parse.
#include "WorldInternal.h"

#include <kimia/Animator.h>
#include <kimia/AssetManager.h>
#include <kimia/Skeleton.h>
#include <kimia/World.h>

#include <algorithm>
#include <cmath>
#include <map>

namespace kimia {

using namespace worldinternal;  // the World module's own toolbox (see the header)


void WorldEditor::stepOnce(f64 seconds) {
  if (seconds <= 0.0) return;
  const bool wasPaused = paused_;
  paused_ = false;
  update(seconds);
  paused_ = wasPaused;
}

bool WorldEditor::enterPlayMode() {
  if (!hasWorld_) return false;
  paused_ = false;  // Play always starts running, like Unity's button
  enterPlay();
  return true;
}

u32 WorldEditor::fireTrigger(const std::string& trigger) {
  animationError_.clear();
  if (trigger.empty()) return 0U;
  u32 fired = 0U;
  world_.scene.forEach([this, &trigger, &fired](EntityHandle, const EntityData& entity) {
    for (const AnimationComponent& clip : entity.animations) {
      if (clip.trigger != trigger) continue;
      startClip(entity.name, clip.clip, clip.loop, clip.speed);
      ++fired;
    }
    for (const SoundComponent& sound : entity.sounds) {
      if (sound.trigger != trigger) continue;
      triggeredSounds_.push_back(sound.sound);
      ++fired;
    }
  });
  // Dialogue listens to the same names: a key press or a game event that
  // plays a clip and a sound plays the line too, with no extra wiring.
  fired += static_cast<u32>(fireDialogueTrigger(trigger));
  return fired;
}

std::vector<std::string> WorldEditor::drainTriggeredSounds() {
  std::vector<std::string> drained;
  drained.swap(triggeredSounds_);
  return drained;
}

std::vector<std::string> WorldEditor::playingAnimations() const {
  std::vector<std::string> playing;
  playing.reserve(playingClips_.size());
  for (const PlayingClip& clip : playingClips_) {
    const std::string& prefix = clip.displayAsset.empty() ? clip.entity : clip.displayAsset;
    playing.push_back(prefix + ":" + clip.clip);
  }
  return playing;
}

void WorldEditor::playClip(const std::string& file, const std::string& clip, const std::string& target) {
  animationError_.clear();
  if (clip.empty()) return;

  std::vector<std::string> targets;
  if (!target.empty()) {
    const EntityData* requested = entity(target);
    if (requested != nullptr && (!requested->meshFile.empty() || !requested->rig.empty())) targets.push_back(target);
  } else {
    // Legacy wiring to a model file still means "all characters using this
    // file". A basename match keeps old worlds portable when their asset root
    // spelling changed.
    world_.scene.forEach([&file, &targets](EntityHandle, const EntityData& candidate) {
      if (candidate.meshFile.empty()) return;
      if (candidate.meshFile == file || baseName(candidate.meshFile) == baseName(file)) targets.push_back(candidate.name);
    });

    // New animator wiring can use a separate one-clip FBX. In that case pick
    // the first compatible character instead of creating a dead request.
    if (targets.empty() && !file.empty()) {
      const assets::SkinnedAsset* source = skinnedFor(file);
      if (source != nullptr) {
        world_.scene.forEach([this, source, &targets](EntityHandle, const EntityData& candidate) {
          if (!targets.empty() || (candidate.meshFile.empty() && candidate.rig.empty())) return;
          const assets::SkinnedAsset* targetAsset = skinnedForEntity(candidate);
          if (targetAsset != nullptr &&
              Animator::retargetable(source->skinned.skeleton, targetAsset->skinned.skeleton)) {
            targets.push_back(candidate.name);
          }
        });
      }
    }
  }

  if (targets.empty()) {
    // A button may be authored before its character is imported. Keep one
    // visible pending state so the Workbench can confirm the wiring; the
    // next explicit play on a compatible target will turn it into a pose.
    const std::string pendingKey = file.empty() && !target.empty() ? target : file;
    for (PlayingClip& pending : playingClips_) {
      if (pending.entity.empty() && pending.displayAsset == pendingKey && pending.clip == clip) {
        pending.pendingTime = 0.0;
        pending.loop = false;
        return;
      }
    }
    PlayingClip pending;
    pending.displayAsset = pendingKey;
    pending.clip = clip;
    pending.loop = false;
    pending.valid = false;
    playingClips_.push_back(std::move(pending));
    animationError_ = "no compatible character target for clip '" + clip + "'";
    if (!file.empty()) animationError_ += " from '" + file + "'";
    return;
  }

  for (const std::string& each : targets) {
    startClipFrom(each, file, clip, false, 1.0, file.empty() ? each : file);
  }
}

const assets::SkinnedAsset* WorldEditor::skinnedFor(const std::string& meshFile) {
  if (meshFile.empty()) return nullptr;
  // The manager caches the parse AND the failure, so a broken path costs the
  // disk once for the whole session rather than once per query.
  const assets::SkinnedAsset* asset = assets_.skinned(meshFile);
  if (asset == nullptr || !asset->hasSkeleton()) return nullptr;
  return asset;
}

const assets::SkinnedAsset* WorldEditor::skinnedForEntity(const EntityData& target) {
  // The real FBX skeleton always wins. An author-provided rig is only a
  // fallback for a character whose model has no usable skin; ordinary OBJ
  // props therefore keep the old hasSkeleton() answer of false.
  if (!target.meshFile.empty()) {
    if (const assets::SkinnedAsset* imported = skinnedFor(target.meshFile)) return imported;
  }
  if (target.rig.empty()) return nullptr;

  const std::string key = "@entity-rig:" + target.name;
  const auto cached = authorRigCache_.find(key);
  if (cached != authorRigCache_.end()) {
    return cached->second.has_value() ? &(*cached->second) : nullptr;
  }

  assets::SkinnedAsset fallback;
  std::vector<bool> added(target.rig.size(), false);
  std::vector<i32> boneIndices(target.rig.size(), kNoParentBone);
  usize built = 0U;
  while (built < target.rig.size()) {
    bool progress = false;
    for (usize sourceIndex = 0; sourceIndex < target.rig.size(); ++sourceIndex) {
      if (added[sourceIndex]) continue;
      const RigBone& source = target.rig[sourceIndex];
      i32 parent = kNoParentBone;
      if (!source.parent.empty()) {
        for (usize candidate = 0; candidate < target.rig.size(); ++candidate) {
          if (target.rig[candidate].name == source.parent && added[candidate]) {
            parent = boneIndices[candidate];
            break;
          }
        }
        if (parent == kNoParentBone && built + 1U < target.rig.size()) continue;
      }

      Bone bone;
      bone.name = source.name;
      bone.parent = parent;
      const Vec3 parentFrom = parent >= 0 ? target.rig[static_cast<usize>(parent)].from : Vec3{};
      bone.restPose.position = source.from - parentFrom;
      bone.restPose.rotation = Quat{};
      bone.restPose.scale = Vec3{1.0, 1.0, 1.0};
      fallback.skinned.skeleton.bones.push_back(std::move(bone));
      boneIndices[sourceIndex] = static_cast<i32>(fallback.skinned.skeleton.bones.size() - 1U);
      added[sourceIndex] = true;
      ++built;
      progress = true;
    }
    if (progress) continue;
    // A malformed rig (usually a parent typo or cycle) should remain
    // queryable rather than hanging the editor. Break the cycle as a root.
    for (usize sourceIndex = 0; sourceIndex < target.rig.size(); ++sourceIndex) {
      if (added[sourceIndex]) continue;
      Bone bone;
      bone.name = target.rig[sourceIndex].name;
      bone.parent = kNoParentBone;
      bone.restPose.position = target.rig[sourceIndex].from;
      fallback.skinned.skeleton.bones.push_back(std::move(bone));
      boneIndices[sourceIndex] = static_cast<i32>(fallback.skinned.skeleton.bones.size() - 1U);
      added[sourceIndex] = true;
      ++built;
      break;
    }
  }

  std::vector<Transform3D> rest;
  rest.reserve(fallback.skinned.skeleton.bones.size());
  for (const Bone& bone : fallback.skinned.skeleton.bones) rest.push_back(bone.restPose);
  std::vector<Mat4> world;
  computeWorldMatrices(fallback.skinned.skeleton, rest, world);
  for (usize i = 0; i < world.size(); ++i) fallback.skinned.skeleton.bones[i].inverseBindPose = world[i].inverse();

  auto inserted = authorRigCache_.emplace(key, std::optional<assets::SkinnedAsset>(std::move(fallback)));
  return inserted.first->second.has_value() ? &(*inserted.first->second) : nullptr;
}

void WorldEditor::startClip(const std::string& entityName, const std::string& clipName, bool loop, f64 speed) {
  startClipFrom(entityName, std::string(), clipName, loop, speed, entityName);
}

void WorldEditor::startClipFrom(const std::string& entityName, const std::string& sourceFile,
                                const std::string& clipName, bool loop, f64 speed,
                                const std::string& action) {
  EntityData* target = world_.scene.get(world_.scene.find(entityName));
  if (target == nullptr || (target->meshFile.empty() && target->rig.empty())) return;

  const assets::SkinnedAsset* targetAsset = skinnedForEntity(*target);
  std::string resolvedSourceFile = sourceFile;
  const assets::SkinnedAsset* sourceAsset = sourceFile.empty() ? targetAsset : skinnedFor(sourceFile);
  // `playClip("hero.fbx", ...)` is intentionally allowed when the scene
  // stores `characters/hero.fbx`; the source cache needs the entity's
  // resolved spelling even though the diagnostic keeps the short spelling.
  if (sourceAsset == nullptr && !sourceFile.empty() &&
      baseName(target->meshFile) == baseName(sourceFile)) {
    resolvedSourceFile = target->meshFile;
    sourceAsset = skinnedFor(resolvedSourceFile);
  }
  const AnimationClip* sourceClip = nullptr;
  if (sourceAsset != nullptr) {
    for (const AnimationClip& candidate : sourceAsset->clips) {
      if (candidate.name == clipName) {
        sourceClip = &candidate;
        break;
      }
    }
  }

  PlayingClip* playing = nullptr;
  for (PlayingClip& candidate : playingClips_) {
    if (candidate.entity == entityName) {
      playing = &candidate;
      break;
    }
  }
  if (playing == nullptr) {
    PlayingClip fresh;
    fresh.entity = entityName;
    playingClips_.push_back(std::move(fresh));
    playing = &playingClips_.back();
  }
  playing->displayAsset = sourceFile;
  playing->clip = clipName;
  playing->loop = loop;
  playing->speed = speed > 0.01 ? speed : 0.01;
  playing->pendingTime = 0.0;
  playing->duration = sourceClip != nullptr && sourceClip->duration > 0.0 ? sourceClip->duration : kTriggerClipSeconds;

  if (targetAsset == nullptr || sourceAsset == nullptr || sourceClip == nullptr ||
      !Animator::retargetable(sourceAsset->skinned.skeleton, targetAsset->skinned.skeleton)) {
    // Keep the diagnostic entry for an invalid/missing clip, matching the
    // editor's forgiving old behavior, but never pretend it can deform a
    // mesh. A bad replacement also stops the old pose rather than leaving a
    // previous action visible under an error message.
    playing->valid = false;
    playing->animator.stop();
    if (targetAsset == nullptr) {
      animationError_ = "target '" + entityName + "' has no usable skeleton";
    } else if (sourceAsset == nullptr) {
      animationError_ = "clip source '" + sourceFile + "' could not be loaded";
    } else if (sourceClip == nullptr) {
      animationError_ = "clip '" + clipName + "' was not found in '" + (sourceFile.empty() ? target->meshFile : sourceFile) + "'";
    } else {
      animationError_ = "clip '" + clipName + "' has no compatible bone names for target '" + entityName + "'";
    }
    return;
  }

  // Do not stop an existing state before playAction(): Animator retains it
  // as the previous state and blends into the replacement for 150 ms.
  playing->animator.setTarget(&targetAsset->skinned.skeleton);
  const AnimatorClip reference{&sourceAsset->skinned.skeleton, sourceClip, resolvedSourceFile};
  const std::string actionName = action.empty() ? clipName : action;
  if (!playing->animator.bindAction(actionName, reference, loop, playing->speed, kAnimationBlendSeconds) ||
      !playing->animator.playAction(actionName)) {
    playing->valid = false;
    playing->animator.stop();
    return;
  }
  playing->valid = true;
  animationError_.clear();
}

std::vector<std::string> WorldEditor::animationClips(const std::string& entityName) {
  std::vector<std::string> names;
  const EntityData* target = entity(entityName);
  if (target == nullptr || target->meshFile.empty()) return names;
  const assets::SkinnedAsset* asset = skinnedForEntity(*target);
  if (asset == nullptr) return names;
  for (const AnimationClip& clip : asset->clips) names.push_back(clip.name);
  return names;
}

bool WorldEditor::hasSkeleton(const std::string& entityName) {
  const EntityData* target = entity(entityName);
  if (target == nullptr) return false;
  return skinnedForEntity(*target) != nullptr;
}

bool WorldEditor::posedMesh(const std::string& entityName, MeshData& out) {
  const EntityData* target = entity(entityName);
  if (target == nullptr || target->meshFile.empty()) return false;
  const assets::SkinnedAsset* asset = skinnedForEntity(*target);
  if (asset == nullptr || asset->skinned.bindMesh.positions.empty()) return false;

  const PlayingClip* active = nullptr;
  for (const PlayingClip& playing : playingClips_) {
    if (playing.entity == entityName && playing.valid) {
      active = &playing;
      break;
    }
  }
  if (active == nullptr) return false;

  std::vector<Transform3D> pose;
  if (!active->animator.samplePose(pose)) return false;
  std::vector<Mat4> matrices;
  computeSkinMatrices(asset->skinned.skeleton, pose, matrices);
  return skinMesh(asset->skinned, matrices, out);
}

bool WorldEditor::posedStickMesh(const std::string& entityName, MeshData& out) {
  const EntityData* target = entity(entityName);
  if (target == nullptr || (target->meshFile.empty() && target->rig.empty())) return false;
  const assets::SkinnedAsset* asset = skinnedForEntity(*target);
  if (asset == nullptr || asset->skinned.skeleton.isEmpty()) return false;
  const Skeleton& skeleton = asset->skinned.skeleton;

  std::vector<Transform3D> pose;
  pose.reserve(skeleton.bones.size());
  for (const Bone& bone : skeleton.bones) pose.push_back(bone.restPose);
  for (const PlayingClip& playing : playingClips_) {
    if (playing.entity != entityName || !playing.valid) continue;
    static_cast<void>(playing.animator.samplePose(pose));
    break;
  }

  std::vector<Mat4> world;
  computeWorldMatrices(skeleton, pose, world);
  std::vector<Vec3> joints;
  joints.reserve(world.size());
  for (const Mat4& matrix : world) {
    joints.push_back(Vec3{matrix.at(3, 0), matrix.at(3, 1), matrix.at(3, 2)});
  }
  // Auto thickness: the file's own units never reach the screen, since the
  // import fit scales the whole figure to the requested size.
  out = skeletonStickMesh(skeleton, joints, 0.0);
  return out.isValid();
}

const assets::MeshAsset* WorldEditor::assetFor(const std::string& meshFile) {
  if (meshFile.empty()) return nullptr;
  // The same table the draw-list builder uses — literally the same object, so
  // the editor and the frame can never disagree about a model's materials,
  // and a file is parsed once however many questions are asked about it.
  const assets::MeshAsset* stored = assets_.meshAsset(meshFile);
  // A mesh with no material info at all draws the old way (one entity-colored
  // piece); the split path is only for files whose materials say something.
  if (stored == nullptr || !assets::splitsByMaterial(*stored)) return nullptr;
  return stored;
}

std::vector<BoneMarker> WorldEditor::characterBoneMarkers(const std::string& entityName) {
  const EntityData* target = entity(entityName);
  if (target == nullptr) return std::vector<BoneMarker>();

  const assets::SkinnedAsset* imported = target->meshFile.empty() ? nullptr : skinnedFor(target->meshFile);
  std::vector<BoneMarker> markers;
  if (imported != nullptr) {
    const Skeleton& skeleton = imported->skinned.skeleton;
    std::vector<Transform3D> rest;
    rest.reserve(skeleton.bones.size());
    for (const Bone& bone : skeleton.bones) rest.push_back(bone.restPose);
    for (const PlayingClip& playing : playingClips_) {
      if (playing.entity != entityName || !playing.valid) continue;
      static_cast<void>(playing.animator.samplePose(rest));
      break;
    }
    markers = boneMarkers(skeleton, rest);
  } else if (!target->rig.empty()) {
    // EntityData::rig is the editor's explicit fallback for an OBJ, a
    // non-skinned FBX, or a missing model. While a separate animation FBX is
    // playing, use the synthesized target skeleton so named anchors follow
    // the live pose; at rest retain each authored leaf endpoint, which is
    // more useful to tools such as hit boxes and muzzle/hand effects.
    const assets::SkinnedAsset* fallback = skinnedForEntity(*target);
    const PlayingClip* active = nullptr;
    for (const PlayingClip& playing : playingClips_) {
      if (playing.entity == entityName && playing.valid) {
        active = &playing;
        break;
      }
    }
    if (active != nullptr && fallback != nullptr) {
      std::vector<Transform3D> pose;
      pose.reserve(fallback->skinned.skeleton.bones.size());
      for (const Bone& bone : fallback->skinned.skeleton.bones) pose.push_back(bone.restPose);
      if (active->animator.samplePose(pose)) markers = boneMarkers(fallback->skinned.skeleton, pose);
    }
    if (markers.empty()) {
      markers.reserve(target->rig.size());
      for (const RigBone& bone : target->rig) {
        BoneMarker marker;
        marker.name = bone.name;
        marker.localStart = bone.from;
        marker.localEnd = bone.to;
        marker.localCenter = (bone.from + bone.to) * 0.5;
        marker.start = marker.localStart;
        marker.end = marker.localEnd;
        marker.center = marker.localCenter;
        marker.length = (bone.to - bone.from).length();
        markers.push_back(std::move(marker));
      }
    }
  } else {
    return markers;
  }

  const Mat4 object = Mat4::translation(target->transform.position) * target->transform.rotation.toMat4() *
                      Mat4::scaling(target->transform.scale);
  for (BoneMarker& marker : markers) {
    // Preserve local values even when the caller receives world-space values.
    marker.localStart = marker.start;
    marker.localEnd = marker.end;
    marker.localCenter = marker.center;
    marker.start = object * marker.localStart;
    marker.end = object * marker.localEnd;
    marker.center = object * marker.localCenter;
    marker.length = (marker.end - marker.start).length();
  }
  return markers;
}

std::optional<Vec3> WorldEditor::characterBoneCenter(const std::string& entityName,
                                                     const std::string& boneName) {
  if (boneName.empty()) return std::nullopt;
  const std::vector<BoneMarker> markers = characterBoneMarkers(entityName);
  for (const BoneMarker& marker : markers) {
    if (marker.name == boneName) return marker.center;
  }
  return std::nullopt;
}

std::vector<Vec3> WorldEditor::modelTints(const std::string& entityName) {
  const EntityData* target = entity(entityName);
  if (target == nullptr || target->meshFile.empty()) return std::vector<Vec3>();
  const assets::MeshAsset* asset = assetFor(target->meshFile);
  if (asset == nullptr || asset->subMeshes.empty()) return std::vector<Vec3>();
  std::vector<Vec3> tints;
  tints.reserve(asset->subMeshes.size());
  for (const MeshData& sub : asset->subMeshes) {
    Vec3 tint = target->color;
    for (const MaterialData& material : asset->materials) {
      if (material.name != sub.materialName) continue;
      tint = Vec3{tint.x * material.color.x, tint.y * material.color.y, tint.z * material.color.z};
      break;
    }
    tints.push_back(tint);
  }
  return tints;
}

bool WorldEditor::stopEntityClips(const std::string& entityName) {
  bool stopped = false;
  for (usize i = playingClips_.size(); i > 0U; --i) {
    if (playingClips_[i - 1U].entity != entityName) continue;
    playingClips_.erase(playingClips_.begin() + static_cast<std::ptrdiff_t>(i - 1U));
    stopped = true;
  }
  return stopped;
}

void WorldEditor::updateTriggers(f64 seconds) {
  if (seconds <= 0.0 || playingClips_.empty()) return;
  for (usize i = playingClips_.size(); i > 0U; --i) {
    PlayingClip& clip = playingClips_[i - 1U];
    if (clip.valid) {
      clip.animator.update(seconds);
      if (!clip.animator.playing()) {
        playingClips_.erase(playingClips_.begin() + static_cast<std::ptrdiff_t>(i - 1U));
      }
      continue;
    }

    // Missing assets and incompatible skeletons remain visible as a
    // diagnostic for one normal trigger beat, but never generate a fake pose.
    clip.pendingTime += seconds * clip.speed;
    if (!clip.loop && clip.pendingTime >= clip.duration) {
      playingClips_.erase(playingClips_.begin() + static_cast<std::ptrdiff_t>(i - 1U));
    }
  }
}

}  // namespace kimia
