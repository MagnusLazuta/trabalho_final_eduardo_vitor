#!/usr/bin/env python3
"""
Parse map.obj to find named objects (DOOR, TORCH, CHEST, LADDER, WATER, etc.)
and generate/update objects.json.

Usage:
    python3 scripts/parse_scene_objects.py --scene 00
    python3 scripts/parse_scene_objects.py --scene 00 --clean
    python3 scripts/parse_scene_objects.py --scene 00 --clean --dry-run
"""

import sys
import os
import json
import re
import argparse
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent
SCENES_DIR = PROJECT_ROOT / "assets" / "scenes"

# Object types to find (uppercase, no VINES)
FIND_TYPES = {"DOOR", "TORCH", "CHEST", "LADDER", "WATER"}

# Mapping from OBJ name to collision_type
COLLISION_MAP = {
    "DOOR": "DOOR",
    "TORCH": "NONE",
    "CHEST": "SOLID",
    "LADDER": "LADDER",
    "WATER": "WATER",
}

# Which types are interactive
INTERACTIVE_TYPES = {"DOOR", "CHEST"}


def parse_obj_by_objects(filepath):
    """Parse .obj file, grouping lines by object."""
    objects = []  # list of {"name": str, "lines": [str], "vert_range": (start, end), "face_lines": [str]}
    current_obj = None
    vert_count = 0
    all_lines = []

    with open(filepath, 'r') as f:
        for line in f:
            all_lines.append(line)
            stripped = line.strip()

            if stripped.startswith('o '):
                name = stripped[2:].strip()
                current_obj = {
                    "name": name,
                    "start_line": len(all_lines) - 1,
                    "lines": [],
                    "vert_start": vert_count + 1,
                    "vert_end": vert_count,
                }
                objects.append(current_obj)

            elif stripped.startswith('v '):
                vert_count += 1
                if current_obj is not None:
                    current_obj["vert_end"] = vert_count

            if current_obj is not None:
                current_obj["lines"].append(line)

    return objects, all_lines, vert_count


def compute_object_center(lines):
    """Compute center of vertices for an object's lines."""
    verts = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('v '):
            parts = stripped.split()
            if len(parts) >= 4:
                verts.append((float(parts[1]), float(parts[2]), float(parts[3])))

    if not verts:
        return None

    xs = [v[0] for v in verts]
    ys = [v[1] for v in verts]
    zs = [v[2] for v in verts]

    return [
        round((min(xs) + max(xs)) / 2, 6),
        round((min(ys) + max(ys)) / 2, 6),
        round((min(zs) + max(zs)) / 2, 6),
    ]


def compute_object_aabb(lines):
    """Compute AABB (min, max) for an object's vertices."""
    verts = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('v '):
            parts = stripped.split()
            if len(parts) >= 4:
                verts.append((float(parts[1]), float(parts[2]), float(parts[3])))

    if not verts:
        return None, None

    xs = [v[0] for v in verts]
    ys = [v[1] for v in verts]
    zs = [v[2] for v in verts]

    aabb_min = [round(min(xs), 6), round(min(ys), 6), round(min(zs), 6)]
    aabb_max = [round(max(xs), 6), round(max(ys), 6), round(max(zs), 6)]
    return aabb_min, aabb_max


def clean_obj(filepath, objects_to_remove, dry_run=False):
    """Remove objects from .obj file and remap vertex indices."""
    with open(filepath, 'r') as f:
        all_lines = f.readlines()

    # Parse structure: track which object each line belongs to
    parsed_objects = []
    current_obj = None
    vert_count = 0

    for i, line in enumerate(all_lines):
        stripped = line.strip()
        if stripped.startswith('o '):
            name = stripped[2:].strip()
            current_obj = {
                "name": name,
                "keep": name not in objects_to_remove,
                "start_idx": i,
                "vert_start": vert_count + 1,
                "lines": [],
            }
            parsed_objects.append(current_obj)
        elif current_obj is not None:
            if stripped.startswith('v '):
                vert_count += 1
            current_obj["lines"].append((i, line))

    # Collect removed vertex ranges
    removed_verts_before = {}  # line_index -> how many verts removed before this point
    removed_count = 0
    removed_ranges = []  # list of (start, end) 1-based inclusive

    for obj in parsed_objects:
        if not obj["keep"]:
            # Count vertices in this object
            obj_vert_count = 0
            for _, line in obj["lines"]:
                if line.strip().startswith('v '):
                    obj_vert_count += 1
            if obj_vert_count > 0:
                removed_ranges.append((obj["vert_start"], obj["vert_start"] + obj_vert_count - 1))

    # Build output
    output_lines = []
    current_obj = None

    for i, line in enumerate(all_lines):
        stripped = line.strip()

        if stripped.startswith('o '):
            name = stripped[2:].strip()
            current_obj = name
            if name in objects_to_remove:
                if not dry_run:
                    print(f"  Removing object: {name}")
                continue
            else:
                output_lines.append(line)
                continue

        # Skip lines of removed objects
        if current_obj is not None and current_obj in objects_to_remove:
            continue

        # Skip orphaned lines before any object
        if current_obj is None and not stripped.startswith(('v ', 'vt ', 'vn ', '#')):
            continue

        # Remap face indices
        if stripped.startswith('f '):
            parts = stripped.split()
            new_parts = [parts[0]]
            for p in parts[1:]:
                # Split by / to get vertex index
                indices = p.split('/')
                vi = int(indices[0])

                # Count how many removed vertices are before this index
                shift = 0
                for rs, re in removed_ranges:
                    if rs <= vi:
                        shift += re - rs + 1
                    else:
                        break  # ranges are sorted

                new_vi = vi - shift
                if new_vi < 1:
                    print(f"  WARNING: vertex index {vi} -> {new_vi} after removing")
                    new_vi = 1

                indices[0] = str(new_vi)
                new_parts.append('/'.join(indices))
            output_lines.append(' '.join(new_parts) + '\n')
        else:
            output_lines.append(line)

    if not dry_run:
        with open(filepath, 'w') as f:
            f.writelines(output_lines)
        print(f"  Cleaned {filepath}: removed {len(objects_to_remove)} objects")

    # Verify
    verts = [l for l in output_lines if l.strip().startswith('v ')]
    errors = 0
    for line in output_lines:
        if line.strip().startswith('f '):
            for p in line.strip().split()[1:]:
                vi = int(p.split('/')[0])
                if vi > len(verts) or vi < 1:
                    errors += 1
    print(f"  Verification: {len(verts)} vertices, {errors} errors")


def main():
    parser = argparse.ArgumentParser(description="Parse map.obj and generate objects.json")
    parser.add_argument("--scene", type=int, required=True, help="Scene number (0-11)")
    parser.add_argument("--clean", action="store_true", help="Remove found objects from map.obj")
    parser.add_argument("--dry-run", action="store_true", help="Don't write files, just show what would happen")
    args = parser.parse_args()

    scene_dir = SCENES_DIR / f"scene{args.scene:02d}"
    map_path = scene_dir / "map.obj"
    json_path = scene_dir / "objects.json"

    if not map_path.exists():
        print(f"ERROR: {map_path} not found")
        sys.exit(1)

    print(f"Parsing {map_path}...")

    # Parse .obj
    objects, all_lines, total_verts = parse_obj_by_objects(map_path)
    print(f"  Found {len(objects)} objects, {total_verts} total vertices")

    # Find matching objects
    found = []
    for obj in objects:
        if obj["name"] in FIND_TYPES:
            center = compute_object_center(obj["lines"])
            aabb_min, aabb_max = compute_object_aabb(obj["lines"])
            if center:
                found.append({
                    "name": obj["name"],
                    "position": center,
                    "aabb_min": aabb_min,
                    "aabb_max": aabb_max,
                })
                print(f"  Found: {obj['name']} at ({center[0]:.2f}, {center[1]:.2f}, {center[2]:.2f})")

    if not found:
        print("  No named objects found.")
        return

    # Load existing objects.json
    existing = []
    if json_path.exists():
        with open(json_path, 'r') as f:
            data = json.load(f)
            existing = data.get("objects", [])
        print(f"  Loaded existing objects.json: {len(existing)} objects")

    # Merge: update positions for existing types, add new ones
    found_types = {f["name"] for f in found}
    merged = []

    # Keep existing objects that are NOT in found (e.g., manually added)
    for obj in existing:
        obj_type = obj.get("type", "")
        if obj_type not in found_types:
            merged.append(obj)

    # Add/update from found
    for f in found:
        entry = {
            "model": "",
            "type": f["name"],
            "position": f["position"],
            "rotation": [0, 0, 0],
            "scale": [1, 1, 1],
            "interactive": f["name"] in INTERACTIVE_TYPES,
            "collision_type": COLLISION_MAP.get(f["name"], "SOLID"),
            "aabb": {
                "min": f["aabb_min"],
                "max": f["aabb_max"],
            },
        }

        # Check if type already existed, preserve its model path
        for obj in existing:
            if obj.get("type") == f["name"] and obj.get("model"):
                entry["model"] = obj["model"]
                break

        merged.append(entry)

    # Write objects.json
    output = {"objects": merged}
    if not args.dry_run:
        with open(json_path, 'w') as f:
            json.dump(output, f, indent=2)
        print(f"  Saved {json_path}: {len(merged)} objects")
    else:
        print(f"  [DRY RUN] Would save {json_path}: {len(merged)} objects")
        print(json.dumps(output, indent=2))

    # Clean .obj if requested
    if args.clean:
        remove_names = {f["name"] for f in found}
        print(f"\nCleaning map.obj (removing: {', '.join(remove_names)})...")
        clean_obj(map_path, remove_names, dry_run=args.dry_run)


if __name__ == "__main__":
    main()
