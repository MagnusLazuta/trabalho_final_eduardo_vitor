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
import math
import argparse
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent
SCENES_DIR = PROJECT_ROOT / "assets" / "scenes"
SHARED_DIR = SCENES_DIR / "shared"

# Object types to find (uppercase, no VINES)
FIND_TYPES = {"DOOR", "TORCH", "CHEST", "LADDER", "WATER", "COBWEB_FLOORHOLE"}

# Mapping from OBJ name to collision_type
COLLISION_MAP = {
    "DOOR": "DOOR",
    "TORCH": "NONE",
    "CHEST": "SOLID",
    "LADDER": "LADDER",
    "WATER": "WATER",
    "COBWEB_FLOORHOLE": "COBWEB_FLOORHOLE",
}

# Which types are interactive
INTERACTIVE_TYPES = {"DOOR", "CHEST", "LADDER"}

# Mapping from type to shared model file
MODEL_MAP = {
    "DOOR": "door.obj",
    "LADDER": "ladder.obj",
    "COBWEB_FLOORHOLE": "COBWEB_FLOORHOLE.obj",
    "CHEST": "chest.obj",
}

# Sub-parts that share rotation/position with their parent
# e.g. CHEST_LID uses the same transform as CHEST_BODY
SHARED_PARTS = {
    "CHEST_LID": "CHEST_BODY",
}


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


def parse_verts_from_lines(lines):
    """Extract vertex positions from OBJ lines."""
    verts = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('v '):
            parts = stripped.split()
            if len(parts) >= 4:
                verts.append([float(parts[1]), float(parts[2]), float(parts[3])])
    return verts


def _kabsch_rigid(src, tgt):
    """Compute rotation matrix R and max vertex error for given correspondence.

    src and tgt must be same-length lists of [x,y,z] already paired by index.
    Both sets are centered internally. Returns (r3x3, max_error).
    """
    n = len(src)
    sc = [sum(s[k] for s in src) / n for k in range(3)]
    tc = [sum(t[k] for t in tgt) / n for k in range(3)]
    src_c = [[src[i][k] - sc[k] for k in range(3)] for i in range(n)]
    tgt_c = [[tgt[i][k] - tc[k] for k in range(3)] for i in range(n)]

    def _normalize(v):
        l = math.sqrt(v[0]**2 + v[1]**2 + v[2]**2)
        return [v[k] / l for k in range(3)] if l > 1e-10 else [0, 0, 0]

    def _mat_vec(m, v):
        return [m[i][0]*v[0] + m[i][1]*v[1] + m[i][2]*v[2] for i in range(3)]

    def _dot(a, b):
        return sum(a[k] * b[k] for k in range(3))

    h = [[0]*3 for _ in range(3)]
    for i in range(n):
        for r in range(3):
            for c in range(3):
                h[r][c] += src_c[i][r] * tgt_c[i][c]

    th = [[0]*3 for _ in range(3)]
    for i in range(3):
        for j in range(3):
            for k in range(3):
                th[i][j] += h[k][i] * h[k][j]

    vs = []
    for eig_idx in range(3):
        e = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]][eig_idx]
        for prev in vs:
            d = _dot(e, prev)
            e = [e[k] - d * prev[k] for k in range(3)]
        e = _normalize(e)
        for _ in range(300):
            ne = _mat_vec(th, e)
            for prev in vs:
                d = _dot(ne, prev)
                ne = [ne[k] - d * prev[k] for k in range(3)]
            ne = _normalize(ne)
            if abs(_dot(ne, e)) > 0.999999:
                break
            e = ne
        vs.append(e)

    U_cols = []
    for i in range(3):
        col = _mat_vec(h, vs[i])
        sv = math.sqrt(sum(col[k]**2 for k in range(3)))
        if sv > 1e-10:
            col = [col[k] / sv for k in range(3)]
        U_cols.append(col)

    r = [[0]*3 for _ in range(3)]
    for i in range(3):
        for j in range(3):
            r[i][j] = sum(vs[k][i] * U_cols[k][j] for k in range(3))

    det = (r[0][0]*(r[1][1]*r[2][2] - r[1][2]*r[2][1])
           - r[0][1]*(r[1][0]*r[2][2] - r[1][2]*r[2][0])
           + r[0][2]*(r[1][0]*r[2][1] - r[1][1]*r[2][0]))
    if det < 0:
        for i in range(3):
            r[2][i] = -r[2][i]

    max_err = 0.0
    for i in range(n):
        transformed = [sum(r[j][k] * src_c[i][k] for k in range(3)) for j in range(3)]
        err = math.sqrt(sum((transformed[k] - tgt_c[i][k])**2 for k in range(3)))
        if err > max_err:
            max_err = err
    return r, max_err


def _matrix_to_euler(r):
    """Extract Euler angles [rx, ry, rz] (degrees) from rotation matrix.

    Convention: R = Ry * Rx * Rz  (matches C++ model_matrix = T * Ry * Rx * Rz * S).
    """
    ry = math.atan2(-r[2][0], math.sqrt(r[0][0]**2 + r[1][0]**2))
    cos_ry = math.cos(ry)
    if abs(cos_ry) > 1e-10:
        rx = math.atan2(r[2][1] / cos_ry, r[2][2] / cos_ry)
        rz = math.atan2(r[1][0] / cos_ry, r[0][0] / cos_ry)
    else:
        rx = math.atan2(-r[0][1], r[1][1])
        rz = 0.0

    rx_deg = round(math.degrees(rx), 2)
    ry_deg = round(math.degrees(ry), 2)
    rz_deg = round(math.degrees(rz), 2)
    if abs(rx_deg) < 0.01: rx_deg = 0.0
    if abs(ry_deg) < 0.01: ry_deg = 0.0
    if abs(rz_deg) < 0.01: rz_deg = 0.0
    return [rx_deg, ry_deg, rz_deg]


def _topology_match(shared_verts, map_verts):
    """Match vertices by Y-group (bottom/top) then nearest-neighbor on relative XZ.

    Works when vertex ordering in map.obj differs from the shared model.
    """
    n = len(shared_verts)
    sc = [sum(s[k] for s in shared_verts) / n for k in range(3)]
    mc = [sum(m[k] for m in map_verts) / n for k in range(3)]

    s_bottom = [i for i in range(n) if shared_verts[i][1] < sc[1]]
    s_top = [i for i in range(n) if shared_verts[i][1] >= sc[1]]
    m_bottom = [i for i in range(n) if map_verts[i][1] < mc[1]]
    m_top = [i for i in range(n) if map_verts[i][1] >= mc[1]]

    pairs = {}

    def _match_group(s_idxs, m_idxs, s_cx, s_cz, m_cx, m_cz):
        if len(s_idxs) > len(m_idxs):
            return False
        used = set()
        for si in s_idxs:
            srel = (shared_verts[si][0] - s_cx, shared_verts[si][2] - s_cz)
            remaining = [j for j in m_idxs if j not in used]
            if not remaining:
                return False
            best = min(remaining, key=lambda j: (map_verts[j][0] - m_cx - srel[0])**2
                                              + (map_verts[j][2] - m_cz - srel[1])**2)
            used.add(best)
            pairs[si] = best
        return True

    # Try group-based matching if group sizes are compatible
    groups_ok = True
    if s_bottom and m_bottom and len(s_bottom) <= len(m_bottom):
        s_bcx = sum(shared_verts[i][0] for i in s_bottom) / len(s_bottom)
        s_bcz = sum(shared_verts[i][2] for i in s_bottom) / len(s_bottom)
        m_bcx = sum(map_verts[i][0] for i in m_bottom) / len(m_bottom)
        m_bcz = sum(map_verts[i][2] for i in m_bottom) / len(m_bottom)
        groups_ok &= _match_group(s_bottom, m_bottom, s_bcx, s_bcz, m_bcx, m_bcz)
    else:
        groups_ok = False

    if s_top and m_top and len(s_top) <= len(m_top) and groups_ok:
        s_tcx = sum(shared_verts[i][0] for i in s_top) / len(s_top)
        s_tcz = sum(shared_verts[i][2] for i in s_top) / len(s_top)
        m_tcx = sum(map_verts[i][0] for i in m_top) / len(m_top)
        m_tcz = sum(map_verts[i][2] for i in m_top) / len(m_top)
        groups_ok &= _match_group(s_top, m_top, s_tcx, s_tcz, m_tcx, m_tcz)
    else:
        groups_ok = False

    # If group matching failed, fall back to global nearest-neighbor
    if not groups_ok or len(pairs) != n:
        pairs = {}
        used = set()
        for i in range(n):
            best = min(range(n), key=lambda j: sum((shared_verts[i][k] - map_verts[j][k])**2 for k in range(3)) if j not in used else float('inf'))
            used.add(best)
            pairs[i] = best

    return pairs


def compute_rotation_from_shared(obj_lines, shared_lines):
    """Compute rotation by comparing map.obj vertices with shared model vertices.

    Tries identity matching and topology-based matching, picks the one with
    lowest Kabsch error.  Both vertex sets are centered before alignment.
    """
    map_verts = parse_verts_from_lines(obj_lines)
    shared_verts = parse_verts_from_lines(shared_lines)

    if len(map_verts) < 3 or len(shared_verts) < 3:
        return [0, 0, 0]

    n = min(len(map_verts), len(shared_verts))
    sv = shared_verts[:n]
    mv = map_verts[:n]

    candidates = []

    # Candidate 1: identity matching (i -> i)
    identity = {i: i for i in range(n)}
    reordered = [mv[identity[i]] for i in range(n)]
    r_id, err_id = _kabsch_rigid(sv, reordered)
    candidates.append((err_id, r_id))

    # Candidate 2: topology-based matching (bottom/top + relative XZ)
    topo = _topology_match(sv, mv)
    if len(topo) == n:
        reordered_topo = [mv[topo[i]] for i in range(n)]
        r_topo, err_topo = _kabsch_rigid(sv, reordered_topo)
        candidates.append((err_topo, r_topo))

    # Candidate 3: all permutations within bottom and top groups (up to 24 each)
    # Only try if groups are small (<=4) to keep it tractable
    s_cy = sum(s[1] for s in sv) / n
    s_bottom = [i for i in range(n) if sv[i][1] < s_cy]
    s_top = [i for i in range(n) if i not in s_bottom]
    m_cy = sum(m[1] for m in mv) / n
    m_bottom = [i for i in range(n) if mv[i][1] < m_cy]
    m_top = [i for i in range(n) if i not in m_bottom]

    if len(s_bottom) <= 4 and len(s_top) <= 4 and len(s_bottom) == len(m_bottom):
        import itertools
        for perm_b in itertools.permutations(m_bottom):
            for perm_t in itertools.permutations(m_top):
                perm_map = {}
                for idx, mi in enumerate(s_bottom):
                    perm_map[mi] = perm_b[idx]
                for idx, mi in enumerate(s_top):
                    perm_map[mi] = perm_t[idx]
                reordered_perm = [mv[perm_map[i]] for i in range(n)]
                r_perm, err_perm = _kabsch_rigid(sv, reordered_perm)
                candidates.append((err_perm, r_perm))

    # Pick the candidate with lowest error
    best_err, best_r = min(candidates, key=lambda c: c[0])
    return _matrix_to_euler(best_r)


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
    body_rotations = {}  # obj_name -> rotation for parent objects (e.g. CHEST_BODY)

    for obj in objects:
        matched_type = None
        for t in FIND_TYPES:
            if t in obj["name"].upper():
                matched_type = t
                break
        if matched_type:
            center = compute_object_center(obj["lines"])
            aabb_min, aabb_max = compute_object_aabb(obj["lines"])

            # Check if this is a sub-part that shares transform with a parent
            shared_parent = None
            for part_key, parent_key in SHARED_PARTS.items():
                if part_key in obj["name"].upper() and parent_key in obj["name"].upper():
                    shared_parent = parent_key
                    break
            # Also check by suffix: CHEST_LID.001 -> CHEST_BODY
            if not shared_parent:
                for part_key, parent_key in SHARED_PARTS.items():
                    if part_key in obj["name"].upper():
                        shared_parent = parent_key
                        break

            if shared_parent:
                # Find the parent's rotation and position
                parent_rotation = [0, 0, 0]
                parent_position = center  # fallback
                for f in found:
                    if shared_parent in f.get("obj_name", "").upper():
                        parent_rotation = f["rotation"]
                        parent_position = f["position"]
                        break
                rotation = parent_rotation
                position = parent_position
                print(f"  Found: {obj['name']} (type: {matched_type}) - shares transform with {shared_parent}")
            else:
                # Calcula rotação comparando vértices do map.obj com o modelo compartilhado
                rotation = [0, 0, 0]
                shared_model = MODEL_MAP.get(matched_type, "")
                if shared_model:
                    shared_path = SHARED_DIR / shared_model
                    if shared_path.exists():
                        with open(shared_path, 'r') as sf:
                            shared_lines = sf.readlines()
                        rotation = compute_rotation_from_shared(obj["lines"], shared_lines)
                        print(f"    Rotation from {shared_model}: {rotation}")
                position = center

                # Store body rotation for sub-parts
                if "BODY" in obj["name"].upper():
                    body_rotations[obj["name"]] = rotation

            if center:
                if shared_parent:
                    # Sub-part with vertices (e.g. CHEST_LID) - skip, already in parent model
                    print(f"  Skipped: {obj['name']} - shares geometry with {shared_parent}")
                else:
                    found.append({
                        "name": matched_type,
                        "obj_name": obj["name"],
                        "position": position,
                        "rotation": rotation,
                        "aabb_min": aabb_min,
                        "aabb_max": aabb_max,
                    })
                    print(f"  Found: {obj['name']} (type: {matched_type}) at ({center[0]:.2f}, {center[1]:.2f}, {center[2]:.2f})")
            elif shared_parent:
                # Sub-part (e.g. CHEST_LID) - skip, already included in parent model
                print(f"  Skipped: {obj['name']} - shares geometry with {shared_parent}")

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
            "model": MODEL_MAP.get(f["name"], ""),
            "type": f["name"],
            "position": f["position"],
            "rotation": f["rotation"],
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
        remove_names = {f["obj_name"] for f in found}
        print(f"\nCleaning map.obj (removing: {', '.join(remove_names)})...")
        clean_obj(map_path, remove_names, dry_run=args.dry_run)


if __name__ == "__main__":
    main()
