# Computational Pattern Making from 3D Garment Models

[Nico Pietroni](www.nicopietroni.com), 
[Corentin Dumery](https://corentindumery.github.io),
[Raphael Falque](https://profiles.uts.edu.au/Raphael.Guenot-Falque),
[Mark Liu](http://www.drmarkliu.com),
[Teresa Vidal-Calleja](https://profiles.uts.edu.au/Teresa.VidalCalleja),
[Olga Sorkine-Hornung](https://igl.ethz.ch/people/sorkine/),
<br/>
*SIGGRAPH 2022*<br/>

![alt text](teaser.jpg)

## Abstract
We propose a method for computing a sewing pattern of a given 3D garment model. Our algorithm segments an input 3D garment shape into patches and computes their 2D parameterization, resulting in pattern pieces that can be cut out of fabric and sewn together to manufacture the garment. Unlike the general state-of-the-art approaches for surface cutting and flattening, our method explicitly targets garment fabrication. It accounts for the unique properties and constraints of tailoring, such as seam symmetry, the usage of darts, fabric grain alignment, and a flattening distortion measure that models woven fabric deformation, respecting its anisotropic behavior. We bootstrap a recent patch layout approach developed for quadrilateral remeshing and adapt it to the purpose of computational pattern making, ensuring that the deformation of each pattern piece stays within prescribed bounds of cloth stress. While our algorithm can automatically produce the sewing patterns, it is fast enough to admit user input to creatively iterate on the pattern design. Our method can take several target poses of the 3D garment into account and integrate them into the sewing pattern design. We demonstrate results on both skintight and loose garments, showcasing the versatile application possibilities of our approach.

Data: [http://www.quadmesh.cloud/parafashion/Figures_Pattern_Making.zip](http://www.quadmesh.cloud/parafashion/Figures_Pattern_Making.zip)<br />
#Website: [www.google.com](TBD)<br />
DOI: [TBD](https://doi.org/???) ACM Transactions on Graphics (SIGGRAPH), 2022

**BibTex**
```
@article{,
author = {Pietroni, Nico and Dumery, Corentin and Falque, Raphael and Liu, Mark and Vidal-Calleja, Teresa and Sorkine-Hurnung, Olga},
title = {Computational Pattern Making from 3D Garment Models
},
year = {2022},
issue_date = {August 2022},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
volume = {41},
number = {4},
issn = {?},
url = {https://doi.org/??},
doi = {??},
abstract = {We propose a method for computing a sewing pattern of a given 3D garment model. Our algorithm segments an input 3D garment shape into patches and computes their 2D parameterization, resulting in pattern pieces that can be cut out of fabric and sewn together to manufacture the garment. Unlike the general state-of-the-art approaches for surface cutting and flattening, our method explicitly targets garment fabrication. It accounts for the unique properties and constraints of tailoring, such as seam symmetry, the usage of darts, fabric grain alignment, and a flattening distortion measure that models woven fabric deformation, respecting its anisotropic behavior. We bootstrap a recent patch layout approach developed for quadrilateral remeshing and adapt it to the purpose of computational pattern making, ensuring that the deformation of each pattern piece stays within prescribed bounds of cloth stress. While our algorithm can automatically produce the sewing patterns, it is fast enough to admit user input to creatively iterate on the pattern design. Our method can take several target poses of the 3D garment into account and integrate them into the sewing pattern design. We demonstrate results on both skintight and loose garments, showcasing the versatile application possibilities of our approach.},
journal = {ACM Trans. Graph.},
articleno = {157},
numpages = {14},
keywords = {geometry processing, modelling, fashion design}
}
```

### Download
```bash
git clone --recursive https://github.com/nicopietroni/parafashion
git submodule update --init --recursive
```

## BUILD

The following packages are required: `qt5-default libqt5svg5-dev freeglut3-dev` (e.g. `sudo apt install ...` on Debian based systems).

```
mkdir build
cd build
cmake ..
make -j parafashion
```

Then, simply run parafashion with an obj or ply mesh as command-line argument: 
```
./parafashion <Mesh>
```

## Getting Started / User Guide

## Running the container

On macOS:

if using `colima` start it first:
```
colima start
```

```
xhost +localhost && docker compose run --rm parafashion
```

Stop `colima` if required:
```
colima stop
```s

### Symmetric vs. Asymmetric Models

By default, Parafashion is tailored for clothing patterns that exhibit bilateral reflectional symmetry across the $X = 0$ plane (slicing the mesh in half, relaxing distortion, and mirroring).

- **`Symmetric Model` Checkbox:** Located in the left control panel above `Symmetrize Deformed`.
  - **Checked `[x]` (Default):** Enforces bilateral symmetry across $X = 0$. Best for symmetric garments (t-shirts, pants, skirts).
  - **Unchecked `[-]`:** Treats the model as asymmetric. Essential for naturally asymmetric models (such as hands, gloves, asymmetric dresses, or accessories) so that geometry is **not** sliced or mirrored across the center plane.

#### Quick Workflow for Asymmetric Models:
1. Load your mesh: `./parafashion <path_to_mesh.obj>`
2. In the menu, uncheck **`Symmetric Model`** (click the box until it shows `[-]`).
3. Click **`Batch Process`** (or step through `Compute Field` $\to$ `Trace Paths` $\to$ `Parametrize Deformed`) to unwrap and flatten the full mesh.


### Textured patterns

Open a model with texture UVs and a source image. Parafashion uses the existing
`txt.png` convention or the diffuse image (`map_Kd`) in the OBJ's MTL file.
The texture workflow currently uses one image per model.

1. Turn on **Textured** to see the original image on the 3D garment.
2. Run **Batch Process**. With **Draw UV Mesh** enabled, the 2D pieces show
   the same surface colors as the 3D garment.
3. Use **Generate SVG** (or **Save Data**) to save the pattern as an SVG
   with embedded texture and as a PNG. The SVG keeps the cut lines
   and seam labels as vectors.

The flattened coordinates determine the shape and placement of the pieces.
The source texture coordinates determine their colors. After a symmetry cut
or remeshing, the source coordinates are transferred from the closest part of
the original surface. Fine texture details near source UV seams may shift if
the operation changes the triangulation. A mesh with no source UVs, such as
the bundled `data/wet/wet.obj`, still works with the usual patch colors.

## (Alternatively) using QT Creator

Open the .pro file, all the path in the .pri should be already ok.

## The output of parafashion consists of several files:
- The output patch subdivided mesh (suffix patch.obj): Per face, per patch colored mesh.
- The output patch index per face (suffix patch.txt)
- The output per face per vert UV (suffix UV.txt)
- For each patch the UV and 3D (suffix patch_UV_#.obj and  patch_3D_#.obj)
- patch decomposition ready to be fabricated (suffix patch.svg)

---

## Note
The code has slightly changed and the results could be different from the ones showed in the paper.

## License
[GPL3](LICENSE) licensed
([FAQ](https://www.gnu.org/licenses/gpl-faq.html))



