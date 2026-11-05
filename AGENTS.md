# Agent Guidelines (AGENTS.md)

## 1. Library Modification Policy (Crucial)

Strict restrictions apply when working with dependencies in the `lib/` directory:

- **Do NOT modify external / public libraries**:
  All third-party and public submodule libraries under `lib/` must remain untouched. This includes, but is not limited to:
  - `lib/libigl`
  - `lib/vcglib`
  - `lib/Directional`
  - `lib/glew-cmake`
  - `lib/json`
  - `lib/AntTweakBar`
  - `lib/Clipper`
  - Any other upstream or vendor dependencies.

- **Allowed / Editable Libraries**:
  Only the following sub-projects / libraries under `lib/` may be edited or adapted:
  - `lib/garment-flattening`
  - `lib/xfield_tracer`