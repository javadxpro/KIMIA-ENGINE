#include <kimia_test.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// The APK itself cannot be executed in a Linux test run, but the contracts that
// decide WHETHER it can execute are plain text in the tree: Java asks the
// dynamic loader for a name, and CMake emits a file name; Java declares native
// methods, and the glue must export exactly those symbols. If either pair ever
// disagrees, the APK builds, installs, launches and dies with an
// UnsatisfiedLinkError — while every CI job that only compiles stays green.
// So both contracts are tested here, on every platform, on every commit.

namespace {

std::string readAll(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return std::string();
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::string sourceRoot() {
#ifdef KIMIA_SOURCE_DIR
  return KIMIA_SOURCE_DIR;
#else
  return ".";
#endif
}

// The soname the APK will contain: OUTPUT_NAME if the CMakeLists sets one for
// kimia_jni, otherwise the target name itself (CMake's default rule).
std::string emittedLibraryName(const std::string& cmakeLists) {
  const std::string marker = "set_target_properties(kimia_jni";
  const std::string::size_type at = cmakeLists.find(marker);
  if (at == std::string::npos) return "kimia_jni";
  const std::string::size_type end = cmakeLists.find(')', at);
  const std::string block =
      cmakeLists.substr(at, end == std::string::npos ? std::string::npos : end - at);
  const std::string::size_type keyAt = block.find("OUTPUT_NAME");
  if (keyAt == std::string::npos) return "kimia_jni";
  const std::string::size_type open = block.find('"', keyAt);
  const std::string::size_type close = open == std::string::npos ? std::string::npos : block.find('"', open + 1);
  if (open == std::string::npos || close == std::string::npos) return "kimia_jni";
  return block.substr(open + 1, close - open - 1);
}

std::string loadedLibraryName(const std::string& javaSource) {
  const std::string key = "System.loadLibrary(\"";
  const std::string::size_type at = javaSource.find(key);
  if (at == std::string::npos) return std::string();
  const std::string::size_type open = at + key.size();
  const std::string::size_type close = javaSource.find('"', open);
  if (close == std::string::npos) return std::string();
  return javaSource.substr(open, close - open);
}

// Every "public static native <type> <name>(" declaration, method names only.
std::vector<std::string> declaredNativeMethods(const std::string& javaSource) {
  std::vector<std::string> names;
  const std::string key = "public static native ";
  std::string::size_type at = 0;
  while ((at = javaSource.find(key, at)) != std::string::npos) {
    std::string::size_type p = at + key.size();
    while (p < javaSource.size() && javaSource[p] != ' ') ++p;  // return type
    while (p < javaSource.size() && javaSource[p] == ' ') ++p;
    std::string::size_type q = p;
    while (q < javaSource.size() && javaSource[q] != '(') ++q;
    if (q > p) names.push_back(javaSource.substr(p, q - p));
    at = q;
  }
  return names;
}

}  // namespace

KIMIA_TEST(android_loadlibrary_name_matches_the_built_library) {
  const std::string cmakeLists = readAll(sourceRoot() + "/CMakeLists.txt");
  const std::string java =
      readAll(sourceRoot() + "/Android/app/src/main/java/com/kimia/world/NativeEngine.java");
  KIMIA_REQUIRE(!cmakeLists.empty());
  KIMIA_REQUIRE(!java.empty());

  // The shared target itself has to exist, and Java has to load one library.
  KIMIA_REQUIRE(cmakeLists.find("add_library(kimia_jni") != std::string::npos);
  const std::string loaded = loadedLibraryName(java);
  KIMIA_REQUIRE(!loaded.empty());

  // THE contract: dlopen of "lib<loaded>.so" must find what the build emitted.
  const std::string emitted = emittedLibraryName(cmakeLists);
  if (loaded != emitted) {
    std::printf("java loads lib%s.so but cmake emits lib%s.so\n", loaded.c_str(), emitted.c_str());
  }
  KIMIA_REQUIRE(loaded == emitted);
}

KIMIA_TEST(android_every_declared_native_method_has_a_glue_symbol) {
  // JNI resolves native methods by symbol: Java_<package>_<class>_<method>,
  // dots as underscores. A method declared in Java but missing from the glue
  // is a crash on first touch, not a compile error — so pin all of them.
  const std::string java =
      readAll(sourceRoot() + "/Android/app/src/main/java/com/kimia/world/NativeEngine.java");
  const std::string glue = readAll(sourceRoot() + "/Android/app/src/main/cpp/jni_glue.cpp");
  KIMIA_REQUIRE(!java.empty());
  KIMIA_REQUIRE(!glue.empty());

  const std::vector<std::string> methods = declaredNativeMethods(java);
  KIMIA_REQUIRE(methods.size() >= 20U);  // the bridge is not quietly shrinking
  for (const std::string& method : methods) {
    const std::string symbol = "Java_com_kimia_world_NativeEngine_" + method;
    if (glue.find(symbol) == std::string::npos) {
      std::printf("java declares %s but jni_glue.cpp has no %s\n", method.c_str(), symbol.c_str());
    }
    KIMIA_REQUIRE(glue.find(symbol) != std::string::npos);
  }
}
