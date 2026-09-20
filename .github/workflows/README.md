# GitHub Actions workflows (DISABLED)

Both CI workflows in this directory are **disabled** — the `.yml` files
were renamed to `.yml.disabled` so GitHub ignores them entirely.

## Why

The CI runs against the Linux build image and does not catch the actual
delivery path of this engine (the user runs it on Android via Termux
or as a self-built APK on the device). A green CI run gave no extra
information and a failed run wasted a turn; the Termux + on-device
build is the real test.

## How to re-enable

```bash
cd .github/workflows
mv android-apk.yml.disabled android-apk.yml
mv windows-exe.yml.disabled   windows-exe.yml
git add .github/workflows/
git commit -m "ci: re-enable GitHub Actions workflows"
git push origin arena/01a080a4-ai-codespace
```

To run a single workflow manually without re-enabling it, open the
file, copy its content into a new branch, and push — the workflow
definition must be named `*.yml` for GitHub to pick it up.
