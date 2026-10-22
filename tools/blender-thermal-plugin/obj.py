import bpy, bmesh

from bpy.props import (
    StringProperty,
    BoolProperty,
    IntProperty,
    FloatProperty,
    FloatVectorProperty,
    EnumProperty,
    PointerProperty,
    CollectionProperty,
)
from bpy.types import (
    Panel,
    Menu,
    Operator,
    PropertyGroup,
)

from .util import str2floats, str2ints

def get_thermal_props():
    selObj = (
        bpy.context.selected_objects[0]
        if len(bpy.context.selected_objects) > 0
        else None
    )
    if selObj is None:
        return None, None
    else:
        return selObj.thermal_props, selObj


def update_custom_properties(self, context):
    scn = context.scene
    bpy.ops.wm.clear_custom_properties()
    bpy.ops.wm.set_custom_properties()


def update_constraint_mesh(self, context):
    obj = self["h_constraint_data_pointer"]
    obj_data = obj.data
    if isinstance(obj_data, bpy.types.Mesh):
        
        bm = bmesh.new()
        bm.from_mesh(obj_data)
        bm.faces.ensure_lookup_table()
        bmesh.ops.dissolve_faces(bm, faces=bm.faces, use_verts=True)
        bm.to_mesh(obj_data)

        print(f"Extracting constraint vector from object '{obj.name}'.")
        _indices = [list(v) for v in [p.vertices for p in obj_data.polygons]]

        indices = []
        for arr in _indices:
            indices += arr

        vertices_co = []
        mat = obj.matrix_world
        for i in indices:
            world_co = obj_data.vertices[i].co
            loc = mat @ world_co
            vertices_co.append(loc)

        constraint_str = "; ".join([f"{v.x:4f}, {-v.y:4f}" for v in vertices_co])
        print(constraint_str)
        self["constraint_data"] = constraint_str
    else:
        print(f"Object '{obj.name}' has no mesh to extract a constraint vector from.")


class ThermalObjectProperties(PropertyGroup):

    h_is_initialized: BoolProperty(
        name="is_initialized",
        description="is_initialized",
        default=False,
    )  # type: ignore

    h_remesh_copy: BoolProperty(
        name="Remesh as copy",
        description="Create remesh as a copy.",
        default=True,
    )  # type: ignore

    h_remesh_factor: FloatProperty(
        name="Remesh Factor",
        description="Max volume for tetgen.",
        default=1.0,
        min=0.0001,
    )  # type: ignore

    kelvin: FloatProperty(
        name="Kelvin",
        description="Object's temperature in kelvin",
        default=0.0,
        min=0.0,
        update=update_custom_properties,
        # max = 3000.0
    )  # type: ignore

    temperature_fixed: BoolProperty(
        name="Fixed",
        description="Sets the temperature to stay fixed.",
        default=False,
        update=update_custom_properties,
    )  # type: ignore

    diffuse_reflectance: FloatProperty(
        name="Diffuse",
        description="object diffuse reflectance factor",
        default=0.5,
        min=0.0,
        max=1.0,
        update=update_custom_properties,
    )  # type: ignore

    specular_reflectance: FloatProperty(
        name="Specular",
        description="object specular reflectance factor",
        default=0.0,
        min=0.0,
        max=1.0,
        update=update_custom_properties,
    )  # type: ignore

    emission: EnumProperty(
        items=(
            (
                "DIFFUSE",
                "Diffuse",
                "Diffuse Emission into all directions in hemisphere.",
            ),
            ("NORMAL", "Along Normal", "Emission along normal direction."),
        ),
        name="Mode",
        default="DIFFUSE",
        description="Emission Mode, diffuse or normal.",
        update=update_custom_properties,
        # set=stereo_mode_setter,
        # get=stereo_mode_getter,
        # update=update_stereo_mode,
    )  # type: ignore

    thickness: FloatProperty(
        name="Thickness",
        description="Object's thickness in mm.",
        default=1.0,
        min=0.0,
        update=update_custom_properties,
        # max = 3000.0
    )  # type: ignore

    density: FloatProperty(
        name="Density",
        description="Object density.",
        default=1.0,
        min=0.0,
        update=update_custom_properties,
        # max = 3000.0
    )  # type: ignore

    heat_capacity: FloatProperty(
        name="Heat Capacity",
        description="Object's heat capacity.",
        default=1.0,
        min=0.0,
        update=update_custom_properties,
        # max = 3000.0
    )  # type: ignore

    heat_conductivity: FloatProperty(
        name="Heat Conductivity",
        description="Object's heat conductivity.",
        default=0.0,
        min=0.0,
        update=update_custom_properties,
        # max = 3000.0
    )  # type: ignore

    traceable: BoolProperty(
        name="Traceable",
        description="Object is can be hit by rays during raytracing.",
        default=True,
        update=update_custom_properties,
    )  # type: ignore

    h_is_sky: BoolProperty(
        name="Is sky",
        description="Object represents the sky dome",
        default=False,
        update=update_custom_properties,
    )  # type: ignore

    sky_values_str: StringProperty(
        name="Sky Values",
        description="Sky value string",
        default="",
        update=update_custom_properties,
    )  # type: ignore

    sky_hours_str: StringProperty(
        name="Hourly",
        description="Sky is computed for each hour in the selected range.",
        default="",
        update=update_custom_properties,
    )  # type: ignore
    
    sky_hour: IntProperty(
        name="Hour",
        description="Selected hour for the sky.",
        default=-1,
        update=update_custom_properties,
    )  # type: ignore

    # ## OPTIMIZATION ## #

    optimize_mode: IntProperty(
        name="Mode",
        description="Optimization Mode.",
        default=0,
        min=0,
        update=update_custom_properties,
    )  # type: ignore

    objective_target: FloatProperty(
        name="Target",
        description="Objective target.",
        default=1.0,
        min=0.0,
        update=update_custom_properties,
    )  # type: ignore

    objective_weight: FloatProperty(
        name="Weight",
        description="Objective weight.",
        default=0.0001,
        min=0.0,
        update=update_custom_properties,
    )  # type: ignore

    constraint_mode: IntProperty(
        name="Mode",
        description="Constraint Mode.",
        default=0,
        min=0,
        update=update_custom_properties,
    )  # type: ignore

    constraint_weight: FloatProperty(
        name="Weight",
        description="Objective weight.",
        default=1.0,
        min=0.0,
        update=update_custom_properties,
    )  # type: ignore

    rotation_scale: FloatProperty(
        name="Rotation Scale",
        description="Rotation Scale fpr XY-Rot Constraint",
        default=1.0,
        min=0.0,
        update=update_custom_properties,
    )  # type: ignore

    constraint_data: StringProperty(
        name="Data",
        description="Constraint data.",
        default="",
        update=update_custom_properties,
    )  # type: ignore

    h_constraint_data_pointer: PointerProperty(
        type=bpy.types.Object,
        name="Mesh",
        description="Constraint data.",
        update=update_constraint_mesh,
    )  # type: ignore
    
    collision_weight: FloatProperty(
        name="Collition Weight",
        description="Collition weight.",
        default=0.0,
        min=0.0,
        update=update_custom_properties,
    )  # type: ignore

    collision_group: IntProperty(
        name="Collision Group",
        description="Collision Group",
        default=0,
        min=0,
        update=update_custom_properties,
    )  # type: ignore


class OBJECT_PT_ThermalPanel(Panel):
    bl_label = "Thermal"
    bl_idname = "OBJECT_PT_custom_panel"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    @classmethod
    def poll(self, context):
        return context.object is not None

    def draw(self, context):
        layout = self.layout
        #scene = context.scene

        objProps: ThermalObjectProperties
        objProps, _ = get_thermal_props()

        if objProps is None:
            return

        if not objProps.h_is_initialized:
            layout.operator("wm.set_custom_properties")
            return

        is_sky = objProps.h_is_sky

        if is_sky:
            layout.box().label(
                text="Settings for the sky object are located in the world panel."
            )

        lay = layout.box()
        lay.enabled = not is_sky
        lay.label(text="SIMULATION")
        lay_col = lay.column()

        lay_row = lay_col.row()
        lay_row.label(text="Temperature:")
        lay_row.prop(objProps, "kelvin")
        lay_row.prop(objProps, "temperature_fixed")

        lay_row = lay_col.row()
        lay_row.label(text="Reflectance:")
        lay_row.prop(objProps, "diffuse_reflectance")
        lay_row.prop(objProps, "specular_reflectance")

        lay_row = lay_col.row()
        split = lay_row.split(factor=0.33)
        split.label(text="Emission:")
        split.prop(objProps, "emission")

        lay_row = lay_col.row()
        lay_row.label(text="Matter:")
        lay_row.prop(objProps, "thickness")
        lay_row.prop(objProps, "density")

        lay_row = lay_col.row()
        lay_row.label(text="Heat:")
        lay_row.prop(objProps, "heat_capacity")
        lay_row.prop(objProps, "heat_conductivity")

        lay_row = lay_col.row()
        split = lay_row.split(factor=0.33)
        split.label(text="Misc:")
        row = split.row()
        split = row.split()
        split.prop(objProps, "traceable")
        sky_row = split.row()
        sky_row.enabled = False
        sky_row.prop(objProps, "h_is_sky")

        layout.separator()

        lay = layout.box()
        lay.enabled = not is_sky
        lay.label(text="OPTIMIZATION")
        lay_col = lay.column()

        lay_row = lay_col.row()
        split = lay_row.split(factor=0.33)
        split.label(text="Optimization:")
        split.prop(objProps, "optimize_mode")

        lay_row = lay_col.row()
        lay_row.label(text="Objective:")
        lay_row.prop(objProps, "objective_target")
        lay_row.prop(objProps, "objective_weight")
        if objProps.optimize_mode == 4:            
            lay_row = lay_col.row()
            lay_row.prop(objProps, "rotation_scale")

        lay_box = lay_col.box()
        lay_box.label(text="Constraint:")
        lay_row = lay_box.row()
        lay_row.prop(objProps, "constraint_mode")
        lay_row.prop(objProps, "constraint_weight")

        lay_row = lay_box.row()
        if objProps.constraint_mode == 9:            
            # prop_search(data, property, search_data, search_property)
            lay_row = lay_box.column()
            lay_row.prop_search(
                objProps, "h_constraint_data_pointer", context.scene, "objects"
            )
            lay_row.prop(objProps, "constraint_data")
        layout.separator()
        lay_row = lay_box.row()
        lay_row.prop(objProps, "collision_group")
        lay_row.prop(objProps, "collision_weight")

        if not is_sky:
            layout.separator()

            row = layout.row()
            row.prop(objProps, "h_remesh_factor")
            row.prop(objProps, "h_remesh_copy")

            row = layout.row()
            row.operator("wm.remesh_operator")

            layout.separator()

            row = layout.row()
            row.operator("wm.set_custom_properties")
            row.operator("wm.clear_custom_properties")
            row.operator("wm.remove_custom_properties")



class SetCustomPropertiesOperator(Operator):
    bl_label = "Set"
    bl_idname = "wm.set_custom_properties"

    prop_map = {
        "emission": lambda n, v: (
            "diffuse-emission",
            1 if (v == "DIFFUSE") else 0,
        ),
        "temperature-fixed": lambda n, v: (
            n,
            1 if v else 0,
        ),
        "traceable": lambda n, v: (
            n,
            1 if v else 0,
        ),
        "traceable": lambda n, v: (
            n,
            1 if v else 0,
        ),
        "sky-values-str": lambda n, v: (
            "sky-values",
            str2floats(v),
        ),        
        "sky-hours-str": lambda n, v: (
            "sky-hours",
            str2ints(v),
        ),
    }

    def execute(self, context):

        objProps, obj = get_thermal_props()

        print()
        print(f"### Setting custom properties for obj '{obj.name}':")

        excl_map = [
            "h_",
            "__",
            "bl_",
            "rna",
            "name",
        ]

        for k in sorted(dir(objProps)):

            skip = False
            for s in excl_map:
                if k.startswith(s):
                    skip = True
                    break
            if skip:
                continue

            name = "-".join(k.split("_"))
            attr_val = getattr(objProps, k)
            if name in self.prop_map:
                name, attr_val = self.prop_map[name](name, attr_val)
            print(f"{name}: '{attr_val}'")
            obj.data[name] = attr_val

        objProps.h_is_initialized = True

        return {"FINISHED"}


class ClearCustomPropertiesOperator(Operator):
    bl_label = "Clear"
    bl_idname = "wm.clear_custom_properties"

    # todo check the original code had all initialized and now keys are not updated

    def execute(self, context):
        objProps, obj = get_thermal_props()
        for k in objProps.keys():
            name = "_".join(k.split("-"))
            if name in obj.data.keys():
                del obj.data[name]
        return {"FINISHED"}
    

class RemoveCustomPropertiesOperator(Operator):
    bl_label = "Remove"
    bl_idname = "wm.remove_custom_properties"

    def execute(self, context):
        objProps, obj = get_thermal_props()        
        bpy.ops.wm.clear_custom_properties()
        objProps.h_is_initialized = False
        return {"FINISHED"}


class RemeshOperator(Operator):

    bl_label = "Remesh"
    bl_idname = "wm.remesh_operator"

    def execute(self, context):
        
        objProps: ThermalObjectProperties
        objProps, obj = get_thermal_props()

        me = obj.data

        # EXTRACT MESH VERTICES AND FACES

        points = [tuple(v.co) for v in me.vertices]
        facets = []

        for poly in me.polygons:
            indices = []
            for loop_index in poly.loop_indices:
                indices.append(me.loops[loop_index].vertex_index)
            facets.append(indices)

        print(f"Extracted {len(points)} points and {len(facets)} facets.")
        
        # TETGEN build

        from meshpy.tet import MeshInfo, build

        mesh_info = MeshInfo()

        mesh_info.set_points(points)
        mesh_info.set_facets(facets)
        
        print(f"Remeshing (max_volume: {objProps.h_remesh_factor}) ...")
        mesh = build(mesh_info, max_volume=objProps.h_remesh_factor)
        
        print(f"Generated {len(mesh.points)} points and {len(mesh.elements)} elements.")

        # COPY OBJECT (?) 
        
        if objProps.h_remesh_copy:            
            print("Copy original ...")
            new_obj = obj.copy()
            new_obj.data = obj.data.copy()
            new_obj.animation_data_clear()
            bpy.context.scene.collection.objects.link(new_obj)
            me = new_obj.data

        # EXTRACT SURFACE
                       
        print("Extracting shell mesh ...")
        
        bm = bmesh.new()
        verts = []
        for i, p in enumerate(mesh.points):
            verts.append(bm.verts.new(p))            
        bm.verts.ensure_lookup_table()

        ted_indices = [
            [1, 2, 3],
            [0, 3, 2],
            [0, 1, 3],
            [0, 2, 1],
        ]

        set_dict = dict()
        for i, elems in enumerate(mesh.elements):
            for tis in ted_indices:
                fset = tuple(sorted(list(set([elems[ti] for ti in tis]))))   
                if fset not in set_dict.keys():
                    set_dict[fset] = (i, 1)
                else:
                    _i, c = set_dict[fset]
                    set_dict[fset] = (_i, c+1) 
                
        for k, f in set_dict.items():    
            _i, c = f
            #print(_i, c, k)   
            if c < 2:
                bm.faces.new([verts[i] for i in k])
        
        
        print("Assigning mesh ...")
        bm.to_mesh(me)
        
        print("Remeshing DONE.")

        return {"FINISHED"}