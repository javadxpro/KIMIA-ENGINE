# GitHub Actions workflows

Five jobs, no secrets (only the default `GITHUB_TOKEN` for checkout). All of
them build from a clean checkout — nothing is generated in the repository.

| File | Workflow | What it proves |
| --- | --- | --- |
| `ci.yml` → `linux-gcc` | Linux GCC | Release build with `-Werror` + the whole CTest suite |
| `ci.yml` → `sanitizers` | ASan+UBSan and TSan | the same suite finds no memory error, no undefined behaviour and no data race |
| `ci.yml` → `windows-msvc-smoke` | Windows MSVC | the engine and editor compile with `/W4` and the binary runs (`--version`) |
| `ci.yml` → `wasm-smoke` | Emscripten | the engine compiles to WebGL2/WebAssembly (advisory, see below) |
| `android-apk.yml` | Android | the native `kimia_jni` library and the debug APK build |
| `windows-exe.yml` | Windows release | the self-contained single-file `kimia_world.exe` with embedded assets |

## Triggers

Every workflow runs on `workflow_dispatch`, on pushes to `main`, `arena/**`
and `feature/**`, and on pull requests.

The Linux and sanitizer jobs issue the same `cmake` and `ctest` commands that
`Tools/run_tests.sh` issues locally, so a green CI run and a green local run
mean the same thing. See `Documentation/CI.md`.

## Running the same checks locally

```bash
bash Tools/run_tests.sh              # what linux-gcc does
bash Tools/run_tests.sh --sanitize   # what sanitizers (ASan+UBSan) does
bash Tools/run_tests.sh --tsan       # what sanitizers (TSan) does
```

## The one advisory job

`wasm-smoke` has `continue-on-error: true`: Emscripten is not available in the
development sandbox this engine was last built in, so the job has never been
seen green. It is kept visible and non-blocking on purpose — a claim of
WebAssembly support that nobody has built would be worse than an honest yellow
job. Once a green run is observed, delete that one line and it becomes a gate.

## Notes

- `windows-exe.yml` installs SDL2 through vcpkg **only for the packaged
  release**, which is the one build that uses the SDL window backend. The
  smoke job in `ci.yml` builds headless and needs no third-party package.
- The Android workflow downloads the SDK/NDK pinned in the file
  (platform 34, build-tools 34.0.0, NDK 26.3.11579264) and builds the APK with
  Gradle 8.7 against this same `CMakeLists.txt`.
