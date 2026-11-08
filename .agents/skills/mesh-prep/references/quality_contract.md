# Parafashion mesh quality contract: facts vs heuristics

## Documented inputs

Parafashion's source paper: Pietroni et al., *Computational Pattern Making from 3D Garment Models* (ACM TOG/SIGGRAPH 2022). Section 3 describes a **triangle mesh representing the target garment in 3D**. Section 3.2 states: "We assume the input garment mesh to be a manifold and with well shaped triangles (otherwise we uniformly remesh with standard tools)." For multiple target poses, each mesh must share connectivity and be in full correspondence with the rest pose. An optional symmetry plane may be used for symmetric garments.

The paper says that small models up to ~3000 triangles allow real-time editing in its experiments. This is **not** a formal triangle cap for loading meshes. Section 6 states that the work does not explicitly handle non-manifold inputs and instead separates them into manifold parts before processing.

The upstream GitHub README specifies `.obj` or `.ply` command-line input: `./parafashion <Mesh>`. Repo: https://github.com/nicopietroni/parafashion

An open surface can still be manifold *with boundary*. Therefore, collars, cuffs and hems can remain open. No preprocessing step should indiscriminately close all boundary loops.

## Skill defaults — **not** dictated by the paper

| Check / setting | Default | Meaning |
| --- | ---: | --- |
| Minimum triangle interior angle | 10 degrees | Heuristic flag for skinny triangles, not a Parafashion hard cutoff |
| Tiny triangle area | <= 1e-12 × (world bbox diagonal)^2 | Scale-relative degeneracy heuristic |
| Candidate flip gain | >= 0.5 degree improvement in worst triangle angle | Avoid gratuitous topology changes |
| Max old dihedral for flip | 10 degrees | Avoid flips on strong folds / sharp detail |
| Max candidate normal change | 12 degrees | Curb geometry distortion due to flipping |
| Optional total vertex displacement | <= 0.0005 × world bbox diagonal | 0.05% shape-preservation budget |
| Direct decimation / voxel remesh | Disabled | User expressly prefers preserving surface resolution |
| Change original | Forbidden | Always generate duplicate |
| UV / material / hard-edge crossing | Forbidden by default | Protect design and authored information |
| Remove tiny faces | Disabled | May produce unwanted fabric holes |

The triangle quality score used in the helper is `4√3·Area / (a² + b² + c²)`, normalized so an equilateral triangle has score 1; the minimum angle is the principal pass/fail quality measure used by this *skill*. These are diagnostic choices, not native Parafashion guarantees.

## Validity and limitations

No numerical report from this helper proves an input will yield a good physical sewing pattern. It does not automatically check near-self-intersection, fabric response, panel distortion after flattening, accidental surface layers, or garment semantics. The actual Parafashion importer and downstream pattern-generation pipeline remain the final integration test.

**Geometry preservation is more important than a zero-skinny count.** If a triangular region is deliberately narrow due to a small dart or detail, leaving it unchanged and reporting the exception may be the correct choice.

## References

- https://doi.org/10.1145/3528223.3530145
- https://github.com/nicopietroni/parafashion
- https://docs.blender.org/api/current/bmesh.ops.html
