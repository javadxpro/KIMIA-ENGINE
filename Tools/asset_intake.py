#!/usr/bin/env python3
"""KIMIA asset intake: what did we just get, and what can the engine actually load?

Run it on any folder that is about to become assets:

    python3 Tools/asset_intake.py assets/
    python3 Tools/asset_intake.py /path/to/football-pack --json pack-manifest.json
    python3 Tools/asset_intake.py assets/ --budget     # against the Assets.md table

It answers three questions with facts instead of hope:

1. Which files can KIMIA load at all?  The loaders accept mesh/skeleton/clip
   .obj/.fbx (ufbx), textures .png/.jpg (stb_image) and audio .wav/.mp3/.ogg
   (dr_wav/dr_mp3/stb_vorbis).  Anything else -- .blend, .psd, .unity, .cs --
   is inventory, not capability, and is listed as such so nobody counts it.
2. How big is it, per file and per folder?  Byte size always; PNG/JPEG pixel
   dimensions are read straight from the headers, no dependencies.
3. Is it inside the phone/Termux budgets from Documentation/Assets.md?  With
   --budget every file that exceeds a table row is flagged, so a 63 MB ball or
   an 8K tile says so out loud before it reaches a build.

Deterministic output (sorted paths), no third-party imports, exit code 0 unless
a file could not be read.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys

# --- what the loaders actually accept (Engine/Assets) ------------------------

KIND_BY_EXT = {
    ".obj": "mesh",
    ".fbx": "mesh_or_skinned",
    ".png": "texture",
    ".jpg": "texture",
    ".jpeg": "texture",
    ".wav": "audio",
    ".mp3": "audio",
    ".ogg": "audio",
    ".kimia": "scene",
    ".json": "logic_or_scene_data",
}

# Extension -> what it is, for things the engine cannot load. Kept short on
# purpose: an honest label beats a long apology.
FOREIGN_EXT = {
    ".blend": "Blender source",
    ".psd": "Photoshop source",
    ".ma": "Maya source",
    ".mb": "Maya binary source",
    ".unity": "Unity scene/prefab",
    ".cs": "Unity C# script (never enters the C++ build)",
    ".uasset": "Unreal asset",
    ".zip": "archive",
    ".rar": "archive",
    ".7z": "archive",
    ".tga": "texture (loader does not read TGA)",
    ".tif": "texture (loader does not read TIFF)",
    ".tiff": "texture (loader does not read TIFF)",
    ".exr": "texture (loader does not read EXR)",
    ".dds": "texture (loader does not read DDS)",
    ".webp": "texture (loader does not read WebP)",
    ".flac": "audio (loader does not read FLAC)",
    ".aiff": "audio (loader does not read AIFF)",
    ".mp4": "video (no video decoder)",
    ".mov": "video (no video decoder)",
    ".txt": "notes",
    ".md": "notes",
    ".pdf": "document",
}

# Documentation/Assets.md budget table, in machine-checkable form.
BUDGETS = {
    "texture_px": 2048,        # per side, desktop; phone target is 1024
    "audio_bytes": 3 * 1024 * 1024,
    "mesh_bytes": 8 * 1024 * 1024,   # a triangle count needs a parser; bytes is the honest proxy
}


def _png_size(head: bytes):
    if len(head) >= 24 and head[:8] == b"\x89PNG\r\n\x1a\n" and head[12:16] == b"IHDR":
        w, h = struct.unpack(">II", head[16:24])
        return int(w), int(h)
    return None


def _jpeg_size(head: bytes):
    if len(head) < 4 or head[:2] != b"\xff\xd8":
        return None
    i = 2
    while i + 9 < len(head):
        if head[i] != 0xFF:
            i += 1
            continue
        marker = head[i + 1]
        if marker in (0xD8, 0xD9) or 0xD0 <= marker <= 0xD7:
            i += 2
            continue
        seg_len = struct.unpack(">H", head[i + 2:i + 4])[0]
        if 0xC0 <= marker <= 0xCF and marker not in (0xC4, 0xC8, 0xCC):
            h, w = struct.unpack(">HH", head[i + 5:i + 9])
            return int(w), int(h)
        i += 2 + seg_len
    return None


def image_size(path: str, ext: str):
    try:
        with open(path, "rb") as fh:
            head = fh.read(65536)
    except OSError:
        return None
    if ext == ".png":
        return _png_size(head)
    if ext in (".jpg", ".jpeg"):
        return _jpeg_size(head)
    return None


def sha256(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def classify(name: str):
    ext = os.path.splitext(name)[1].lower()
    if ext in KIND_BY_EXT:
        return KIND_BY_EXT[ext], ext, True
    if ext in FOREIGN_EXT:
        return FOREIGN_EXT[ext], ext, False
    return ("unknown extension", ext, False)


def intake(root: str, hash_files: bool, budget: bool):
    files = []
    errors = []
    for base, dirs, names in os.walk(root):
        dirs.sort()
        for name in sorted(names):
            path = os.path.join(base, name)
            rel = os.path.relpath(path, root)
            kind, ext, loadable = classify(name)
            try:
                size = os.path.getsize(path)
            except OSError as exc:
                errors.append(f"{rel}: {exc}")
                continue
            entry = {
                "path": rel.replace(os.sep, "/"),
                "bytes": size,
                "kind": kind,
                "ext": ext,
                "loadable": loadable,
            }
            if loadable and ext in (".png", ".jpg", ".jpeg"):
                dims = image_size(path, ext)
                if dims:
                    entry["px"] = [dims[0], dims[1]]
            if budget:
                over = []
                if entry.get("px") and max(entry["px"]) > BUDGETS["texture_px"]:
                    over.append(f"texture {entry['px'][0]}x{entry['px'][1]} > {BUDGETS['texture_px']}px")
                if kind == "audio" and size > BUDGETS["audio_bytes"]:
                    over.append(f"audio {size / 1048576:.1f} MB > 3 MB")
                if kind in ("mesh", "mesh_or_skinned") and size > BUDGETS["mesh_bytes"]:
                    over.append(f"mesh {size / 1048576:.1f} MB > 8 MB")
                entry["over_budget"] = over
            if hash_files:
                try:
                    entry["sha256"] = sha256(path)
                except OSError as exc:
                    errors.append(f"{rel}: {exc}")
            files.append(entry)
    return files, errors


def summarize(root: str, files):
    total = sum(f["bytes"] for f in files)
    by_kind, loadable_bytes, foreign_bytes = {}, 0, 0
    for f in files:
        by_kind[f["kind"]] = by_kind.get(f["kind"], 0) + f["bytes"]
        if f["loadable"]:
            loadable_bytes += f["bytes"]
        else:
            foreign_bytes += f["bytes"]
    over = [f for f in files if f.get("over_budget")]
    return {
        "root": root,
        "file_count": len(files),
        "total_bytes": total,
        "total_mb": round(total / 1048576, 2),
        "loadable_bytes": loadable_bytes,
        "foreign_bytes": foreign_bytes,
        "bytes_by_kind": {k: by_kind[k] for k in sorted(by_kind)},
        "over_budget_count": len(over),
        "over_budget_files": [f["path"] for f in over],
    }


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="KIMIA asset intake report")
    ap.add_argument("root", help="folder to scan (e.g. assets/ or the incoming pack)")
    ap.add_argument("--json", metavar="OUT", help="also write the manifest (files + summary) as JSON")
    ap.add_argument("--hash", action="store_true", help="include sha256 for every file (slower)")
    ap.add_argument("--budget", action="store_true", help="flag files above the Assets.md budgets")
    args = ap.parse_args(argv)

    if not os.path.isdir(args.root):
        print(f"asset_intake: not a directory: {args.root}", file=sys.stderr)
        return 2

    files, errors = intake(args.root, args.hash, args.budget)
    summary = summarize(args.root, files)

    print(f"KIMIA asset intake -- {summary['root']}")
    print(f"  {summary['file_count']} files, {summary['total_mb']} MB")
    print(f"  loadable by the engine : {summary['loadable_bytes'] / 1048576:.2f} MB")
    print(f"  inventory only (not loadable): {summary['foreign_bytes'] / 1048576:.2f} MB")
    print("  by kind:")
    for kind, nbytes in summary["bytes_by_kind"].items():
        print(f"    {kind:<22} {nbytes / 1048576:>9.2f} MB")
    if args.budget:
        if summary["over_budget_files"]:
            print(f"  over budget ({summary['over_budget_count']}):")
            for f in files:
                if f.get("over_budget"):
                    print(f"    {f['path']}: {'; '.join(f['over_budget'])}")
        else:
            print("  over budget: none")
    if errors:
        print("  unreadable:", file=sys.stderr)
        for e in errors:
            print(f"    {e}", file=sys.stderr)

    if args.json:
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump({"summary": summary, "files": files}, fh, indent=1, sort_keys=True)
            fh.write("\n")
        print(f"  manifest written: {args.json}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
