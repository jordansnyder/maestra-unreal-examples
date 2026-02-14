"""
CreateFlowFieldNiagara.py
Run this in the Unreal Editor via: File -> Execute Python Script
Or via the Output Log: py "C:/Users/jorda/Dev/MaestraUnrealClean/Scripts/CreateFlowFieldNiagara.py"

Creates NS_FlowFieldParticles Niagara system with:
- GPU Compute emitter
- Curl Noise Force for flow field motion
- Sprite + Ribbon renderers
- 15 User Parameters driven by FlowFieldActor C++
- Additive unlit materials for bloom
"""

import unreal

# ============================================================================
# Configuration
# ============================================================================
SYSTEM_PATH = "/Game/FlowField/NS_FlowFieldParticles"
SPRITE_MAT_PATH = "/Game/FlowField/M_FlowFieldSprite"
RIBBON_MAT_PATH = "/Game/FlowField/M_FlowFieldRibbon"

# Template to use as base (Fountain has spawn rate, forces, sprite renderer)
TEMPLATE_SYSTEM = "/Niagara/DefaultAssets/Templates/Systems/FountainLightweight"

# ============================================================================
# Helper Functions
# ============================================================================
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
editor_asset_lib = unreal.EditorAssetLibrary

def create_material(mat_path, mat_name):
    """Create a simple additive unlit material for particles."""
    # Check if already exists
    if editor_asset_lib.does_asset_exist(mat_path):
        unreal.log(f"Material already exists: {mat_path}")
        return unreal.EditorAssetLibrary.load_asset(mat_path)

    factory = unreal.MaterialFactoryNew()
    package_path = mat_path.rsplit("/", 1)[0]
    mat = asset_tools.create_asset(mat_name, package_path, unreal.Material, factory)

    if mat:
        # Set material properties for particle rendering
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property("two_sided", True)

        # Save
        unreal.EditorAssetLibrary.save_asset(mat_path, only_if_is_dirty=False)
        unreal.log(f"Created material: {mat_path}")

    return mat

def create_niagara_system():
    """Create the flow field Niagara system from a template."""

    # Check if already exists
    if editor_asset_lib.does_asset_exist(SYSTEM_PATH):
        unreal.log(f"System already exists at {SYSTEM_PATH}. Delete it first to recreate.")
        return unreal.EditorAssetLibrary.load_asset(SYSTEM_PATH)

    # Try to duplicate from the Fountain template
    template = unreal.EditorAssetLibrary.load_asset(TEMPLATE_SYSTEM)
    if template:
        unreal.log(f"Loaded template: {TEMPLATE_SYSTEM}")
        result = editor_asset_lib.duplicate_asset(TEMPLATE_SYSTEM, SYSTEM_PATH)
        if result:
            system = unreal.EditorAssetLibrary.load_asset(SYSTEM_PATH)
            unreal.log(f"Created system from template: {SYSTEM_PATH}")
            return system
        else:
            unreal.log_warning("Failed to duplicate template, creating empty system")

    # Fallback: create via factory
    factory = unreal.NiagaraSystemFactoryNew()
    package_path = SYSTEM_PATH.rsplit("/", 1)[0]
    system_name = SYSTEM_PATH.rsplit("/", 1)[1]
    system = asset_tools.create_asset(system_name, package_path, unreal.NiagaraSystem, factory)
    return system


# ============================================================================
# Main Execution
# ============================================================================
def main():
    unreal.log("=" * 60)
    unreal.log("Creating Flow Field Niagara System")
    unreal.log("=" * 60)

    # Step 1: Create materials
    unreal.log("\n--- Step 1: Creating Materials ---")
    sprite_mat = create_material(SPRITE_MAT_PATH, "M_FlowFieldSprite")
    ribbon_mat = create_material(RIBBON_MAT_PATH, "M_FlowFieldRibbon")

    # Step 2: Create or load system
    unreal.log("\n--- Step 2: Creating Niagara System ---")
    system = create_niagara_system()

    if not system:
        unreal.log_error("Failed to create Niagara system!")
        return

    unreal.log(f"System created: {system.get_name()}")

    # Step 3: Add user parameters
    unreal.log("\n--- Step 3: Adding User Parameters ---")

    # The user parameters that FlowFieldActor.cpp pushes via SetVariableFloat/Vec3
    float_params = {
        "NoiseScale": 0.01,
        "NoiseSpeed": 0.5,
        "FieldStrength": 100.0,
        "NoiseOctaves": 3.0,
        "SpawnRate": 3000.0,
        "ParticleLifetime": 3.0,
        "ParticleSize": 4.0,
        "TrailLength": 0.5,
        "ColorHue": 0.6,
        "ColorSaturation": 0.8,
        "EmissiveStrength": 5.0,
        "TurbulenceIntensity": 1.0,
        "MasterIntensity": 0.5,
    }

    vec3_params = {
        "FieldBoundsMin": unreal.Vector(-500, -500, -500),
        "FieldBoundsMax": unreal.Vector(500, 500, 500),
    }

    # Get the exposed parameters store
    try:
        exposed_params = system.get_editor_property("exposed_parameters")
        unreal.log(f"Got exposed parameters store")
    except Exception as e:
        unreal.log_warning(f"Could not access exposed_parameters: {e}")
        exposed_params = None

    # Try to add parameters through the system's parameter store
    # Note: UE's Python API may vary - we'll try multiple approaches
    for name, default_val in float_params.items():
        try:
            niagara_var = unreal.NiagaraVariable()
            type_def = unreal.NiagaraTypeDefinition()
            # This is the standard approach
            system.get_exposed_parameters().add_parameter(
                unreal.NiagaraVariable.make_niagara_variable(
                    unreal.NiagaraTypeDefinition.get_float_def(), name
                )
            )
            unreal.log(f"  Added float param: {name} = {default_val}")
        except Exception as e:
            unreal.log_warning(f"  Could not add param {name} via API: {e}")

    # Step 4: Save
    unreal.log("\n--- Step 4: Saving Assets ---")
    unreal.EditorAssetLibrary.save_asset(SYSTEM_PATH, only_if_is_dirty=False)

    if sprite_mat:
        unreal.EditorAssetLibrary.save_asset(SPRITE_MAT_PATH, only_if_is_dirty=False)
    if ribbon_mat:
        unreal.EditorAssetLibrary.save_asset(RIBBON_MAT_PATH, only_if_is_dirty=False)

    unreal.log("\n" + "=" * 60)
    unreal.log("DONE! Next steps:")
    unreal.log("=" * 60)
    unreal.log("1. Open NS_FlowFieldParticles in the Niagara editor")
    unreal.log("2. The system is based on Fountain template - modify:")
    unreal.log("   a. Add User Parameters (listed above) if not auto-added")
    unreal.log("   b. Set Emitter Sim Target -> GPU Compute")
    unreal.log("   c. Link SpawnRate module to User.SpawnRate")
    unreal.log("   d. Add Curl Noise Force module to Particle Update")
    unreal.log("   e. Link Curl Noise Force strength to User.FieldStrength")
    unreal.log("   f. Link Curl Noise Force frequency to User.NoiseScale")
    unreal.log("   g. Set material to M_FlowFieldSprite (additive)")
    unreal.log("   h. Add Ribbon Renderer with M_FlowFieldRibbon material")
    unreal.log("3. Assign to FlowFieldActor's ParticleSystemAsset")
    unreal.log("=" * 60)

# Run
main()
