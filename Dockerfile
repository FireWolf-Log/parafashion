# Build/runtime environment for parafashion.
#
# The repository is NOT copied into the image: it is bind-mounted at /app at
# runtime (see docker-compose.yml). Build it inside the container following
# the README (cmake into /app/build), then run ./build/parafashion <mesh>.
FROM ubuntu:22.04

# - build-essential/cmake/ninja-build/ccache/git: toolchain (libigl fetches
#   Eigen, GLFW, ImGui, CoMISo, Triangle, ... with git at CMake configure time)
# - qtbase5-dev/libqt5opengl5-dev/libqt5svg5-dev: Qt5 Widgets/OpenGL/Svg
#   (replaces README's `qt5-default`, which no longer exists on 22.04)
# - freeglut3-dev, libgl/glu: OpenGL + GLUT
# - libx*-dev: required by GLFW (pulled in by libigl / garment-flattening)
# - libgl1-mesa-dri: Mesa software rendering (llvmpipe) for X11 forwarding
# - libblas/liblapack: required by libigl's CoMISo module
# - locales: the app calls setlocale(LC_NUMERIC, "en_US.UTF-8")
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        ccache \
        git \
        ca-certificates \
        pkg-config \
        qtbase5-dev \
        libqt5opengl5-dev \
        libqt5svg5-dev \
        freeglut3-dev \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        libgl1-mesa-dri \
        libblas-dev \
        liblapack-dev \
        libx11-dev \
        libxext-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev \
        libxxf86vm-dev \
        fontconfig \
        fonts-dejavu-core \
        locales \
    && locale-gen en_US.UTF-8 \
    && rm -rf /var/lib/apt/lists/*

# The mounted repo is owned by the host user; let git (used by libigl's
# dependency downloads inside the submodule) operate on it anyway.
RUN git config --system --add safe.directory '*'

WORKDIR /app

CMD ["bash"]
