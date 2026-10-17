# ==============================================================================
# Parafashion — Build & Runtime Environment (Ubuntu 22.04)
# ==============================================================================
# This image provides the C++ toolchain, Qt5/OpenGL libraries, and X11 runtime
# required to compile and run Parafashion.
#
# The repository itself is NOT copied into the image; it is bind-mounted at
# `/app` via docker-compose.yml so source edits and `/app/build` artifacts stay
# synchronized with the host.
# ==============================================================================

FROM ubuntu:22.04

# Install build toolchain and runtime dependencies.
# Note: DEBIAN_FRONTEND=noninteractive is passed inline because it is only
# needed at image build time to prevent apt/tzdata interactive prompts.
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        # --- C++ Toolchain & Build System ---
        # git + ca-certificates are required by libigl to fetch external
        # dependencies (Eigen, GLFW, ImGui, CoMISo, Triangle) at CMake configure time.
        build-essential \
        cmake \
        ninja-build \
        ccache \
        pkg-config \
        git \
        ca-certificates \
        # --- Qt5 (Widgets, OpenGL, SVG) ---
        # Replaces `qt5-default` from the README, which was removed in Ubuntu 22.04.
        qtbase5-dev \
        libqt5opengl5-dev \
        libqt5svg5-dev \
        # --- OpenGL, GLUT & Mesa Software Rasterizer ---
        # libgl1-mesa-dri provides llvmpipe so OpenGL works over X11 forwarding.
        freeglut3-dev \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        libgl1-mesa-dri \
        # --- Linear Algebra (BLAS & LAPACK) ---
        # Required by libigl's CoMISo solver module.
        libblas-dev \
        liblapack-dev \
        # --- X11 Windowing Headers ---
        # Required when building GLFW (pulled in by libigl / garment-flattening).
        libx11-dev \
        libxext-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev \
        libxxf86vm-dev \
        # --- Fonts & Locale ---
        # Parafashion explicitly sets `setlocale(LC_NUMERIC, "en_US.UTF-8")` in main().
        fontconfig \
        fonts-dejavu-core \
        locales \
    && locale-gen en_US.UTF-8 \
    && rm -rf /var/lib/apt/lists/*

# Allow git to operate on the bind-mounted `/app` repo and submodules even when
# host and container UIDs differ.
RUN git config --system --add safe.directory '*'

WORKDIR /app

CMD ["bash"]
