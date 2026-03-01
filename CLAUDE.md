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

## What NOT to Do
- Don't use `UPROPERTY()` Blueprint-exposed variables for every field — keep internals C++ private
- Don't add new input bindings via legacy Input settings in Project Settings
- Don't commit directly to `main`
- Don't add `.uasset` without LFS — always verify `git lfs status` before pushing large additions
- Don't put code in `Content/` folder — C++ lives in `Source/`, BPs in `Content/Pirate/`
