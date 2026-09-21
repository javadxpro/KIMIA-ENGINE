#include <kimia/Scene.h>

namespace kimia {

namespace {

// Keeps the handle counter above a handle that came from a file. Skipping null
// matters: kNullEntity is not an entity, so it must never be handed out.
void raiseNextHandle(u32& next, EntityHandle handle) {
  if (handle < next) return;
  next = handle + 1U;
  if (next == kNullEntity) ++next;
}

}  // namespace

Scene Scene::clone() const {
  Scene copy;
  // Handles are copied, not reassigned: an id is part of an entity's identity
  // once files can carry it (scene v2), and a stage switch must not renumber
  // anything that a world refers to.
  forEach([&copy](EntityHandle handle, const EntityData& entity) {
    static_cast<void>(copy.restore(handle, entity));
  });
  copy.demoShot = demoShot;
  return copy;
}

EntityHandle Scene::create(const std::string& name) {
  EntityData data;
  data.name = name;
  return create(data);
}

EntityHandle Scene::create(const EntityData& data) {
  const EntityHandle handle = nextHandle_;
  ++nextHandle_;
  if (nextHandle_ == kNullEntity) ++nextHandle_;  // never wrap onto null
  entities_.emplace(handle, data);
  indexName(data, handle);
  return handle;
}

bool Scene::restore(EntityHandle handle, const EntityData& data) {
  if (handle == kNullEntity) return false;
  if (!entities_.emplace(handle, data).second) return false;
  raiseNextHandle(nextHandle_, handle);
  indexName(data, handle);
  return true;
}

bool Scene::destroy(EntityHandle handle) {
  const auto found = entities_.find(handle);
  if (found == entities_.end()) return false;
  // Drop the index entry only when it really points at this entity: with
  // duplicate names it belongs to the lowest handle, which may be another.
  const auto named = names_.find(found->second.name);
  if (named != names_.end() && named->second == handle) names_.erase(named);
  entities_.erase(found);
  return true;
}

bool Scene::rename(EntityHandle handle, const std::string& name) {
  EntityData* entity = get(handle);
  if (entity == nullptr) return false;
  if (entity->name == name) return true;
  const auto named = names_.find(entity->name);
  if (named != names_.end() && named->second == handle) names_.erase(named);
  entity->name = name;
  indexName(*entity, handle);
  return true;
}

EntityData* Scene::get(EntityHandle handle) {
  const auto found = entities_.find(handle);
  return found == entities_.end() ? nullptr : &found->second;
}

const EntityData* Scene::get(EntityHandle handle) const {
  const auto found = entities_.find(handle);
  return found == entities_.end() ? nullptr : &found->second;
}

bool Scene::alive(EntityHandle handle) const { return entities_.find(handle) != entities_.end(); }

EntityHandle Scene::find(const std::string& name) const {
  const auto found = names_.find(name);
  if (found != names_.end()) {
    const auto entity = entities_.find(found->second);
    // The stored name must still match: entity->name is a public field and a
    // lot of code renames by writing it directly. An index that silently
    // disagrees with the scene is worse than no index.
    if (entity != entities_.end() && entity->second.name == name) return found->second;
  }
  // A miss, or an entry somebody invalidated behind our back. One rebuild
  // answers both, and a rebuild is a rare event next to a lookup.
  reindex();
  const auto again = names_.find(name);
  if (again == names_.end()) return kNullEntity;
  return alive(again->second) ? again->second : kNullEntity;
}

std::string Scene::uniqueName(const std::string& wanted) const {
  const std::string base = wanted.empty() ? std::string("Entity") : wanted;
  if (find(base) == kNullEntity) return base;
  for (u32 index = 2U;; ++index) {
    const std::string candidate = base + "_" + std::to_string(index);
    if (find(candidate) == kNullEntity) return candidate;
  }
}

void Scene::forEach(const std::function<void(EntityHandle, const EntityData&)>& callback) const {
  for (const auto& [handle, entity] : entities_) {
    callback(handle, entity);
  }
}

void Scene::clear() {
  entities_.clear();
  names_.clear();
  // nextHandle_ is deliberately NOT reset: clearing a scene must not make an
  // id from before reusable.
}

void Scene::indexName(const EntityData& entity, EntityHandle handle) const {
  // First one wins (the map iterates in ascending handle order, so a rebuild
  // lands on the same answer). Empty names are not indexed.
  if (entity.name.empty()) return;
  names_.emplace(entity.name, handle);
}

void Scene::reindex() const {
  ++nameIndexRebuilds_;
  names_.clear();
  for (const auto& [handle, entity] : entities_) indexName(entity, handle);
}

}  // namespace kimia
