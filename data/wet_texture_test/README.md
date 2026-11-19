# Textured wet model test

The input `wet-texture.obj` uses the geometry from [wet.obj](../wet/wet.obj) with
planar UV coordinates added for this test. It uses `wet-texture.mtl` and
`fabric.png`. The contrasting colors and stripes make it easy to check that
the colors on the garment carry over to the flattened pieces.

Open the OBJ in Parafashion, enable **Textured**, and click **Batch Process**
with **Symmetric Model** enabled and **UV Mode** set to **Cloth**. Use
**Generate SVG** to reproduce the saved pattern outputs.

- `wet-texture_patch.svg` contains the embedded texture and vector cut lines.
- `wet-texture_patch.png` is the rendered pattern.
- `wet-texture_patch.txt` contains the face-to-patch assignments.
- `textured-patterns-preview.jpg` shows the 3D garment and flattened pieces in the UI.

The original `wet.obj` lacks source UVs and was used separately to check the
fallback without texture.
