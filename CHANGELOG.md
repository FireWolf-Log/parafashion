# Changelog

All notable committed changes to the **Parafashion** project made this year (2026) are documented in this file.

---

## 1. Environment Setup (Dockerized Build & Runtime Environment)

- **Timestamp:** 2026-10-17 21:32 *(Initial: commit `93770d2`)*; 2026-10-17 22:41 *(Refinement & documentation: commit `a728ea4`)*
- **Commits:**
  - `93770d2` — *Added docker env*
  - `a728ea4` — *Reformat & update comment*
- **Files Added:**
  - `Dockerfile`
  - `docker-compose.yml`
  - `.dockerignore`

### Problem / Root Cause
- **Outdated Package References in Modern Linux Distributions:** The original project documentation referenced legacy packages such as `qt5-default`, which has been completely removed in modern Ubuntu releases (Ubuntu 20.04+ / 22.04 LTS).
- **Complex Multi-Component Dependency Matrix:** Building and running Parafashion requires a wide array of specialized system libraries:
  - Modern C++ toolchain with CMake and Ninja.
  - Qt5 development components (`qtbase5-dev`, `libqt5opengl5-dev`, `libqt5svg5-dev`).
  - Graphics rendering packages (OpenGL, GLUT, and Mesa DRI drivers).
  - Linear algebra solvers (`libblas-dev`, `liblapack-dev`) required by libigl's CoMISo solver module.
  - X11 client libraries required by GLFW.
  - Specific UTF-8 numerical locale support (`setlocale(LC_NUMERIC, "en_US.UTF-8")` invoked in `apps/parafashion.cpp`).
- **Host OpenGL & Remote Display Incompatibilities:** Running OpenGL applications inside containers across macOS (XQuartz) and Linux hosts frequently breaks due to hardware acceleration mismatches and MIT-SHM shared memory permission restrictions.
- **Git Submodule Ownership in Docker:** When bind-mounting the host repository into a container, different UID/GID mappings between host and container cause Git to reject directory access (`fatal: detected dubious ownership in repository`), blocking submodules downloaded during CMake configuration.

### Changes Made & Technical Fixes
1. **Containerized Build Environment (`Dockerfile`):**
   - Base image upgraded to `ubuntu:22.04`.
   - Replaced deprecated `qt5-default` with modular Qt5 libraries: `qtbase5-dev`, `libqt5opengl5-dev`, and `libqt5svg5-dev`.
   - Included full development toolchain: `build-essential`, `cmake`, `ninja-build`, `ccache`, `pkg-config`, `git`, and `ca-certificates`.
   - Included OpenGL and headless software rendering support: `freeglut3-dev`, `libgl1-mesa-dev`, `libglu1-mesa-dev`, and `libgl1-mesa-dri` (providing Mesa llvmpipe software rasterizer for reliable X11 forwarding).
   - Installed CoMISo solver dependencies: `libblas-dev` and `liblapack-dev`.
   - Added X11 windowing headers for GLFW: `libx11-dev`, `libxext-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`, `libxxf86vm-dev`.
   - Configured `fontconfig`, `fonts-dejavu-core`, and generated the `en_US.UTF-8` locale.
   - Configured global Git safety override: `git config --system --add safe.directory '*'` to allow seamless submodule fetching and build operations regardless of host UID.
2. **Container Orchestration (`docker-compose.yml`):**
   - Bind-mounted root repository directly to `/app` (`.:/app`) so build artifacts in `/app/build` and code edits stay synchronized with the host without rebuilding Docker images.
   - Mounted `/tmp/.X11-unix` for X11 socket forwarding.
   - Configured display and runtime flags:
     - `DISPLAY: host.docker.internal:0` for out-of-the-box macOS (XQuartz) compatibility.
     - `QT_X11_NO_MITSHM: "1"` to prevent shared memory errors across the container boundary.
     - `LIBGL_ALWAYS_SOFTWARE: "1"` to ensure OpenGL rendering succeeds over network/socket displays.
     - `CMAKE_GENERATOR: Ninja` for accelerated parallel builds.
     - `LANG: en_US.UTF-8` and `LC_ALL: en_US.UTF-8` for numerical parsing consistency.
3. **Build Context Optimization (`.dockerignore`):**
   - Added wildcard ignore (`*`) to eliminate Docker build context transfer overhead since the repository is mounted at runtime.

---

## 2. Fix Build Issues

- **Timestamp:**
  - 2026-10-18 15:08 *(CMake executable linking: commit `2f1fece`)*
  - 2026-10-18 15:24 *(Submodule fork migration: commit `1b99226`)*
  - 2026-11-08 09:49 *(Static GLEW linking on macOS: commit `7e65970`)*
- **Commits:**
  - `2f1fece` — *Resolve build error.*
  - `1b99226` — *Updated submodule to my forked version*
  - `7e65970` — *Make libglew static lib to prevent symlink issue on case insensetive OS (MacOS).*
- **Files Modified:**
  - `CMakeLists.txt`
  - `.gitmodules`
  - *(Submodule `lib/garment-flattening` repository updates)*

### Problem / Root Cause
1. **Redundant Source Compilation and Target Misconfiguration:**
   - In `CMakeLists.txt`, `SRCPARAM` was already compiled into the `woven_param` library target. However, `add_executable(param apps/param.cpp ${SRCPARAM})` recompiled the parameterization sources directly rather than linking against `woven_param`, leading to duplicate symbols and missing header directory resolution.
2. **Upstream Submodule Incompatibilities & Missing Symbols in `garment-flattening`:**
   - The upstream submodule `lib/garment-flattening` (`CorentinDumery/garment-flattening`) was outdated relative to modern `libigl`:
     - Missing includes for standard I/O and mesh operations (`<igl/writeOBJ.h>`, `<igl/readPLY.h>`, `<iostream>`, `<fstream>`).
     - Incompatible API call to `igl::cut_mesh`: the upstream codebase used an obsolete explicit template instantiation signature `igl::cut_mesh<...>(SV, SF, cuts2, V_cut3, F_cut3, corres)` which failed to compile with modern libigl.
     - Numerical instability in `barycentricCoords` (division by near-zero `denom`) and rotation calculations producing `NaN`s during ARAP optimization.
3. **macOS Case-Insensitive Filesystem Symlink Conflict (`glew-cmake`):**
   - When building on macOS (APFS filesystem, case-insensitive by default), CMake's default target generation for `lib/glew-cmake` (`add_subdirectory(lib/glew-cmake)`) attempts to create shared library symlinks (e.g. casing collisions between `libGLEW` and `libglew`). This led to configuration and build failures on macOS.

### Changes Made & Technical Fixes
1. **Corrected Target Linkage for `param` Executable (`CMakeLists.txt` - 2026-10-18 15:08):**
   - Updated `add_executable(param apps/param.cpp)` to remove redundant `${SRCPARAM}` sources.
   - Linked `woven_param` directly:
     ```cmake
     add_executable(param apps/param.cpp)
     target_link_libraries(param woven_param igl::opengl_glfw igl::core igl::opengl_glfw_imgui)
     ```
2. **Migrated to Maintained Submodule Fork (`.gitmodules` - 2026-10-18 15:24):**
   - Switched submodule repository URL from `https://github.com/CorentinDumery/garment-flattening.git` to `https://github.com/FireWolf-Log/garment-flattening.git`.
   - Incorporated upstream fixes within the fork:
     - Added missing header includes (`<igl/writeOBJ.h>`, `<igl/readPLY.h>`, etc.).
     - Migrated `igl::cut_mesh` to the supported signature: `igl::cut_mesh(V_cut3, F_cut3, cuts2, corres);`.
     - Hardened numerical stability in `src/param/param_utils.cpp` to prevent `NaN` propagation (handling `denom < 1e-12` in barycentric coordinate calculations and replacing unstable 3D cross products with 2D planar angles in `computeRotation`).
3. **Static GLEW Library on Case-Insensitive Filesystems (`CMakeLists.txt` - 2026-11-08 09:49):**
   - Replaced `add_subdirectory(lib/glew-cmake EXCLUDE_FROM_ALL)` with a dedicated static library target:
     ```cmake
     add_definitions(-DGLEW_STATIC)
     add_library(libglew_static STATIC lib/glew-cmake/src/glew.c)
     target_include_directories(libglew_static PUBLIC lib/glew-cmake/include)
     target_compile_definitions(libglew_static PUBLIC GLEW_STATIC GLEW_NO_GLU)
     include_directories(lib/glew-cmake/include)
     ```
   - Avoids symlink generation on macOS and ensures consistent static linking across all target platforms.

---

## 3. Fix Output SVG Issues (Scaling & Canvas Padding)

- **Timestamp:** 2026-10-18 16:37 *(commit `e8182bf`)*
- **Commit:** `e8182bf` — *Fix SVG scaling issue & add margin.*
- **Files Modified:**
  - `src/myglwidget.cpp`
  - `include/svg_exporter.h`
  - `apps/param.cpp`

### Problem / Root Cause
1. **Hardcoded Scale Factor Causing Giant/Distorted SVGs:**
   - In `src/myglwidget.cpp` (`DoGenerateSVG`), the export scale factor was hardcoded to `float scale = 1000;`.
   - This assumed all input 3D garment meshes were modeled in meters (multiplying by 1000 to convert to millimeters). When loading models already in millimeters (diagonal > 100 mm, e.g. dress models) or in centimeters (diagonal > 10 cm, e.g. skirt models), the resulting SVG canvas and polyline coordinates were scaled by an astronomical 1000x factor, making files unviewable or exceeding vector renderer limits.
2. **Clipped Seam Lines and Truncated Text Labels:**
   - In `include/svg_exporter.h`, the SVG canvas dimensions (`pp.width`, `pp.height`) were calculated solely using the 2D pattern bounding box (`GetBox(outline2Vec)`).
   - This bounding box ignored:
     - Outer stroke widths (`penwidth`).
     - Text labels (`trText`) and circular label radii (`LabelRad`).
     - Seam allowance boundaries (`boundSize`).
   - Consequently, text annotations and outer boundary curves sitting on the perimeter were clipped by the SVG canvas edges.
3. **Zero Margin / Missing Canvas Padding:**
   - Geometry positioned near `(0, 0)` had zero clearance from the document edge, cutting off visual elements in standard SVG/PNG viewers.

### Changes Made & Technical Fixes
1. **Adaptive Unit Scale Detection (`src/myglwidget.cpp`):**
   - Evaluated the 3D bounding box diagonal of `deformed_mesh` (`meshDiag = deformed_mesh.bbox.Diag()`) to dynamically set scale units:
     ```cpp
     float scale = 1000.0f;
     float meshDiag = deformed_mesh.bbox.Diag();
     if (meshDiag > 100.0f)
         scale = 1.0f;   // Mesh coordinates already in millimeters (e.g. dress)
     else if (meshDiag > 10.0f)
         scale = 10.0f;  // Mesh coordinates in centimeters (e.g. skirt)
     // Otherwise mesh is in meters (scale = 1000.0f to convert to mm)
     ```
2. **Expanded Bounding Box Covering All Labels & Annotations (`include/svg_exporter.h`):**
   - Extended `TotalBBox` computation to iterate through all text labels (`trText[i][j]`) and their radius boundaries (`LabelRad[i][j]` or `fontsize * 2.0f`):
     ```cpp
     vcg::Box2<ScalarType> TotalBBox = GetBox(outline2Vec);
     for (size_t i = 0; i < trText.size(); i++) {
         for (size_t j = 0; j < trText[i].size(); j++) {
             float rad = (j < LabelRad[i].size()) ? LabelRad[i][j] : (fontsize * 2.0f);
             TotalBBox.Add(trText[i][j].tra - vcg::Point2f(rad, rad));
             TotalBBox.Add(trText[i][j].tra + vcg::Point2f(rad, rad));
         }
     }
     ```
3. **Generous Canvas Margin and Coordinate Offset Translation (`include/svg_exporter.h`):**
   - Added dynamic margin calculation:
     ```cpp
     float margin = std::max(30.0f, (float)(boundSize + fontsize * 3.0f + penwidth * 2.0f));
     ```
   - Shifted all pattern outlines (`outline2Vec`) and text annotations (`trText`) by `shift = Point2f(margin, margin) - TotalBBox.min`, guaranteeing all canvas coordinates are positive and comfortably inset.
   - Sized SVG canvas width and height with padding:
     ```cpp
     pp.width = std::ceil(TotalBBox.DimX() + 2.0f * margin);
     pp.height = std::ceil(TotalBBox.DimY() + 2.0f * margin);
     ```
   - Added console logging showing computed SVG dimensions and margin.
4. **Added Missing Include (`apps/param.cpp`):**
   - Added `#include <igl/lscm.h>` to provide necessary declarations for Least Squares Conformal Maps parameterization.

---

## 4. Texture Loading & Coordinate Alignment Fixes (OpenGL UV Inversion & Material Resolution)

- **Date:** 2026-11-08
- **Files Modified / Added:**
  - `include/texture_gl_utils.h` *(New)*
  - `src/myglwidget.cpp`
  - `tests/test_texture_loading.cpp` *(New)*
  - `CMakeLists.txt`

### Problem / Root Cause
1. **Vertical Texture Inversion (`v -> 1 - v`):**
   - In `MyGLWidget::initializeGL()`, texture maps were loaded via `txt_image = QImage(pathTxt.c_str())`. Qt's `QImage` memory layout stores scanlines top-to-bottom (`row 0` at top).
   - The raw buffer was passed directly to `glTexImage2D(...)` without a vertical flip. OpenGL texture samplers unpack scanlines bottom-to-top (`row 0` at `t = 0`, matching the standard bottom-left UV origin in Wavefront `.obj` `vt` coordinates loaded by VCGlib).
   - This caused a vertical texture flip across all loaded models: for body scans (e.g., `data/test_human_body/Male_T-Shirt.obj`), anatomical landmarks such as the chest/nipple texture (`v ≈ 0.57`) were shifted downward onto the belly (`v ≈ 0.43`), diverging visibly from viewers like MeshLab.
2. **Unsafe GL Pixel Upload Format (`GL_BGRA_EXT`):**
   - `glTexImage2D` hardcoded `GL_BGRA_EXT` on `txt_image.constBits()` without converting `QImage` to a consistent format or setting `GL_UNPACK_ALIGNMENT`.
   - While 32-bit RGBA PNGs accidentally matched little-endian BGRA memory layouts, 3-channel 24-bit RGB PNGs (such as `data/test_human_body/txt.png`), indexed 8-bit images, and non-dword-aligned scanlines suffered channel corruption or stride offsets.
3. **Ignored `.mtl` Material Files & Path Crashes:**
   - Textures were resolved via `pathTxt.erase(pathTxt.find_last_of("/")) + "/txt.png"`, throwing `std::out_of_range` when the model path contained no `/` and ignoring `.mtl` definitions. If a dataset used `.mtl` / `map_Kd` instead of `txt.png`, texture loading failed entirely.
4. **2D UV View Texture Binding & GL Resource Leak:**
   - In `MyGLWidget::paintGL()`, `MeshDrawing<TraceMesh>::GLDrawUV(...)` was passed `bool textured` (`0` or `1`) for `int TxtIndex`, causing invalid texture ID binding.
   - `resizeGL()` invoked `initializeGL()`, creating new textures via `glGenTextures` without deleting or reusing existing IDs.
   - SVG layout preview (`SVGTxt`) suffered from the same unflipped top-down pixel upload bug.

### Changes Made & Technical Fixes
1. **Dedicated Texture & OpenGL Utilities (`include/texture_gl_utils.h`):**
   - `prepareTextureForGL(const QImage &src)`: Converts arbitrary `QImage` formats to standard `QImage::Format_RGBA8888` and mirrors vertically (`mirrored(false, true)`), aligning scanline 0 with OpenGL `t = 0` / OBJ UV bottom-left `(0, 0)`.
   - `uploadQImageTexture2D(GLuint &texId, const QImage &src)`: Reuses existing OpenGL texture IDs to eliminate leaks on resize, sets `GL_UNPACK_ALIGNMENT = 1` around upload, and uploads cleanly as `GL_RGBA` / `GL_UNSIGNED_BYTE`.
   - `resolveMeshTexturePath(const std::string &meshPath)`: Safely extracts the base directory across Unix and Windows separators. Prioritizes `<dir>/txt.png` and falls back to parsing `mtllib` -> `map_Kd` relative to the mesh directory.
   - `resolveUVTextureIndex(bool hasTxt, bool isTextured, unsigned int textureId)`: Passes the actual texture ID to `GLDrawUV` when texturing is enabled, or `-1` otherwise.
2. **Widget Integration (`src/myglwidget.cpp`):**
   - Replaced raw `txt_image` and `SVGTxt` uploads with `uploadQImageTexture2D`.
   - Routed texture resolution through `resolveMeshTexturePath`.
   - Passed `resolveUVTextureIndex` to `GLDrawUV` in `paintGL()`.
   - Fixed `GLDrawSVGLayout` texture check (`TxtIndex > 0`).
3. **Unit Tests & Build Target (`tests/test_texture_loading.cpp`, `CMakeLists.txt`):**
   - Added `test_texture_loading` test suite registered with `CTest`.
   - Validates bottom-up vertical orientation, body scan UV landmark alignment, odd-width RGB888 / Indexed8 conversion, bare-filename path safety, and `.mtl` fallback resolution.
