/*
 * Model rendering implementation
 */

#include "model.h"
#include <stdlib.h>

// Material flag bit definitions
#define MAT_FLAG_UNLIT        (1 << 0)
#define MAT_FLAG_TEXTURED     (1 << 1)
#define MAT_FLAG_SMOOTH       (1 << 2)
#define MAT_FLAG_VERTEX_COLOR (1 << 3)
#define MAT_FLAG_ALPHA        (1 << 4)
#define MAT_FLAG_CUTOUT       (1 << 5)
#define MAT_FLAG_SPECULAR     (1 << 6)
#define MAT_FLAG_METALLIC     (1 << 7)

//----------------------------------------------------------
// Check if model has any faces with cutout transparency
//----------------------------------------------------------
static int modelNeedsCutout(ModelData *model) {
    int i;
    // Check triangles
    for (i = 0; i < model->tri_count; i++) {
        if (model->material_flags[i] & MAT_FLAG_CUTOUT) {
            return 1;
        }
    }
    // Check quads (they start after triangles in material_flags)
    for (i = 0; i < model->quad_count; i++) {
        if (model->material_flags[model->tri_count + i] & MAT_FLAG_CUTOUT) {
            return 1;
        }
    }
    return 0;
}

//----------------------------------------------------------
// Apply specular lighting enhancement to color
// Uses simplified Blinn-Phong approximation for PS1
//----------------------------------------------------------
static void applySpecular(CVECTOR *color, SVECTOR *normal, unsigned char specular) {
    // Simple specular calculation using normal and a fixed light direction
    // Assumes light coming from above-forward (0, -4096, -2048)
    SVECTOR light_dir = {0, -4096, -2048};
    long dot;
    int spec_factor;
    
    // Normalize and calculate dot product (approximate)
    dot = (normal->vx * light_dir.vx + normal->vy * light_dir.vy + normal->vz * light_dir.vz) >> 12;
    
    // Only apply specular if facing the light
    if (dot > 0) {
        // Scale by specular value (0-255)
        spec_factor = (dot * specular) >> 8;
        if (spec_factor > 255) spec_factor = 255;
        
        // Add specular highlight (brighten the color)
        color->r = (color->r + spec_factor) > 255 ? 255 : (color->r + spec_factor);
        color->g = (color->g + spec_factor) > 255 ? 255 : (color->g + spec_factor);
        color->b = (color->b + spec_factor) > 255 ? 255 : (color->b + spec_factor);
    }
}

//----------------------------------------------------------
// Apply metallic property to color
// Metallic surfaces reflect more light and have stronger contrast
//----------------------------------------------------------
static void applyMetallic(CVECTOR *color, SVECTOR *normal, unsigned char metallic) {
    // Metallic surfaces have stronger reflections
    // Enhance the lighting effect based on metallic value
    if (metallic > 0) {
        // Calculate reflection factor
        int reflect_factor = (metallic * abs(normal->vy)) >> 12;  // Use Y component as approximation
        
        // Enhance contrast: darken darks, brighten brights
        int avg = (color->r + color->g + color->b) / 3;
        
        if (avg > 128) {
            // Brighten bright areas
            int boost = (reflect_factor * (255 - avg)) >> 8;
            color->r = (color->r + boost) > 255 ? 255 : (color->r + boost);
            color->g = (color->g + boost) > 255 ? 255 : (color->g + boost);
            color->b = (color->b + boost) > 255 ? 255 : (color->b + boost);
        } else {
            // Darken dark areas
            int reduce = (metallic * avg) >> 8;
            color->r = color->r > reduce ? (color->r - reduce) : 0;
            color->g = color->g > reduce ? (color->g - reduce) : 0;
            color->b = color->b > reduce ? (color->b - reduce) : 0;
        }
    }
}

//----------------------------------------------------------
// Render triangles
//----------------------------------------------------------
static char* renderTriangles(
    SVECTOR *verts,
    ModelData *model,
    char *nextpri,
    u_long *ot,
    int ot_length,
    u_short tpage,
    u_short clut
) {
    int i;
    long otz, p, flg;
    
    for (i = 0; i < model->tri_count; i++) {
        unsigned char flags = model->material_flags[i];
        int v0 = model->tri_faces[i][0];
        int v1 = model->tri_faces[i][1];
        int v2 = model->tri_faces[i][2];
        
        // Choose primitive type based on flags
        int is_textured = flags & MAT_FLAG_TEXTURED;
        int is_smooth = flags & MAT_FLAG_SMOOTH;
        int has_alpha = flags & MAT_FLAG_ALPHA;
        int has_vcolors = flags & MAT_FLAG_VERTEX_COLOR;
        int is_unlit = flags & MAT_FLAG_UNLIT;
        
        if (is_textured) {
            if (is_smooth) {
                // Smooth textured triangle (GT3)
                POLY_GT3 *poly = (POLY_GT3 *)nextpri;
                setPolyGT3(poly);
                
                otz = RotAverage3(
                    &verts[v0], &verts[v1], &verts[v2],
                    (long*)&poly->x0, (long*)&poly->x1, (long*)&poly->x2,
                    &p, &flg
                );
                
                if (otz > 0 && otz < ot_length) {
                    int uv0 = model->tri_uvs[i][0];
                    int uv1 = model->tri_uvs[i][1];
                    int uv2 = model->tri_uvs[i][2];
                    
                    setUV3(poly,
                        model->uvs[uv0].vx, model->uvs[uv0].vy,
                        model->uvs[uv1].vx, model->uvs[uv1].vy,
                        model->uvs[uv2].vx, model->uvs[uv2].vy
                    );
                    
                    poly->tpage = tpage;
                    poly->clut = clut;
                    
                    // Apply lighting or set vertex colors
                    if (is_unlit) {
                        // Unlit: use vertex colors directly
                        if (has_vcolors) {
                            setRGB0(poly, model->vertex_colors[v0].r, model->vertex_colors[v0].g, model->vertex_colors[v0].b);
                            setRGB1(poly, model->vertex_colors[v1].r, model->vertex_colors[v1].g, model->vertex_colors[v1].b);
                            setRGB2(poly, model->vertex_colors[v2].r, model->vertex_colors[v2].g, model->vertex_colors[v2].b);
                        } else {
                            setRGB0(poly, 128, 128, 128);
                            setRGB1(poly, 128, 128, 128);
                            setRGB2(poly, 128, 128, 128);
                        }
                    } else {
                        // Lit: calculate lighting with normals
                        CVECTOR base_color = has_vcolors ? model->vertex_colors[v0] : (CVECTOR){128, 128, 128, 0};
                        CVECTOR col0, col1, col2;
                        
                        // Calculate lighting for each vertex
                        NormalColorCol(&model->normals[v0], &base_color, &col0);
                        base_color = has_vcolors ? model->vertex_colors[v1] : (CVECTOR){128, 128, 128, 0};
                        NormalColorCol(&model->normals[v1], &base_color, &col1);
                        base_color = has_vcolors ? model->vertex_colors[v2] : (CVECTOR){128, 128, 128, 0};
                        NormalColorCol(&model->normals[v2], &base_color, &col2);
                        
                        // Apply specular if enabled
                        if ((flags & MAT_FLAG_SPECULAR) && model->specular) {
                            applySpecular(&col0, &model->normals[v0], model->specular[i]);
                            applySpecular(&col1, &model->normals[v1], model->specular[i]);
                            applySpecular(&col2, &model->normals[v2], model->specular[i]);
                        }
                        
                        // Apply metallic if enabled
                        if ((flags & MAT_FLAG_METALLIC) && model->metallic) {
                            applyMetallic(&col0, &model->normals[v0], model->metallic[i]);
                            applyMetallic(&col1, &model->normals[v1], model->metallic[i]);
                            applyMetallic(&col2, &model->normals[v2], model->metallic[i]);
                        }
                        
                        setRGB0(poly, col0.r, col0.g, col0.b);
                        setRGB1(poly, col1.r, col1.g, col1.b);
                        setRGB2(poly, col2.r, col2.g, col2.b);
                    }
                    
                    if (has_alpha) {
                        setSemiTrans(poly, 1);
                    }
                    
                    addPrim(&ot[otz], poly);
                    nextpri += sizeof(POLY_GT3);
                }
            } else {
                // Flat textured triangle (FT3)
                POLY_FT3 *poly = (POLY_FT3 *)nextpri;
                setPolyFT3(poly);
                
                otz = RotAverage3(
                    &verts[v0], &verts[v1], &verts[v2],
                    (long*)&poly->x0, (long*)&poly->x1, (long*)&poly->x2,
                    &p, &flg
                );
                
                if (otz > 0 && otz < ot_length) {
                    int uv0 = model->tri_uvs[i][0];
                    int uv1 = model->tri_uvs[i][1];
                    int uv2 = model->tri_uvs[i][2];
                    
                    setUV3(poly,
                        model->uvs[uv0].vx, model->uvs[uv0].vy,
                        model->uvs[uv1].vx, model->uvs[uv1].vy,
                        model->uvs[uv2].vx, model->uvs[uv2].vy
                    );
                    
                    poly->tpage = tpage;
                    poly->clut = clut;
                    
                    // Apply lighting or set color
                    if (is_unlit) {
                        // Unlit: use vertex color directly
                        if (has_vcolors) {
                            setRGB0(poly, model->vertex_colors[v0].r, model->vertex_colors[v0].g, model->vertex_colors[v0].b);
                        } else {
                            setRGB0(poly, 128, 128, 128);
                        }
                    } else {
                        // Lit: calculate lighting with face normal (use first vertex normal for flat shading)
                        CVECTOR base_color = has_vcolors ? model->vertex_colors[v0] : (CVECTOR){128, 128, 128, 0};
                        CVECTOR col;
                        NormalColorCol(&model->normals[v0], &base_color, &col);
                        
                        // Apply specular if enabled
                        if ((flags & MAT_FLAG_SPECULAR) && model->specular) {
                            applySpecular(&col, &model->normals[v0], model->specular[i]);
                        }
                        
                        // Apply metallic if enabled
                        if ((flags & MAT_FLAG_METALLIC) && model->metallic) {
                            applyMetallic(&col, &model->normals[v0], model->metallic[i]);
                        }
                        
                        setRGB0(poly, col.r, col.g, col.b);
                    }
                    
                    if (has_alpha) {
                        setSemiTrans(poly, 1);
                    }
                    
                    addPrim(&ot[otz], poly);
                    nextpri += sizeof(POLY_FT3);
                }
            }
        } else {
            if (is_smooth) {
                // Smooth shaded triangle (G3)
                POLY_G3 *poly = (POLY_G3 *)nextpri;
                setPolyG3(poly);
                
                otz = RotAverage3(
                    &verts[v0], &verts[v1], &verts[v2],
                    (long*)&poly->x0, (long*)&poly->x1, (long*)&poly->x2,
                    &p, &flg
                );
                
                if (otz > 0 && otz < ot_length) {
                    // Apply lighting or set vertex colors
                    if (is_unlit) {
                        // Unlit: use vertex colors directly
                        if (has_vcolors) {
                            setRGB0(poly, model->vertex_colors[v0].r, model->vertex_colors[v0].g, model->vertex_colors[v0].b);
                            setRGB1(poly, model->vertex_colors[v1].r, model->vertex_colors[v1].g, model->vertex_colors[v1].b);
                            setRGB2(poly, model->vertex_colors[v2].r, model->vertex_colors[v2].g, model->vertex_colors[v2].b);
                        } else {
                            setRGB0(poly, 128, 128, 128);
                            setRGB1(poly, 128, 128, 128);
                            setRGB2(poly, 128, 128, 128);
                        }
                    } else {
                        // Lit: calculate lighting with normals
                        CVECTOR base_color = has_vcolors ? model->vertex_colors[v0] : (CVECTOR){128, 128, 128, 0};
                        CVECTOR col0, col1, col2;
                        
                        NormalColorCol(&model->normals[v0], &base_color, &col0);
                        base_color = has_vcolors ? model->vertex_colors[v1] : (CVECTOR){128, 128, 128, 0};
                        NormalColorCol(&model->normals[v1], &base_color, &col1);
                        base_color = has_vcolors ? model->vertex_colors[v2] : (CVECTOR){128, 128, 128, 0};
                        NormalColorCol(&model->normals[v2], &base_color, &col2);
                        
                        // Apply specular if enabled
                        if ((flags & MAT_FLAG_SPECULAR) && model->specular) {
                            applySpecular(&col0, &model->normals[v0], model->specular[i]);
                            applySpecular(&col1, &model->normals[v1], model->specular[i]);
                            applySpecular(&col2, &model->normals[v2], model->specular[i]);
                        }
                        
                        // Apply metallic if enabled
                        if ((flags & MAT_FLAG_METALLIC) && model->metallic) {
                            applyMetallic(&col0, &model->normals[v0], model->metallic[i]);
                            applyMetallic(&col1, &model->normals[v1], model->metallic[i]);
                            applyMetallic(&col2, &model->normals[v2], model->metallic[i]);
                        }
                        
                        setRGB0(poly, col0.r, col0.g, col0.b);
                        setRGB1(poly, col1.r, col1.g, col1.b);
                        setRGB2(poly, col2.r, col2.g, col2.b);
                    }
                    
                    if (has_alpha) {
                        setSemiTrans(poly, 1);
                    }
                    
                    addPrim(&ot[otz], poly);
                    nextpri += sizeof(POLY_G3);
                }
            } else {
                // Flat shaded triangle (F3)
                POLY_F3 *poly = (POLY_F3 *)nextpri;
                setPolyF3(poly);
                
                otz = RotAverage3(
                    &verts[v0], &verts[v1], &verts[v2],
                    (long*)&poly->x0, (long*)&poly->x1, (long*)&poly->x2,
                    &p, &flg
                );
                
                if (otz > 0 && otz < ot_length) {
                    // Apply lighting or set color
                    if (is_unlit) {
                        // Unlit: use vertex color directly
                        if (has_vcolors) {
                            setRGB0(poly, model->vertex_colors[v0].r, model->vertex_colors[v0].g, model->vertex_colors[v0].b);
                        } else {
                            setRGB0(poly, 128, 128, 128);
                        }
                    } else {
                        // Lit: calculate lighting with face normal
                        CVECTOR base_color = has_vcolors ? model->vertex_colors[v0] : (CVECTOR){128, 128, 128, 0};
                        CVECTOR col;
                        NormalColorCol(&model->normals[v0], &base_color, &col);
                        
                        // Apply specular if enabled
                        if ((flags & MAT_FLAG_SPECULAR) && model->specular) {
                            applySpecular(&col, &model->normals[v0], model->specular[i]);
                        }
                        
                        // Apply metallic if enabled
                        if ((flags & MAT_FLAG_METALLIC) && model->metallic) {
                            applyMetallic(&col, &model->normals[v0], model->metallic[i]);
                        }
                        
                        setRGB0(poly, col.r, col.g, col.b);
                    }
                    
                    if (has_alpha) {
                        setSemiTrans(poly, 1);
                    }
                    
                    addPrim(&ot[otz], poly);
                    nextpri += sizeof(POLY_F3);
                }
            }
        }
    }
    
    return nextpri;
}

//----------------------------------------------------------
// Render quads
//----------------------------------------------------------
static char* renderQuads(
    SVECTOR *verts,
    ModelData *model,
    char *nextpri,
    u_long *ot,
    int ot_length,
    u_short tpage,
    u_short clut
) {
    int i;
    long otz, p, flg;
    int face_index = model->tri_count;  // Quads start after triangles in material_flags
    
    for (i = 0; i < model->quad_count; i++, face_index++) {
        unsigned char flags = model->material_flags[face_index];
        int v0 = model->quad_faces[i][0];
        int v1 = model->quad_faces[i][1];
        int v2 = model->quad_faces[i][2];
        int v3 = model->quad_faces[i][3];
        
        // Choose primitive type based on flags
        int is_textured = flags & MAT_FLAG_TEXTURED;
        int is_smooth = flags & MAT_FLAG_SMOOTH;
        int has_alpha = flags & MAT_FLAG_ALPHA;
        int has_vcolors = flags & MAT_FLAG_VERTEX_COLOR;
        int is_unlit = flags & MAT_FLAG_UNLIT;
        
        if (is_textured) {
            if (is_smooth) {
                // Smooth textured quad (GT4)
                POLY_GT4 *poly = (POLY_GT4 *)nextpri;
                setPolyGT4(poly);
                
                otz = RotAverage4(
                    &verts[v0], &verts[v1], &verts[v2], &verts[v3],
                    (long*)&poly->x0, (long*)&poly->x1,
                    (long*)&poly->x2, (long*)&poly->x3,
                    &p, &flg
                );
                
                if (otz > 0 && otz < ot_length) {
                    int uv0 = model->quad_uvs[i][0];
                    int uv1 = model->quad_uvs[i][1];
                    int uv2 = model->quad_uvs[i][2];
                    int uv3 = model->quad_uvs[i][3];
                    
                    setUV4(poly,
                        model->uvs[uv0].vx, model->uvs[uv0].vy,
                        model->uvs[uv1].vx, model->uvs[uv1].vy,
                        model->uvs[uv2].vx, model->uvs[uv2].vy,
                        model->uvs[uv3].vx, model->uvs[uv3].vy
                    );
                    
                    poly->tpage = tpage;
                    poly->clut = clut;
                    
                    // Apply lighting or set vertex colors
                    if (is_unlit) {
                        // Unlit: use vertex colors directly
                        if (has_vcolors) {
                            setRGB0(poly, model->vertex_colors[v0].r, model->vertex_colors[v0].g, model->vertex_colors[v0].b);
                            setRGB1(poly, model->vertex_colors[v1].r, model->vertex_colors[v1].g, model->vertex_colors[v1].b);
                            setRGB2(poly, model->vertex_colors[v2].r, model->vertex_colors[v2].g, model->vertex_colors[v2].b);
                            setRGB3(poly, model->vertex_colors[v3].r, model->vertex_colors[v3].g, model->vertex_colors[v3].b);
                        } else {
                            setRGB0(poly, 128, 128, 128);
                            setRGB1(poly, 128, 128, 128);
                            setRGB2(poly, 128, 128, 128);
                            setRGB3(poly, 128, 128, 128);
                        }
                    } else {
                        // Lit: calculate lighting with normals
                        CVECTOR base_color = has_vcolors ? model->vertex_colors[v0] : (CVECTOR){128, 128, 128, 0};
                        CVECTOR col0, col1, col2, col3;
                        
                        // Calculate lighting for each vertex
                        NormalColorCol(&model->normals[v0], &base_color, &col0);
                        base_color = has_vcolors ? model->vertex_colors[v1] : (CVECTOR){128, 128, 128, 0};
                        NormalColorCol(&model->normals[v1], &base_color, &col1);
                        base_color = has_vcolors ? model->vertex_colors[v2] : (CVECTOR){128, 128, 128, 0};
                        NormalColorCol(&model->normals[v2], &base_color, &col2);
                        base_color = has_vcolors ? model->vertex_colors[v3] : (CVECTOR){128, 128, 128, 0};
                        NormalColorCol(&model->normals[v3], &base_color, &col3);
                        
                        // Apply specular if enabled
                        if ((flags & MAT_FLAG_SPECULAR) && model->specular) {
                            applySpecular(&col0, &model->normals[v0], model->specular[face_index]);
                            applySpecular(&col1, &model->normals[v1], model->specular[face_index]);
                            applySpecular(&col2, &model->normals[v2], model->specular[face_index]);
                            applySpecular(&col3, &model->normals[v3], model->specular[face_index]);
                        }
                        
                        // Apply metallic if enabled
                        if ((flags & MAT_FLAG_METALLIC) && model->metallic) {
                            applyMetallic(&col0, &model->normals[v0], model->metallic[face_index]);
                            applyMetallic(&col1, &model->normals[v1], model->metallic[face_index]);
                            applyMetallic(&col2, &model->normals[v2], model->metallic[face_index]);
                            applyMetallic(&col3, &model->normals[v3], model->metallic[face_index]);
                        }
                        
                        setRGB0(poly, col0.r, col0.g, col0.b);
                        setRGB1(poly, col1.r, col1.g, col1.b);
                        setRGB2(poly, col2.r, col2.g, col2.b);
                        setRGB3(poly, col3.r, col3.g, col3.b);
                    }
                    
                    if (has_alpha) {
                        setSemiTrans(poly, 1);
                    }
                    
                    addPrim(&ot[otz], poly);
                    nextpri += sizeof(POLY_GT4);
                }
            } else {
                // Flat textured quad (FT4)
                POLY_FT4 *poly = (POLY_FT4 *)nextpri;
                setPolyFT4(poly);
                
                otz = RotAverage4(
                    &verts[v0], &verts[v1], &verts[v2], &verts[v3],
                    (long*)&poly->x0, (long*)&poly->x1,
                    (long*)&poly->x2, (long*)&poly->x3,
                    &p, &flg
                );
                
                if (otz > 0 && otz < ot_length) {
                    int uv0 = model->quad_uvs[i][0];
                    int uv1 = model->quad_uvs[i][1];
                    int uv2 = model->quad_uvs[i][2];
                    int uv3 = model->quad_uvs[i][3];
                    
                    setUV4(poly,
                        model->uvs[uv0].vx, model->uvs[uv0].vy,
                        model->uvs[uv1].vx, model->uvs[uv1].vy,
                        model->uvs[uv2].vx, model->uvs[uv2].vy,
                        model->uvs[uv3].vx, model->uvs[uv3].vy
                    );
                    
                    poly->tpage = tpage;
                    poly->clut = clut;
                    
                    // Apply lighting or set color
                    if (is_unlit) {
                        // Unlit: use vertex color directly
                        if (has_vcolors) {
                            setRGB0(poly, model->vertex_colors[v0].r, model->vertex_colors[v0].g, model->vertex_colors[v0].b);
                        } else {
                            setRGB0(poly, 128, 128, 128);
                        }
                    } else {
                        // Lit: calculate lighting with face normal (use first vertex normal for flat shading)
                        CVECTOR base_color = has_vcolors ? model->vertex_colors[v0] : (CVECTOR){128, 128, 128, 0};
                        CVECTOR col;
                        NormalColorCol(&model->normals[v0], &base_color, &col);
                        
                        // Apply specular if enabled
                        if ((flags & MAT_FLAG_SPECULAR) && model->specular) {
                            applySpecular(&col, &model->normals[v0], model->specular[face_index]);
                        }
                        
                        // Apply metallic if enabled
                        if ((flags & MAT_FLAG_METALLIC) && model->metallic) {
                            applyMetallic(&col, &model->normals[v0], model->metallic[face_index]);
                        }
                        
                        setRGB0(poly, col.r, col.g, col.b);
                    }
                    
                    if (has_alpha) {
                        setSemiTrans(poly, 1);
                    }
                    
                    addPrim(&ot[otz], poly);
                    nextpri += sizeof(POLY_FT4);
                }
            }
        } else {
            if (is_smooth) {
                // Smooth shaded quad (G4)
                POLY_G4 *poly = (POLY_G4 *)nextpri;
                setPolyG4(poly);
                
                otz = RotAverage4(
                    &verts[v0], &verts[v1], &verts[v2], &verts[v3],
                    (long*)&poly->x0, (long*)&poly->x1,
                    (long*)&poly->x2, (long*)&poly->x3,
                    &p, &flg
                );
                
                if (otz > 0 && otz < ot_length) {
                    // Apply lighting or set vertex colors
                    if (is_unlit) {
                        // Unlit: use vertex colors directly
                        if (has_vcolors) {
                            setRGB0(poly, model->vertex_colors[v0].r, model->vertex_colors[v0].g, model->vertex_colors[v0].b);
                            setRGB1(poly, model->vertex_colors[v1].r, model->vertex_colors[v1].g, model->vertex_colors[v1].b);
                            setRGB2(poly, model->vertex_colors[v2].r, model->vertex_colors[v2].g, model->vertex_colors[v2].b);
                            setRGB3(poly, model->vertex_colors[v3].r, model->vertex_colors[v3].g, model->vertex_colors[v3].b);
                        } else {
                            setRGB0(poly, 128, 128, 128);
                            setRGB1(poly, 128, 128, 128);
                            setRGB2(poly, 128, 128, 128);
                            setRGB3(poly, 128, 128, 128);
                        }
                    } else {
                        // Lit: calculate lighting with normals
                        CVECTOR base_color = has_vcolors ? model->vertex_colors[v0] : (CVECTOR){128, 128, 128, 0};
                        CVECTOR col0, col1, col2, col3;
                        
                        NormalColorCol(&model->normals[v0], &base_color, &col0);
                        base_color = has_vcolors ? model->vertex_colors[v1] : (CVECTOR){128, 128, 128, 0};
                        NormalColorCol(&model->normals[v1], &base_color, &col1);
                        base_color = has_vcolors ? model->vertex_colors[v2] : (CVECTOR){128, 128, 128, 0};
                        NormalColorCol(&model->normals[v2], &base_color, &col2);
                        base_color = has_vcolors ? model->vertex_colors[v3] : (CVECTOR){128, 128, 128, 0};
                        NormalColorCol(&model->normals[v3], &base_color, &col3);
                        
                        // Apply specular if enabled
                        if ((flags & MAT_FLAG_SPECULAR) && model->specular) {
                            applySpecular(&col0, &model->normals[v0], model->specular[face_index]);
                            applySpecular(&col1, &model->normals[v1], model->specular[face_index]);
                            applySpecular(&col2, &model->normals[v2], model->specular[face_index]);
                            applySpecular(&col3, &model->normals[v3], model->specular[face_index]);
                        }
                        
                        // Apply metallic if enabled
                        if ((flags & MAT_FLAG_METALLIC) && model->metallic) {
                            applyMetallic(&col0, &model->normals[v0], model->metallic[face_index]);
                            applyMetallic(&col1, &model->normals[v1], model->metallic[face_index]);
                            applyMetallic(&col2, &model->normals[v2], model->metallic[face_index]);
                            applyMetallic(&col3, &model->normals[v3], model->metallic[face_index]);
                        }
                        
                        setRGB0(poly, col0.r, col0.g, col0.b);
                        setRGB1(poly, col1.r, col1.g, col1.b);
                        setRGB2(poly, col2.r, col2.g, col2.b);
                        setRGB3(poly, col3.r, col3.g, col3.b);
                    }
                    
                    if (has_alpha) {
                        setSemiTrans(poly, 1);
                    }
                    
                    addPrim(&ot[otz], poly);
                    nextpri += sizeof(POLY_G4);
                }
            } else {
                // Flat shaded quad (F4)
                POLY_F4 *poly = (POLY_F4 *)nextpri;
                setPolyF4(poly);
                
                otz = RotAverage4(
                    &verts[v0], &verts[v1], &verts[v2], &verts[v3],
                    (long*)&poly->x0, (long*)&poly->x1,
                    (long*)&poly->x2, (long*)&poly->x3,
                    &p, &flg
                );
                
                if (otz > 0 && otz < ot_length) {
                    // Apply lighting or set color
                    if (is_unlit) {
                        // Unlit: use vertex color directly
                        if (has_vcolors) {
                            setRGB0(poly, model->vertex_colors[v0].r, model->vertex_colors[v0].g, model->vertex_colors[v0].b);
                        } else {
                            setRGB0(poly, 128, 128, 128);
                        }
                    } else {
                        // Lit: calculate lighting with face normal
                        CVECTOR base_color = has_vcolors ? model->vertex_colors[v0] : (CVECTOR){128, 128, 128, 0};
                        CVECTOR col;
                        NormalColorCol(&model->normals[v0], &base_color, &col);
                        
                        // Apply specular if enabled
                        if ((flags & MAT_FLAG_SPECULAR) && model->specular) {
                            applySpecular(&col, &model->normals[v0], model->specular[face_index]);
                        }
                        
                        // Apply metallic if enabled
                        if ((flags & MAT_FLAG_METALLIC) && model->metallic) {
                            applyMetallic(&col, &model->normals[v0], model->metallic[face_index]);
                        }
                        
                        setRGB0(poly, col.r, col.g, col.b);
                    }
                    
                    if (has_alpha) {
                        setSemiTrans(poly, 1);
                    }
                    
                    addPrim(&ot[otz], poly);
                    nextpri += sizeof(POLY_F4);
                }
            }
        }
    }
    
    return nextpri;
}

//----------------------------------------------------------
// Render the model
//----------------------------------------------------------
char* renderModel(SVECTOR *verts, ModelData *model, char *nextpri, u_long *ot, int ot_length, u_short tpage, u_short clut) {
    // Only set mask bit control if this model has cutout transparency
    if (modelNeedsCutout(model)) {
        // FromSource mode (0): GPU reads bit 15 from texture/CLUT for each pixel
        // If bit 15 = 0: pixel is skipped (transparent)
        // If bit 15 = 1: pixel is drawn (opaque)
        DR_STP *stp = (DR_STP *)nextpri;
        SetDrawStp(stp, 0);
        addPrim(&ot[ot_length - 1], stp);
        nextpri += sizeof(DR_STP);
    }
    
    // Render all primitives
    nextpri = renderTriangles(verts, model, nextpri, ot, ot_length, tpage, clut);
    nextpri = renderQuads(verts, model, nextpri, ot, ot_length, tpage, clut);
    return nextpri;
}

