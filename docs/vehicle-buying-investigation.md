# Vehicle Buying (Campaign Building Mode) — Investigation Notes

## Status: Blocked — vanilla building system is incompatible with COALITION-Lobby's editor activation path

This document records the investigation into why the vanilla "buy vehicle" building-mode
action (`SCR_CampaignBuildingStartUserAction` on the command trucks' `_combox` child
entities) does not work after migrating from PSCore to COALITION-Lobby. Kept so the next
attempt doesn't repeat several hours of dead ends.

## Symptom

Standing near the OPFOR/BLUFOR command truck as commander, the vanilla "Start Building"
interaction prompt was initially invisible entirely. After a series of fixes it became
visible and clickable, but clicking it opens the editor for a single frame and then
immediately crashes/closes with:

```
Virtual Machine Exception
Reason: Recovering of SCR_CampaignBuildingBudgetEditorComponent was unable to find
SCR_CampaignBuildingEditorComponent on SCR_EditorModeEntity<0x...> @"ENTITY:..."
('SCR_EditorModeEntity') at <0,0,0> @"{E8DD27917B43F8BD}Prefabs/Editor/Modes/EditorModeBuilding.et"
and because of that editor will be closed now!

Class:      'SCR_CampaignBuildingBudgetEditorComponent'
Function:   'CheckAndRecoverCampaignBuildingComponent'
Stack trace:
Scripts/Game/Editor/Components/Editor/SCR_CampaignBuildingBudgetEditorComponent.c:86 CheckAndRecoverCampaignBuildingComponent
Scripts/Game/Editor/Components/Editor/SCR_CampaignBuildingBudgetEditorComponent.c:305 GetProviderMaxValue
Scripts/Game/Editor/Components/Editor/SCR_CampaignBuildingBudgetEditorComponent.c:205 GetMaxBudgetValue
Scripts/Game/Editor/Components/Editor/SCR_CampaignBuildingBudgetEditorComponent.c:458 OnEntityCoreBudgetUpdatedOwner
Scripts/Game/Editor/Components/Editor/SCR_BudgetEditorComponent.c:626 OnEntityCoreBudgetUpdated
Scripts/Game/Editor/Core/SCR_EditableEntityCore.c:992 ApplyQueuedBudgetChanges
Scripts/Game/game.c:872 OnUpdate
```

Reproduced identically on repeated attempts, including immediately retrying a second time
after the first crash — same entity address (`0x0000020E23F1C520` /
`ENTITY:4611686018427388520`) every time, same failure. **This rules out a one-time
first-open timing race** — the mode entity persists across attempts but never successfully
resolves `SCR_CampaignBuildingEditorComponent` on itself.

## What was ruled out (confirmed, not speculation)

- **Faction mismatch** — was a real, separate bug, now fixed. BC's spawn code was setting
  `FactionAffiliationComponent`/`SCR_FactionAffiliationComponent` using BC's own legacy
  faction keys (`"USSR"`/`"US"`, tied to `Configs/Factions/USSR_Campaign.conf`, a distinct
  `SCR_CampaignFaction` resource from COALITION's own faction setup) instead of the keys
  COALITION's slotting system actually assigns players (`"OPFOR"`/`"BLUFOR"`). Fixed in
  `GRAD_BC_BreakingContactManager.c` (`SetVehicleAndChildrenFaction` calls now pass
  `"OPFOR"`/`"BLUFOR"`). This alone got `CanBeShownScript` to return `true` and made the
  action visible/clickable — necessary but not sufficient.
- **Component placement** — `SCR_CampaignBuildingProviderComponent` genuinely exists on the
  vehicle prefabs' `_combox` child entities (`Ural4320_combox.et` / `M923A1_combox.et`),
  confirmed visually in Workbench. Unchanged from `main` (prefabs are binary/untouched by
  the migration, confirmed via git diff of the whole branch).
- **`SCR_CampaignBuildingManagerComponent.c`** (BC's own resynced copy of the vanilla
  gamemode-sibling component) — byte-identical between `main` and this branch.
  `EOnInit`/`EnterEditorMode`/`SetEditorMode` all read as expected; no BC-side bug found.
- **Rank gate** (`m_ProviderComponent.GetAccessRank() > GetUserRank(user)`, requires
  Captain) — commander already has rank 7 (well above Captain) via COALITION's own
  `COA_COY.et` prefab. A `GrantCommanderRank` helper was added to
  `GRAD_PlayerComponent.c` as defense-in-depth (RPC'd to server, sets
  `SCR_CharacterRankComponent` to `CAPTAIN` if below), but was never actually the blocker.
- **`SCR_CompartmentAccessComponent`, `m_MainParent`, vehicle velocity, damage state,
  `char.IsInVehicle()`** — all checked via temporary debug overrides in
  `GRAD_BC_VehicleSpawnAction.c` (`CanBeShownScript`/`CanBePerformedScript`), all resolve
  to expected/passing values. Not the blocker.
- **`SCR_EntityCatalogManagerComponent` missing from the gamemode** — flagged early as a
  suspect (was absent from both `main`'s `PS_GameModeCoop` and this branch's `COA_Lobby`
  for the whole session), since a related vanilla warning
  (`'SCR_BaseResupplySupportStationComponent' needs a entity catalog manager!`) appears in
  logs. Added to `COA_Lobby` — did not change the crash at all (confirmed same error
  message before and after).
- **`SCR_EditorSettingsEntity`'s `Editor Manager Prefab` field** (was empty for the whole
  session) — tried assigning it to `EditorModeBuilding.et` directly (wrong — that's a single
  mode's entity prefab, not a manager container) and then to "a general editor-manager
  prefab" found in Workbench. Neither changed the crash outcome (same exact error message
  both before and after, and with the field empty).
- **`Base Modes` / `Override Base Modes`** on `SCR_EditorSettingsEntity` — `BUILDING` is
  checked, `Override Base Modes` is checked. This does matter for `CanOpen()` (COALITION's
  own `m_Modes.IsEmpty()` gate) but not for the crash itself, which happens after the
  editor has already opened.
- **The vanilla `EditorModeBuilding.et` prefab is missing `SCR_CampaignBuildingEditorComponent`**
  — directly disproven. The user inherited a copy of the vanilla prefab in Workbench
  (vanilla prefabs can't be edited directly, only inherited) specifically to inspect its
  component list, and `SCR_CampaignBuildingEditorComponent` is present on it without any
  edits. The component is NOT missing from the prefab definition.
- **One-time first-open race** — disproven by the identical-entity-address retry test
  above.

## Root cause (best understanding, not fully proven — vanilla internals are closed-source)

Fetched actual vanilla source for `SCR_CampaignBuildingBudgetEditorComponent.c` via a
public Doxygen-style mirror (arexplorer.zeroy.com). Key structure:

```
protected SCR_CampaignBuildingEditorComponent m_CampaignBuildingComponent;
// ...
protected bool CheckAndRecoverCampaignBuildingComponent()
{
    if (m_CampaignBuildingComponent)
        return true;                                   // cached-pointer fast path

    m_CampaignBuildingComponent = SCR_CampaignBuildingEditorComponent.Cast(
        FindEditorComponent(SCR_CampaignBuildingEditorComponent, true, true));

    if (!m_CampaignBuildingComponent)
        // ... logs the "Recovering of..." error, closes editor
}
```

`m_CampaignBuildingComponent` is a cached sibling-component pointer on the SAME
`SCR_EditorModeEntity`. `FindEditorComponent(...)` is a sibling lookup, not a
different-entity/prefab resolution — so this really is "the component isn't attached to
THIS live entity instance," despite existing on the prefab it was supposedly created from.

Since retrying against the exact same persistent entity instance fails identically every
time (not just once), the working theory is that whatever code path COALITION's modded
`SCR_EditorManagerEntity.ToggleServer()` uses to activate/instantiate the building mode
entity is **not fully instantiating the prefab's component list** — i.e., `CreateEditorMode()`
under COALITION's custom RPC-driven activation flow produces an entity that never actually
gets `SCR_CampaignBuildingEditorComponent` attached, even though the prefab it *claims* to
be instantiated from (per the debug string in the crash) has it.

COALITION's own `ToggleServer()` override (`Scripts/Game/!Systems/VanillaOverrides/Managers/COA_SCR_EditorManagerEntity.c`)
always takes the custom path (never falls through to `super.ToggleServer()`, since
`COA_Gamemode.GetInstance()` is always true under this gamemode) and calls
`m_CurrentModeEntity.ActivateModeServer()` directly inline, inside an
`[RplRpc(RplChannel.Reliable, RplRcver.Server)]`-marked method. This is plausibly a
different instantiation/replication context than vanilla's own (un-modded) activation flow
would use, and is the most likely proximate cause — though this specific mechanism was not
confirmed with certainty (would require decompiling/stepping through vanilla's actual
`SCR_EditorManagerEntity.Open()`/mode-entity-instantiation code, which wasn't accessible
this session).

No public documentation of this exact crash was found (checked Bohemia's feedback
tracker, forums, Reddit, COALITION-Lobby's own GitHub issues — only 3 issues total, none
related). This appears to be previously unreported — plausible since COALITION-Lobby has
its own gearscript/loadout system and doesn't use vanilla building/composition purchasing
itself, so nobody using COALITION "as intended" would ever hit this path.

## Changes made this session (kept, working)

- `GRAD_BC_BreakingContactManager.c` — `SetVehicleAndChildrenFaction()` helper, walks the
  spawned command truck's root + all children setting
  `FactionAffiliationComponent.SetAffiliatedFactionByKey()` to `"OPFOR"`/`"BLUFOR"` (the
  actual COALITION faction keys, not BC's legacy `"USSR"`/`"US"`). Necessary because
  `SCR_CampaignBuildingProviderComponent`'s faction check lives on the `_combox` child
  entity specifically, not the vehicle root, and does not walk up the parent chain.
- `M923A1_combox.et` / `Ural4320_combox.et` — `SCR_FactionAffiliationComponent` added
  directly to both prefabs in Workbench (the base `FactionAffiliationComponent` class
  alone was insufficient/wrong — `SCR_FactionAffiliationComponent` is the subclass that
  actually resolves correctly via `SCR_FactionManager.SGetFaction()`).
- `GRAD_PlayerComponent.c` — `GrantCommanderRank`/`Ask_GrantCommanderRank`/
  `DoGrantCommanderRank`, ensures the commander has at least `CAPTAIN` rank server-side.
  Not the blocker for this specific bug, but a real gap (BC has no rank/XP progression of
  its own) worth keeping regardless.
- `test_mode_2.layer` (kolgujev) — added `SCR_EntityCatalogManagerComponent` to `COA_Lobby`
  (was missing entirely, both before and after the COALITION migration). Silenced a
  separate pre-existing vanilla warning
  (`'SCR_BaseResupplySupportStationComponent' needs a entity catalog manager!`); did not
  fix this crash, but is a legitimate gap-fill regardless and should stay.
- `SCR_EditorSettingsEntity` (`COA_Lobby` sub-entity) — `Editor Manager Prefab` field
  assigned to a general editor-manager prefab (was empty). `Override Base Modes` + `BUILDING`
  checked under `Base Modes`. Neither resolved the crash but both are plausibly still
  correct/necessary configuration and were left in place.
- `GRAD_BC_VehicleSpawnAction.c` — currently contains TEMPORARY DEBUG overrides
  (`CanBeShownScript` logging faction/compartment/velocity/damage-state; no override on
  `CanBePerformedScript` currently). **Should be cleaned up / reverted to the no-op modded
  class before this is considered done** — left in for now since the investigation isn't
  finished.

## Follow-up: a more precise root-cause theory (found after the decision to go native, kept for reference)

After deciding to abandon the vanilla system, the user shared the actual vanilla source for
`SCR_CampaignBuildingBudgetEditorComponent` and `SCR_CampaignBuildingEditorComponent`
(fetched from a public Doxygen mirror is not needed here — user pasted the real class
bodies directly). This surfaced a more specific and better-supported theory than the
earlier "component never registers" guess:

`SCR_CampaignBuildingBudgetEditorComponent.EOnEditorActivateServer()` does:
```
array<ref SCR_RankInfo> ranks = factionManager.GetFactionRanks(m_Manager.GetPlayerID()).GetAllRanks();
if (!ranks)
    return;   // <-- bails BEFORE ever calling RefreshBudgetSettings()
```
`RefreshBudgetSettings()` is what populates `m_BudgetSettingsMap` and is the only place that
calls `CheckAndRecoverCampaignBuildingComponent()` under normal (non-crash) circumstances to
establish the `m_CampaignBuildingComponent` cache server-side. If `GetFactionRanks(playerID)`
returns null/empty — plausible if COALITION's OPFOR/BLUFOR faction resources have no rank
ladder defined, since COALITION uses its own gearscript-role system
(`COA_GearscriptManager`/`COA_EGearRole`) instead of vanilla rank/XP progression — then this
whole method silently no-ops, budget settings never get set up server-side, and the later
deferred `ApplyQueuedBudgetChanges` → `OnEntityCoreBudgetUpdatedOwner` → `GetProviderMaxValue`
→ `CheckAndRecoverCampaignBuildingComponent()` call (a *different* call site than the one
inside `RefreshBudgetSettings()`) fails with the crash, because nothing ever put things in a
working state to begin with. This is consistent with the persistent-not-one-time nature of
the crash (same entity, same failure, every retry).

Checked COALITION-Lobby's own modded `SCR_Faction`/`SCR_FactionManager` overrides
(`Scripts/Game/!Systems/VanillaOverrides/COA_SCR_Faction.c`,
`Scripts/Game/!Systems/VanillaOverrides/Managers/COA_SCR_FactionManager.c`) — neither
contains any rank-related logic at all (COA_SCR_Faction.c is entirely about VoN radio
channel assignment). This doesn't prove the underlying `.conf` faction resource has an
empty rank list (data configs aren't fully visible via the GitHub source tree), but the
total absence of rank-handling code anywhere in COALITION's own scripts is consistent with
them never populating or relying on this system.

**Not verified further** — this was found after already committing to the native-replacement
decision, and time/session constraints meant this wasn't pursued to a confirmed fix. If
someone wants to revisit the vanilla-patch path later: check whether COALITION's `OPFOR.conf`
(or equivalent) `SCR_Faction` resource has `m_aRanks`/rank-ladder data populated, and if not,
whether adding a minimal rank ladder resolves `EOnEditorActivateServer`'s early return and
the resulting crash. If confirmed, this could be a small, targeted fix rather than a full
native rebuild.

## Decision

Continuing to fight COALITION's editor-activation path is not obviously a quick fix — the
failure is inside two different closed-source/third-party code paths (vanilla's building
editor internals AND COALITION's modded activation override) with no public prior art.
Decision: **stop pursuing the vanilla building/composition-purchase system entirely** and
build a BreakingContact-native vehicle-purchase action instead — keep the "funds/budget"
concept (BC already has `GRAD_BC_VehicleSupplyComponent` for a supply-currency system, used
elsewhere for the building-refund flow), but implement the actual purchase UI/flow
ourselves rather than routing through `SCR_EditorManagerEntity`/`EEditorMode.BUILDING` at
all. Scope for that native replacement is a separate, follow-up planning task — expected to
be non-trivial (needs its own UI, its own vehicle-catalog definition, its own spawn/refund
logic), not a quick patch.
