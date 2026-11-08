---
name: mesh-prep
description: Inspect, conservatively normalize, and validate 3D garment meshes for Parafashion using Blender via an available Blender MCP server. Use for OBJ/PLY garment input checks, manifold topology repair, skinny/degenerate triangle diagnosis, edge flipping, triangulation, and non-destructive preparation without decimation.
---

# Parafashion Mesh Preparation (Blender MCP)

## Mission

Prepare a **garment surface mesh**, not an entire body scan, for input into **Parafashion**. Use **Blender through the connected Blender MCP tool(s)**, preserve the source object and dimensional accuracy, favor local changes, and produce an auditable *before → after* result. Do **not** simplify, decimate, globally remesh, voxelize, resculpt, subdivide, smooth away, or delete useful fabric surface merely to hit a polygon target. A small amount of removal is permissible only when demonstrably necessary to eliminate invalid/redundant geometry and backed by preservation checks.

**Must-have vs preferences:** Parafashion's paper explicitly assumes **a manifold input garment mesh with well-shaped triangles** (Sec. 3.2). It accepts a triangle mesh for the 3D garment, and matching connectivity/vertex correspondence if multiple poses are supplied. The released program accepts **OBJ / PLY**. Its paper says uniform remeshing is an alternative when triangle shape is poor; **the user specifically prefers local fixes without changing overall mesh resolution**, so do not reach for uniform remeshing first. Open, manifold garment boundaries such as collars/hem/cuffs are valid; do not automatically close them. The paper describes interactive editing up to ~3000 triangles as a **performance observation, not an import limit**.

Read `references/quality_contract.md` to distinguish source-backed requirements from this skill's tunable heuristics.

## Invocation and MCP adapter

1. Discover the installed Blender MCP tools **at runtime**. Different servers expose different names such as `execute_blender_code`, `blender_execute_python`, or `execute_python`. Do **not** assume one name or schema. Prefer scene inspection + unrestricted (or explicitly permitted) Blender `bpy` Python execution. If arbitrary Python is unavailable, run the audit using available read-only tools and clearly state which repairs cannot be automated. Never pretend a Blender operation succeeded without a returned tool result.
2. Verify Blender is running and the correct mesh object is active. If the user specifies an OBJ/PLY path, import it with the available MCP import action / Blender UI before operating. Do not select other mesh objects silently. If the active object is a body mesh or consists of body + apparel, **stop and report that garment-surface extraction is a separate step**, unless the garment is already reliably isolated.
3. Read `scripts/parafashion_mesh_prep.py`. Send the script code through the MCP Python execution tool, with `PF_CONFIG = {...}` placed **before** the script in the same execution context. An external agent's filesystem is **not necessarily mounted in Blender**: avoid `exec(open('/some/agent/path').read())` unless that exact file is demonstrably accessible from Blender. The default `mode` is `audit` (read-only).
4. If the server has a screenshot tool, capture before/after screenshots as supplementary visual checks; **numerical geometry checks take priority**.
5. If MCP is disconnected or Python execution fails, do not fabricate a result. Provide the diagnostic or repair commands and report the blocker.

## Priority order (strict)

1. **Do not harm the source.** Never overwrite the input file, apply destructive modifiers to the original, or edit its mesh datablock in place. In `repair` mode the helper duplicates the object and its mesh to `<original>_PF_PREP` in the `PF_PREP_OUTPUT` collection. Work on the duplicate. Retain the original object for rollback.
2. **Preserve topology where possible.** First fix bad triangulation by *internal edge flips* (`bmesh.ops.rotate_edges`) that increase the minimum triangle angle. Flips preserve vertex positions, vertex count, and triangle count. The helper only accepts a local edge flip when its adjacent triangles are approximately coplanar, its proposed triangles are valid and convex in projection, and the minimum angle improves. Never flip across garment boundaries, UV seam flags, hard edges, or material borders.
3. **Preserve appearance and fit.** Preserve world-space size, origin, transforms, boundary edges, garment openings, separate fabric components, and material assignments. Respect surface detail such as darts, folds, intentionally narrow regions, structural notches and seam marks; a narrow triangle is **not automatically defective** if it models a deliberate feature. Do not turn a cloth sheet into a watertight solid.
4. **Use localized movement only when explicitly permitted.** Optional vertex motion is restricted to interior, unprotected vertices and to a maximum total displacement relative to the original mesh's world-space bounding-box diagonal. It must improve local triangle quality and avoid inverted/degenerate faces. Never use unconstrained smoothing as a substitute for repair.
5. **Delete only invalid/redundant elements.** Removing loose wire edges, isolated vertices, or exact duplicate faces is acceptable. Deletion of near-zero-area surface faces is disabled by default because it can create holes; enable only when justified, bounded in quantity, and validated. Never apply global `Decimate`, `Remesh`, `Voxel Remesh`, `Limited Dissolve`, `Merge by Distance`, `Solidify`, `Smooth`, or blanket `Delete Loose` without independently considering garment boundaries and coincident layers. `Merge by Distance` in particular can accidentally weld intentionally separate garment layers.
6. **Never invent Parafashion thresholds.** The paper does not mandate a numerical minimum angle, quality score, or maximum triangle count. The helper's 10° minimum-angle target, 0.05% maximum optional vertex displacement, and flip dihedral thresholds are **heuristics** to adjust to the mesh scale and intended use.

## Procedure

### Phase A — Read-only audit (always run first)

- Identify active mesh, source filepath if known, object mode, object transforms, relevant modifiers, shape keys, UV layers, custom attributes, vertex groups, material boundaries, and whether this is one garment surface or multiple overlapping layers.
- Run helper with `PF_CONFIG = {"mode": "audit"}`. Look for: non-triangle faces; internal edges with more than two incident faces; bow-tie vertex fans; malformed boundary vertex degrees; loose/wire geometry; inconsistent face winding; duplicate faces; tiny-area triangles; and triangles under the minimum angle threshold. Record connected face components, garment boundary edge count, world-space bounding-box diagonal, face/vertex count, worst minimum angle, and skinny-triangle count.
- Check visualization to distinguish accidental problems from intentionally thin design features. A valid garment mesh can have **boundary edges**. Non-manifold is *not synonymous* with open or non-watertight.
- Warn if a live modifier changes evaluated geometry versus the base mesh; the helper intentionally blocks automatic repair while modifiers are present unless a knowledgeable agent explicitly opts in. Also warn if the object contains shape keys or is one of multiple pose-corresponding meshes.
- Audit reports are printed as `PARAFASHION_MESH_REPORT_JSON=...` and saved in Blender's Text datablock `PF_MESH_PREP_REPORT`.

### Phase B — Conservative repairs

When the audit finds actionable defects, run the helper with `PF_CONFIG = {"mode": "repair"}`. It will:

1. Make a new copy for output and never modify the source.
2. Remove exact duplicate faces, loose wire edges, and isolated vertices when present.
3. Triangulate any remaining quads / n-gons without decimating surface detail.
4. Prefer guarded *internal edge flips* to improve bad minimum angles. It will not flip across UV data if `preserve_existing_uv=True` and UV layers are present. If an already-textured model truly requires flips, explicitly decide whether its existing UV layout can be discarded/rebuilt **on the duplicate only** and set `preserve_existing_uv=False` accordingly. Flag possible UV changes in the report.
5. Optionally correct inconsistent face orientations, with a topology check afterward.
6. **Only on explicit instruction** use `allow_local_vertex_motion=True`, capped by `max_vertex_move_rel` (default 0.0005 of the world-space bounding-box diagonal). Local vertex movement is not a substitute for geometry reconstruction.
7. Reject changes on the duplicated geometry if the boundary graph changes, non-manifold counts regress, skinny-triangle counts worsen (on comparable already-triangulated inputs), or bounding-box size grows unexpectedly. Inspect resulting object and status.

If isolated degeneracy cannot be repaired by edge flips without modifying garment boundaries, report the unresolved face indices/regions and propose *the smallest* localized alternatives. Do not silently run full remeshing. For overlapping shells, pinched topology, coincident cloth layers, or self intersections, the appropriate repair is ambiguous: seek geometric evidence (and user guidance when needed) before deleting/splitting/filling.

### Phase C — Verify and export

**Never assert a full pass simply because the MCP command returned successfully.** Inspect the output object's `PF_PREP_STATUS` and the `after` metrics:

- 100% triangular faces; no wire edges / isolated vertices / exact duplicate faces.
- Zero internal edges with >2 incident faces; zero bow-tie vertices and invalid branching garment boundaries; consistent winding on interior manifold edges.
- No zero/tiny-area faces, and ideally no triangles below the configured minimum angle. If some remain, label as `PARTIAL` rather than `PASS`.
- Stable **world-space dimensions**, preserved boundary geometry, original design features, and unchanged (or justified) face/vertex counts. Explain every deletion.
- Audit **does not** prove freedom from self-intersections, physical stitchability, or good resulting pattern shape; if these matter, additional visual/BVH checks or a Parafashion import run are required.

Only if the output merits use, export the **duplicate** to a new `*_pf_ready.obj` or `*_pf_ready.ply` file via the Blender MCP exporter or appropriate Blender Python API discovered for that Blender version. Use a new filepath and explicitly confirm export succeeded. Do not alter object scale / geometry units without permission. Optionally reload the exported OBJ/PLY for a second audit because export can reorder/triangulate mesh data.

If `PF_PREP_STATUS` is `PARTIAL`, the agent may save a `*_pf_review` intermediate but must clearly distinguish it from validated output; by default, do not label it `ready`.

## Concrete agent calls (adapt to discovered MCP schema)

**Audit**:

```python
# Prepend this to the full contents of scripts/parafashion_mesh_prep.py
PF_CONFIG = {"mode": "audit", "min_angle_deg": 10.0}
# ...paste script contents below this line, then execute inside Blender...
```

**Conservative normalization**:

```python
PF_CONFIG = {
    "mode": "repair",
    "min_angle_deg": 10.0,
    "remove_wire_geometry": True,
    "remove_duplicate_faces": True,
    "remove_tiny_faces": False,
    "preserve_existing_uv": True,
    "allow_local_vertex_motion": False,
}
# ...paste full helper script and run through MCP...
```

**Additional local movement** (only when preserving world shape within 0.05% of bounding-box diagonal is acceptable):

```python
PF_CONFIG = {
    "mode": "repair",
    "allow_local_vertex_motion": True,
    "max_vertex_move_rel": 0.0005,
}
# ...paste full helper script through MCP...
```

Never invoke external shell code inside Blender or require network access for this skill. If Blender MCP only exposes a generic Python tool, write clear `bpy`/`bmesh` operations in that tool; no third-party remesher is a prerequisite.

## Special cases and stop rules

- **Multiple target poses:** The paper requires identical mesh connectivity and full correspondence. Do not flip or split faces on just one pose. Only use topology-changing edits if they can be applied identically to *all* poses and verified by face-index and vertex-index correspondence. Otherwise audit and report.
- **Shape keys / armatures:** The helper blocks existing shape keys. Do not apply destructively because vertex correspondence may be essential. Coordinate with the user's deformation pipeline.
- **UV atlas or textures:** Topology flips can invalidate UV interpolation and seams. The helper blocks flips when existing UVs are present by default. If the task prioritizes tailoring and old UVs are expendable, ask/confirm or rely on explicit user instruction before opting in.
- **Closed scan with body included:** Do not treat a body surface, a thick solid, or a scanned combination of body and garment as if it were an extracted garment surface. Remove/extract those surfaces in a separate, explicit step.
- **Unrepairable non-manifold geometry:** Do not declare successful normalization. Present the offending region, reason, smallest safe manual fix, and what remains unverified.
- **Performance:** Preserve resolution even if >3000 triangles. The ~3000 figure is for interactive editing in the paper, **not a requirement to decimate**. If runtime is slow, let user decide whether to optimize density.

## Required final reply from the acting agent

State: input and output object/file; whether run was only an audit, a partial repair, or a basic preflight pass; numerical `before → after` comparison (faces, vertices, skinny triangles, min angle, nonmanifold issues, boundary edges); exact repair operations; any topology changes or surface deletion; unresolved issues and whether it was imported successfully into Parafashion. If the actual Blender MCP connection is unavailable, say so and do not claim the model was processed.

## Sources

- Pietroni et al., *Computational Pattern Making from 3D Garment Models*, ACM TOG 41(4), 2022, Sections 3 and 6: https://doi.org/10.1145/3528223.3530145
- Parafashion implementation and OBJ/PLY usage: https://github.com/nicopietroni/parafashion
- Blender BMesh operators: https://docs.blender.org/api/current/bmesh.ops.html
- Blender BMesh types: https://docs.blender.org/api/current/bmesh.types.html
