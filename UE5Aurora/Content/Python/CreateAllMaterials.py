"""
Minimal Aurora Material Creator — just passes VertexColor through.
Run in UE5 Editor: Tools > Execute Python Script
"""

import unreal

mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.MaterialFactoryNew()

# ====================================================================
# 1. AURORA CURTAIN MATERIAL — simplest possible
# ====================================================================
mat_path = "/Game/M_AuroraCurtain"
if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
    unreal.EditorAssetLibrary.delete_asset(mat_path)

mat = asset_tools.create_asset("M_AuroraCurtain", "/Game", unreal.Material, factory)

if mat:
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)

    # VertexColor node
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -400, 0)

    # EmissiveStrength parameter
    emissive = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 200)
    emissive.set_editor_property("parameter_name", "EmissiveStrength")
    emissive.set_editor_property("default_value", 8.0)

    # Multiply node: VertexColor * EmissiveStrength
    mult = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 0)

    # Try connecting with empty string for the full RGBA output
    # In UE5, VertexColor outputs are indexed, not named "RGB"
    # The default output (empty string "") gives the full color
    mel.connect_material_expressions(vc, "", mult, "A")
    mel.connect_material_expressions(emissive, "", mult, "B")

    # Multiply result -> Emissive Color
    mel.connect_material_property(mult, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # VertexColor Alpha -> Opacity
    # Try "A" first, then "Alpha" if that fails
    try:
        mel.connect_material_property(vc, "A", unreal.MaterialProperty.MP_OPACITY)
        unreal.log("Connected VertexColor.A to Opacity")
    except:
        try:
            mel.connect_material_property(vc, "Alpha", unreal.MaterialProperty.MP_OPACITY)
            unreal.log("Connected VertexColor.Alpha to Opacity")
        except:
            unreal.log_warning("Could not connect VertexColor alpha to opacity")

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat_path)
    unreal.log("Created M_AuroraCurtain: Additive, Unlit, Two-Sided")
    unreal.log("VertexColor * EmissiveStrength(8.0) -> Emissive")
else:
    unreal.log_error("Failed to create M_AuroraCurtain")

# ====================================================================
# 2. MOUNTAIN MATERIAL
# ====================================================================
mtn_path = "/Game/M_MountainDark"
if unreal.EditorAssetLibrary.does_asset_exist(mtn_path):
    unreal.EditorAssetLibrary.delete_asset(mtn_path)

mtn_mat = asset_tools.create_asset("M_MountainDark", "/Game", unreal.Material, factory)

if mtn_mat:
    mtn_mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mtn_mat.set_editor_property("two_sided", True)

    dark = mel.create_material_expression(mtn_mat, unreal.MaterialExpressionConstant3Vector, -200, 0)
    dark.set_editor_property("constant", unreal.LinearColor(0.005, 0.005, 0.008, 1.0))
    mel.connect_material_property(dark, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mtn_mat)
    unreal.EditorAssetLibrary.save_asset(mtn_path)
    unreal.log("Created M_MountainDark")
else:
    unreal.log_error("Failed to create M_MountainDark")

unreal.log("")
unreal.log("=== DONE ===")
unreal.log("If aurora is still white: open M_AuroraCurtain and verify")
unreal.log("VertexColor is connected to Multiply.A (not just wired through)")

# Also list the output pin names of VertexColor for debugging
unreal.log("")
unreal.log("Debugging: VertexColor output pins:")
if mat:
    for expr in mat.get_editor_property("expressions"):
        if isinstance(expr, unreal.MaterialExpressionVertexColor):
            outputs = expr.get_editor_property("outputs")
            for i, out in enumerate(outputs):
                unreal.log(f"  Output {i}: '{out.get_editor_property('output_name')}'")
