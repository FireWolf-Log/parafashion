# Parafashion Mesh Prep — Blender MCP Agent Skill

A reusable **Agent Skills (`SKILL.md`)** package that instructs an AI agent to inspect and conservatively repair garment meshes in Blender via **any discovered Blender MCP server**. It provides a runnable `bpy`/`bmesh` helper; no mesh decimation is performed by default.

## Install (Google Antigravity)

Copy the **entire folder** into:

```text
<workspace>/.agents/skills/mesh-prep/
├── SKILL.md
├── README.md
├── references/
│   └── quality_contract.md
└── scripts/
    └── parafashion_mesh_prep.py
```

Antigravity documentation: https://antigravity.google/docs/skills

You must separately install/configure a Blender MCP server and start Blender; this package is an **agent skill**, not an MCP server or a Blender add-on. The agent discovers the MCP tool names at runtime.

## Example prompt

> Use `/mesh-prep` to normalize the active Blender garment mesh for Parafashion. Fix skinny triangles and topology problems, but preserve geometry and mesh density; do not decimate or globally remesh. Audit first, work on a duplicate, compare before/after metrics, and export to OBJ only if the basic preflight passes.

## How to run the helper via MCP

The agent reads `scripts/parafashion_mesh_prep.py` and sends its contents to an available Blender MCP **Python execution** tool. Prepend a configuration expression:

```python
PF_CONFIG = {"mode": "audit"}  # or: {"mode": "repair"}
# entire contents of scripts/parafashion_mesh_prep.py must follow
```

Default mode is **audit** and makes no mesh changes. **Repair** duplicates the active mesh to `PF_PREP_OUTPUT` and leaves the original untouched. The resulting report is printed as JSON and written into Blender's `PF_MESH_PREP_REPORT` text datablock. An output clone marked `PARTIAL` is **not** validated as ready for Parafashion.

**Important:** A shared path between the agent host and Blender cannot be assumed. If `open()` cannot access the script from Blender, the agent must transmit the actual Python source through the MCP call, rather than assume installation of this skill automatically installs the script in Blender.

## Not claimed

The script has been syntax-checked using Python, **not run against a live Blender instance in this environment**. Behavior should be validated on a backup model with the actual installed Blender/MCP versions. It does not certify geometric self-intersections, perfect UV preservation after explicitly allowed topology changes, cloth simulation realism, or final manufacturability.
