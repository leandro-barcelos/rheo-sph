# RheoSPH

**RheoSPH** is a GPU-accelerated, physics-based simulator for tailings dam flow events. Built in C++23 with Vulkan compute shaders, it enables engineers and researchers to simulate non-Newtonian fluid dynamics over real-world terrain using only physically measurable parameters — no game engine required.

The project is a high-performance reimplementation of a simulation originally developed in Unity, designed to be a viable tool for the mining industry.

> ⚠️ **Status:** Active development. Not yet production-ready.

---

## Table of Contents

- [Overview](#overview)
- [Physics Model](#physics-model)
- [Dependencies](#dependencies)
- [Building](#building)
- [Terrain Data](#terrain-data)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [License](#license)

---

## Overview

Tailings dam failures are among the most catastrophic industrial accidents in the mining sector. RheoSPH provides a simulation environment where users can model a potential flow event using only real-world geotechnical parameters and actual terrain data — without requiring knowledge of game engines or graphics programming.

**Key features:**

- Smoothed Particle Hydrodynamics (SPH) simulation running entirely on the GPU via Vulkan compute shaders
- Bingham plastic rheology model, suitable for tailings slurries
- Real terrain support via GeoTIFF elevation data (DEM)
- Optional visualization texture overlay for the terrain (aerial imagery, orthophotos, etc.)
- Interactive top-down camera with pan and zoom
- Parameter configuration and persistence via YAML files
- ImGui-based UI for real-time parameter adjustment

---

## Physics Model

RheoSPH implements a **Weakly Compressible SPH (WCSPH)** formulation for non-Newtonian fluids. The simulation pipeline runs three GPU compute passes per timestep:

### 1. Spatial Hashing (Bucket Pass)
Particles are sorted into a uniform 3D grid of voxels. This reduces neighbor search complexity from O(N²) to O(N), enabling real-time performance with large particle counts.

### 2. Density Estimation
Particle density is computed using the **Poly6 kernel**:

$$W(\mathbf{r}, h) = \frac{315}{64\pi h^9}(h^2 - |\mathbf{r}|^2)^3$$

### 3. Velocity and Position Integration
Forces are accumulated and positions integrated. Three force contributions are computed per particle:

**Pressure force** — using the Spiky kernel gradient to model repulsion between particles:

$$\mathbf{f}_i^{press} = -\sum_j m_j \frac{p_i + p_j}{2\rho_j} \nabla W_{spiky}(\mathbf{r}_{ij})$$

Pressure follows the equation of state: $p = k(\rho - \rho_0)$

**Viscous / rheological force** — using the **Bingham plastic** constitutive model. The effective stress tensor $\boldsymbol{\tau}$ is computed from the strain rate tensor $\mathbf{D}$:

$$\boldsymbol{\tau} = \left(\frac{\tau_y}{|\mathbf{D}|} + 2\mu_p\right)\mathbf{D}, \quad |\mathbf{D}| > 0$$

where $\tau_y$ is the yield stress and $\mu_p$ is the plastic viscosity. When $|\mathbf{D}| \approx 0$ the material behaves as a rigid solid.

**Boundary force** — wall particles enforce domain boundaries and terrain contact using a penalty-based method with normal and tangential components. Coulomb friction is applied at the contact surface:

$$\|\mathbf{f}_s\| \leq \mu \|\mathbf{f}_n\|$$

A coefficient of restitution controls energy loss on impact.

### Terrain Interaction
Elevation is sampled from the uploaded DEM at runtime using bilinear interpolation. The terrain surface normal is estimated via finite differences to correctly resolve the contact direction.

---

## Dependencies

The following must be installed on the system before building:

| Dependency | Purpose |
|---|---|
| CMake ≥ 4.2 | Build system |
| Vulkan SDK (≥ 1.3) | Graphics and compute API |
| GLFW3 | Window and input management |
| GLM | Math library |
| yaml-cpp | Configuration file I/O |
| GDAL | GeoTIFF parsing and elevation extraction |
| Slang compiler (`slangc`) | Shader compilation to SPIR-V |

On Ubuntu/Debian:

```bash
sudo apt install cmake libglfw3-dev libglm-dev libgdal-dev libyaml-cpp-dev
```

Install the [Vulkan SDK](https://vulkan.lunarg.com/) and ensure `slangc` is available in `$VULKAN_SDK/bin` or on your `PATH`.

---

## Building

```bash
# Clone the repository with submodules (ImGui and ImGuiFileDialog)
git clone --recurse-submodules https://github.com/your-org/rheo-sph.git
cd rheo-sph

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The compiled binary and shader SPIR-V files will be placed inside the `build/` directory. Shaders are compiled automatically as part of the build.

> The `resources/fonts/` directory must be present at the working directory when running the application (it contains the Font Awesome icon font used by the UI).

---

## Terrain Data

RheoSPH requires a **Digital Elevation Model (DEM)** in **GeoTIFF format** (`.tif` / `.tiff`). Any source providing GeoTIFF files is supported.

### Obtaining terrain data from OpenTopography

[OpenTopography](https://opentopography.org/) provides free, high-resolution LiDAR and global DEM datasets.

1. Go to [https://opentopography.org/](https://opentopography.org/) and create a free account.
2. On the main map, navigate to the area of interest (e.g., the location of a tailings facility).
3. Select a dataset — recommended options:
   - **SRTM GL1 (30m)** — global coverage, good for large areas
   - **SRTM GL3 (90m)** — lower resolution, suitable for regional-scale simulations
   - Regional LiDAR datasets when available (highest accuracy)
4. Draw a bounding box around the area you want to simulate.
5. Select **GeoTIFF** as the output format and submit the job.
6. Download the resulting `.tif` file.

### Using data from other sources

Any GeoTIFF file with a valid single-band elevation raster is supported. Common sources include:

- **IBGE** (Brazil) — [https://www.ibge.gov.br/](https://www.ibge.gov.br/)
- **Copernicus DEM** (Europe, global) — [https://spacedata.copernicus.eu/](https://spacedata.copernicus.eu/)
- **USGS Earth Explorer** — [https://earthexplorer.usgs.gov/](https://earthexplorer.usgs.gov/)
- Internal company survey data exported to GeoTIFF

### DEM Resolution

When loading a DEM, you must specify the **spatial resolution in meters per pixel**. This value is used to correctly scale world-space positions. If the DEM was downloaded at 30 m/pixel, set the resolution parameter to `30.0`. The default is `10.0 m/pixel`.

### Visualization Texture (optional)

An optional RGB image can be overlaid on the terrain mesh for visual reference (e.g., an orthophoto or satellite image). Supported formats: PNG, JPEG, BMP, TGA, and GeoTIFF.

---

## Usage

```bash
cd build
./rheo-sph
```

### Workflow

1. **Load terrain** — open `File → Select DEM` and choose your `.tif` file. Set the DEM resolution to match your dataset.
2. **Load visualization texture** *(optional)* — open `File → Select visualization texture` to overlay an image on the terrain.
3. **Configure parameters** — fill in the physical parameters in the settings panel (see table below).
4. **Play** — press the play button. The simulation starts immediately.
5. **Pause / Reset** — use the control panel buttons at any time.
6. **Save / Load** — simulation parameters can be saved and loaded as YAML files via `File → Save` and `File → Open`.

### Physical Parameters

| Parameter | Unit | Description |
|---|---|---|
| Total Tailings Volume | m³ | Total volume of the simulated fluid mass |
| Initial Particle Spacing | m | Distance between particles at initialization |
| Max Particles Per Voxel | — | Controls spatial hash capacity; increase if particles escape the grid |
| Rest Density | kg/m³ | Reference density of the tailings slurry |
| Gas Constant (k) | Pa·m³/kg | Stiffness of the equation of state |
| Plastic Viscosity | cP | Dynamic viscosity (Bingham model) |
| Yield Stress | Pa | Minimum stress required to initiate flow (Bingham model) |
| Coefficient of Restitution | — | Energy conservation on boundary impact (0 = fully inelastic) |
| Friction Coefficient | — | Coulomb friction at boundary contacts |
| DEM Resolution | m/pixel | Pixel size of the uploaded elevation model |

---

## Project Structure

```
rheo-sph/
├── shaders/
│   ├── defines.slang           # Shared constants (thread counts, kernel macros)
│   ├── compute/
│   │   ├── bucket.slang        # Spatial hashing (clear, fluid, wall bucket passes)
│   │   ├── density.slang       # SPH density estimation (Poly6 kernel)
│   │   └── vel_pos.slang       # Force integration and position update
│   └── graphics/
│       ├── particle.slang      # Point sprite rendering for fluid particles
│       └── terrain.slang       # Terrain mesh vertex/fragment shader
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── rheo_sph_app.*      # Application entry point and main loop
│   │   ├── simulation_session.*# Lifecycle management of the fluid simulator
│   │   └── ui_controller.*     # Bridges UI events to simulation intents
│   ├── core/
│   │   ├── vulkan_context.*    # Vulkan instance creation
│   │   ├── vulkan_device.*     # Physical/logical device and queue selection
│   │   ├── vulkan_swap_chain.* # Swapchain management
│   │   ├── command_pool.*      # Graphics and compute command pools
│   │   ├── frame_sync.*        # Timeline semaphore and fence synchronization
│   │   ├── pipeline.*          # Graphics and compute pipeline builder
│   │   └── window.*            # GLFW window and input events
│   ├── renderer/
│   │   ├── renderer.*          # Top-level render loop and image layout transitions
│   │   ├── fluid_renderer.*    # Point-sprite rendering of SPH particles
│   │   ├── terrain_renderer.*  # Indexed triangle mesh rendering of the DEM
│   │   └── camera.*            # Top-down orthographic-style camera (pan/zoom)
│   ├── resources/
│   │   ├── buffer.*            # GPU buffer allocation and SSBO helpers
│   │   ├── descriptor.*        # Descriptor pool and set allocation
│   │   ├── elevation.*         # Elevation vertex layout and attribute descriptions
│   │   ├── geotiff.*           # GDAL-backed GeoTIFF reader
│   │   ├── images.*            # Texture image loading (STB + GDAL fallback)
│   │   ├── immediate_submit.*  # Single-use command buffer helpers
│   │   └── memory.*            # Vulkan memory type selection
│   ├── simulation/
│   │   └── fluid_simulator.*   # GPU simulation: buffers, pipelines, dispatch loop
│   └── ui/
│       ├── imgui_layer.*       # ImGui Vulkan/GLFW backend initialization
│       └── panels/
│           ├── control_panel.* # Play / Pause / Reset buttons
│           └── parameters_panel.* # Parameter inputs and file dialogs
├── external/
│   ├── imgui/                  # Dear ImGui (docking branch, submodule)
│   └── imgui-file-dialog/      # ImGuiFileDialog (submodule)
├── resources/
│   └── fonts/                  # Font Awesome TTF (required at runtime)
└── CMakeLists.txt
```

