#pragma once

#include <kimia/AssetPipeline.h>
#include <kimia/Image.h>
#include <kimia/Mesh.h>
#include <kimia/Types.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace kimia {

// --- AssetManager: the one place a file is read ----------------------------
//
// Before this existed the frame loop kept its own std::map caches and called
// assets::loadMesh / loadImage directly, in three different places, each
// resolving the path its own way. That made three things impossible:
//
//   * saying WHY a missing model is missing (it was simply never drawn),
//   * not asking the disk again for a file that is not there (no negative
//     caching, so a bad path cost a failed open every frame),
//   * telling "same image as last frame" cheaply (pixel comparison).
//
// The manager answers all three: every load goes through here, every result
// (including a failure) is cached, and a texture carries a revision number
// that only moves when the pixels actually change.
//
// Threading: not thread safe. The frame loop owns it; the Workbench's asset
// scan runs on the same thread it always did.
class AssetManager {
public:
  // Counters, so "the cache works" is a measurement rather than a claim.
  // requests is every call; hits is a call answered from memory; loads is a
  // call that really read and parsed the file; failures is loads that failed
  // (each remembered, so a broken path fails once, not once per frame).
  struct Stats {
    u64 requests = 0U;
    u64 hits = 0U;
    u64 loads = 0U;
    u64 failures = 0U;
  };

  AssetManager() = default;
  // Copying is refused (a second copy of every cached mesh is never wanted),
  // moving is fine: the editor owns one of these and is itself returned by
  // value from the helpers that build one.
  AssetManager(const AssetManager&) = delete;
  AssetManager& operator=(const AssetManager&) = delete;
  AssetManager(AssetManager&&) = default;
  AssetManager& operator=(AssetManager&&) = default;

  // --- Where the files live ------------------------------------------------
  //
  // Roots are searched in order for every relative path a world stores, so
  // the same .kimia file works from a build tree, from an installed package
  // and from a phone (where the assets sit under the app's data directory).
  void addRoot(const std::string& directory);
  void setRoots(const std::vector<std::string>& directories);
  // One root at a time: the project's own asset folder.
  void setProjectRoot(const std::string& directory);
  const std::vector<std::string>& roots() const { return roots_; }

  // Turns a path stored in a world file into a path that can be opened NOW.
  //
  // Order:
  //   1. the path as given, when it is absolute, or relative to the working
  //      directory and not climbing out of it — this is what every world made
  //      before the project root existed stores, so it must keep working;
  //   2. each root, in order.
  //
  // A relative path can never escape a root through "..": a world file is
  // data, and data does not get to read the whole disk. Returns the empty
  // string when the file is nowhere.
  std::string resolve(const std::string& file) const;
  bool exists(const std::string& file) const { return !resolve(file).empty(); }

  // --- Cached loaders ------------------------------------------------------
  // Every one of these returns nullptr when the asset is unusable, and
  // records the reason in lastError(). Pointers stay valid until
  // invalidate()/clear() (the containers are node-based).
  // The merged mesh of a file, for callers that just want to draw it. Same
  // cache and same parse as meshAsset(): a file is read once however many
  // shapes of it are asked for.
  const MeshData* mesh(const std::string& file);
  // The merged mesh plus its material table and per-material sub-meshes.
  const assets::MeshAsset* meshAsset(const std::string& file);
  const assets::SkinnedAsset* skinned(const std::string& file);

  // --- Textures ------------------------------------------------------------
  //
  // An image plus its revision. `revision` changes only when the pixels do
  // (a load, or a reload after invalidate()), so a renderer can compare one
  // number instead of hashing bytes every frame.
  struct Texture {
    const Image* image = nullptr;
    u64 revision = 0U;
    bool empty() const { return image == nullptr || image->isEmpty(); }
  };
  Texture texture(const std::string& file);
  // The revision of a path already asked for; 0 when it was never loaded or
  // has no pixels.
  u64 textureRevision(const std::string& file) const;

  // Forgets one path (all four caches) or everything. The next request reads
  // the file again — and bumps the texture revision, which is how the asset
  // browser tells the renderer "this image changed on disk".
  bool invalidate(const std::string& file);
  void clear();

  // --- Diagnostics ---------------------------------------------------------
  // Why the LAST request failed; empty after a request that succeeded. Kept
  // in the same shape whether the failure is fresh or remembered, so a caller
  // that only wants to explain itself never gets a blank answer.
  //
  // "Failed" includes a file that exists but cannot answer THIS request (an
  // OBJ asked for as a skeleton, a PNG asked for as a mesh). That is a real
  // failure of the request, and lastError() says so.
  const std::string& lastError() const { return lastError_; }
  // Paths whose FILE could not be found, in a stable (sorted) order: what the
  // editor shows in its "missing assets" list, and what the frame report
  // counts. A file that exists but does not fit the request is deliberately
  // NOT in here: "is my skeleton rig missing" is a different question from
  // "did the model ship with the build", and answering the second with the
  // first makes the list unreadable on any world with OBJ props.
  std::vector<std::string> missingAssets() const;

  const Stats& stats() const { return stats_; }
  void resetStats() { stats_ = Stats{}; }

private:
  template <typename T>
  struct Entry {
    bool tried = false;       // a load was attempted (success or not)
    bool missingFile = false;  // ...and the file itself was not there
    T value{};
    std::string error;
  };

  struct ImageEntry {
    bool tried = false;
    bool missingFile = false;
    Image image;
    u64 revision = 0U;
    std::string error;
  };

  // Resolves and remembers the reason when it cannot.
  std::string resolveFor(const std::string& file, bool& missing);

  std::vector<std::string> roots_;
  std::map<std::string, Entry<std::optional<assets::MeshAsset>>> meshAssets_;
  std::map<std::string, Entry<std::optional<assets::SkinnedAsset>>> skinned_;
  std::map<std::string, ImageEntry> images_;
  std::string lastError_;
  u64 nextRevision_ = 1U;
  Stats stats_;
};

}  // namespace kimia
