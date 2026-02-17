bl_info = {
    "name": "PlayStation 1 Exporter",
    "author": "parkerallan",
    "version": (1, 2, 2),
    "blender": (4, 0, 0),
    "location": "File > Export > PlayStation 1 (.h)",
    "description": "Exports models and animations to PlayStation 1 C header files with SVECTOR format",
    "warning": "",
    "doc_url": "",
    "category": "Import-Export",
}

import bpy
import bmesh
import mathutils
import os
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty, IntProperty, EnumProperty
from bpy.types import Operator

# PlayStation 1 fixed-point scale factor (standard for PS1 hardware)
PS1_SCALE_FACTOR = 3072

def show_message(message="", title="Message", icon='INFO'):
    """Show a message box to the user"""
    def draw(self, context):
        self.layout.label(text=message)
    bpy.context.window_manager.popup_menu(draw, title=title, icon=icon)

def mesh_triangulate(obj):
    """Triangulate faces with more than 4 vertices, then convert back to quads where possible"""
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    
    # Try to convert back to quads (PS1 supports both tris and quads)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.tris_convert_to_quads()
    bpy.ops.object.mode_set(mode='OBJECT')

def convert_coordinate(coord, convert_to_z_up):
    """Convert Blender Y-up coordinates to PS1 Z-up: (x, -z, y)"""
    if convert_to_z_up:
        return (coord[0], -coord[2], coord[1])
    return coord

def get_vertex_colors(mesh):
    """Extract vertex colors from mesh - Blender 4.0+ compatible
    Returns: (colors_list, domain_type)
    """
    vert_colors = []
    color_domain = None
    
    color_attrs = []
    
    # Try color_attributes (Blender 4.0+)
    if hasattr(mesh, 'color_attributes') and mesh.color_attributes:
        color_attrs = list(mesh.color_attributes)
    
    # Try attributes with color data type
    if not color_attrs and hasattr(mesh, 'attributes') and mesh.attributes:
        for attr in mesh.attributes:
            if hasattr(attr, 'data_type') and attr.data_type in ['BYTE_COLOR', 'FLOAT_COLOR']:
                color_attrs.append(attr)
    
    # Extract first color attribute
    for attr in color_attrs:
        color_domain = attr.domain
        
        for d in attr.data:
            color = None
            if hasattr(d, 'color_srgb'):
                color = mathutils.Color((d.color_srgb[0], d.color_srgb[1], d.color_srgb[2]))
            elif hasattr(d, 'color'):
                color = mathutils.Color((d.color[0], d.color[1], d.color[2]))
            elif hasattr(d, 'vector'):
                color = mathutils.Color((d.vector[0], d.vector[1], d.vector[2]))
            
            if color:
                vert_colors.append(color)
        
        if vert_colors:
            break
    
    return vert_colors, color_domain

def get_poly_color(poly, vert_colors, color_domain, mesh):
    """Get color for a specific polygon"""
    if not vert_colors or color_domain is None:
        return None
    
    colors = []
    
    if color_domain == 'CORNER':
        for i in range(len(poly.vertices)):
            idx = poly.loop_start + i
            if idx < len(vert_colors):
                colors.append(vert_colors[idx])
    elif color_domain == 'VERTEX':
        for vert_idx in poly.vertices:
            if vert_idx < len(vert_colors):
                colors.append(vert_colors[vert_idx])
    elif color_domain == 'FACE':
        if poly.index < len(vert_colors):
            colors.append(vert_colors[poly.index])
    
    return colors

def is_solid_white(poly, vert_colors, color_domain, mesh):
    """Check if polygon is solid white"""
    colors = get_poly_color(poly, vert_colors, color_domain, mesh)
    
    if not colors or len(colors) == 0:
        return True
    
    for c in colors:
        if c.r < 0.99 or c.g < 0.99 or c.b < 0.99:
            return False
    return True

def detect_material_properties(mesh, poly, enable_unlit, enable_specular, enable_metallic):
    """Detect material properties: lit/unlit, textured, vertex colors, smooth/flat, alpha, specular, metallic"""
    result = {
        'is_lit': True,
        'is_textured': False,
        'has_vertex_colors': False,
        'is_smooth': poly.use_smooth,
        'texture_name': None,
        'texture_index': -1,  # Index into texture list, -1 = no texture
        'texture_width': 255,  # Default if no texture
        'texture_height': 255,
        'is_emission': False,
        'has_alpha': False,  # Texture has alpha channel
        'has_cutout': False,  # Sharp cutout transparency using mask bit
        'specular': 0.5,  # Default specular value (0-1.0)
        'metallic': 0.0  # Default metallic value
    }
    
    # Check material
    if poly.material_index < len(mesh.materials):
        mat = mesh.materials[poly.material_index]
        
        if mat and mat.node_tree:
            # Check for Principled BSDF to get specular and metallic values
            for node in mat.node_tree.nodes:
                if node.type == 'BSDF_PRINCIPLED':
                    # Get specular value if enabled (0.0 to 1.0 range in Blender 4.0)
                    if enable_specular:
                        # Blender 4.0 uses "Specular IOR Level"
                        for input_name in ['Specular IOR Level', 'Specular', 'Specular IOR']:
                            if input_name in node.inputs:
                                try:
                                    value = node.inputs[input_name].default_value
                                    result['specular'] = float(value)
                                    break
                                except Exception as e:
                                    pass
                    
                    # Get metallic value if enabled
                    if enable_metallic:
                        if 'Metallic' in node.inputs:
                            try:
                                result['metallic'] = float(node.inputs['Metallic'].default_value)
                            except:
                                pass
                
                # Check for texture and get dimensions
                if node.type in ['TEX_IMAGE', 'ShaderNodeTexImage']:
                    if node.image:
                        result['is_textured'] = True
                        result['texture_name'] = node.image.name
                        # Get texture dimensions
                        result['texture_width'] = node.image.size[0]
                        result['texture_height'] = node.image.size[1]
                        
                        # Check if texture has alpha channel
                        if node.image.channels == 4 or (hasattr(node.image, 'alpha_mode') and node.image.alpha_mode != 'NONE'):
                            result['has_alpha'] = True
    
    # Check vertex colors (now returns both colors and domain)
    vert_colors, color_domain = get_vertex_colors(mesh)
    if vert_colors and color_domain and not is_solid_white(poly, vert_colors, color_domain, mesh):
        result['has_vertex_colors'] = True
    
    # Only set unlit if enable_unlit is checked
    if enable_unlit:
        result['is_lit'] = False
    
    return result

class ExportPS1(Operator, ExportHelper):
    """Export to PlayStation 1 C header format"""
    bl_idname = "export_scene.ps1"
    bl_label = "Export PS1"
    
    filename_ext = ".h"
    
    filter_glob: StringProperty(
        default="*.h",
        options={'HIDDEN'},
        maxlen=255,
    )
    
    convert_coords: BoolProperty(
        name="Convert to Z-up",
        description="Convert Blender Y-up coordinates to PS1 Z-up (x, -z, y)",
        default=True
    )
    
    enable_unlit: BoolProperty(
        name="Enable Unlit",
        description="Export all polygons as unlit (disable lighting calculations)",
        default=False
    )
    
    enable_semi_transparency: BoolProperty(
        name="Enable Semi-Transparency",
        description="Enable semi-transparency for materials with alpha channels",
        default=False
    )
    
    enable_cutout_transparency: BoolProperty(
        name="Enable Cutout Transparency",
        description="Enable sharp cutout transparency using mask bit (bit 15) for textures with alpha",
        default=False
    )
    
    enable_specular: BoolProperty(
        name="Enable Specular",
        description="Export specular IOR values from Principled BSDF for specular highlights",
        default=False
    )
    
    enable_metallic: BoolProperty(
        name="Enable Metallic",
        description="Export metallic values from Principled BSDF for metallic materials",
        default=False
    )
    
    export_animations: BoolProperty(
        name="Export Animations",
        description="Export all actions as separate animation header files",
        default=True
    )
    
    header_type: EnumProperty(
        name="Header Type",
        description="Choose the header file format",
        items=[
            ('PSYQ', "PSyQ (C)", "Export for PSyQ SDK (C) with libgte.h"),
            ('PSYQO', "PSyQo (C++)", "Export for PSyQo SDK (C++) with stdint.h"),
        ],
        default='PSYQ'
    )
    
    def draw(self, context):
        layout = self.layout
        layout.label(text="Export options:")
        layout.prop(self, "convert_coords")
        layout.prop(self, "enable_unlit")
        layout.prop(self, "enable_semi_transparency")
        layout.prop(self, "enable_cutout_transparency")
        layout.prop(self, "enable_specular")
        layout.prop(self, "enable_metallic")
        layout.prop(self, "export_animations")
        layout.label(text="Header Type:")
        layout.prop(self, "header_type", text="")
    
    def execute(self, context):
        return self.export_ps1(context)
    
    def export_ps1(self, context):
        """Main export function"""
        blend_filepath = bpy.data.filepath
        if not blend_filepath:
            show_message("Please save your .blend file first!", "Error", 'ERROR')
            return {'CANCELLED'}
        
        base_name = os.path.splitext(os.path.basename(blend_filepath))[0]
        export_dir = os.path.dirname(self.filepath)
        
        mesh_objects = sorted(
            [obj for obj in bpy.data.objects if obj.type == 'MESH'],
            key=lambda obj: obj.name
        )
        
        if not mesh_objects:
            show_message("No mesh objects found in scene!", "Error", 'ERROR')
            return {'CANCELLED'}
        
        needs_triangulation = False
        for obj in mesh_objects:
            for poly in obj.data.polygons:
                if len(poly.vertices) > 4:
                    needs_triangulation = True
                    mesh_triangulate(obj)
                    break
        
        if needs_triangulation:
            show_message("Some faces had more than 4 vertices and were triangulated.", "Info", 'INFO')
        
        model_filepath = os.path.join(export_dir, base_name + ".h")
        self.export_model(mesh_objects, model_filepath, base_name)
        
        if self.export_animations and len(bpy.data.actions) > 0:
            self.export_all_animations(mesh_objects, export_dir, base_name)
        
        show_message(f"Export complete! Files saved to {export_dir}", "Success", 'INFO')
        return {'FINISHED'}
    
    def export_model(self, mesh_objects, filepath, base_name):
        """Export main model geometry to C header file"""
        all_vertices = []
        all_normals = []  # Vertex normals for lighting
        all_uvs = []
        all_faces = []
        all_materials = []
        all_vertex_colors = []
        texture_names = []  # Ordered list of unique texture names
        texture_name_to_idx = {}  # Maps texture name to index
        has_any_vertex_colors = False
        
        vertex_offset = 0
        uv_offset = 0
        
        # Combine all mesh objects
        for mesh_index, obj in enumerate(mesh_objects):
            mesh = obj.data
            
            # Get the world matrix to apply object transforms (location, rotation, scale)
            world_matrix = obj.matrix_world
            
            # Get vertex colors and domain for this mesh
            vert_colors, color_domain = get_vertex_colors(mesh)
            
            if vert_colors and color_domain:
                has_any_vertex_colors = True
            
            # Extract vertices (apply world transform, then scale to PS1 fixed-point integers)
            for vert_idx, vert in enumerate(mesh.vertices):
                # Apply object's world transform (includes scale, rotation, location)
                world_coord = world_matrix @ vert.co
                coord = convert_coordinate(world_coord, self.convert_coords)
                all_vertices.append({
                    'x': int(coord[0] * PS1_SCALE_FACTOR),
                    'y': int(coord[1] * PS1_SCALE_FACTOR),
                    'z': int(coord[2] * PS1_SCALE_FACTOR)
                })
                
                # Extract vertex normal (apply rotation, normalize to ONE=4096)
                world_normal = (world_matrix.to_3x3() @ vert.normal).normalized()
                normal_coord = convert_coordinate(world_normal, self.convert_coords)
                all_normals.append({
                    'x': int(normal_coord[0] * 4096),
                    'y': int(normal_coord[1] * 4096),
                    'z': int(normal_coord[2] * 4096)
                })
                
                # Extract vertex color for this vertex (if domain is VERTEX)
                if vert_colors and color_domain == 'VERTEX':
                    if vert_idx < len(vert_colors):
                        color = vert_colors[vert_idx]
                        all_vertex_colors.append({
                            'r': int(color.r * 255),
                            'g': int(color.g * 255),
                            'b': int(color.b * 255)
                        })
                    else:
                        # Default to white if index out of range
                        all_vertex_colors.append({'r': 255, 'g': 255, 'b': 255})
            
            # If color domain is CORNER, we need to extract per-loop colors
            if vert_colors and color_domain == 'CORNER':
                for loop_idx in range(len(mesh.loops)):
                    if loop_idx < len(vert_colors):
                        color = vert_colors[loop_idx]
                        all_vertex_colors.append({
                            'r': int(color.r * 255),
                            'g': int(color.g * 255),
                            'b': int(color.b * 255)
                        })
                    else:
                        all_vertex_colors.append({'r': 255, 'g': 255, 'b': 255})
            
            # Get UV layer reference
            uv_layer = mesh.uv_layers.active.data if mesh.uv_layers.active else None
            
            # Extract faces and materials
            for poly in mesh.polygons:
                mat_props = detect_material_properties(mesh, poly, self.enable_unlit, self.enable_specular, self.enable_metallic)
                
                # Assign texture index
                if mat_props['texture_name']:
                    tex_name = mat_props['texture_name']
                    if tex_name not in texture_name_to_idx:
                        texture_name_to_idx[tex_name] = len(texture_names)
                        texture_names.append(tex_name)
                    mat_props['texture_index'] = texture_name_to_idx[tex_name]
                
                # Get texture dimensions for THIS polygon (from its material)
                tex_width = mat_props['texture_width']
                tex_height = mat_props['texture_height']
                
                # Get vertex indices with proper winding order
                if len(poly.vertices) == 3:
                    # Triangle: [0, 2, 1]
                    vert_indices = [
                        mesh.loops[poly.loop_start].vertex_index + vertex_offset,
                        mesh.loops[poly.loop_start + 2].vertex_index + vertex_offset,
                        mesh.loops[poly.loop_start + 1].vertex_index + vertex_offset
                    ]
                    
                    # Extract UVs for this triangle with THIS polygon's texture dimensions
                    uv_indices = [0, 0, 0]
                    if uv_layer:
                        # UV 0
                        uv = uv_layer[poly.loop_start].uv
                        all_uvs.append({
                            'u': round(uv.x * tex_width),
                            'v': round(tex_height - (uv.y * tex_height))
                        })
                        # UV 2
                        uv = uv_layer[poly.loop_start + 2].uv
                        all_uvs.append({
                            'u': round(uv.x * tex_width),
                            'v': round(tex_height - (uv.y * tex_height))
                        })
                        # UV 1
                        uv = uv_layer[poly.loop_start + 1].uv
                        all_uvs.append({
                            'u': round(uv.x * tex_width),
                            'v': round(tex_height - (uv.y * tex_height))
                        })
                        uv_indices = [uv_offset, uv_offset + 1, uv_offset + 2]
                        uv_offset += 3
                    
                elif len(poly.vertices) == 4:
                    # Quad: [3, 2, 0, 1]
                    vert_indices = [
                        mesh.loops[poly.loop_start + 3].vertex_index + vertex_offset,
                        mesh.loops[poly.loop_start + 2].vertex_index + vertex_offset,
                        mesh.loops[poly.loop_start].vertex_index + vertex_offset,
                        mesh.loops[poly.loop_start + 1].vertex_index + vertex_offset
                    ]
                    
                    # Extract UVs for this quad with THIS polygon's texture dimensions
                    uv_indices = [0, 0, 0, 0]
                    if uv_layer:
                        # UV 3
                        uv = uv_layer[poly.loop_start + 3].uv
                        all_uvs.append({
                            'u': round(uv.x * tex_width),
                            'v': round(tex_height - (uv.y * tex_height))
                        })
                        # UV 2
                        uv = uv_layer[poly.loop_start + 2].uv
                        all_uvs.append({
                            'u': round(uv.x * tex_width),
                            'v': round(tex_height - (uv.y * tex_height))
                        })
                        # UV 0
                        uv = uv_layer[poly.loop_start].uv
                        all_uvs.append({
                            'u': round(uv.x * tex_width),
                            'v': round(tex_height - (uv.y * tex_height))
                        })
                        # UV 1
                        uv = uv_layer[poly.loop_start + 1].uv
                        all_uvs.append({
                            'u': round(uv.x * tex_width),
                            'v': round(tex_height - (uv.y * tex_height))
                        })
                        uv_indices = [uv_offset, uv_offset + 1, uv_offset + 2, uv_offset + 3]
                        uv_offset += 4
                else:
                    continue
                
                all_faces.append({
                    'vertices': vert_indices,
                    'uvs': uv_indices,
                    'is_tri': len(poly.vertices) == 3,
                    'mesh_id': mesh_index,
                    'mesh_name': obj.name
                })
                all_materials.append(mat_props)
            
            vertex_offset += len(mesh.vertices)
        
        # Write C header file
        self.write_header_file(filepath, base_name, all_vertices, all_normals, all_uvs, all_faces, all_materials, texture_names, all_vertex_colors, has_any_vertex_colors, self.enable_semi_transparency, self.enable_cutout_transparency)
    
    def write_header_file(self, filepath, base_name, vertices, normals, uvs, faces, materials, texture_names, vertex_colors, has_vertex_colors, enable_semi_transparency, enable_cutout_transparency):
        """Write C header file"""
        guard_name = base_name.upper().replace('-', '_').replace(' ', '_')
        prefix = base_name.lower().replace('-', '_').replace(' ', '_')
        prefix_upper = prefix.upper()
        
        # Choose includes and type definitions based on header type
        if self.header_type == 'PSYQO':
            includes = """#include <stdint.h>

#ifndef SVECTOR_DEFINED
#define SVECTOR_DEFINED
typedef struct {
    int16_t vx, vy, vz;
} SVECTOR;
#endif"""
        else:  # PSYQ
            includes = """#include <sys/types.h>
#include <libgte.h>"""
        
        tri_count = sum(1 for f in faces if f['is_tri'])
        quad_count = len(faces) - tri_count
        
        content = f"""// PlayStation 1 Model Export
// Generated by PS1 Exporter for Blender 4.0
// Model: {base_name}
// Coordinate System: {'Z-up (PS1)' if self.convert_coords else 'Y-up (Blender)'}

#ifndef {guard_name}_H
#define {guard_name}_H

{includes}

#define {prefix_upper}_VERTICES_COUNT {len(vertices)}
#define {prefix_upper}_UVS_COUNT {len(uvs)}
#define {prefix_upper}_FACES_COUNT {len(faces)}
#define {prefix_upper}_TRI_COUNT {tri_count}
#define {prefix_upper}_QUAD_COUNT {quad_count}
#define {prefix_upper}_PS1_SCALE {PS1_SCALE_FACTOR}

// Vertices (fixed-point, scaled by {PS1_SCALE_FACTOR})
SVECTOR {prefix}_vertices[{prefix_upper}_VERTICES_COUNT] = {{
"""
        
        for v in vertices:
            content += f"    {{ {v['x']}, {v['y']}, {v['z']} }},\n"
        
        content += "};\n\n"
        
        # Normals (normalized, scaled by 4096 = ONE)
        content += f"// Normals (for lighting calculations)\n"
        content += f"SVECTOR {prefix}_normals[{prefix_upper}_VERTICES_COUNT] = {{\n"
        
        for n in normals:
            content += f"    {{ {n['x']}, {n['y']}, {n['z']} }},\n"
        
        content += "};\n\n"
        
        # Texture defines (prefixed to avoid conflicts)
        if texture_names:
            content += "// Texture references\n"
            content += f"#define {prefix_upper}_TEXTURE_COUNT {len(texture_names)}\n"
            for i, tex_name in enumerate(sorted(texture_names)):
                # Clean texture name for C identifier (remove extension, replace invalid chars)
                clean_name = os.path.splitext(tex_name)[0].upper().replace(' ', '_').replace('-', '_').replace('.', '_')
                content += f"#define {prefix_upper}_TEXTURE_{i}_NAME \"{tex_name}\"\n"
                content += f"#define {prefix_upper}_TEXTURE_{clean_name} {i}\n"
            content += "\n"
        
        # UVs
        if uvs:
            content += f"// UV Coordinates\n"
            content += f"SVECTOR {prefix}_uvs[{prefix_upper}_UVS_COUNT] = {{\n"
            for uv in uvs:
                content += f"    {{ {uv['u']}, {uv['v']}, 0 }},\n"
            content += "};\n\n"
        
        content += "// Faces\n"
        tri_count = sum(1 for f in faces if f['is_tri'])
        quad_count = len(faces) - tri_count
        
        if tri_count > 0:
            content += f"int {prefix}_tri_faces[{tri_count}][3] = {{\n"
            for face in faces:
                if face['is_tri']:
                    v = face['vertices']
                    content += f"    {{ {v[0]}, {v[1]}, {v[2]} }},\n"
            content += "};\n\n"
            
            if uvs:
                content += f"int {prefix}_tri_uvs[{tri_count}][3] = {{\n"
                for face in faces:
                    if face['is_tri']:
                        uv = face['uvs']
                        content += f"    {{ {uv[0]}, {uv[1]}, {uv[2]} }},\n"
                content += "};\n\n"
        else:
            # Export dummy arrays for models with no triangles
            content += f"int {prefix}_tri_faces[1][3] = {{ {{0, 0, 0}} }};\n"
            if uvs:
                content += f"int {prefix}_tri_uvs[1][3] = {{ {{0, 0, 0}} }};\n"
            content += "\n"
        
        if quad_count > 0:
            content += f"int {prefix}_quad_faces[{quad_count}][4] = {{\n"
            for face in faces:
                if not face['is_tri']:
                    v = face['vertices']
                    content += f"    {{ {v[0]}, {v[1]}, {v[2]}, {v[3]} }},\n"
            content += "};\n\n"
            
            if uvs:
                content += f"int {prefix}_quad_uvs[{quad_count}][4] = {{\n"
                for face in faces:
                    if not face['is_tri']:
                        uv = face['uvs']
                        content += f"    {{ {uv[0]}, {uv[1]}, {uv[2]}, {uv[3]} }},\n"
                content += "};\n\n"
        else:
            # Export dummy arrays for models with no quads
            content += f"int {prefix}_quad_faces[1][4] = {{ {{0, 0, 0, 0}} }};\n"
            if uvs:
                content += f"int {prefix}_quad_uvs[1][4] = {{ {{0, 0, 0, 0}} }};\n"
            content += "\n"
        
        # Material flags: Bit 0: unlit, Bit 1: textured, Bit 2: smooth, Bit 3: vertex_color, Bit 4: alpha, Bit 5: cutout, Bit 6: specular, Bit 7: metallic
        content += f"unsigned char {prefix}_material_flags[{prefix_upper}_FACES_COUNT] = {{\n"
        
        for mat in materials:
            flags = 0
            if not mat['is_lit']:
                flags |= (1 << 0)
            if mat['is_textured']:
                flags |= (1 << 1)
            if mat['is_smooth']:
                flags |= (1 << 2)
            if mat['has_vertex_colors']:
                flags |= (1 << 3)
            if mat['has_alpha'] and enable_semi_transparency:
                flags |= (1 << 4)
            if mat['has_alpha'] and enable_cutout_transparency:
                flags |= (1 << 5)
            if self.enable_specular:
                flags |= (1 << 6)
            if self.enable_metallic:
                flags |= (1 << 7)
            content += f"    0b{flags:08b},  // "
            desc = []
            desc.append("unlit" if not mat['is_lit'] else "lit")
            if mat['is_textured']:
                desc.append("textured")
            desc.append("smooth" if mat['is_smooth'] else "flat")
            if mat['has_vertex_colors']:
                desc.append("vertex-colored")
            if mat['has_alpha']:
                if enable_semi_transparency:
                    desc.append("alpha")
                if enable_cutout_transparency:
                    desc.append("cutout")
            if self.enable_specular:
                desc.append(f"specular({mat.get('specular', 0.5):.2f})")
            if self.enable_metallic:
                desc.append(f"metallic({mat.get('metallic', 0.0):.2f})")
            content += ", ".join(desc) + "\n"
        
        content += "};\n\n"
                # Export specular values if enabled
        if self.enable_specular:
            content += f"// Specular values (0-255, where 255 = maximum specular)\n"
            content += f"unsigned char {prefix}_specular[{prefix_upper}_FACES_COUNT] = {{\n"
            for mat in materials:
                specular_value = int(mat.get('specular', 0.5) * 255)
                content += f"    {specular_value},  // {mat.get('specular', 0.5):.2f}\n"
            content += "};\n\n"
        
        # Export metallic values if enabled
        if self.enable_metallic:
            content += f"// Metallic values (0-255, where 255 = fully metallic)\n"
            content += f"unsigned char {prefix}_metallic[{prefix_upper}_FACES_COUNT] = {{\n"
            for mat in materials:
                metallic_value = int(mat.get('metallic', 0.0) * 255)
                content += f"    {metallic_value},  // {mat.get('metallic', 0.0):.2f}\n"
            content += "};\n\n"
        
        # Export mesh IDs for per-mesh visibility control
        content += f"// Mesh IDs (for per-mesh visibility)\n"
        content += f"unsigned char {prefix}_mesh_ids[{prefix_upper}_FACES_COUNT] = {{\n"
        # Write triangle mesh IDs first
        for face in faces:
            if face['is_tri']:
                mesh_id = face.get('mesh_id', 0)
                mesh_name = face.get('mesh_name', 'unknown')
                content += f"    {mesh_id},  // {mesh_name}\n"
        # Then write quad mesh IDs
        for face in faces:
            if not face['is_tri']:
                mesh_id = face.get('mesh_id', 0)
                mesh_name = face.get('mesh_name', 'unknown')
                content += f"    {mesh_id},  // {mesh_name}\n"
        content += "};\n\n"
        
        # Always export vertex_colors array (even if empty) so code compiles
        content += f"// Vertex Colors\n"
        if has_vertex_colors and vertex_colors:
            content += f"#define {prefix_upper}_VERTEX_COLORS_COUNT {len(vertex_colors)}\n"
            content += f"CVECTOR {prefix}_vertex_colors[{prefix_upper}_VERTEX_COLORS_COUNT] = {{\n"
            for vc in vertex_colors:
                content += f"    {{ {vc['r']}, {vc['g']}, {vc['b']}, 0 }},\n"
            content += "};\n\n"
        else:
            # Export empty array for models without vertex colors
            content += f"#define {prefix_upper}_VERTEX_COLORS_COUNT 1\n"
            content += f"CVECTOR {prefix}_vertex_colors[{prefix_upper}_VERTEX_COLORS_COUNT] = {{\n"
            content += "    { 128, 128, 128, 0 }  // Default gray\n"
            content += "};\n\n"
        
        content += "#endif\n"
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
    
    def export_all_animations(self, mesh_objects, export_dir, base_name):
        """Export animations"""
        if not bpy.data.actions:
            return
        
        original_frame = bpy.context.scene.frame_current
        
        # Find armature objects that control the mesh
        armature_objects = []
        for obj in mesh_objects:
            for modifier in obj.modifiers:
                if modifier.type == 'ARMATURE' and modifier.object:
                    if modifier.object not in armature_objects:
                        armature_objects.append(modifier.object)
        
        # Store original actions for all animated objects
        original_actions = {}
        for obj in mesh_objects:
            if obj.animation_data:
                original_actions[obj.name] = obj.animation_data.action
        for armature in armature_objects:
            if armature.animation_data:
                original_actions[armature.name] = armature.animation_data.action
        
        for action in bpy.data.actions:
            action_name = action.name.replace(' ', '_').replace('-', '_')
            anim_filepath = os.path.join(export_dir, f"{base_name}-{action_name}.h")
            self.export_animation(mesh_objects, armature_objects, action, anim_filepath, base_name, action_name)
        
        bpy.context.scene.frame_set(original_frame)
        for obj in mesh_objects:
            if obj.name in original_actions and obj.animation_data:
                obj.animation_data.action = original_actions[obj.name]
        for armature in armature_objects:
            if armature.name in original_actions and armature.animation_data:
                armature.animation_data.action = original_actions[armature.name]
    
    def export_animation(self, mesh_objects, armature_objects, action, filepath, base_name, action_name):
        """Export animation"""
        guard_name = f"{base_name}_{action_name}".upper().replace('-', '_').replace(' ', '_')
        
        # Set the action on armature objects (where animations actually live)
        for armature in armature_objects:
            if armature.animation_data is None:
                armature.animation_data_create()
            armature.animation_data.action = action
        
        # Also set on mesh objects if they have animation data
        for obj in mesh_objects:
            if obj.animation_data:
                obj.animation_data.action = action
        
        # Force depsgraph update to ensure action is applied
        bpy.context.view_layer.update()
        
        frame_start = int(action.frame_range[0])
        frame_end = int(action.frame_range[1])
        frame_count = frame_end - frame_start + 1
        
        animation_data = []
        
        for frame in range(frame_start, frame_end + 1):
            bpy.context.scene.frame_set(frame)
            # Force update after frame change
            bpy.context.view_layer.update()
            
            frame_vertices = []
            
            for obj in mesh_objects:
                # Get fresh depsgraph for this frame
                depsgraph = bpy.context.evaluated_depsgraph_get()
                depsgraph.update()
                eval_obj = obj.evaluated_get(depsgraph)
                mesh = eval_obj.to_mesh()
                
                # Get the world matrix to apply object transforms
                world_matrix = eval_obj.matrix_world
                
                for vert in mesh.vertices:
                    # Apply object's world transform (includes scale, rotation, location)
                    world_coord = world_matrix @ vert.co
                    coord = convert_coordinate(world_coord, self.convert_coords)
                    frame_vertices.append({
                        'x': int(coord[0] * PS1_SCALE_FACTOR),
                        'y': int(coord[1] * PS1_SCALE_FACTOR),
                        'z': int(coord[2] * PS1_SCALE_FACTOR)
                    })
                
                eval_obj.to_mesh_clear()
            
            animation_data.append(frame_vertices)
        
        # Choose includes and type definitions based on header type
        if self.header_type == 'PSYQO':
            includes = """#include <stdint.h>

#ifndef SVECTOR_DEFINED
#define SVECTOR_DEFINED
typedef struct {
    int16_t vx, vy, vz;
} SVECTOR;
#endif"""
        else:  # PSYQ
            includes = """#include <sys/types.h>
#include <libgte.h>"""
        
        content = f"""// PlayStation 1 Animation Export
// Model: {base_name}
// Animation: {action_name}
// Frames: {frame_count}

#ifndef {guard_name}_H
#define {guard_name}_H

{includes}

#define {action_name.upper()}_FRAMES_COUNT {frame_count}
#define {action_name.upper()}_VERTICES_COUNT {len(animation_data[0]) if animation_data else 0}

SVECTOR {action_name}_anim[{action_name.upper()}_FRAMES_COUNT][{action_name.upper()}_VERTICES_COUNT] = {{
"""
        
        for frame_idx, frame_verts in enumerate(animation_data):
            content += f"    {{ // Frame {frame_start + frame_idx}\n"
            for v in frame_verts:
                content += f"        {{ {v['x']}, {v['y']}, {v['z']} }},\n"
            content += "    },\n"
        
        content += "};\n\n#endif\n"
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)

def menu_func_export(self, context):
    self.layout.operator(ExportPS1.bl_idname, text="PlayStation 1 (.h)")

def register():
    bpy.utils.register_class(ExportPS1)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)

def unregister():
    bpy.utils.unregister_class(ExportPS1)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)

if __name__ == "__main__":
    register()
