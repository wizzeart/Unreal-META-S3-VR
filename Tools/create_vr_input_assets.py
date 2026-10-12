"""Create the four OpenXR button actions used by the house menu.

Run in Unreal Editor with -ExecutePythonScript=Tools/create_vr_input_assets.py.
"""

import unreal


ROOT = "/Game/HouseInput"
BUTTONS = (
    ("IA_HouseX", "OculusTouch_Left_X_Click", "House X"),
    ("IA_HouseY", "OculusTouch_Left_Y_Click", "House Y"),
    ("IA_HouseA", "OculusTouch_Right_A_Click", "House A"),
    ("IA_HouseB", "OculusTouch_Right_B_Click", "House B"),
)
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory(ROOT)


def ensure_asset(name, asset_class):
    path = f"{ROOT}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", asset_class)
        asset = TOOLS.create_asset(name, ROOT, asset_class, factory)
    if not asset or not isinstance(asset, asset_class):
        raise RuntimeError(f"Could not create {path}")
    return asset


context = ensure_asset("IMC_HouseButtons", unreal.InputMappingContext)
context.unmap_all()
context.set_editor_property("context_description", "House VR buttons")

for name, key_name, description in BUTTONS:
    action = ensure_asset(name, unreal.InputAction)
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    action.set_editor_property("action_description", description)
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    context.map_key(action, key)
    if not unreal.EditorAssetLibrary.save_loaded_asset(action):
        raise RuntimeError(f"Could not save {name}")

if not unreal.EditorAssetLibrary.save_loaded_asset(context):
    raise RuntimeError("Could not save IMC_HouseButtons")

unreal.log("HOUSE_INPUT_READY: X Y A B mapped to four boolean OpenXR actions")
