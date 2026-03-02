# PirateGame — Claude Context

## Project
- **Engine:** Unreal Engine 5.7
- **Language:** C++ (with Blueprints for iteration/design)
- **Team:** 3 developers
- **Genre:** Open-world pirate game — ocean, ships, combat
- **Platform:** Windows (DX12 / SM6)

## Current State
- Based on UE5 Third Person template
- Three prototype variants in Source (Combat, Platforming, SideScrolling) — reference code, not final
- Default map: `/Game/ThirdPerson/Lvl_ThirdPerson`
- Pirate maps under `Content/Pirate/Maps/` (OceanTest, OpenWorldTest1)

## C++ Module
**Module:** `PirateGame` — `Source/PirateGame/`

**Build dependencies** (`PirateGame.Build.cs`):
- `EnhancedInput` — use this always, never legacy input
- `AIModule` + `StateTreeModule` + `GameplayStateTreeModule` — AI via StateTree
- `UMG` + `Slate` — UI

**Variant subdirectories** (template reference, adapt don't copy blindly):
- `Variant_Combat/` — AI controller, enemies, StateTree utility, EQS contexts, damageable actors
- `Variant_Platforming/` — basic platforming character
- `Variant_SideScrolling/` — side-scroll character, camera manager

**Base class:** `APirateGameCharacter` (abstract, UE spring arm + follow camera)

## Renderer / Engine Config
- Lumen GI enabled (`r.DynamicGlobalIlluminationMethod=1`)
- Virtual Shadow Maps enabled
- Ray Tracing enabled
- Substrate materials enabled (`r.Substrate=True`)
- No static lighting (`r.AllowStaticLighting=False`)
- Audio: 48kHz

## Git Setup
- **Git LFS** fully configured — 1,091 binary assets in LFS
- `.gitattributes` tracks: `*.uasset`, `*.umap`, all texture/audio/mesh/video formats
- Line endings: `.cpp/.h/.cs/.ini` → LF, `.bat` → CRLF
- New devs must `git lfs install` then fresh clone — do NOT pull into old clone

## Branching Strategy (to be enforced)
```
main          ← stable milestone builds only
dev           ← integration, always compiles
feature/xxx   ← individual work off dev
```
- Never commit directly to `main`
- `.umap` files cannot be merged — coordinate who owns the map each day
- Use `git lfs lock <file>` for map files when actively editing

## Asset Naming Conventions
| Prefix | Type |
|--------|------|
| `BP_`  | Blueprint |
| `SK_`  | Skeletal Mesh |
| `SM_`  | Static Mesh |
| `ABP_` | Anim Blueprint |
| `AM_`  | Anim Montage |
| `T_`   | Texture |
| `MT_`  | Material |
| `MI_`  | Material Instance |
| `WBP_` | Widget Blueprint |
| `NS_`  | Niagara System |
| `DT_`  | Data Table |
| `DA_`  | Data Asset |
| `AI_`  | AI Behavior (StateTree) |

## Content Folder Structure
```
Content/
  Pirate/
    Characters/     — player + NPC meshes, anims, BPs
    Ships/          — ship meshes, BPs, physics assets
    Ocean/          — water materials, Niagara, buoyancy
    Combat/         — weapons, projectiles, VFX
    UI/             — all WBP_ widgets
    Audio/
      SFX/
      Music/
    VFX/            — NS_ Niagara systems
    Maps/           — .umap files (OceanTest, OpenWorldTest1)
    Data/           — DT_ and DA_ assets
    Environment/    — rocks, props, islands
```

## Key Architecture Decisions
- **Input:** Enhanced Input only — no legacy `BindAxis`/`BindAction`
- **AI:** StateTree (already in project) — not Behavior Trees
- **UI:** UMG — design in Blueprint, logic in C++ via `UUserWidget` subclasses
- **GAS (Gameplay Ability System):** TBD — decide before writing combat code

## Common Commands
```bash
# Build (from project root)
# Use Rider or VS — or UAT:
Engine\Build\BatchFiles\Build.bat PirateGame Win64 Development "d:/Project/PirateGame/PirateGame.uproject"

# Check LFS status
git lfs ls-files | wc -l     # should be 1091+
git lfs status               # shows pending LFS changes

# Lock a map before editing
git lfs lock Content/Pirate/Maps/OpenWorldTest1.umap
git lfs unlock Content/Pirate/Maps/OpenWorldTest1.umap
```

## PirateMCP Plugin (Custom MCP Server)
**Status:** COMPLETE — all 10 steps done, plugin builds, bridge registered in Claude Code ✓

**Location:** `Plugins/PirateMCP/`
**Port:** 55558 (config: `Config/DefaultPirateMCP.ini`)
**Log category:** `LogPirateMCP`

**Architecture:** UE5 EditorSubsystem → TCP server on 55558 → JSON commands → game thread dispatch
**Python bridge:** `Scripts/pirate_mcp_bridge.py` (stdio↔TCP for Claude Code MCP integration)

**Implementation Progress:**
- ✅ Step 1: Plugin scaffold (uplugin, Build.cs, module .h/.cpp)
- ✅ Step 2: Core infra — PirateMCPBridge (EditorSubsystem, config, dispatch), PirateMCPServerRunnable (TCP listener, reconnect fix, buffer size fix)
- ✅ Step 3: Port 33 existing commands (Editor, Blueprint, BlueprintGraph + all 18 BlueprintGraph subfiles)
- ✅ Step 4: Undo/redo — FScopedTransaction on SpawnActor, DeleteActor, SetActorTransform
- ✅ Step 5: Ocean commands — ocean_set_material_scalar/vector, ocean_spawn_niagara, ocean_set_niagara_float/vector, ocean_list_water_bodies
- ✅ Step 6: DataTable commands — datatable_list/get_row_names/get_row/get_all_rows/set_row/delete_row
- ✅ Step 7: World/Level commands — world_get_info/open_level/list_levels/set_editor_camera/list_streaming_levels/load_streaming_level/unload_streaming_level
- ✅ Step 8: Ship/Actor extensions — actor_get_by_class/set_collision_profile/set_material_scalar_param/add_niagara_component
- ✅ Step 9: Python MCP bridge — Scripts/pirate_mcp_bridge.py (MCP stdio↔TCP, all 35+ tools, auto-reconnect)
- ✅ Step 10: Claude Code registration — ~/.claude/settings.json updated, Scripts/pirate_mcp_bridge.bat created

**Command categories being built:**
- Editor (actor spawn/delete/transform) — porting from UnrealMCP
- Blueprint (create/compile/modify) — porting from UnrealMCP
- Ocean (`ocean_*`) — material params, Niagara VFX
- DataTable (`datatable_*`) — read/write DT rows
- World (`world_*`) — level open, camera, streaming levels
- Ship/Actor extensions (`actor_*`) — get by class, collision, material params

**Build command (correct format):**
```bash
"D:/unreal/UE_5.7/Engine/Build/BatchFiles/Build.bat" PirateGameEditor Win64 Development -Project="d:/Project/PirateGame/PirateGame.uproject" -WaitMutex
```
Note: Config is `Development` not `DevelopmentEditor` — the target name `PirateGameEditor` already implies editor.

**UnrealMCP** (original, Flopperam) stays in `Plugins/UnrealMCP/` on port 55557 — untouched.

## What NOT to Do
- Don't use `UPROPERTY()` Blueprint-exposed variables for every field — keep internals C++ private
- Don't add new input bindings via legacy Input settings in Project Settings
- Don't commit directly to `main`
- Don't add `.uasset` without LFS — always verify `git lfs status` before pushing large additions
- Don't put code in `Content/` folder — C++ lives in `Source/`, BPs in `Content/Pirate/`
