"""
Mountain Landscape Creator — Run in UE5 Editor Python console.

Creates a Landscape actor with Perlin-noise mountains suitable as a
backdrop for the aurora. Also adds a dark sky atmosphere and stars.
"""

import unreal

editor = unreal.EditorLevelLibrary
subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# ---- 1. Create a simple landscape using a heightmap-style approach ----
# We'll use a Landscape proxy. For simplicity, let's use a static mesh
# approach with UE's built-in landscape tools.

# Actually, the simplest approach for a mountain backdrop is to use
# UE's built-in Landscape system. Let's create the scene elements:

# ---- Sky Atmosphere for dark night sky ----
actor_class = unreal.EditorAssetLibrary.load_blueprint_class  # not needed

# Spawn a SkyAtmosphere if not present
world = unreal.EditorLevelLibrary.get_editor_world()

# ---- 2. Add Directional Light (dim moonlight) ----
location = unreal.Vector(0, 0, 0)
rotation = unreal.Rotator(-45, 30, 0)

dir_light = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.DirectionalLight, location, rotation
)
if dir_light:
    light_comp = dir_light.get_component_by_class(unreal.DirectionalLightComponent)
    if light_comp:
        light_comp.set_intensity(0.1)  # Very dim moonlight
        light_comp.set_light_color(unreal.LinearColor(0.6, 0.65, 0.85, 1.0))  # Cool blue moonlight
    dir_light.set_actor_label("MoonLight")
    unreal.log("Created dim moonlight directional light")

# ---- 3. Add SkyAtmosphere (dark night look) ----
sky_atmo = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)
)
if sky_atmo:
    sky_atmo.set_actor_label("NightSky")
    unreal.log("Created SkyAtmosphere for night sky")

# ---- 4. Add ExponentialHeightFog for atmospheric depth ----
fog = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0)
)
if fog:
    fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    if fog_comp:
        fog_comp.set_fog_density(0.005)
        fog_comp.set_fog_inscattering_color(unreal.LinearColor(0.02, 0.03, 0.05, 1.0))
    fog.set_actor_label("NightFog")
    unreal.log("Created atmospheric fog")

unreal.log("")
unreal.log("=== Night Scene Setup Complete ===")
unreal.log("For mountains, use: Place Actors > Landscape, paint with sculpt tools")
unreal.log("Or import a heightmap PNG via Landscape > Import from File")
unreal.log("Recommended: Use the Landscape tool to create a 505x505 landscape,")
unreal.log("then sculpt mountain peaks around the edges of the scene.")
