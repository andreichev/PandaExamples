# Neverland Roadmap

## Goal

Make Neverland feel more like a complete playable voxel demo while keeping the current infinite-world pipeline stable.

## Order

### 1. Held item and HUD selection

- Add first-person hands and show the selected block in the player's hand.
- Improve the hotbar so selected blocks are represented by their actual block texture/preview, not only a color.
- Keep selection owned by gameplay (`BlocksCreation`) and make HUD/held-view read from the same selected item state.
- Prepare the model for future non-block items such as torch and campfire.

Start here because it is low-risk, immediately improves game feel, and gives the next gameplay features a visible item pipeline.

### 2. Torch, campfire, and gameplay lighting

- Add torch and campfire as placeable items/blocks.
- Spawn point lights for them and keep light entities synchronized with block placement/removal.
- Add simple visual meshes and warm flicker.
- Avoid engine changes unless the current light API blocks the feature.

### 3. World persistence

- Save world seed and settings.
- Save voxel edits as deltas over procedural generation instead of serializing all generated chunks.
- Save player position and placed gameplay entities such as torches/campfires.
- Add versioning to the save format early.

### 4. Main menu and world management

- Add a main menu with continue, create world, load world, and quit.
- Create world profiles with name, seed, and settings.
- Load the selected profile before gameplay starts.
- Keep menu UI isolated from in-game HUD to reduce future conflicts.

### 5. Marching cubes terrain experiment

- Prototype marching cubes as an optional terrain/mesh mode, not as a direct replacement for the current voxel pipeline.
- Evaluate impact on chunk meshing, region LOD, collision, raycast, block placement, and save deltas.
- Decide later whether it becomes a production terrain mode or remains a visual experiment.

Do this last because it changes core world assumptions and can easily destabilize the currently working demo.

