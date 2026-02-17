/*
 * Model rendering functions
 */

#ifndef MODEL_H
#define MODEL_H

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

// Model data structure to pass to renderer
typedef struct {
    int tri_count;
    int quad_count;
    int (*tri_faces)[3];
    int (*tri_uvs)[3];
    int (*quad_faces)[4];
    int (*quad_uvs)[4];
    SVECTOR *uvs;
    SVECTOR *normals;  // Vertex normals for lighting
    unsigned char *material_flags;
    CVECTOR *vertex_colors;
    unsigned char *specular;  // Specular values (0-255)
    unsigned char *metallic;  // Metallic values (0-255)
    unsigned char *mesh_ids;  // Mesh ID per face (for visibility control)
    unsigned int visible_meshes;  // Bitmask: bit N = mesh N visible
} ModelData;

// Render the complete model with given vertices
// Returns updated nextpri pointer
char* renderModel(
    SVECTOR *verts,
    ModelData *model,
    char *nextpri,
    u_long *ot,
    int ot_length,
    u_short tpage,
    u_short clut
);

#endif
