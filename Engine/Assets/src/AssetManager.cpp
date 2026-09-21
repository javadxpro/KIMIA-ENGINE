#include <kimia/AssetManager.h>

#include <algorithm>
#include <filesystem>

namespace kimia {

namespace {

namespace fs = std::filesystem;

bool isRegularFile(const fs::path& path) {
  std::error_code error;
  const bool found = fs::is_regular_file(path, error);
  return found && !error;
}

// The path climbing above its own start ("../x", "a/../../x"). Checked on the
// normalized form, so "assets/../Worlds/x" is fine while "../x" is not: the
// first stays inside the tree it was given, the second leaves it.
bool climbsOut(const fs::path& normalized) {
  auto part = normalized.begin();
  if (part == normalized.end()) return false;
  const std::string first = part->string();
  return first == "..";
}

// `child` must stay inside `root` — used before a root-relative candidate is
// even handed to the filesystem.
bool insideRoot(const fs::path& root, const fs::path& child) {
  const std::string rootText = root.generic_string();
  const std::string childText = child.generic_string();
  if (rootText.empty()) return true;  // no root: nothing to stay inside
  if (childText.compare(0, rootText.size(), rootText) != 0) return false;
  return childText.size() == rootText.size() || childText[rootText.size()] == '/';
}

std::string joinRoots(const std::vector<std::string>& roots) {
  if (roots.empty()) return "(no asset root configured)";
  std::string out;
  for (const std::string& root : roots) {
    if (!out.empty()) out += ", ";
    out += root;
  }
  return out;
}

}  // namespace

void AssetManager::addRoot(const std::string& directory) {
  if (directory.empty()) return;
  if (std::find(roots_.begin(), roots_.end(), directory) != roots_.end()) return;
  roots_.push_back(directory);
}

void AssetManager::setRoots(const std::vector<std::string>& directories) {
  roots_.clear();
  for (const std::string& directory : directories) addRoot(directory);
}

void AssetManager::setProjectRoot(const std::string& directory) {
  setRoots(directory.empty() ? std::vector<std::string>{} : std::vector<std::string>{directory});
}

std::string AssetManager::resolve(const std::string& file) const {
  if (file.empty()) return std::string();
  std::error_code error;

  const fs::path input(file);
  if (input.is_absolute()) {
    // Worlds saved on a phone or in an embedded build carry absolute paths;
    // they are used as stored, and reported as missing when they are gone.
    return isRegularFile(input) ? input.generic_string() : std::string();
  }

  const fs::path normalized = input.lexically_normal();
  // Windows and the web page both hand over "a\\b" occasionally; treat the
  // two separators the same so a path saved on one platform opens on another.
  std::string normalizedText = normalized.generic_string();
  if (normalizedText.empty() || climbsOut(normalized)) return std::string();

  // 1. The legacy spelling: relative to the working directory.
  if (isRegularFile(normalized)) return normalizedText;

  // 2. Each configured root, in order.
  for (const std::string& root : roots_) {
    if (root.empty()) continue;
    const fs::path rootPath = fs::path(root).lexically_normal();
    const fs::path candidate = (rootPath / normalized).lexically_normal();
    if (!insideRoot(rootPath, candidate)) continue;
    if (isRegularFile(candidate)) {
      error.clear();
      return candidate.generic_string();
    }
    // "assets/foo.obj" against the root ".../assets" must not become
    // "assets/assets/foo.obj": that spelling was in older Workbench pages.
    const fs::path rootName = rootPath.filename();
    if (!rootName.empty()) {
      auto first = normalized.begin();
      if (first != normalized.end() && first->string() == rootName.string() &&
          rootPath.has_parent_path()) {
        const fs::path beside = (rootPath.parent_path() / normalized).lexically_normal();
        if (isRegularFile(beside)) return beside.generic_string();
      }
    }
  }
  return std::string();
}

std::string AssetManager::resolveFor(const std::string& file, bool& missing) {
  const std::string path = resolve(file);
  missing = path.empty();
  if (missing) {
    lastError_ = "asset not found: '" + file + "' (searched: " + joinRoots(roots_) +
                 " and the working directory)";
    return std::string();
  }
  return path;
}

const MeshData* AssetManager::mesh(const std::string& file) {
  if (file.empty()) {
    lastError_ = "empty asset path";
    return nullptr;
  }
  ++stats_.requests;
  Entry<std::optional<MeshData>>& entry = meshes_[file];
  if (entry.tried) {
    ++stats_.hits;
    if (!entry.error.empty()) {
      lastError_ = entry.error;
      return nullptr;
    }
    lastError_.clear();
    return entry.value.has_value() ? &entry.value.value() : nullptr;
  }
  entry.tried = true;

  bool missing = false;
  const std::string path = resolveFor(file, missing);
  if (!missing) {
    std::string error;
    std::optional<assets::MeshLoadResult> loaded = assets::loadMesh(path, error);
    if (loaded.has_value()) {
      entry.value = std::move(loaded->mesh);
      ++stats_.loads;
      lastError_.clear();
      return &entry.value.value();
    }
    entry.error = "cannot read '" + file + "': " + (error.empty() ? std::string("unknown error") : error);
  } else {
    entry.error = lastError_;
  }
  ++stats_.failures;
  lastError_ = entry.error;
  return nullptr;
}

const assets::MeshAsset* AssetManager::meshAsset(const std::string& file) {
  if (file.empty()) {
    lastError_ = "empty asset path";
    return nullptr;
  }
  ++stats_.requests;
  Entry<std::optional<assets::MeshAsset>>& entry = meshAssets_[file];
  if (entry.tried) {
    ++stats_.hits;
    if (!entry.error.empty()) {
      lastError_ = entry.error;
      return nullptr;
    }
    lastError_.clear();
    return entry.value.has_value() ? &entry.value.value() : nullptr;
  }
  entry.tried = true;

  bool missing = false;
  const std::string path = resolveFor(file, missing);
  if (!missing) {
    std::string error;
    std::optional<assets::MeshAsset> loaded = assets::loadMeshAsset(path, error);
    if (loaded.has_value()) {
      entry.value = std::move(*loaded);
      ++stats_.loads;
      lastError_.clear();
      return &entry.value.value();
    }
    entry.error = "cannot read '" + file + "': " + (error.empty() ? std::string("unknown error") : error);
  } else {
    entry.error = lastError_;
  }
  ++stats_.failures;
  lastError_ = entry.error;
  return nullptr;
}

const assets::SkinnedAsset* AssetManager::skinned(const std::string& file) {
  if (file.empty()) {
    lastError_ = "empty asset path";
    return nullptr;
  }
  ++stats_.requests;
  Entry<std::optional<assets::SkinnedAsset>>& entry = skinned_[file];
  if (entry.tried) {
    ++stats_.hits;
    if (!entry.error.empty()) {
      lastError_ = entry.error;
      return nullptr;
    }
    lastError_.clear();
    return entry.value.has_value() ? &entry.value.value() : nullptr;
  }
  entry.tried = true;

  bool missing = false;
  const std::string path = resolveFor(file, missing);
  if (!missing) {
    std::string error;
    std::optional<assets::SkinnedAsset> loaded = assets::loadFBXSkinned(path, error);
    if (loaded.has_value()) {
      entry.value = std::move(*loaded);
      ++stats_.loads;
      lastError_.clear();
      return &entry.value.value();
    }
    entry.error = "cannot read '" + file + "': " + (error.empty() ? std::string("unknown error") : error);
  } else {
    entry.error = lastError_;
  }
  ++stats_.failures;
  lastError_ = entry.error;
  return nullptr;
}

AssetManager::Texture AssetManager::texture(const std::string& file) {
  if (file.empty()) {
    lastError_ = "empty asset path";
    return Texture{};
  }
  ++stats_.requests;
  ImageEntry& entry = images_[file];
  if (entry.tried) {
    ++stats_.hits;
    if (!entry.error.empty()) {
      lastError_ = entry.error;
      return Texture{};
    }
    lastError_.clear();
    return Texture{&entry.image, entry.revision};
  }
  entry.tried = true;

  bool missing = false;
  const std::string path = resolveFor(file, missing);
  if (!missing) {
    std::string error;
    std::optional<Image> loaded = assets::loadImage(path, error);
    if (loaded.has_value() && !loaded->isEmpty()) {
      entry.image = std::move(*loaded);
      entry.revision = nextRevision_++;
      ++stats_.loads;
      lastError_.clear();
      return Texture{&entry.image, entry.revision};
    }
    entry.error = "cannot read '" + file + "': " +
                  (error.empty() ? std::string("not a usable image") : error);
  } else {
    entry.error = lastError_;
  }
  ++stats_.failures;
  lastError_ = entry.error;
  return Texture{};
}

u64 AssetManager::textureRevision(const std::string& file) const {
  const auto found = images_.find(file);
  if (found == images_.end() || !found->second.error.empty()) return 0U;
  return found->second.revision;
}

bool AssetManager::invalidate(const std::string& file) {
  bool removed = meshes_.erase(file) > 0U;
  removed = meshAssets_.erase(file) > 0U || removed;
  removed = skinned_.erase(file) > 0U || removed;
  removed = images_.erase(file) > 0U || removed;
  return removed;
}

void AssetManager::clear() {
  meshes_.clear();
  meshAssets_.clear();
  skinned_.clear();
  images_.clear();
  lastError_.clear();
}

std::vector<std::string> AssetManager::missingAssets() const {
  std::vector<std::string> missing;
  for (const auto& entry : meshes_) {
    if (!entry.second.error.empty()) missing.push_back(entry.first);
  }
  for (const auto& entry : meshAssets_) {
    if (!entry.second.error.empty()) missing.push_back(entry.first);
  }
  for (const auto& entry : skinned_) {
    if (!entry.second.error.empty()) missing.push_back(entry.first);
  }
  for (const auto& entry : images_) {
    if (!entry.second.error.empty()) missing.push_back(entry.first);
  }
  std::sort(missing.begin(), missing.end());
  missing.erase(std::unique(missing.begin(), missing.end()), missing.end());
  return missing;
}

}  // namespace kimia
