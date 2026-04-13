# GTopia Feature Patch Summary - 2026-04-13

## What Changed
- Added reference-style machine interaction flow for vending and Magplant-class blocks.
- Aligned dialog names and control flow with ReferensiSrc style:
  - `VendEdit`
  - `VendConfirm`
  - `Magplant`
- Added dialog builder primitives needed by reference dialogs:
  - `AddQuickExit`
  - `AddItemPicker`
- Added and wired tile machine data support for vending and Magplant-like blocks.
- Added Magplant remote position state to the player model.
- Added door use packet handling and checkpoint respawn updates.
- Fixed `WorldPathfinding` build issues for cross-platform compilation.

## Security / Stability Notes
- Machine interactions now revalidate world/tile state before applying changes.
- Item compatibility checks block unsupported items from vending and Magplant storage.
- Vending interactions use lock-based payment logic and re-check machine state before completing.
- Door and machine interactions are gated through world access checks.

## Gameplay Coverage
- Door interaction and local door routing.
- Checkpoint respawn updates on movement.
- Vending machine owner edit flow and buyer purchase flow.
- Magplant 5000, Unstable Tesseract, and Gaia Beacon item storage/collection flow.

## Build Fixes
- Fixed missing `Vector2Float` include in `WorldPathfinding.h`.
- Fixed missing `std::sqrt` include in `WorldPathfinding.cpp`.

## Validation Status
- Targeted file-level error checks passed for edited gameplay files.
- Full repository build was not executed in this session, so a complete end-to-end platform build is still recommended.

## Community Announcement Draft

GTopia just received a major gameplay parity update focused on classic machine interactions.

We aligned vending machine and Magplant behavior much closer to ReferensiSrc, including dialog flow, stock handling, item compatibility rules, and the expected machine messaging. Door interactions and checkpoint respawn logic were also tightened, and the pathfinding/build issue in the server pathing module was fixed for cross-platform compilation.

If you notice any edge cases while testing vending, Magplant, doors, or checkpoints, report them with a screenshot and the world name so we can verify and tune the final parity pass.
