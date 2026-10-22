import bpy

import bmesh
import mathutils as mu
import math
import sys

import subprocess
import sys
import os

from .util import str2floats, floats2str, ints2str, str2ints
from .sky_matrix import SkyMatrix
from .obj import get_thermal_props, ThermalObjectProperties

import bpy.props
from bpy.props import (
    FloatProperty,
    IntProperty,
    BoolProperty,
    PointerProperty,
    StringProperty,
)
from bpy.types import (
    Panel,
    Operator,
    PropertyGroup,
)

SKY_NAME = "sky"

def get_sky_obj():
    if SKY_NAME in bpy.data.objects.keys():
        return bpy.data.objects[SKY_NAME]
    else:
        return None

def update_sky_props(self, context):

    sky_obj = get_sky_obj()
    skyObjProbs: ThermalObjectProperties = sky_obj.thermal_props
    bpy.context.view_layer.objects.active = sky_obj

    skyObjProbs.h_is_sky = True

    skyObjProbs.kelvin = 1.0
    skyObjProbs.temperature_fixed = True

    skyObjProbs.diffuse_reflectance = 0.0
    skyObjProbs.specular_reflectance = 0.0

    skyObjProbs.emission = "NORMAL"

    skyObjProbs.thickness = 1.0
    skyObjProbs.density = 1.0

    skyObjProbs.heat_capacity = 1.0
    skyObjProbs.heat_conductivity = 0.0

    skyObjProbs.traceable = False

    skyObjProbs.optimize_mode = 0
    skyObjProbs.objective_target = 1.0
    skyObjProbs.objective_weight = 0.0

    skyObjProbs.constraint_mode = 0
    skyObjProbs.objective_weight = 0.0

    skyObjProbs.sky_values_str = self["sky_values"]
    skyObjProbs.sky_hours_str = self["sky_hours"]
    skyObjProbs.sky_hour = self["sky_hour"]

    bpy.ops.wm.set_custom_properties()


def update_hour(self, context):
    #print("update_hour", self, context, self["sky_hour"], self.start_hoy)
    self["sky_hour"] = self.start_hoy

def update_sky_geometry(self, context):
    self["dome_radius"] = max(self["dome_radius"], 10.0 * sys.float_info.epsilon)
    bpy.ops.wm.generate_sky_geometry_operator()
    bpy.ops.wm.update_sky_values_operator()

def set_sky_hour(self, value):
    miv = self.start_hoy
    mav = self.start_hoy + self.duration_hours - 1
    self["sky_hour"] = max(miv, min(mav, value))

def get_sky_hour(self):
    return self["sky_hour"]

def update_sky_values(self, context):
    update_sky_props(self, context)
    bpy.ops.wm.update_sky_values_operator()

def compute_sky_values(self, context):
    bpy.ops.wm.compute_sky_values_operator()


class ThermalSkyProperties(PropertyGroup):

    dome_radius: FloatProperty(
        name="Dome Radius",
        description="The radius of the sky dome.",
        default=1.0,
        min=10.0 * sys.float_info.epsilon,
        # set=dome_radius_setter,
        # get=int_prop_getter,
        update=update_sky_geometry,
    )  # type: ignore

    start_hoy: IntProperty(
        name="Start HOY",
        description="The start hour of the year.",
        default=0,
        min=0,
        update=update_hour,
        # update=compute_sky_values,
    )  # type: ignore

    duration_hours: IntProperty(
        name="Hours",
        description="Number of hours (starting from start HOY).",
        default=1,
        min=1,
        max=8760,
        update=update_hour,
        # update=compute_sky_values,
    )  # type: ignore

    visualization_mode: BoolProperty(
        name="Visualization Mode",
        default=False,
        description="Scales down quads for visualization.",
        update=update_sky_geometry,
    )  # type: ignore

    dependencies_valid: BoolProperty(
        name="dependencies valid",
        description="dependencies are valid",
        default=False,
    )  # type: ignore

    values_valid: BoolProperty(
        default=False,
    )  # type: ignore

    geom_pointer: PointerProperty(
        type=bpy.types.Mesh,
        name="Sky Mesh",
        description="Sky",
    )  # type: ignore

    sky_values: StringProperty(
        name="Sky Values",
        description="Sky value string",
        default="",
        update=update_sky_props,
    )  # type: ignore

    sky_hours: StringProperty(
        name="Sky Hours",
        description="The hours represented by the sky.",
        default="",
        update=update_sky_props,
    )  # type: ignore

    sky_hour: IntProperty(
        name="Hour",
        description="Selected hour for the sky.",
        default=-1,
        min=-1,
        max=8760,
        set=set_sky_hour,
        get=get_sky_hour,
        update=update_sky_values,
    )  # type: ignore

    hourly_sky: BoolProperty(
        name="Hourly",
        description="Sky is computed for each hour in the selected range.",
        default=False,
    )  # type: ignore

    epw_path: StringProperty(
        name="EPW Path",
        default="",
        description="Define the path of the EPW weather file.",
        subtype="FILE_PATH",
    )  # type: ignore

    gltf_export_postfix: StringProperty(
        name="Postfix",
        description="ostfix for the GLTF export filename.",
        default="export",
    )  # type: ignore


class PROPERTIES_PT_ThermalSkyPanel(Panel):

    bl_label = "Thermal"
    bl_idname = "PROPERTIES_PT_thermal_sky_panel"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "world"

    def draw(self, context):
        layout = self.layout

        tprop = context.scene.thermal_sky_props

        dep_valid = tprop.dependencies_valid
        hourly_sky = tprop.hourly_sky

        if not dep_valid:
            layout.operator(InstallDependenciesOperator.bl_idname)
        else:
            sky_box = layout.box()
            sky_box.label(text="SKY")

            box = sky_box.box()
            box.label(text="GEOMETRY")

            if tprop.geom_pointer is None:
                box.operator(GenerateSkyGeometryOperator.bl_idname)
            else:
                box.prop(tprop, "geom_pointer")
                row = box.row()
                row.prop(tprop, "dome_radius")
                row.prop(tprop, "visualization_mode")

            if tprop.geom_pointer is not None:
                box = sky_box.box()
                box.label(text="SKY VALUES")                
                row = box.row()                
                row.prop(tprop, "epw_path")
                row = box.row()
                row.prop(tprop, "start_hoy")
                row.prop(tprop, "duration_hours")
                row = box.row()
                row.prop(tprop, "hourly_sky")
                if hourly_sky:
                    row.prop(tprop, "sky_hour")
                row = box.row()
                row.operator(ComputeSkyValuesOperator.bl_idname)
                col = box.column()
                col.prop(tprop, "sky_values")
                col.prop(tprop, "sky_hours")

        misc_box = layout.box()
        misc_box.label(text="EXPORT")        
        misc_box.prop(tprop, "gltf_export_postfix")  
        misc_box.operator(ExportGltfOperator.bl_idname)


class InstallDependenciesOperator(Operator):

    bl_idname = "wm.install_ladybug_operator"
    bl_label = "Install Dependencies"

    def execute(self, context):

        print("Setting up Ladybug ...")

        python_exe = os.path.join(sys.prefix, "bin", "python.exe")
        target = os.path.join(sys.prefix, "lib", "site-packages")

        print("Setting up pip ...")
        try:
            import pip
        except ImportError:
            subprocess.call([python_exe, "-m", "ensurepip"])
            subprocess.call([python_exe, "-m", "pip", "install", "--upgrade", "pip"])

        try:
            import ladybug_radiance
        except ImportError:
            print("Installing Ladybug Core ...")
            subprocess.call(
                [
                    python_exe,
                    "-m",
                    "pip",
                    "install",
                    "--upgrade",
                    "ladybug-core",
                    "-t",
                    target,
                ]
            )
        try:
            from ladybug.sunpath import Sunpath
        except ImportError:
            print("Installing Ladybug Radiance ...")
            subprocess.call(
                [
                    python_exe,
                    "-m",
                    "pip",
                    "install",
                    "--upgrade",
                    "ladybug-radiance",
                    "-t",
                    target,
                ]
            )

        try:
            import dateutil
        except ImportError:
            print("Installing dateutil ...")
            subprocess.call(
                [
                    python_exe,
                    "-m",
                    "pip",
                    "install",
                    "--upgrade",
                    "python-dateutil",
                    "-t",
                    target,
                ]
            )

        try:
            import numpy
        except ImportError:
            print("Installing numpy ...")
            subprocess.call(
                [python_exe, "-m", "pip", "install", "--upgrade", "numpy", "-t", target]
            )

        
        try:
            import pybind11
        except ImportError:
            print("Installing pybind11 ...")
            subprocess.call(
                [python_exe, "-m", "pip", "install", "--upgrade", "pybind11", "-t", target]
                )

        print('pybind11 done')

        try:
            import meshpy.tet
        except ImportError:
            print("Installing MeshPy ...")
            subprocess.call(
                [python_exe, "-m", "pip", "install", "--upgrade", "MeshPy", "-t", target]
                )
            
        print('MeshPy done')

        tprop = context.scene.thermal_sky_props
        tprop["dependencies_valid"] = True

        tprop["dome_radius"] = 1.0
        tprop["start_hoy"] = 0
        tprop["duration_hours"] = 1
        tprop["visualization_mode"] = False
        tprop["values_valid"] = False
        tprop["sky_values"] = ""
        tprop["sky_hours"] = ""
        tprop["sky_hour"] = -1
        tprop["hourly_sky"] = False

        return {"FINISHED"}


class ComputeSkyValuesOperator(Operator):

    bl_idname = "wm.compute_sky_values_operator"
    bl_label = "Compute Values"

    def execute(self, context):

        props: ThermalSkyProperties = context.scene.thermal_sky_props

        start_hoy = props.start_hoy
        end_hoy = start_hoy + props.duration_hours

        print(start_hoy, end_hoy)

        epw_path = props.epw_path
        if epw_path == "":
            self.report({"WARNING"}, "EPW path is empty! Cancelling operation.")
            return {"CANCELLED"}
        
        epw_path = bpy.path.abspath(epw_path)

        hours = []
        values = []
        hour = -1

        if props.hourly_sky:  
            for h in range(start_hoy, end_hoy):
                print(f'Computing Hour: {h}')
                vals = SkyMatrix().compute(epw_path, h, h + 1, _only_sun=False)
                vals = list(reversed(vals))
                values += vals
                hours.append(h)
            print('Hours:', len(values) / SkyMatrix.TREGENZA_PATCH_COUNT) 
            hour = start_hoy
        else:        
            values = SkyMatrix().compute(epw_path, start_hoy, end_hoy, _only_sun=False)
            values = list(reversed(values))
        
        values_W_per_m2 = [v * 1000.0 for v in values[:]] # convert from kW/m2 to W/m2

        props.sky_values = floats2str(values_W_per_m2)
        props.sky_hours = ints2str(hours)
        props["sky_hour"] = hour
        
        bpy.ops.wm.set_custom_properties()
        bpy.ops.wm.update_sky_values_operator()

        return {"FINISHED"}


class UpdateSkyValuesOperator(Operator):

    bl_idname = "wm.update_sky_values_operator"
    bl_label = "Update Values"

    def execute(self, context):

        props: ThermalSkyProperties = context.scene.thermal_sky_props

        if props.sky_values == "":
            print("Sky values string empty. Nothing to update.")
            return {"FINISHED"}

        values = str2floats(props.sky_values)

        if props.hourly_sky:
            hours = str2ints(props.sky_hours)
            ind = props.sky_hour - hours[0]
            values = values[SkyMatrix.TREGENZA_PATCH_COUNT * ind:SkyMatrix.TREGENZA_PATCH_COUNT * (ind + 1)]

        max_val = max(values)
        if max_val <= 0.0:
            max_val = 1.0
        values = [v / max_val for v in values]

        mesh_data = props.geom_pointer
        bm = bmesh.new()
        bm.from_mesh(mesh_data)

        if not bm.loops.layers.color.get("color"):
            color_layer = bm.loops.layers.color.new("color")
        else:
            color_layer = bm.loops.layers.color.get("color")

        qi = 0
        for face in bm.faces:
            for loop in face.loops:
                v = values[qi]
                v = v / (v + 1)
                # col = mu.Vector((1, 0, 0, 1)).lerp(mu.Vector((0, 0, 1, 1)), v)
                col = mu.Vector((0, 0, 0, 1)).lerp(mu.Vector((1, 1, 1, 1)), v)
                loop[color_layer] = col
            qi += 1

        # writes the bmesh data into the mesh data
        bm.to_mesh(mesh_data)

        # [Optional] update the mesh data (helps with redrawing the mesh in the viewport)
        mesh_data.update()

        # clean up/free memory that was allocated for the bmesh
        bm.free()

        return {"FINISHED"}


class GenerateSkyGeometryOperator(Operator):

    bl_idname = "wm.generate_sky_geometry_operator"
    bl_label = "Generate Geometry"

    def sph2cart_Yup(_r, _incl, _azi):
        sin_incl = math.sin(_incl)
        cos_incl = math.cos(_incl)
        sin_azi = math.sin(_azi)
        cos_azi = math.cos(_azi)
        return mu.Vector(
            (
                _r * sin_incl * cos_azi,
                _r * sin_incl * sin_azi,
                _r * cos_incl,
            )
        )

    def execute(self, context):

        props = context.scene.thermal_sky_props

        obj_name = "sky"
        mesh_name = f"{obj_name}_data"

        if props.geom_pointer is None:
            # create the mesh data
            mesh_data = bpy.data.meshes.new(mesh_name)
            # create the mesh object using the mesh data
            mesh_obj = bpy.data.objects.new(obj_name, mesh_data)
            # add the mesh object into the scene
            bpy.context.scene.collection.objects.link(mesh_obj)
            props.geom_pointer = mesh_data

        mesh_data = props.geom_pointer

        dome_radius = props.dome_radius
        quad_size = (
            dome_radius * 2
            if not props.visualization_mode
            else 0.7 * dome_radius * math.pi * SkyMatrix.DEGREES_PER_ROW / 180.0
        )

        # create a new bmesh
        bm = bmesh.new()

        vert_coords = []
        face_vert_indices = []

        vind = 0
        inclination = 0.0
        for ci in range(SkyMatrix.NUM_ROWS):

            c = SkyMatrix.TREGENZA_PATCHES_PER_ROW[SkyMatrix.NUM_ROWS - 1 - ci]

            azi_off = 360.0 / float(c)
            azimuth = 0.0

            for i in range(c):

                qc = GenerateSkyGeometryOperator.sph2cart_Yup(
                    dome_radius,
                    inclination * SkyMatrix.DEG2RAD,
                    azimuth * SkyMatrix.DEG2RAD,
                )  # quad center

                normal = qc.normalized()
                tangent = normal.cross(mu.Vector((0, 0, 1))).normalized()
                if c == 1:
                    tangent = normal.cross(mu.Vector((0, 1, 0))).normalized()
                bitangent = normal.cross(tangent).normalized()

                vert_coords.append(qc + quad_size * (tangent + bitangent).normalized())
                vert_coords.append(qc + quad_size * (tangent - bitangent).normalized())
                vert_coords.append(qc + quad_size * (-tangent - bitangent).normalized())
                vert_coords.append(qc + quad_size * (-tangent + bitangent).normalized())

                # v0.normal = v1.normal = v2.normal = v3.normal = vec4(normal, 0.0);
                # v0.tangent = v1.tangent = v2.tangent = v3.tangent = vec4(tangent, 0.0);

                face_vert_indices.append((vind + 0, vind + 1, vind + 2, vind + 3))

                vind += 4

                azimuth += azi_off

            inclination += SkyMatrix.DEGREES_PER_ROW

        # create and add a vertices
        for coord in vert_coords:
            bm.verts.new(coord)

        bm.verts.ensure_lookup_table()

        for vert_indices in face_vert_indices:
            face = bm.faces.new([bm.verts[index] for index in vert_indices])

        # writes the bmesh data into the mesh data
        bm.to_mesh(mesh_data)

        # [Optional] update the mesh data (helps with redrawing the mesh in the viewport)
        mesh_data.update()

        # clean up/free memory that was allocated for the bmesh
        bm.free()

        return {"FINISHED"}


class ExportGltfOperator(Operator):

    bl_idname = "wm.export_gltf_operator"
    bl_label = "Export GLTF"

    def execute(self, context):

        print("###########")
        for i in bpy.context.view_layer.items():
            print(i)
        print("###########")
        for obj in bpy.data.objects:
            #if obj not in bpy.context.view_layer.objects.keys():
            #    continue
            tp: ThermalObjectProperties = obj.thermal_props
            if tp.h_is_initialized:
                obj.select_set(True)

        tprop: ThermalSkyProperties = context.scene.thermal_sky_props

        export_path = os.path.splitext(bpy.data.filepath)[0] + '-' + tprop.gltf_export_postfix + '.gltf'

        bpy.ops.export_scene.gltf(
            filepath=export_path,
            check_existing=True,
            export_import_convert_lighting_mode='SPEC',
            gltf_export_id='',
            export_use_gltfpack=False,
            export_gltfpack_tc=False,
            export_gltfpack_tq=8,
            export_gltfpack_si=1.0,
            export_gltfpack_sa=False,
            export_gltfpack_slb=False,
            export_gltfpack_vp=14,
            export_gltfpack_vt=12,
            export_gltfpack_vn=8,
            export_gltfpack_vc=8,
            export_gltfpack_vpi='Integer',
            export_gltfpack_noq=True,
            export_format='GLTF_SEPARATE',
            ui_tab='GENERAL',
            export_copyright='',
            export_image_format='AUTO',
            export_image_add_webp=False,
            export_image_webp_fallback=False,
            export_texture_dir='',
            export_jpeg_quality=75,
            export_image_quality=75,
            export_keep_originals=False,
            export_texcoords=True,
            export_normals=True,
            export_gn_mesh=False,
            export_draco_mesh_compression_enable=False,
            export_draco_mesh_compression_level=6,
            export_draco_position_quantization=14,
            export_draco_normal_quantization=10,
            export_draco_texcoord_quantization=12,
            export_draco_color_quantization=10,
            export_draco_generic_quantization=12,
            export_tangents=False,
            export_materials='EXPORT',
            export_unused_images=False,
            export_unused_textures=False,
            export_vertex_color='MATERIAL',
            export_all_vertex_colors=True,
            export_active_vertex_color_when_no_material=True,
            export_attributes=False,
            use_mesh_edges=False,
            use_mesh_vertices=False,
            export_cameras=True,
            use_selection=True,
            use_visible=False,
            use_renderable=False,
            use_active_collection_with_nested=True,
            use_active_collection=False,
            use_active_scene=False,
            collection='',
            at_collection_center=False,
            export_extras=True,
            export_yup=True,
            export_apply=True,
            export_shared_accessors=False,
            export_animations=False,
            export_frame_range=False,
            export_frame_step=1,
            export_force_sampling=True,
            export_pointer_animation=False,
            export_animation_mode='ACTIONS',
            export_nla_strips_merged_animation_name='Animation',
            export_def_bones=False,
            export_hierarchy_flatten_bones=False,
            export_hierarchy_flatten_objs=False,
            export_armature_object_remove=False,
            export_leaf_bone=False,
            export_optimize_animation_size=True,
            export_optimize_animation_keep_anim_armature=True,
            export_optimize_animation_keep_anim_object=False,
            export_optimize_disable_viewport=False,
            export_negative_frame='SLIDE',
            export_anim_slide_to_zero=False,
            export_bake_animation=False,
            export_anim_single_armature=True,
            export_reset_pose_bones=True,
            export_current_frame=False,
            export_rest_position_armature=True,
            export_anim_scene_split_object=True,
            export_skins=True,
            export_influence_nb=4,
            export_all_influences=False,
            export_morph=True,
            export_morph_normal=True,
            export_morph_tangent=False,
            export_morph_animation=True,
            export_morph_reset_sk_data=True,
            export_lights=False,
            export_try_sparse_sk=True,
            export_try_omit_sparse_sk=False,
            export_gpu_instances=False,
            export_action_filter=False,
            export_convert_animation_pointer=False,
            export_nla_strips=True,
            export_original_specular=False,
            will_save_settings=False,
            export_hierarchy_full_collections=False,
            export_extra_animations=False,
            filter_glob='*.gltf')
        
        print(f"Exported: {export_path}")

        return {"FINISHED"}
