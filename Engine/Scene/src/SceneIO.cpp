#include <kimia/SceneIO.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

namespace kimia {

namespace {

// The versions this file format has had. What a build WRITES is decided per
// scene (see needsIds below); what it can READ is the whole list.
constexpr const char* kHeaderV1 = "# KIMIA scene v1";
constexpr const char* kHeaderV2 = "# KIMIA scene v2";
constexpr const char* kHeaderPrefix = "# KIMIA scene v";

const char* headerFor(int version) { return version >= 2 ? kHeaderV2 : kHeaderV1; }

// Reads the version out of a header line, or 0 when the line is not one.
// The text comes from the file, so anything at all can be in it.
int versionOfHeader(const std::string& line) {
  const std::string prefix = kHeaderPrefix;
  if (line.compare(0U, prefix.size(), prefix) != 0) return 0;
  const std::string digits = line.substr(prefix.size());
  if (digits.empty()) return 0;
  int version = 0;
  for (const char c : digits) {
    if (c < '0' || c > '9') return 0;
    version = version * 10 + (c - '0');
    if (version > 1000) return 0;  // a corrupt header, not a version
  }
  return version;
}

// True when the scene's handles are not 1..N, which is the only thing v2 adds.
// forEach visits in ascending handle order, so one pass answers it.
bool needsIds(const Scene& scene) {
  u32 expected = 1U;
  bool needed = false;
  scene.forEach([&expected, &needed](EntityHandle handle, const EntityData&) {
    if (handle != expected) needed = true;
    expected = handle + 1U;
  });
  return needed;
}

std::string trimmed(const std::string& line) {
  usize begin = 0;
  while (begin < line.size() && (line[begin] == ' ' || line[begin] == '\t' || line[begin] == '\r')) ++begin;
  usize end = line.size();
  while (end > begin && (line[end - 1U] == ' ' || line[end - 1U] == '\t' || line[end - 1U] == '\r')) --end;
  return line.substr(begin, end - begin);
}

// Splits a line into tokens. A double-quoted section becomes ONE token with
// quotes removed and \" / \\ unescaped, so names may contain spaces.
std::vector<std::string> tokenizeLine(const std::string& line) {
  std::vector<std::string> tokens;
  usize i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size()) break;
    if (line[i] == '"') {
      ++i;
      std::string token;
      bool closed = false;
      while (i < line.size()) {
        const char c = line[i];
        if (c == '\\' && i + 1U < line.size()) {
          token += line[i + 1U];
          i += 2U;
          continue;
        }
        if (c == '"') {
          ++i;
          closed = true;
          break;
        }
        token += c;
        ++i;
      }
      tokens.push_back(std::move(token));
      if (!closed) break;  // unterminated quote: rest of line is the token
      continue;
    }
    const usize start = i;
    while (i < line.size() && line[i] != ' ' && line[i] != '\t') ++i;
    tokens.push_back(line.substr(start, i - start));
  }
  return tokens;
}

const char* bodyKindName(BodyKind kind) {
  switch (kind) {
    case BodyKind::Static: return "static";
    case BodyKind::Dynamic: return "dynamic";
    case BodyKind::Sphere: return "sphere";
    case BodyKind::None: break;
  }
  return "none";
}

std::optional<BodyKind> bodyKindFromName(const std::string& name) {
  if (name == "none") return BodyKind::None;
  if (name == "static") return BodyKind::Static;
  if (name == "dynamic") return BodyKind::Dynamic;
  if (name == "sphere") return BodyKind::Sphere;
  return std::nullopt;
}

bool parseF64(const std::string& token, f64& out) {
  if (token.empty()) return false;
  try {
    usize consumed = 0;
    out = std::stod(token, &consumed);
    if (consumed != token.size()) return false;
  } catch (...) {
    return false;
  }
  // std::stod happily reads "nan" and "inf". A NaN position poisons every
  // physics step that touches it and shows up as an object that vanished, so
  // the loader refuses the number instead (the line is dropped with a
  // warning — see the entity loop).
  return std::isfinite(out);
}

// Reads a positive whole handle. 0 is not an entity, and neither is a
// negative number or a fractional one.
bool parseHandle(const std::string& token, EntityHandle& out) {
  if (token.empty()) return false;
  for (const char c : token) {
    if (c < '0' || c > '9') return false;
  }
  try {
    const unsigned long value = std::stoul(token);
    if (value == 0UL || value > static_cast<unsigned long>(std::numeric_limits<EntityHandle>::max())) {
      return false;
    }
    out = static_cast<EntityHandle>(value);
    return true;
  } catch (...) {
    return false;
  }
}

// Reads tokens[i+1 .. i+3] as a Vec3 and advances i past them.
bool parseVec3(const std::vector<std::string>& tokens, usize& i, Vec3& out) {
  if (i + 3U >= tokens.size()) return false;
  f64 x = 0.0, y = 0.0, z = 0.0;
  if (!parseF64(tokens[i + 1U], x) || !parseF64(tokens[i + 2U], y) || !parseF64(tokens[i + 3U], z)) return false;
  out = Vec3{x, y, z};
  i += 4U;
  return true;
}

// Deterministic shortest-ish formatting: guarantees byte-stable round-trips.
std::string format(f64 value) {
  std::ostringstream stream;
  stream << std::setprecision(9) << value;
  return stream.str();
}

std::string quoteName(const std::string& name) {
  std::string out;
  out.reserve(name.size() + 2U);
  out += '"';
  for (const char c : name) {
    if (c == '\\' || c == '"') out += '\\';
    out += c;
  }
  out += '"';
  return out;
}

const char* meshName(MeshKind kind) {
  switch (kind) {
    case MeshKind::cube:
      return "cube";
    case MeshKind::plane:
      return "plane";
    case MeshKind::sphere:
      return "sphere";
  }
  return "cube";
}

std::optional<MeshKind> meshKind(const std::string& token) {
  if (token == "cube") return MeshKind::cube;
  if (token == "plane") return MeshKind::plane;
  if (token == "sphere") return MeshKind::sphere;
  return std::nullopt;
}

}  // namespace

bool SceneIO::save(const Scene& scene, std::string& out) {
  // The oldest version that can express this scene: v1 unless the handles
  // need recording. Every scene saved before ids existed, and every scene
  // that has never had an object deleted, still writes exactly the same bytes
  // it always did.
  const bool withIds = needsIds(scene);
  std::ostringstream stream;
  stream << headerFor(withIds ? kVersion : 1) << '\n';
  scene.forEach([&stream, withIds](EntityHandle handle, const EntityData& entity) {
    const Transform& t = entity.transform;
    stream << "e " << quoteName(entity.name);
    if (withIds) stream << " id " << handle;
    stream << " mesh " << meshName(entity.mesh);
    if (!entity.meshFile.empty()) stream << " meshfile " << quoteName(entity.meshFile);
    if (!entity.texture.empty()) stream << " texture " << quoteName(entity.texture);
    stream << " pos " << format(t.position.x) << ' ' << format(t.position.y) << ' ' << format(t.position.z);
    stream << " scale " << format(t.scale.x) << ' ' << format(t.scale.y) << ' ' << format(t.scale.z);
    // Rotation (the Unity-style rotate tool). Written only when the entity
    // is actually turned, so every scene saved before this existed still
    // saves byte-identically.
    const Quat& r = t.rotation;
    if (r.x != 0.0 || r.y != 0.0 || r.z != 0.0 || r.w != 1.0) {
      stream << " rot " << format(r.x) << ' ' << format(r.y) << ' ' << format(r.z) << ' ' << format(r.w);
    }
    stream << " color " << format(entity.color.x) << ' ' << format(entity.color.y) << ' ' << format(entity.color.z);
    stream << " rough " << format(entity.roughness);
    if (entity.metallic != 0.0) stream << " metal " << format(entity.metallic);
    if (entity.emissive.x != 0.0 || entity.emissive.y != 0.0 || entity.emissive.z != 0.0) {
      stream << " emissive " << format(entity.emissive.x) << ' ' << format(entity.emissive.y) << ' '
             << format(entity.emissive.z);
    }
    if (entity.alpha != 1.0) stream << " alpha " << format(entity.alpha);
    // Components (stage 31). Each is optional, so a scene that uses none
    // of them saves byte-identically to before they existed.
    for (const std::string& tag : entity.tags) stream << " tag " << quoteName(tag);
    if (entity.body.has_value()) {
      const BodyComponent& b = *entity.body;
      stream << " body " << bodyKindName(b.kind) << ' ' << format(b.mass) << ' ' << format(b.friction) << ' '
             << format(b.restitution) << ' ' << format(b.radius);
    }
    for (const AnimationComponent& clip : entity.animations) {
      stream << " anim " << quoteName(clip.clip) << ' ' << quoteName(clip.trigger) << ' '
             << (clip.loop ? "loop" : "once") << ' ' << format(clip.speed);
    }
    // Rig bones (stage 35): name, parent, from, to, thickness, swing.
    for (const RigBone& bone : entity.rig) {
      stream << " bone " << quoteName(bone.name) << ' ' << quoteName(bone.parent.empty() ? "-" : bone.parent)
             << ' ' << format(bone.from.x) << ' ' << format(bone.from.y) << ' ' << format(bone.from.z) << ' '
             << format(bone.to.x) << ' ' << format(bone.to.y) << ' ' << format(bone.to.z) << ' '
             << format(bone.thickness) << ' ' << format(bone.swing);
    }
    for (const SoundComponent& sound : entity.sounds) {
      stream << " sound " << quoteName(sound.sound) << ' ' << quoteName(sound.trigger) << ' '
             << format(sound.volume);
    }
    // Dialogue (phase 3): one line per component, all four fields or none.
    for (const DialogueComponent& line : entity.dialogue) {
      stream << " say " << quoteName(line.line) << ' ' << quoteName(line.trigger) << ' '
             << format(line.volume) << ' ' << format(line.holdSeconds);
    }
    // Camera target (phase 3): what the camera should look at, if the world
    // says so. Written only when present, so no old file grows a line.
    if (entity.cameraTarget.has_value()) {
      const CameraTargetComponent& target = *entity.cameraTarget;
      stream << " camtarget " << format(target.weight) << ' ' << (target.whilePlaying ? "play" : "edit")
             << ' ' << format(target.offset.x) << ' ' << format(target.offset.y) << ' '
             << format(target.offset.z);
    }
    stream << '\n';
  });
  if (scene.demoShot.has_value()) {
    stream << "# demo " << format(scene.demoShot->aim) << ' ' << format(scene.demoShot->power) << '\n';
  }
  out = stream.str();
  return true;
}

bool SceneIO::saveToFile(const Scene& scene, const std::string& path) {
  std::string text;
  if (!save(scene, text)) return false;
  std::ofstream file(path, std::ios::binary);
  if (!file) return false;
  file << text;
  return static_cast<bool>(file);
}

bool SceneIO::load(const std::string& text, Scene& out, std::string& error) {
  LoadReport ignored;
  return load(text, out, error, ignored);
}

bool SceneIO::load(const std::string& text, Scene& out, std::string& error, LoadReport& report) {
  report = LoadReport{};
  Scene result;
  std::istringstream stream(text);
  std::string rawLine;
  usize lineNumber = 0U;
  std::vector<std::string> ignoredKeywords;
  while (std::getline(stream, rawLine)) {
    ++lineNumber;
    const std::string line = trimmed(rawLine);
    if (line.empty()) continue;
    if (line[0] == '#') {
      // The version line. A world file can carry more than one (WorldIO
      // writes its own header and embeds the scene text), so the highest
      // one seen wins: the file as a whole is as new as its newest part.
      const int declared = versionOfHeader(line);
      if (declared != 0) {
        report.version = std::max(report.version, declared);
        if (declared > kVersion) {
          error = "scene file version " + std::to_string(declared) +
                  " is newer than this engine understands (" + std::to_string(kVersion) + ")";
          return false;
        }
        continue;
      }
      // Comments are ignored except "# demo <aim> <power>".
      const std::string body = trimmed(line.substr(1));
      const std::vector<std::string> tokens = tokenizeLine(body);
      if (tokens.size() >= 3U && tokens[0] == "demo") {
        f64 aim = 0.0;
        f64 power = 0.0;
        if (parseF64(tokens[1], aim) && parseF64(tokens[2], power)) {
          result.demoShot = DemoShot{aim, power};
        }
      }
      continue;
    }
    const std::vector<std::string> tokens = tokenizeLine(line);
    if (tokens.empty() || tokens[0] != "e" || tokens.size() < 2U) continue;

    EntityData entity;
    entity.name = tokens[1];
    EntityHandle wantedId = kNullEntity;  // v2: the id the file asks for
    bool complete = true;
    std::string dropped;  // why the line was thrown away, for the report
    usize i = 2U;
    while (i < tokens.size() && complete) {
      const std::string& keyword = tokens[i];
      if (keyword == "id") {
        if (i + 1U >= tokens.size() || !parseHandle(tokens[i + 1U], wantedId)) {
          complete = false;
          dropped = "id is not a positive whole number";
          break;
        }
        i += 2U;
        continue;
      }
      if (keyword == "mesh" && i + 1U < tokens.size()) {
        const auto kind = meshKind(tokens[i + 1U]);
        if (!kind.has_value()) {
          complete = false;  // unknown mesh kind: ignore the entity line
          dropped = "unknown mesh kind '" + tokens[i + 1U] + "'";
          break;
        }
        entity.mesh = *kind;
        i += 2U;
        continue;
      }
      if (keyword == "meshfile" && i + 1U < tokens.size()) {
        entity.meshFile = tokens[i + 1U];
        i += 2U;
        continue;
      }
      if (keyword == "texture" && i + 1U < tokens.size()) {
        entity.texture = tokens[i + 1U];
        i += 2U;
        continue;
      }
      if (keyword == "tag" && i + 1U < tokens.size()) {
        entity.addTag(tokens[i + 1U]);
        i += 2U;
        continue;
      }
      if (keyword == "body") {
        // body <kind> <mass> <friction> <restitution> <radius> — all of it
        // or none, like every other multi-value line in the project.
        if (i + 5U >= tokens.size()) {
          complete = false;
          break;
        }
        const auto kind = bodyKindFromName(tokens[i + 1U]);
        BodyComponent component;
        f64 mass = 0.0;
        f64 friction = 0.0;
        f64 restitution = 0.0;
        f64 radius = 0.0;
        if (!kind.has_value() || !parseF64(tokens[i + 2U], mass) || !parseF64(tokens[i + 3U], friction) ||
            !parseF64(tokens[i + 4U], restitution) || !parseF64(tokens[i + 5U], radius)) {
          complete = false;
          break;
        }
        component.kind = *kind;
        component.mass = mass;
        component.friction = friction;
        component.restitution = restitution;
        component.radius = radius;
        entity.body = component;
        i += 6U;
        continue;
      }
      if (keyword == "anim") {
        if (i + 4U >= tokens.size()) {
          complete = false;
          break;
        }
        AnimationComponent clip;
        f64 speed = 1.0;
        if (!parseF64(tokens[i + 4U], speed)) {
          complete = false;
          break;
        }
        clip.clip = tokens[i + 1U];
        clip.trigger = tokens[i + 2U];
        clip.loop = tokens[i + 3U] == "loop";
        clip.speed = speed;
        entity.animations.push_back(clip);
        i += 5U;
        continue;
      }
      if (keyword == "bone") {
        // All ten fields or none, like every other multi-value line here.
        if (i + 10U >= tokens.size()) {
          complete = false;
          break;
        }
        RigBone bone;
        f64 fx = 0.0, fy = 0.0, fz = 0.0, tx = 0.0, ty = 0.0, tz = 0.0, thick = 0.0, swing = 0.0;
        if (!parseF64(tokens[i + 3U], fx) || !parseF64(tokens[i + 4U], fy) || !parseF64(tokens[i + 5U], fz) ||
            !parseF64(tokens[i + 6U], tx) || !parseF64(tokens[i + 7U], ty) || !parseF64(tokens[i + 8U], tz) ||
            !parseF64(tokens[i + 9U], thick) || !parseF64(tokens[i + 10U], swing)) {
          complete = false;
          break;
        }
        bone.name = tokens[i + 1U];
        bone.parent = tokens[i + 2U] == "-" ? std::string() : tokens[i + 2U];
        bone.from = Vec3{fx, fy, fz};
        bone.to = Vec3{tx, ty, tz};
        bone.thickness = thick;
        bone.swing = swing;
        entity.rig.push_back(bone);
        i += 11U;
        continue;
      }
      if (keyword == "sound") {
        if (i + 3U >= tokens.size()) {
          complete = false;
          break;
        }
        SoundComponent sound;
        f64 volume = 1.0;
        if (!parseF64(tokens[i + 3U], volume)) {
          complete = false;
          break;
        }
        sound.sound = tokens[i + 1U];
        sound.trigger = tokens[i + 2U];
        sound.volume = volume;
        entity.sounds.push_back(sound);
        i += 4U;
        continue;
      }
      if (keyword == "say") {
        // say <line> <trigger> <volume> <hold> — a dialogue line.
        if (i + 4U >= tokens.size()) {
          complete = false;
          break;
        }
        DialogueComponent spoken;
        f64 volume = 1.0;
        f64 hold = 3.0;
        if (!parseF64(tokens[i + 3U], volume) || !parseF64(tokens[i + 4U], hold)) {
          complete = false;
          break;
        }
        spoken.line = tokens[i + 1U];
        spoken.trigger = tokens[i + 2U];
        spoken.volume = volume;
        spoken.holdSeconds = hold;
        entity.dialogue.push_back(spoken);
        i += 5U;
        continue;
      }
      if (keyword == "camtarget") {
        // camtarget <weight> play|edit <offset x y z>
        if (i + 5U >= tokens.size()) {
          complete = false;
          break;
        }
        CameraTargetComponent target;
        f64 weight = 1.0;
        f64 ox = 0.0, oy = 0.0, oz = 0.0;
        if (!parseF64(tokens[i + 1U], weight) || !parseF64(tokens[i + 3U], ox) ||
            !parseF64(tokens[i + 4U], oy) || !parseF64(tokens[i + 5U], oz)) {
          complete = false;
          break;
        }
        target.weight = weight;
        target.whilePlaying = tokens[i + 2U] == "play";
        target.offset = Vec3{ox, oy, oz};
        entity.cameraTarget = target;
        i += 6U;
        continue;
      }
      if (keyword == "pos") {
        Vec3 value;
        if (!parseVec3(tokens, i, value)) {
          complete = false;
          break;
        }
        entity.transform.position = value;
        continue;
      }
      if (keyword == "scale") {
        Vec3 value;
        if (!parseVec3(tokens, i, value)) {
          complete = false;
          break;
        }
        entity.transform.scale = value;
        continue;
      }
      if (keyword == "rot") {
        if (i + 4U >= tokens.size()) {
          complete = false;
          break;
        }
        f64 x = 0.0, y = 0.0, z = 0.0, w = 1.0;
        if (!parseF64(tokens[i + 1U], x) || !parseF64(tokens[i + 2U], y) || !parseF64(tokens[i + 3U], z) ||
            !parseF64(tokens[i + 4U], w)) {
          complete = false;
          break;
        }
        entity.transform.rotation = Quat{x, y, z, w}.normalized();
        i += 5U;
        continue;
      }
      if (keyword == "color") {
        Vec3 value;
        if (!parseVec3(tokens, i, value)) {
          complete = false;
          break;
        }
        entity.color = value;
        continue;
      }
      if (keyword == "rough" && i + 1U < tokens.size()) {
        f64 value = 0.0;
        if (!parseF64(tokens[i + 1U], value)) {
          complete = false;
          break;
        }
        entity.roughness = value;
        i += 2U;
        continue;
      }
      if (keyword == "metal" && i + 1U < tokens.size()) {
        f64 value = 0.0;
        if (!parseF64(tokens[i + 1U], value)) {
          complete = false;
          break;
        }
        entity.metallic = value;
        i += 2U;
        continue;
      }
      if (keyword == "emissive") {
        Vec3 value;
        if (!parseVec3(tokens, i, value)) {
          complete = false;
          break;
        }
        entity.emissive = value;
        continue;
      }
      if (keyword == "alpha" && i + 1U < tokens.size()) {
        f64 value = 1.0;
        if (!parseF64(tokens[i + 1U], value)) {
          complete = false;
          break;
        }
        entity.alpha = value;
        i += 2U;
        continue;
      }
      // Unknown keyword: skip it (tolerant), but remember it. A file written
      // by a newer engine lands here, and "which words did I not understand"
      // is the first question anybody asks about a scene that came out
      // smaller than it went in.
      //
      // Numbers are not remembered: an unknown keyword is followed by its
      // own values ("glow 3" would otherwise report "3" as a keyword), and
      // the word is what a person can act on.
      f64 unused = 0.0;
      if (!parseF64(tokens[i], unused)) ignoredKeywords.push_back(tokens[i]);
      ++i;
    }
    if (!complete) {
      if (dropped.empty() && i < tokens.size()) dropped = "cannot read the value of '" + tokens[i] + "'";
      // Partial line: ignored, exactly as before — but now with a reason, so
      // a broken file can say which line and which entity it lost.
      report.warnings.push_back("line " + std::to_string(lineNumber) + ": entity \"" + entity.name +
                                "\" ignored (" + (dropped.empty() ? std::string("incomplete line") : dropped) +
                                ")");
      continue;
    }
    // The file's own id when there is one (v2): a scene whose handles are not
    // 1..N must come back the way it left, or every later save renumbers it
    // and any reference to an entity becomes a reference to another entity.
    if (wantedId != kNullEntity) {
      if (result.restore(wantedId, entity)) {
        ++report.restoredIds;
      } else {
        report.warnings.push_back("line " + std::to_string(lineNumber) + ": entity \"" + entity.name +
                                  "\" asked for id " + std::to_string(wantedId) +
                                  ", which is already taken; it was given the next free one");
        result.create(entity);
        ++report.assignedIds;
      }
      continue;
    }
    result.create(entity);
    ++report.assignedIds;
  }
  std::sort(ignoredKeywords.begin(), ignoredKeywords.end());
  ignoredKeywords.erase(std::unique(ignoredKeywords.begin(), ignoredKeywords.end()), ignoredKeywords.end());
  report.ignoredKeywords = std::move(ignoredKeywords);
  // The file was an older version and the result is the current model. There
  // is no rewrite step: v2 is a superset of v1, so this is what "migration"
  // means here.
  report.migrated = report.version < kVersion;
  out = std::move(result);
  return true;
}

bool SceneIO::loadFromFile(const std::string& path, Scene& out, std::string& error) {
  LoadReport ignored;
  return loadFromFile(path, out, error, ignored);
}

bool SceneIO::loadFromFile(const std::string& path, Scene& out, std::string& error, LoadReport& report) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error = "cannot open scene file: " + path;
    return false;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return load(buffer.str(), out, error, report);
}

}  // namespace kimia
