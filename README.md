# PlayStation 1 Blender Exporter

A Blender 4.0 addon that exports 3D models and animation data to C header files compatible with Sony's PSY-Q or PsyQo for PlayStation 1 development.

## Features

- **Multi-texture support** - Per-face texture index tracking for models with multiple materials  
- **Animation export** - Bakes vertex animations to per-frame data  
- **Vertex color support** - Exports per-corner colors  
- **Material flags** - Tracks lit/unlit, textured, smooth/flat, cutout, semi-transparency, and vertex color states per face. Metallic and specular are included, but it is still a WIP.  
- **Automatic triangulation** - Converts n-gons to tris/quads  
- **Mesh IDs** - exports with references to individual submeshes on your model, allowing for visibility toggling or other after effects  

<img width="1226" height="942" alt="Screenshot 2026-01-10 124122" src="https://github.com/user-attachments/assets/57d44c12-bf90-4601-ae0e-88db7622bb51" />
<img src="https://github.com/user-attachments/assets/12f6a7f9-9cfb-4097-a4a3-9363cf9adae4"/>
<img src="https://github.com/user-attachments/assets/fecd8a42-7061-4f8a-8793-3c459b1149de"/>



## Installation

1. Download `ps1_exporter.py` from the Releases tab
2. Extract from zip
3. Open Blender 4.0
4. Go to `Edit → Preferences → Add-ons`
5. Click `Install...` and select `ps1_exporter.py`
6. Enable "PlayStation 1 Exporter" in the addon list

## Usage

1. Select your mesh objects
2. Go to `File → Export → PlayStation 1 (.h)`
3. Choose export options:
   - **Convert to Z-up** - Converts Blender's Y-up to PS1's Z-up coordinate system
   - **Force Unlit** - Disable lighting calculations for all faces
   - **Export Animations** - Export all actions as separate animation files
   - **Header Type** - This will format the headers depending on the sdk you are using
4. Click Export

## Export Options

| Option | Description |
|--------|-------------|
| Convert to Z-up | Transforms coordinates from Blender (Y-up) to PS1 (Z-up) |
| Force Unlit | Sets all faces to unlit mode |
| Export Animations | Creates separate `.h` files for each animation action |

## Output Format

### Model Header (`modelname.h`)

```c
#define VERTICES_COUNT 8
#define UVS_COUNT 24
#define FACES_COUNT 6
#define PS1_SCALE 3072
#define TEXTURE_COUNT 1

SVECTOR vertices[VERTICES_COUNT];      // Vertex positions
SVECTOR uvs[UVS_COUNT];                // UV coordinates
int quad_faces[N][4];                  // Quad face indices
int quad_uvs[N][4];                    // Quad UV indices
int tri_faces[N][3];                   // Triangle face indices
int tri_uvs[N][3];                     // Triangle UV indices
signed char face_texture_idx[FACES_COUNT];  // Per-face texture index
unsigned char material_flags[FACES_COUNT];  // Per-face material properties
CVECTOR vertex_colors[N];              // Vertex colors (if present)
```

### Animation Header (`modelname-ActionName.h`)

```c
#define ACTIONNAME_FRAMES_COUNT 20
#define ACTIONNAME_VERTICES_COUNT 8

SVECTOR ActionName_anim[FRAMES_COUNT][VERTICES_COUNT] = {
    { // Frame 1..X
        { x, y, z }  // entry per vertex
    }
};
```

### Material Flags

Each face has a material flags byte:

| Bit | Flag | Description |
|-----|------|-------------|
| 0 | Unlit | Face ignores lighting |
| 1 | Textured | Face has a texture |
| 2 | Smooth | Gouraud shading (vs flat) |
| 3 | Vertex Colors | Face uses vertex colors |

As PSY-Q primitives:

| Flags | Primitive | Description |
|-------|-----------|-------------|
| Textured + Flat | POLY_FT3/FT4 | Flat-textured |
| Textured + Smooth | POLY_GT3/GT4 | Gouraud-textured |
| Vertex Colors | POLY_G3/G4 | Gouraud-shaded |
| None | POLY_F3/F4 | Flat-colored |

## Scale Factor

The exporter uses a scale factor of **3072** to convert Blender units to PS1 fixed-point:

| Blender Size | PS1 Value |
|--------------|-----------|
| 1 unit | 3072 |
| 2 units | 6144 |
| 10 units | 30720 |

**PS1 Limits**: Vertex values must be between -32768 and +32767. Models larger than ~10 Blender units may overflow.

## Texture Workflow

1. Assign materials with Image Texture nodes in Blender
2. Export the model (texture filenames are captured)
3. Convert textures to TIM format and then C header (see tools folder):
   ```bash
   .\png2tim.exe -p 320 0 texture.png
   python bin2header.py texture.tim texture.h texture_tim
   ```
4. Include and load in your project, see the examples folder for working examples of an animated model


## Requirements

- Blender 4.0
- PSY-Q or PsyQo SDK
- png2tim for image conversion to the TIM format
- bin2header.py, bin2c, or similar header conversion tool for rendering textured models.
