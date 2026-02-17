"""
Aurora Material Creator — Run in UE5 Editor Python console:
  Tools > Execute Python Script, or paste into Output Log Python tab.

Creates M_AuroraCurtain with proper node graph:
  VertexColor.RGB * ScalarParam("EmissiveStrength") -> Emissive Color
  VertexColor.A -> Opacity
  Blend Mode = Additive, Shading = Unlit, Two-Sided = true
"""

import unreal

# Check if already exists
if unreal.EditorAssetLibrary.does_asset_exist("/Game/M_AuroraCurtain"):
    unreal.log_warning("M_AuroraCurtain already exists — deleting and recreating")
    unreal.EditorAssetLibrary.delete_asset("/Game/M_AuroraCurtain")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.MaterialFactoryNew()

material = asset_tools.create_asset("M_AuroraCurtain", "/Game", unreal.Material, factory)

if material is None:
    unreal.log_error("Failed to create M_AuroraCurtain")
else:
    # Configure material properties
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    # Create material expression nodes using MaterialEditingLibrary
    mel = unreal.MaterialEditingLibrary

    # Node 1: VertexColor
    vertex_color = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -400, 0)

    # Node 2: ScalarParameter "EmissiveStrength" with default 5.0
    emissive_param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 200)
    emissive_param.set_editor_property("parameter_name", "EmissiveStrength")
    emissive_param.set_editor_property("default_value", 5.0)

    # Node 3: Multiply (VertexColor.RGB * EmissiveStrength)
    multiply = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, 0)

    # Wire: VertexColor -> Multiply.A
    mel.connect_material_expressions(vertex_color, "RGB", multiply, "A")

    # Wire: EmissiveStrength -> Multiply.B
    mel.connect_material_expressions(emissive_param, "", multiply, "B")

    # Wire: Multiply -> Emissive Color
    mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Wire: VertexColor.A -> Opacity
    mel.connect_material_property(vertex_color, "A", unreal.MaterialProperty.MP_OPACITY)

    # Recompile and save
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset("/Game/M_AuroraCurtain")

    unreal.log("SUCCESS: Created M_AuroraCurtain — Additive, Unlit, Two-Sided")
    unreal.log("  VertexColor.RGB * EmissiveStrength(5.0) -> Emissive")
    unreal.log("  VertexColor.A -> Opacity")
    unreal.log("Now assign it to AuroraBorealisActor -> Curtain Material")
