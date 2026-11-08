"""Conservative Parafashion garment-mesh preflight / repair in Blender.

Run INSIDE Blender using an MCP Python execution tool; Blender's Python
must provide bpy, bmesh and mathutils. Default mode is AUDIT ONLY.

To override settings from an MCP code call, prefix this script with:
    PF_CONFIG = {"mode": "repair", "min_angle_deg": 10.0}

Then include the script contents in that same Blender-Python execution call.
Do not assume the MCP host and the Blender process share a file system.

This script does NOT guarantee absence of self-intersections, valid fabric
stress, or suitability of every seam layout. It is a topology/quality preflight.
"""

import json
import math
import statistics
import bpy
import bmesh
from mathutils import Vector

USER_CONFIG = globals().get("PF_CONFIG", {})
CONFIG = {
    "mode": "audit",                      # audit | repair
    "object_name": None,                 # None => active object
    "min_angle_deg": 10.0,               # engineering heuristic, NOT from the paper
    "tiny_area_rel": 1e-12,              # triangle area / world bbox diagonal^2
    "flip_min_gain_deg": 0.5,
    "flip_max_dihedral_deg": 10.0,
    "flip_max_normal_change_deg": 12.0,
    "max_flip_passes": 3,
    "max_flips": 1000,
    "preserve_existing_uv": True,        # blocks topology edits if UV data exists
    "remove_wire_geometry": True,
    "remove_duplicate_faces": True,
    "fix_inconsistent_winding": True,
    "remove_tiny_faces": False,          # changes fabric surface: explicit opt-in only
    "max_removed_face_fraction": 0.001,
    "allow_local_vertex_motion": False, # explicit opt-in; see SKILL.md
    "max_vertex_move_rel": 0.0005,       # <=0.05% of world bbox diagonal
    "max_move_passes": 2,
    "allow_edit_with_modifiers": False,
}
CONFIG.update(USER_CONFIG)


def angle_between(a, b):
    if a.length < 1e-30 or b.length < 1e-30:
        return math.pi
    return math.acos(max(-1.0, min(1.0, a.normalized().dot(b.normalized()))))


def world_pos(v, matrix):
    return matrix @ v.co


def triangle_data(pts):
    a, b, c = pts
    ab, ac, bc = b-a, c-a, c-b
    cross = ab.cross(ac)
    area = 0.5 * cross.length
    lens = sorted((ab.length, ac.length, bc.length))
    if lens[0] <= 1e-30 or area <= 1e-30:
        return {"min_angle": 0.0, "quality": 0.0, "area": area,
                "normal": Vector((0, 0, 0))}
    # Stable law-of-cosines approach for the smallest angle.
    p = (a, b, c)
    angles = []
    for i in range(3):
        u, v = p[(i+1) % 3] - p[i], p[(i+2) % 3] - p[i]
        angles.append(math.degrees(angle_between(u, v)))
    quality = 4.0 * math.sqrt(3.0) * area / sum(x*x for x in lens)
    return {"min_angle": min(angles), "quality": quality,
            "area": area, "normal": cross.normalized()}


def face_data(f, matrix):
    if len(f.verts) != 3:
        return None
    return triangle_data([world_pos(v, matrix) for v in f.verts])


def bbox_diagonal(bm, matrix):
    if not bm.verts:
        return 0.0
    pts = [world_pos(v, matrix) for v in bm.verts]
    mn = Vector(tuple(min(p[i] for p in pts) for i in range(3)))
    mx = Vector(tuple(max(p[i] for p in pts) for i in range(3)))
    return (mx - mn).length


def boundary_edge_signature(bm):
    # Compare the geometric endpoints, not mutable BMesh indices: deletion of
    # isolated elements can renumber vertices without changing the boundary.
    def key(v):
        return tuple(round(float(v.co[i]), 11) for i in range(3))
    return frozenset(tuple(sorted((key(e.verts[0]), key(e.verts[1]))))
                     for e in bm.edges if len(e.link_faces) == 1)


def face_components(bm):
    unvisited = set(bm.faces)
    components = 0
    while unvisited:
        components += 1
        stack = [unvisited.pop()]
        while stack:
            f = stack.pop()
            for edge in f.edges:
                for neighbor in edge.link_faces:
                    if neighbor in unvisited:
                        unvisited.remove(neighbor)
                        stack.append(neighbor)
    return components


def bowtie_vertex_count(bm):
    # A bow-tie vertex has more than one disconnected incident face fan.
    # This is a stricter manifold check than just counting edge incident faces.
    count = 0
    for v in bm.verts:
        faces = set(v.link_faces)
        if len(faces) <= 1:
            continue
        stack = [next(iter(faces))]
        seen = set(stack)
        while stack:
            f = stack.pop()
            for e in f.edges:
                if v not in e.verts:
                    continue
                for nf in e.link_faces:
                    if nf in faces and nf not in seen:
                        seen.add(nf)
                        stack.append(nf)
        if len(seen) < len(faces):
            count += 1
    return count


def audit_bmesh(bm, matrix, min_angle_deg, area_rel):
    bm.verts.index_update()
    bm.faces.index_update()
    dia = bbox_diagonal(bm, matrix)
    area_eps = max(dia * dia * area_rel, 1e-30)
    pts_all = [world_pos(v, matrix) for v in bm.verts]
    if pts_all:
        dims = [max(p[i] for p in pts_all)-min(p[i] for p in pts_all)
                for i in range(3)]
    else:
        dims = [0.0, 0.0, 0.0]
    results = []
    nontri = 0
    for f in bm.faces:
        d = face_data(f, matrix)
        if d is None:
            nontri += 1
        else:
            results.append((f.index, d))
    min_angles = [d["min_angle"] for _, d in results]
    qualities = [d["quality"] for _, d in results]
    thin = [fi for fi, d in results if d["min_angle"] < min_angle_deg]
    tiny = [fi for fi, d in results if d["area"] <= area_eps]
    bm.verts.index_update()
    seen_faces = set()
    duplicate_faces = 0
    for f in bm.faces:
        key = (tuple(sorted(v.index for v in f.verts)), f.material_index)
        if key in seen_faces:
            duplicate_faces += 1
        seen_faces.add(key)
    edge_bad = sum(len(e.link_faces) > 2 for e in bm.edges)
    wind_bad = sum(len(e.link_faces) == 2 and not e.is_contiguous for e in bm.edges)
    wire = sum(len(e.link_faces) == 0 for e in bm.edges)
    v_boundary_degree = {v: 0 for v in bm.verts}
    for e in bm.edges:
        if len(e.link_faces) == 1:
            for v in e.verts:
                v_boundary_degree[v] += 1
    boundary_branch = sum(deg not in (0, 2) for deg in v_boundary_degree.values())
    result = {
        "vertices": len(bm.verts), "edges": len(bm.edges), "faces": len(bm.faces),
        "triangles": len(results), "non_triangle_faces": nontri,
        "boundary_edges": sum(len(e.link_faces) == 1 for e in bm.edges),
        "nonmanifold_edges_gt2_faces": edge_bad,
        "wire_edges": wire, "isolated_vertices": sum(len(v.link_edges) == 0 for v in bm.verts),
        "duplicate_faces": duplicate_faces,
        "inconsistent_winding_edges": wind_bad,
        "nonmanifold_boundary_degree_vertices": boundary_branch,
        "bowtie_vertices": bowtie_vertex_count(bm),
        "face_components": face_components(bm),
        "min_angle_deg": min(min_angles, default=None),
        "median_min_angle_deg": statistics.median(min_angles) if min_angles else None,
        "min_triangle_quality": min(qualities, default=None),
        "skinny_triangles": len(thin),
        "tiny_area_triangles": len(tiny),
        "worst_skinny_face_indices": thin[:20],
        "world_bbox_diagonal": dia, "world_bbox_dimensions": dims,
        "world_mesh_surface_area": sum(d["area"] for _, d in results),
        "tiny_area_threshold": area_eps,
    }
    result["basic_preflight_pass"] = bool(
        result["faces"] > 0 and result["non_triangle_faces"] == 0
        and edge_bad == 0 and wind_bad == 0 and wire == 0
        and result["isolated_vertices"] == 0 and boundary_branch == 0
        and result["bowtie_vertices"] == 0 and duplicate_faces == 0
        and result["skinny_triangles"] == 0
        and result["tiny_area_triangles"] == 0
    )
    return result


def candidate_flip(edge, matrix, config, area_eps):
    if not edge.is_valid or len(edge.link_faces) != 2 or edge.seam or not edge.smooth:
        return None
    f1, f2 = edge.link_faces
    if len(f1.verts) != 3 or len(f2.verts) != 3:
        return None
    if f1.material_index != f2.material_index:
        return None
    a, b = edge.verts
    c = next((v for v in f1.verts if v not in edge.verts), None)
    d = next((v for v in f2.verts if v not in edge.verts), None)
    if c is None or d is None or c is d:
        return None
    a, b, c, d = (world_pos(v, matrix) for v in (a, b, c, d))
    old1, old2 = triangle_data([a, b, c]), triangle_data([b, a, d])
    if min(old1["area"], old2["area"]) <= area_eps:
        return None
    # Require the existing surface to be locally near-planar.
    normal_angle = math.degrees(angle_between(old1["normal"], old2["normal"]))
    if normal_angle > config["flip_max_dihedral_deg"]:
        return None
    n = old1["normal"] + old2["normal"]
    if n.length < 1e-30:
        return None
    n.normalize()
    # Both diagonals must lie inside the same projected convex quadrilateral.
    s1 = n.dot((b-a).cross(c-a))
    s2 = n.dot((b-a).cross(d-a))
    s3 = n.dot((d-c).cross(a-c))
    s4 = n.dot((d-c).cross(b-c))
    if s1 * s2 >= 0 or s3 * s4 >= 0:
        return None
    new1 = triangle_data([d, c, a])
    new2 = triangle_data([c, d, b])
    if min(new1["area"], new2["area"]) <= area_eps:
        return None
    if max(math.degrees(angle_between(n, new1["normal"])),
           math.degrees(angle_between(n, new2["normal"]))) > config["flip_max_normal_change_deg"]:
        return None
    old_worst = min(old1["min_angle"], old2["min_angle"])
    new_worst = min(new1["min_angle"], new2["min_angle"])
    gain = new_worst - old_worst
    if old_worst >= config["min_angle_deg"] or gain < config["flip_min_gain_deg"]:
        return None
    return gain


def improve_flips(bm, matrix, config, area_eps):
    n = 0
    for _ in range(config["max_flip_passes"]):
        ranked = []
        for e in tuple(bm.edges):
            gain = candidate_flip(e, matrix, config, area_eps)
            if gain is not None:
                ranked.append((gain, e))
        if not ranked:
            break
        ranked.sort(key=lambda pair: pair[0], reverse=True)
        step = 0
        for _, e in ranked:
            if n >= config["max_flips"]:
                break
            # An earlier flip can change the neighborhood; re-evaluate.
            if candidate_flip(e, matrix, config, area_eps) is None:
                continue
            result = bmesh.ops.rotate_edges(bm, edges=[e], use_ccw=False)
            if result.get("edges"):
                n += 1
                step += 1
        if step == 0 or n >= config["max_flips"]:
            break
    return n


def local_tangent_moves(bm, matrix, config, area_eps):
    # Opt-in only: bounded local tangential Laplacian step, accepted only if
    # all incident triangles remain oriented and the worst angle improves.
    # Bound TOTAL movement per vertex by fraction of world-space bbox diagonal.
    dia = bbox_diagonal(bm, matrix)
    allowed = dia * config["max_vertex_move_rel"]
    if allowed <= 0:
        return 0
    originals = {v: world_pos(v, matrix).copy() for v in bm.verts}
    inverse3 = matrix.to_3x3().inverted_safe()
    moved = set()
    for _ in range(config["max_move_passes"]):
        selected = set()
        for f in bm.faces:
            fd = face_data(f, matrix)
            if fd and fd["min_angle"] < config["min_angle_deg"]:
                selected.update(f.verts)
        changes = 0
        for v in list(selected):
            if any(len(e.link_faces) != 2 or e.seam or not e.smooth for e in v.link_edges):
                continue
            ring = [f for f in v.link_faces if len(f.verts) == 3]
            if not ring or len(ring) != len(v.link_faces):
                continue
            neighbors = {w for e in v.link_edges for w in e.verts if w is not v}
            if len(neighbors) < 3:
                continue
            before = [face_data(f, matrix) for f in ring]
            normal = sum((d["normal"] for d in before), Vector((0, 0, 0)))
            if normal.length <= 1e-30:
                continue
            normal.normalize()
            pos = world_pos(v, matrix)
            target = sum((world_pos(w, matrix) for w in neighbors), Vector((0, 0, 0))) / len(neighbors)
            tangent = target-pos
            tangent -= normal * tangent.dot(normal)
            remaining = allowed - (pos-originals[v]).length
            if tangent.length < 1e-30 or remaining <= 0:
                continue
            displacement = tangent.normalized() * min(tangent.length * 0.35, remaining)
            if displacement.length <= 0:
                continue
            old_co = v.co.copy()
            v.co += inverse3 @ displacement
            after = [face_data(f, matrix) for f in ring]
            valid = all(d["area"] > area_eps for d in after)
            valid = valid and all(
                math.degrees(angle_between(a["normal"], b["normal"]))
                <= config["flip_max_normal_change_deg"]
                for a, b in zip(before, after)
            )
            old_min = min(d["min_angle"] for d in before)
            new_min = min(d["min_angle"] for d in after)
            valid = valid and new_min > old_min + 0.05
            if valid:
                changes += 1
                moved.add(v)
            else:
                v.co = old_co
        if changes == 0:
            break
    return len(moved)


def report_result(report):
    name = "PF_MESH_PREP_REPORT"
    textblock = bpy.data.texts.get(name) or bpy.data.texts.new(name)
    textblock.clear()
    textblock.write(json.dumps(report, indent=2, sort_keys=True))
    # Full structured report is also available in Blender's Text editor.
    print("PARAFASHION_MESH_REPORT_JSON=" + json.dumps(report, separators=(",", ":")))


def run():
    obj = (bpy.data.objects.get(CONFIG["object_name"])
           if CONFIG["object_name"] else bpy.context.view_layer.objects.active)
    if obj is None or obj.type != 'MESH':
        raise RuntimeError("Select an active MESH object or set PF_CONFIG['object_name']")
    if obj.mode != 'OBJECT':
        raise RuntimeError("Switch mesh object to OBJECT mode first; no mode was changed")
    if CONFIG["mode"] not in ("audit", "repair"):
        raise ValueError("PF_CONFIG['mode'] must be 'audit' or 'repair'")
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    matrix = obj.matrix_world.copy()
    before = audit_bmesh(bm, matrix, CONFIG["min_angle_deg"], CONFIG["tiny_area_rel"])
    report = {
        "mode": CONFIG["mode"], "source_object": obj.name,
        "blender_version": bpy.app.version_string,
        "thresholds_are_engineering_defaults_not_paper_rules": True,
        "config": CONFIG, "before": before, "actions": [],
        "limitations": [
            "No self-intersection / near-collision or fabricability certification",
            "No automatic separation of non-manifold garment shells",
            "No automatic closure of garment openings",
            "No multi-pose vertex correspondence validation",
        ],
    }
    if CONFIG["mode"] == "audit":
        bm.free()
        report_result(report)
        return report
    # All modifications happen on a new mesh copy. The source remains untouched.
    if obj.data.shape_keys is not None:
        bm.free()
        report["status"] = "BLOCKED: shape keys; coordinate retopology must be synchronized"
        report_result(report)
        return report
    if obj.modifiers and not CONFIG["allow_edit_with_modifiers"]:
        bm.free()
        report["status"] = "BLOCKED: modifiers present; inspect evaluated vs base geometry first"
        report_result(report)
        return report
    if before["faces"] == 0:
        bm.free()
        report["status"] = "BLOCKED: no surface faces"
        report_result(report)
        return report

    boundary_before = boundary_edge_signature(bm)
    area_eps = before["tiny_area_threshold"]
    try:
        if CONFIG["remove_duplicate_faces"]:
            bm.verts.index_update()
            face_keys = set()
            duplicates = []
            for f in bm.faces:
                key = (tuple(sorted(v.index for v in f.verts)), f.material_index)
                if key in face_keys:
                    duplicates.append(f)
                else:
                    face_keys.add(key)
            if duplicates:
                bmesh.ops.delete(bm, geom=duplicates, context='FACES_ONLY')
                report["actions"].append({"removed_exact_duplicate_faces": len(duplicates)})

        if CONFIG["remove_wire_geometry"]:
            wires = [e for e in bm.edges if len(e.link_faces) == 0]
            if wires:
                bmesh.ops.delete(bm, geom=wires, context='EDGES')
                report["actions"].append({"remove_wire_edges": len(wires)})
            isolated = [v for v in bm.verts if len(v.link_edges) == 0]
            if isolated:
                bmesh.ops.delete(bm, geom=isolated, context='VERTS')
                report["actions"].append({"remove_isolated_vertices": len(isolated)})

        # Preserve original mesh resolution. Triangulation is a format/topology
        # requirement, not a simplification or decimation operation.
        nontri = [f for f in bm.faces if len(f.verts) > 3]
        if nontri:
            bmesh.ops.triangulate(bm, faces=nontri,
                                  quad_method='BEAUTY', ngon_method='BEAUTY')
            report["actions"].append({"triangulated_nontri_faces": len(nontri)})

        if CONFIG["remove_tiny_faces"]:
            tiny = [f for f in bm.faces
                    if len(f.verts) == 3 and face_data(f, matrix)["area"] <= area_eps]
            max_delete = math.ceil(before["faces"] * CONFIG["max_removed_face_fraction"])
            if len(tiny) > max_delete:
                report["actions"].append({"skipped_tiny_face_deletion_over_limit": len(tiny)})
            elif tiny:
                bmesh.ops.delete(bm, geom=tiny, context='FACES_ONLY')
                report["actions"].append({"deleted_tiny_faces": len(tiny)})
                # Strict boundary invariant below may roll back these edits.

        uv_blocks_flips = bool(obj.data.uv_layers) and CONFIG["preserve_existing_uv"]
        if uv_blocks_flips:
            report["actions"].append("skipped_flips_to_preserve_existing_uv")
        else:
            flipped = improve_flips(bm, matrix, CONFIG, area_eps)
            report["actions"].append({"flipped_internal_edges": flipped})

        if CONFIG["fix_inconsistent_winding"]:
            # BMesh recalculates normals; validation will reject any increase
            # in inconsistent shared-edge winding.
            bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
            report["actions"].append("recalculated_face_normals")

        if CONFIG["allow_local_vertex_motion"]:
            moved = local_tangent_moves(bm, matrix, CONFIG, area_eps)
            report["actions"].append({"locally_moved_vertices": moved,
                                      "max_total_move_world_units":
                                          CONFIG["max_vertex_move_rel"] * before["world_bbox_diagonal"]})

        bm.normal_update()
        after = audit_bmesh(bm, matrix, CONFIG["min_angle_deg"], CONFIG["tiny_area_rel"])
        report["after"] = after
        boundary_after = boundary_edge_signature(bm)
        # Invariants: no new non-manifold defects, all original garment
        # boundary edges intact, and no reduction in valid surface area by
        # default (except explicit tiny-face opt-in).
        regressions = []
        for key in ("nonmanifold_edges_gt2_faces", "inconsistent_winding_edges",
                    "nonmanifold_boundary_degree_vertices", "bowtie_vertices"):
            if after[key] > before[key]:
                regressions.append(key + "_increased")
        if boundary_after != boundary_before:
            regressions.append("garment_boundary_edges_changed")
        if (before["non_triangle_faces"] == 0
                and after["skinny_triangles"] > before["skinny_triangles"]):
            regressions.append("skinny_triangle_count_increased")
        for old_size, new_size in zip(before["world_bbox_dimensions"],
                                      after["world_bbox_dimensions"]):
            if abs(new_size-old_size) > max(before["world_bbox_diagonal"] * 1e-6, 1e-12):
                regressions.append("world_bbox_dimensions_changed")
                break
        old_area = before["world_mesh_surface_area"]
        new_area = after["world_mesh_surface_area"]
        if before["non_triangle_faces"] == 0 and old_area > 0:
            area_drift_rel = abs(new_area-old_area)/old_area
            report["surface_area_drift_rel"] = area_drift_rel
            if area_drift_rel > 0.001:
                regressions.append("surface_area_changed_over_0.1_percent")
        removed_count = sum(item.get("removed_exact_duplicate_faces", 0) +
                            item.get("deleted_tiny_faces", 0)
                            for item in report["actions"] if isinstance(item, dict))
        if after["faces"] < before["faces"] - removed_count:
            regressions.append("unexplained_face_loss")
        report["regressions"] = regressions
        if regressions:
            report["status"] = "ROLLED_BACK: preservation invariant failed"
            bm.free()
            report_result(report)
            return report

        bm.verts.index_update()
        bm.faces.index_update()
        copy = obj.copy()
        copy.data = obj.data.copy()
        copy.name = obj.name + "_PF_PREP"
        coll = bpy.data.collections.get("PF_PREP_OUTPUT")
        if coll is None:
            coll = bpy.data.collections.new("PF_PREP_OUTPUT")
            bpy.context.scene.collection.children.link(coll)
        coll.objects.link(copy)
        # Inherits world pose and dimensions, but never edits the source data.
        copy.matrix_world = obj.matrix_world.copy()
        bm.to_mesh(copy.data)
        copy.data.update()
        bm.free()
        report["output_object"] = copy.name
        report["status"] = ("BASIC_PREFLIGHT_PASS" if after["basic_preflight_pass"]
                            else "PARTIAL: remaining issues require review")
        copy["PF_PREP_STATUS"] = report["status"]
        copy["PF_PREP_SOURCE"] = obj.name
        report_result(report)
        return report
    except Exception as exc:
        bm.free()
        report["status"] = "ERROR; source object untouched; output not committed"
        report["error"] = str(exc)
        report_result(report)
        raise


run()
