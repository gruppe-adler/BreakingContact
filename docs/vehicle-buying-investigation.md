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

## Decision (superseded — see below)

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

## Native-replacement attempt: `SCR_CatalogEntitySpawnerComponent` (tried, abandoned)

Following the decision above, investigated and partially implemented a real vanilla vehicle
purchasing system that is architecturally independent of Campaign Building:
`SCR_CatalogEntitySpawnerComponent` (inherits `SCR_MilitaryBaseLogicComponent`, a **sibling**
of `SCR_CampaignBuildingProviderComponent`, not touching `SCR_EditorManagerEntity`/
`EEditorMode.BUILDING`/`SCR_CampaignBuildingEditorComponent` at all). Confirmed via real
vanilla source (user pasted the full class + its companion `...Class` attribute holder) that
its supply/cost system genuinely reads from the same `SCR_ResourceComponent`/
`EResourceType.SUPPLIES` container BC's `GRAD_BC_VehicleSupplyComponent` already uses — no
coupling to the broken Campaign Building budget system.

**What was set up in Workbench this session** (later fully reverted — see below):
- `SCR_CatalogEntitySpawnerComponent` added to both `_combox` prefabs (`Catalog Types:
  VEHICLE`, `Allowed Labels: FACTION_US`/`FACTION_USSR`, `Supplies Consumption Enabled:
  false` for initial testing, base-logic `Type: ARMORY`).
- Removed the dead `SCR_CampaignBuildingStartUserAction` and `SCR_CampaignBuildingProviderComponent`
  from both `_combox` prefabs (since they were no longer needed for this approach).
- Removed a leftover `SCR_CampaignBuildingBudgetEditorComponent` that had been directly
  (and incorrectly) attached to `COA_Lobby` itself during earlier investigation — this
  component belongs on an editor-mode entity, never on the gamemode.
- Added an empty child entity per truck with `SCR_EntitySpawnerSlotComponent` as the spawn
  marker (needed its own `RplComponent` — confirmed via runtime warning
  `SCR_EntitySpawnerSlotComponent is missing RplComponent. It won't work properly without it`).
- Added `SCR_CatalogSpawnerUserAction` to each truck's `ActionsManagerComponent`.
- Investigated how to wire an actual vehicle list in. Confirmed empirically in Workbench
  (by duplicating existing catalog resources to reveal their real inherited vanilla content,
  since the `.conf` files as authored are sparse GUID-toggle overrides on top of vanilla
  base-game catalogs, not real content) that: the real field name is `m_sEntityPrefab`
  (not `m_sPrefab`), `SCR_EntityCatalogSpawnerData.m_eSlotTypes` takes named flag values
  (`VEHICLE_SMALL`/`VEHICLE_MEDIUM`/`VEHICLE_LARGE`, single-select per slot — a slot only
  matches vehicles whose allowed-types include that exact flag, there is no size hierarchy),
  and — most importantly — **the vehicle catalog lives on the faction resource itself**
  (`SCR_CampaignFaction.m_aEntityCatalogs`), not on the gamemode's
  `SCR_EntityCatalogManagerComponent` (that component's own `Multi Lists` field wants
  inline-authored entries, not a `.conf` file reference, and turned out to be an
  unrelated/non-faction-scoped fallback catalog list). Vanilla's own USSR/US factions
  already ship with a real, populated, working vehicle catalog (14+ enabled entries with
  cost/slot data) inherited by BC's faction overrides for free.

**Why abandoned**: got as far as an interaction prompt appearing (blank/untitled — never
got as far as confirming why the title didn't resolve) but hit diminishing returns and
mounting complexity (RplComponent gaps, empty-catalog debugging, a confusing UI overlap
where an unrelated existing action appeared to fire instead) without a clean, working
end-to-end result. The user made the call to stop layering a second uncertain system on
top of an already-uncertain one ("we changed a flaky solution with a flaky solution") and
go back to making the *original* vanilla Campaign Building path work under `COA_Gamemode`,
since that's the system `main` already used successfully pre-migration.

**Reverted**: none of the catalog-spawner Workbench edits were ever actually saved to disk
(confirmed via `git status` showing zero file changes despite the extensive in-session
Workbench work) — restarting Workbench fresh from the last commit (`b657b33`) was
sufficient to fully discard the whole detour with no manual cleanup needed.
`GRAD_BC_VehicleSpawnAction.c` was manually reverted back to the no-op
`SCR_CampaignBuildingStartUserAction` modded class before the Workbench restart.

## Critical new finding: the crash is NOT triggered by opening the building menu

Re-confirmed the original Campaign Building crash still occurs after the full revert to
baseline (expected — nothing about the underlying bug changed). But this time, systematic
debug logging (see below) revealed something the whole investigation had assumed wrong:

**The crash fires ~78ms after the player joins the Spectator faction at mission load —
before any player has walked near a truck or clicked anything.** Confirmed by grepping
timestamps: `has joined faction Spectator (SPEC)` at `17:45:42.155`, first
`Virtual Machine Exception` at `17:45:42.233`, and it recurs constantly (dozens of times)
throughout the entire mission on the exact same `SCR_EditorModeEntity` address
(`ENTITY:4611686018427388520`, stable across the whole session) — every single frame's
`ApplyQueuedBudgetChanges()` pass appears to re-trigger it.

This means the earlier working theory (COALITION's `ToggleServer()` override breaking
component registration specifically during a player-initiated `EnterEditorMode()` call) is
likely wrong, or at least incomplete — **this `SCR_EditorModeEntity` instance already
exists and is already being processed by the budget-queue system before any player ever
requests building mode.** The user separately confirmed that walking up to a truck and
using the action DOES cause a building-menu UI to visibly flash open before instantly
closing — so the player-triggered path is real and additionally exercises this same crash,
but the crash is clearly not gated behind that trigger.

New leading hypothesis: enabling `BUILDING` under `COA_Lobby`'s `SCR_EditorSettingsEntity`
→ `Base Modes` (done earlier this session, still enabled) may cause
`SCR_EditorManagerEntity` to eagerly pre-instantiate the `EditorModeBuilding.et` mode
entity at mission start (as part of initializing all configured base modes), rather than
lazily creating it on first player request as vanilla's un-modded flow presumably does.
This eagerly-created instance may never go through the same initialization sequence a
lazily-created one would, explaining why `SCR_CampaignBuildingEditorComponent` never
finishes registering on it. **Test in progress**: disabling `BUILDING` in `Base Modes`
to see if the mission-start crash disappears entirely.

**Debug instrumentation added** (`GRAD_BC_VehicleSpawnAction.c`, temporary, still in place):
- `modded class SCR_CampaignBuildingStartUserAction.CanBePerformedScript` — logs the
  player's resolved rank (`SCR_CharacterRankComponent.GetCharacterRank`) and whether
  `SCR_FactionManager.GetFactionRanks(playerId)` returns a non-null container (tests the
  earlier rank-ladder hypothesis; return type unknown/unverified so captured as `Managed`
  to avoid a repeat of an earlier compile break from guessing an unverified type).
- `modded class SCR_CampaignBuildingEditorComponent.EOnEditorActivate` /
  `EOnEditorPostActivate` — logs entry/exit to check whether and when this component
  actually attaches/registers on the live mode entity.
- **First test after adding this logging showed NONE of these debug lines fired at all**,
  despite the crash still occurring and despite a confirmed clean compile — most likely
  because the Play session being tested against was already running from before the
  `wb_reload`, i.e. a stale-build false negative, not evidence the code paths aren't hit.
  `wb_reload` recompiles scripts but does not appear to hot-swap an already-running Play
  session; a full stop + relaunch of Play mode is needed to pick up new script code.
  **This "stale build" lesson recurred multiple times this session — always fully stop and
  relaunch Play mode after any script change before trusting a test result.**

## ROOT CAUSE FOUND AND FIXED: missing `Faction Rank Info` on OPFOR/BLUFOR factions

After the stale-build issue was resolved and testing continued with correctly-scoped debug
hooks (moved onto the actual crashing class, `SCR_CampaignBuildingBudgetEditorComponent`,
rather than `SCR_CampaignBuildingStartUserAction`/`SCR_CampaignBuildingEditorComponent`
which never appear in the crash's own stack trace), the real root cause was found and
**confirmed via the actual vanilla + COALITION source, not just log inference**:

**`SCR_Faction.m_FactionRankInfo`** (`[Attribute(desc: "List of ranks")] protected ref
SCR_RankContainer m_FactionRankInfo;`) has **no default value** in the class declaration.
`GetRanks()` just returns this field directly with no null-check:
```
SCR_RankContainer GetRanks() { return m_FactionRankInfo; }
```
`SCR_FactionManager.GetFactionRanks(playerId)` calls `faction.GetRanks()` for the player's
*actual* faction (falling back to `m_DefaultRanks` only if the player has no faction *at
all* — not relevant here, the player has a real faction) — confirmed via full vanilla
`SCR_FactionManager.c` source (user pasted it). COALITION's own `modded class
SCR_FactionManager`/`modded class SCR_Faction` overrides (both pasted, user has direct
access to COALITION's source) touch only VoN radio-channel bookkeeping — **neither touches
rank data at all**, confirming COALITION doesn't populate or break this field itself; it's
simply never configured for BC's setup.

**BC's players are affiliated with COALITION's own inline `SCR_Faction` entities**
(`OPFOR`/`BLUFOR`, defined directly inside `COA_Lobby`'s `SCR_FactionManager` component in
`test_mode_2.layer`, as standalone `SCR_Faction` instances with **no parent `.conf`
reference at all** — confirmed by inspecting them directly in Workbench, `Faction Key:
BLUFOR` field visible, no inheritance chain to any vanilla base faction file) — **not**
BC's own legacy `Configs/Factions/USSR_Campaign.conf`/`US_Campaign.conf` (which DO inherit
from vanilla `USSR.conf`/`US.conf` and likely have real rank data, but are unused/dead
resources under COALITION's slotting system). Since COALITION's inline `SCR_Faction`
entities have no parent to inherit ranks from, and nothing ever explicitly set their
`Faction Rank Info` field, it was confirmed **directly and visually in Workbench**: the
field showed literal placeholder text `"set class"` — genuinely never assigned, not merely
defaulting to something reasonable.

**Fix applied**: assigned a real `SCR_RankContainer` to `Faction Rank Info` on the BLUFOR
`SCR_Faction` entity (nested in `COA_Lobby`'s `SCR_FactionManager` in `test_mode_2.layer`),
populated with two entries — `PRIVATE` (regular soldiers) and `CAPTAIN` (commander,
matching the command truck's existing `SCR_CampaignBuildingProviderComponent.Rank:
CAPTAIN` requirement exactly) — deliberately NOT the full vanilla 7-tier ladder, since BC
has no rank/XP progression system and the only distinction that matters is
commander-vs-not. **Confirmed via debug logging this completely eliminates the
`EOnEditorActivateServer` NULL pointer crash** (`hasRankContainer=1`, full method executes
to `EXIT (super returned, no crash)`, repeatable across multiple fresh test sessions).
**The OPFOR faction entity needs the identical fix** (same `SCR_FactionManager` component,
different `SCR_Faction` entry with `Faction Key: OPFOR`) — not yet done as of this
writing, only BLUFOR was fixed and tested.

## Second, independent bug found and fixed: eager mode-entity creation via `Base Modes`

Separately, a second and completely distinct crash was found and understood: the *original*
`CheckAndRecoverCampaignBuildingComponent` "Recovering..." crash (as opposed to the
`EOnEditorActivateServer` NULL-pointer crash above) was firing automatically at mission
start — confirmed by exact timestamp correlation, ~78ms after `has joined faction
Spectator (SPEC)`, completely independent of any player walking near a truck or clicking
anything, recurring dozens of times throughout an entire mission on one stable entity
address. Root cause: `SCR_EditorSettingsEntity.m_BaseModes`/`m_bOverrideBaseModes`
(confirmed via its real vanilla source, doc comment literally says *"Editor modes added to
every player when they connect"*) had `BUILDING` checked — this eagerly creates/registers
the building mode entity for every connecting player at connect time, bypassing the normal
on-demand `SCR_CampaignBuildingManagerComponent.EnterEditorMode()` → `CreateEditorMode()`
path entirely, and entities created this way never go through
`SCR_CampaignBuildingEditorComponent`'s real activation lifecycle (confirmed:
`EOnEditorActivate` never fires even once on the eagerly-created instance, despite firing
correctly on properly on-demand-created instances once this was fixed).

**Fix applied**: unchecked `BUILDING` in `COA_Lobby`'s `SCR_EditorSettingsEntity` → `Base
Modes`. **Confirmed this alone completely eliminates the mission-start crash** (zero
`CampaignBuildingBudgetEditorComponent` matches in a full fresh-session log after the fix).
**Re-tested re-enabling `BUILDING` after the rank-info fix (above) was already in place —
the eager-creation crash returned identically**, confirming these really are two fully
independent bugs, not two symptoms of one root cause. `Base Modes`/`BUILDING` must stay
unchecked; it was never the correct/needed mechanism for enabling the truck-triggered
on-demand building flow in the first place (that's `EnterEditorMode()`'s job, unrelated to
this global connect-time default-modes setting).

## Current status: backend confirmed clean, but menu still doesn't work for the user

With both fixes in place (BLUFOR rank info populated, `Base Modes`/`BUILDING` unchecked),
extensive debug logging across the *entire* activation chain
(`SCR_CampaignBuildingStartUserAction` → `EOnEditorActivateServer` →
`SCR_CampaignBuildingEditorComponent.EOnEditorActivate` → `EOnEditorPostActivate`) shows
**zero errors, zero crashes, correct provider registration (`providerCount=1`,
`hasProviderComponent=1`), and confirmed successful UI layout loading**
(`RESOURCES: GetResourceObject @"...UI/layouts/Editor/Modes/Mode_CampaignBuilding.layout"`,
followed by `GUI: WidgetManager: CrateWidgets` for the same layout — the real building-menu
UI genuinely instantiates). No `Close()`/`EOnEditorDeactivate` call was ever observed in
logs following a successful open in the tests run so far.

**Despite this, the user reports the menu still "instantly closes" and appears
unpopulated when actually testing in-game.** This is now a confirmed contradiction between
what the logs show (clean, successful activation with no teardown) and what's observed
visually — meaning either:
- The close is happening at a layer with no server-authoritative script logging (pure
  client-side rendering/widget issue, input-binding conflict where the same key that opens
  also closes, or a UI-only close path that doesn't route through
  `SCR_CampaignBuildingEditorComponent.EOnEditorDeactivate()`), or
- The specific test run(s) where "instantly closes" was reported don't actually correspond
  to the clean logs being read (timing/session mismatch — a recurring risk given how many
  times stale-build/stale-log mismatches happened earlier this session).

**Debug hooks added but not yet tested** (still in `GRAD_BC_VehicleSpawnAction.c` as of
this writing):
- `modded class SCR_CampaignBuildingEditorComponent.EOnEditorDeactivate` — logs
  entry/exit, to directly catch the real in-script teardown if one exists.
- `modded class SCR_EditorManagerEntity.CanOpen` — logs the result and `GetCurrentMode()`;
  **confirmed via log grep this NEVER fires**, ruling out `CanOpen()`/COALITION's
  `m_Modes.IsEmpty()` gate as the cause of anything observed so far (it's simply not on the
  path being exercised by this specific open flow).
- An attempted `override void Close()` on `SCR_EditorManagerEntity` caused a genuine
  compile error (`Close` is not overridable/virtual — confirmed by the compiler, not
  removed speculatively) and was reverted; this was an unverified guess, not confirmed from
  real source, unlike the other hooks in this file.

**Unrelated crash discovered while testing**: a hard engine-level assertion (`Condition
'!BadMatrix43(val)' has not been met` — indicates a degenerate/NaN transform matrix
somewhere) in `GRAD_BC_BreakingContactManager.OnBreakingContactPhaseChanged`, reached via
`ConfirmSpawn` → `RequestInitiateOpforSpawn` → `SetBreakingContactPhase` (BC's own OPFOR
spawn/phase-transition code, not COALITION or vanilla). This froze the Workbench session
with a native Windows dialog (not a script-catchable exception) and is **completely
unrelated to the building-menu investigation** — flagged here only because it interrupted
the most recent test session before a fresh result could be captured. Needs its own
separate investigation if it recurs; not pursued further this session.

## UPDATE: `EOnEditorDeactivate` CONFIRMED to fire — real in-script teardown found

A follow-up test (fresh Play session, `EOnEditorDeactivate` hook already in place) answered
the open question directly: **`EOnEditorDeactivate` DOES fire**, immediately after the
widget layout finishes loading:

```
GUI          : WidgetManager: CrateWidgets @"...Mode_CampaignBuilding.layout"
   SCRIPT    (W): Duplicate instance of UI component ... found on widget 'MapFrame'! ...
   SCRIPT    (W): Slot_Chat Has no Content!
   SCRIPT    (W): Slot_VON Has no Content!
   SCRIPT    (W): Slot_AvailableActions Has no Content!
   SCRIPT    (W): Slot_GameVersion Has no Content!
WORLD        : UpdateEntities
 WORLD        : Frame
  SCRIPT    (W): BC Debug - CampaignBuildingEditorComponent.EOnEditorDeactivate: ENTER owner=...
  SCRIPT    (W): BC Debug - CampaignBuildingEditorComponent.EOnEditorDeactivate: EXIT
```

This **confirms the "instantly closes" symptom is a real, in-script teardown** — not a
client-rendering/input illusion as hypothesized. The menu genuinely opens, fully loads its
widget layout, and is then immediately deactivated in the same or next frame.

**New concrete lead**: the `CanOpen()` debug hook (previously observed to never fire —
that was wrong, it just hadn't been captured in earlier shorter log excerpts) fires twice
around this sequence, with **different `GetCurrentMode()` values**:
- On open: `BC Debug - SCR_EditorManagerEntity.CanOpen: result=1, currentMode=32`
- Immediately after the `EOnEditorDeactivate` sequence: `BC Debug -
  SCR_EditorManagerEntity.CanOpen: result=1, currentMode=1`

`EEditorMode` is a flags enum (bit values); `32` and `1` are different single-bit values
(almost certainly `BUILDING` and something like `EDIT`/a base mode respectively, exact
mapping not yet confirmed — needs checking the actual `EEditorMode` enum values). This
strongly suggests **the editor's current mode is being switched away from `BUILDING`
immediately after it activates**, and this mode-switch is what triggers
`EOnEditorDeactivate` as a side effect — not an explicit "close the whole editor" call.
`result=1` (true) both times, so `CanOpen()` itself is NOT rejecting anything either time —
ruling it out as a blocking gate entirely, in either direction.

**Not yet investigated**: what actually triggers the mode switch away from `BUILDING`
right after opening. Candidates not yet checked:
- Something in COALITION's own `ToggleServer()`/mode-switch logic defaulting back to a
  base mode immediately after any non-base mode activates (possible interaction with the
  `Base Modes` mechanism again, in a different way than the eager-creation bug already
  fixed — e.g. maybe the editor manager tries to "restore" the player to their configured
  base mode set right after any mode change, and since `BUILDING` was removed from `Base
  Modes` to fix the eager-creation bug, the manager immediately switches back to whatever
  IS still configured, tearing down `BUILDING` in the process).
- A second/competing `SetCurrentMode()` or `ToggleEditorMode()` call firing right after
  the first (matching the earlier "double-trigger" hypothesis, but now with evidence it's
  a mode-switch, not a raw duplicate-open).
- Something specific to `SCR_CampaignBuildingManagerComponent.EnterEditorMode()`'s own
  flow calling `ToggleEditorMode()` twice in some code path not yet traced.

This is a strong, evidence-based lead for the next session to pick up directly — no more
guessing needed about whether the close is real; it's now about finding what triggers the
mode switch.

## Next steps (as of end of session)

1. ~~Apply the identical `Faction Rank Info` fix to the OPFOR `SCR_Faction` entity~~ —
   **DONE**, confirmed directly in `test_mode_2.layer`: both remaining unnamed
   `SCR_Faction` entries (`{628B22E9B4056C88}` and `{628C2D2BFC8C6447}`) now carry a
   2-entry `m_FactionRankInfo` (Private, Captain @ 10000 XP threshold — threshold is
   irrelevant since BC has no XP system, just needs to exist and be non-null).
2. Investigate what triggers the `CanOpen()` mode change from `32` to `1` immediately
   after building mode activates (see "UPDATE" section above) — this is the direct,
   evidence-based next step, not more speculative hypothesis-testing.
3. Check the exact `EEditorMode` enum values to confirm what `32` and `1` actually
   correspond to (`BUILDING` vs. some base mode) — would help interpret the mode-switch
   precisely.
4. Once fully resolved, clean up all TEMP DEBUG modded classes in
   `GRAD_BC_VehicleSpawnAction.c` (currently: `SCR_CampaignBuildingBudgetEditorComponent`
   × 2 methods, `SCR_CampaignBuildingEditorComponent` × 3 methods,
   `SCR_EditorManagerEntity` × 4 methods) — none of these are meant to be permanent.
5. Investigate the unrelated `BadMatrix43` engine assertion in
   `GRAD_BC_BreakingContactManager.OnBreakingContactPhaseChanged` separately, if it recurs
   (hit once this session, froze Workbench with a native assertion dialog, not
   script-catchable, completely unrelated to the building-menu investigation — stack trace
   was `ConfirmSpawn` → `RequestInitiateOpforSpawn` → `SetBreakingContactPhase` →
   `OnBreakingContactPhaseChanged`, all in BC's own `GRAD_BC_BreakingContactManager.c`/
   `GRAD_PlayerComponent.c`).

## UPDATE 2: COALITION-Lobby real source pulled from GitHub; second working hypothesis
## added (menu populates empty, not just closes)

`CoalitionArma/COALITION-Lobby` is a public GitHub repo (default branch `release`). Pulled
`Scripts/Game/!Systems/VanillaOverrides/Managers/COA_SCR_EditorManagerEntity.c` and
`Scripts/Game/!Systems/UI/Menus/COA_EditorMenuUI.c` directly — no more guessing needed
about COALITION's own editor-mode code, real source is available going forward via
`https://api.github.com/repos/CoalitionArma/COALITION-Lobby/git/trees/release?recursive=1`
for a full file listing, then `raw.githubusercontent.com/CoalitionArma/COALITION-Lobby/release/<path>`
(URL-encode `!` as `%21`) for individual files.

**`COA_SCR_EditorManagerEntity.c` findings:**
- `CanOpen()`: if `GetCurrentMode() == EEditorMode.BUILDING`, forces `SetIsLimited(true)`
  and returns `true` unconditionally — building mode is never blocked by this gate.
  Confirmed NOT the cause of anything observed.
- `ToggleServer(bool open)`: standard open/close RPC, calls
  `m_CurrentModeEntity.ActivateModeServer()` / `DeactivateModeServer()`. No hidden
  double-toggle logic found here.
- `StartEvents(EEditorEventOperation type)`: on `OPEN`, closes COALITION's own Lobby
  menus (`COA_PreviewMenu`, `COA_SlottingMenu`, `COA_SpectatorMenu`, `COA_AARMenu`) — this
  is expected/benign. On `CLOSE`, schedules `OpenUI()` via callqueue, which reopens the
  appropriate Lobby menu based on `gamemode.m_GamemodeState` (briefing/slotting/game/AAR)
  — again benign, this only *reacts* to a close, doesn't *cause* one.
- `COA_HasUnlimitedEditorAccess()`: true for spectators/moderators/admins only. A regular
  commander player in a vehicle is none of these, so building mode correctly runs in
  "limited" mode — not a bug.

**`COA_EditorMenuUI.c` findings:** this is COALITION's override of the *generic* vanilla
editor pause-overlay menu (spectator listen-toggle, dead-body-cleanup admin button) — a
different menu from the Campaign Building UI itself. Notable: `OnMenuUpdate` calls
`m_wCrossWidget.SetVisible(...)` and reads `m_PlayerControllerManager.m_bIsListeningToSpec`
every frame with no null-guard on `m_PlayerControllerManager` — same class of latent bug as
the PSCore `SetPosX`-on-hidden-widget issue from the spectator-menu investigation, but not
yet confirmed to be involved in the building-menu bug specifically. Confirms `EEditorMode.EDIT`
is a real, distinct mode value COALITION checks against (`GetCurrentMode() != EEditorMode.EDIT`
gates a widget's visibility) — consistent with the `currentMode=1` observation after the
mode-switch (`32`→`1`), though the exact enum mapping is still unconfirmed.

**New debug hooks added** (on top of the existing `CanOpen()`/`EOnEditorDeactivate()` hooks)
to `SCR_EditorManagerEntity`, to catch what triggers the `32`→`1` mode switch directly:
`Toggle()`, `Action_EditorToggle(float, EActionTrigger)`, `SetCurrentMode(EEditorMode)`.
Whichever of these logs first after the building UI populates is the real trigger.

**Second working hypothesis (raised independently, not yet ruled out):** the menu might not
be "closing" at all in the sense of a bug — it might be opening correctly but rendering
*empty* because the catalog/provider resolves zero available entries, and something (maybe
even vanilla itself) then closes an empty building menu as expected behavior. Investigated:
- `SCR_CampaignBuildingProviderComponent`'s config on `Ural4320_combox.et` is **byte-identical**
  between `main` (last known working, pre-COALITION-migration commit) and this branch, per
  `git diff main HEAD -- Prefabs/Vehicles/Wheeled/Ural4320/VehParts/Ural4320_combox.et` — the
  *only* diff is a newly-added empty `SCR_FactionAffiliationComponent {}` block. Everything
  about budgets-to-evaluate, traits (`VEHICLE_CAR`/`VEHICLE_TRUCK`/`VEHICLE_APC`), rank
  requirement (`CAPTAIN`) is unchanged.
- `Vehicles_EntityCatalog_USSR.conf` has not been touched by any commit on this branch
  (`git log --follow` shows last change was the original `5eba7e2` integration commit, same
  as `main`). Currently 3 enabled `SCR_EntityCatalogEntry` GUIDs
  (`{63229A276327C12F}`, `{63229A274340C655}`, and one newly-added-but-disabled
  `{5D2348264B456B9D}`), rest explicitly `m_bEnabled 0`. Catalog content itself is not the
  difference between branches.
- `main`'s `SCR_FactionManager` (on the old `PS_GameModeCoop` entity) had only 2 minimal
  `SCR_Faction` entries with almost no fields set (`m_bIsPlayable 1` on one, nothing on the
  other) — confirms the whole `Faction Rank Info` problem is COALITION-specific and didn't
  exist in the simpler `main` setup.
- Current branch's faction keys (read from `m_aFriendlyFactionsIds` on the first
  `SCR_Faction` in `test_mode_2.layer`) are `"BLUFOR"`, `"OPFOR"`, `"INDFOR"` — matching what
  `GRAD_BC_BreakingContactManager.SetVehicleAndChildrenFaction()` already passes to
  `SetAffiliatedFactionByKey()`. Keys line up; not an obvious mismatch on paper.
- Could not find local/vendored source for `SCR_CampaignBuildingEditorComponent`'s actual
  catalog-entry resolution logic (it's not in `Scripts/Game/Campaign/SCR_CampaignBuildingManagerComponent.c`,
  which is BC's own full class-name override of the vanilla gamemode-level manager — 1027
  lines, real vanilla source, but doesn't contain catalog/trait filtering). API search found
  `SCR_CampaignBuildingProviderComponent.GetAvailableTraits()` (returns
  `array<EEditableEntityLabel>`, not raw traits) and `SCR_CampaignBuildingEditableEntityFilter`
  (31 public + 20 protected methods, handles `IsInBaseArea`/composition-ownership checks —
  looks like it's for *already-placed* buildables, not the purchasable-vehicle catalog, so
  likely not directly relevant). Did not find the specific method that cross-references
  provider traits against faction catalog entries — 90-method API surface on
  `SCR_CampaignBuildingProviderComponent`, only partially enumerated via `api_search`.
  User separately flagged `UseAllAvailableProviders()`/`m_bUseAllAvailableProviders` as
  worth checking directly in Workbench (not yet done) — low suspicion since it's also
  unchanged from `main`, but cheap to verify.
- Added a new debug block to `EOnEditorPostActivate()`: resolves the provider's owner's
  `FactionAffiliationComponent` directly (mirroring vanilla's own
  `GetRequesterFaction(user) != GetFaction()` pattern documented earlier in this file) and
  logs the resolved `Faction`/`GetFactionKey()` plus `GetAvailableTraits()` — this will show
  directly in the next test log whether the truck's faction is actually set to a resolvable
  faction at the moment the building UI tries to populate.

**Process note — real bug caught by user, not self-caught:** the first version of this new
debug block used a ternary operator (`? :`) in a `string.Format()` call. This project's own
`CODING_GUIDELINES.md` says "no ternary operators" — turns out this isn't just style, it's
a hard parser break in this Enforce Script setup. Compile failed with
`Broken expression (missing ';'?)` at that line, confirmed in `error.log`
(`SCRIPT (E): Can't compile "Game" script module!` — the whole module failed, not just a
warning). **The user tested against this broken build before the fix landed and got a
real, reproduced compile-error dialog** — correctly caught and reported it rather than
assuming a stale build. Fixed by replacing the ternary with a plain `if`/`else` block
assigning to a local `factionKeyStr` variable. Rule already known, re-confirmed the hard
way: **never use `? :` in this codebase, even in throwaway debug code.**

Workbench MCP tools (`wb_state`, `wb_reload`) started returning `"Undefined API func"`
immediately after the broken compile — the failed "Game" module load likely leaves
Workbench's own API surface partially uninitialized until a full restart. User restarted
Workbench to recover; next step is a fresh reload with the fixed file and a clean Play test.

## UPDATE 3: real root cause found — `GetProviderMaxValue()` returns -1 despite
## `GetMaxBudgetValue()` reporting correct values moments earlier

**The empty-catalog theory (UPDATE 2) was real but insufficient.** User added a genuine
vehicle prefab (`UAZ469.et`) directly to OPFOR's `VEHICLE` entity catalog in Workbench
(confirmed via screenshot: `Entity Catalogs > VEHICLE (1) > Entity Entry List` now shows
`UAZ469.et`, `Entity Prefab` field populated, `Enabled` checked) — **the menu still opened,
populated its layout, and closed on the exact same schedule.** This proves the catalog was
a real, separate bug (worth keeping fixed) but not the cause of the close.

**Root cause of the empty catalog (context for future sessions):** `SCR_EntityCatalogManagerComponent`
does not exist anywhere in either `main`'s or this branch's `test_mode_2.layer` — it was
never the real wiring mechanism. Instead, each `SCR_Faction` entity has its own inline
`Entity Catalogs` property (`m_aEntityCatalogs`, an array of `SCR_EntityCatalog` split by
`Entity Catalog Type`: `ITEM`/`CHARACTER`/`VEHICLE`/`GROUP`/`WEAPONS_TRIPOD`), populated
via **direct prefab references** (the `+` picker only accepts `.et` prefabs, confirmed live
in Workbench — no way to reference `SCR_EntityCatalogEntry`/`.conf`-based catalogs like
`Configs/EntityCatalog/USSR/Vehicles_EntityCatalog_USSR.conf` from here). That `.conf` file
is now confirmed to be **leftover from `main`'s pre-COALITION setup and structurally
incompatible with how COALITION's inline `SCR_Faction` entities source their catalogs** —
not reusable, a fresh per-faction prefab list is the correct approach going forward.

**The real close mechanism, found via `RemoveProvider()` call-order:** added a debug print
directly inside BC's own vendored `SCR_CampaignBuildingManagerComponent.RemoveProvider()`
(this function calls `editorManager.Close()` when `isActiveUser && editorManager.IsOpened()`).
Confirmed via log: **`RemoveProvider` fires AFTER `EOnEditorDeactivate`/`DeactivateModeServer`
have already completed** — it's a downstream cleanup consequence, not the trigger. The
`SetCheckProviderMove()` movement-based auto-close theory was raised and **ruled out**:
`GRAD_BC_BreakingContactManager.SetVehiclePhysics()` (handbrake + zero velocity/angular
velocity) is confirmed called via `GetGame().GetCallqueue().CallLater()` 50ms after spawn
for both command trucks — the vehicles are stationary, user confirmed standing 1m away
watching it happen with nothing moving.

**The actual smoking gun**, found by hooking `GetMaxBudgetValue()` (already overridden) and
adding a one-time deep check in `EOnEditorPostActivate()` that calls `GetProviderMaxValue()`
directly (a public, non-overridable method on `SCR_CampaignBuildingBudgetEditorComponent`):

```
[before EOnEditorActivateServer]
GetMaxBudgetValue: type=5,   maxBudget=0,    result=1
GetMaxBudgetValue: type=150, maxBudget=0,    result=1

[after EOnEditorActivateServer completes cleanly]
GetMaxBudgetValue: type=5,   maxBudget=1000, result=1
GetMaxBudgetValue: type=150, maxBudget=5,    result=1

[at EOnEditorPostActivate, UI already rendering]
Budget deep check: shownBudget=0, providerMax(150)=-1, providerMax(5)=-1, providerMax(shown)=-1
```

`GetMaxBudgetValue()` correctly reports real values (`1000`, `5`) for budget types `5` and
`150` right after activation. But `GetProviderMaxValue()` — called moments later, in the
same frame group, for the exact same budget types on the exact same provider — returns
`-1` for **every single budget type checked, including whichever one `GetShownBudget()`
itself returns**. `-1` is almost certainly a sentinel for "no matching provider budget
entry found." This is a genuine internal state disagreement: two closely-related budget
lookup paths on the same component give contradictory answers within the same activation
sequence, and this happens **100% reproducibly** on every open (2 full test cycles, both
identical down to the exact log lines).

**Working theory**: `GetMaxBudgetValue()` likely reads from the entity-core budget system
(the `SCR_EditableEntityCore`/`ApplyQueuedBudgetChanges` system referenced in this doc's
very first "ruled out" section) which is confirmed live and correctly wired. `GetProviderMaxValue()`
likely reads from a separate provider-specific budget-registration map/cache that either
(a) hasn't been populated yet for this provider at the moment `EOnEditorPostActivate` reads
it (a genuine registration-timing race exposed by COALITION's different
`ToggleServer`/`ActivateModeServer` sequencing vs. vanilla/PSCore's), or (b) was never
populated because something in the registration path (`SetCheckProviderMove()`'s neighbors
in the vanilla `EnterEditorMode` flow, not yet traced) silently fails under COALITION. This
disagreement is very likely what a vanilla/internal `CheckAndRecoverCampaignBuildingComponent`-style
consistency check (the same function named in the ORIGINAL crash from before the rank-info
fix) detects and reacts to by closing the editor cleanly instead of crashing — the rank-info
fix removed the NULL-pointer crash but not the underlying provider-budget registration gap
that the recovery check exists to catch.

**Not yet done**: `CheckAndRecoverCampaignBuildingComponent` is `protected` (confirmed not
in the public API surface via `api_search`), so it cannot be hooked directly with a modded
`override`. Next investigative step is to find what actually populates the provider-budget
map that `GetProviderMaxValue()` reads — likely something registered during
`SCR_CampaignBuildingManagerComponent.EnterEditorMode()` (BC's own vendored copy, lines
~500-575, right before the `SetCheckProviderMove()` call) that either runs too late
relative to `EOnEditorPostActivate`, or never runs at all for this provider under
COALITION's activation path specifically.

## UPDATE 4 (RETRACTED — see UPDATE 5): claimed `m_aBudgetsToEvaluate` was empty at runtime

> **This conclusion was WRONG and has been disproven. Kept only so the next session does not
> re-derive the same false lead. Read UPDATE 5 before acting on anything below.**

User correctly pushed back on the "timing race" framing (`GetProviderMaxValue()` returning
`-1` on 2+ consecutive open/close cycles in the same session, not just once) — right call,
this led directly to the actual proof.

Hooked `SCR_CampaignBuildingProviderComponent.GetMaxBudgetValue(EEditableEntityBudget)` —
**the provider's own direct, non-indirected accessor to `m_aBudgetsToEvaluate`** (confirmed
via its own doc comment: *"Return max value of the given budget if this budget is added to
be evaluated with this provider and has max value set."*). Force-probed it from
`EOnEditorPostActivate` with a spread of raw ints covering every budget-type value seen
anywhere in this investigation, including `2147483647` — the literal sentinel value used by
the truck's own `SCR_CampaignBuildingCooldownWithRankBudgetToEvaluateData.m_eBudget` in the
`.et` prefab file on disk:

```
Provider budget probe sweep: [0]=-1 [1]=-1 [2]=-1 [5]=-1 [6]=-1 [130]=-1 [150]=-1 [2147483647]=-1
```

**Every single probed value returns `-1`, with zero exceptions, across 3+ repeated
open/close cycles in the same Play session.** Per the method's own contract, this can only
mean one thing: **`m_aBudgetsToEvaluate` is empty on the runtime instance of
`SCR_CampaignBuildingProviderComponent` attached to the spawned command truck** — even
though `git diff main HEAD` on `Ural4320_combox.et` already confirmed the prefab file on
disk is byte-identical to `main`'s (still has all 4 entries: `PROPS`, cooldown,
`RANK_CAPTAIN`, `CAMPAIGN`) except for the added `SCR_FactionAffiliationComponent`.

**This is not a config/data problem — it is confirmed to be a runtime instantiation gap.**
The prefab says one thing; the live entity's component has none of it. `GetBudgetValue()`
returning `componentToUse=NULL` (UPDATE 3) is the direct symptom of this same emptiness —
there's nothing in the array to search through, so no component is ever found to hold any
budget, for any type, ever.

**Why this only breaks under COALITION**: the truck is spawned identically in both branches
via `GetGame().SpawnEntityPrefab(Resource.Load(...), GetGame().GetWorld(), params)` inside
`GRAD_BC_BreakingContactManager` (unchanged code, confirmed via `git diff` showing no
changes to the spawn call itself). The only known-changed variable between the two branches
is the gamemode entity type (`PS_GameModeCoop` → `COA_Gamemode`) and everything downstream
of that (COALITION's own `SCR_EditorManagerEntity`/`ToggleServer` override, its RPC-driven
activation sequencing). The leading theory is that vanilla's `SCR_CampaignBuildingProviderComponent`
normally has `m_aBudgetsToEvaluate` populated by some `EOnInit`/attribute-deserialization
step tied to a specific entity-lifecycle event that fires reliably under
PSCore/vanilla but does not fire (or fires with an empty/default-constructed component
instead of the real one) under COALITION's different `ActivateModeServer`/RPC timing.

**Next concrete step**: find what actually reads/deserializes `m_aBudgetsToEvaluate` from
the prefab into the runtime component. This is a `[Attribute]`-backed `ref array` field —
normally these are populated automatically by the engine's prefab/entity instantiation
system before any script code runs (i.e. this should not require any explicit "registration"
call at all under normal circumstances). If it's genuinely empty at runtime, candidates are:
1. The truck is being spawned via `GetGame().SpawnEntityPrefab()` directly rather than
   through whatever COALITION's own vehicle/entity spawning pipeline expects — worth
   comparing against how COALITION's own `COA_VehicleSpawner`
   (`Scripts/Game/!Systems/Components/VehicleSpawning/COA_VehicleSpawner.c`, found via the
   GitHub source pull) spawns vehicles, in case there's a required post-spawn
   initialization step BC's own direct `SpawnEntityPrefab` call skips.
2. A modded `SCR_CampaignBuildingProviderComponent` constructor/`EOnInit` hook (if this
   class exposes one) that dumps `m_aBudgetsToEvaluate.Count()` immediately, as early as
   possible after spawn — to determine whether it's ever populated at all (even briefly)
   or is empty from entity creation onward.
3. Compare directly against a vanilla-spawned building provider (e.g. a stock Conflict-mode
   military base) under the same COALITION session, to check whether this is truly
   COALITION-wide (affecting all `SCR_CampaignBuildingProviderComponent` instances) or
   specific to entities spawned via `GetGame().SpawnEntityPrefab()` at runtime (as opposed
   to entities placed statically in a `.layer`/world file, which is how vanilla military
   bases' provider components normally exist).

## UPDATE 5: UPDATE 4 was WRONG — `-1` is the correct return value, the budget array is fine

Two checks disproved UPDATE 4's "empty array" conclusion. Both are cheap and should be the
first thing re-run if anyone is ever tempted to revive that theory.

**Disproof 1 — COALITION spawns vehicles exactly the same way BC does.** Pulled
`Scripts/Game/!Systems/Components/VehicleSpawning/COA_VehicleSpawner.c` from the COALITION
GitHub repo. Its spawn call (line 54, and again on respawn at line 73) is:

```c
GetGame().SpawnEntityPrefab(Resource.Load(m_rVehicle), GetGame().GetWorld(), COA_EntityHelper.CreateSpawnParams(pos));
```

That is character-for-character the same API call `GRAD_BC_BreakingContactManager` uses.
There is **no** COALITION-specific spawn pipeline, no post-spawn init step, no registration
call that BC is skipping. `SpawnEntityPrefab` is the correct and only way, and BC uses it
correctly. **The "BC skips a COALITION initialization step" theory is dead.**

**Disproof 2 — `-1` means "no max value configured", not "budget not found".** The doc
comment on `SCR_CampaignBuildingProviderComponent.GetMaxBudgetValue()` requires **two**
conditions: *"if this budget is added to be evaluated with this provider **and has max value
set**"*. Re-reading the truck's actual `m_aBudgetsToEvaluate` config in
`Ural4320_combox.et`: **not one of the four entries sets any max-value field.** They set
only `m_eBudget`, `m_bShowBudgetInUI`, and cooldown/rank data. So `-1` for every type is the
**correct, expected, by-design** return — including for `PROPS` and `CAMPAIGN`, which *are*
present in the list. A uniform `-1` therefore proves nothing about whether the array is
populated.

**Confirming enum value: `PROPS == 0`.** `GetShownBudget()` returns `0`, and it resolves by
walking `m_aBudgetsToEvaluate` for the first entry with `m_bShowBudgetInUI 1` — which in the
truck's config is the `PROPS` entry. Cross-checked against the editor-component-level
`GetMaxBudgetValue` log values, which are internally consistent with this:

```
type=0   -> maxBudget=-1     (PROPS: in the list, no max set -> -1, correct & uncapped by design)
type=5   -> maxBudget=1000   (real cap, populated after EOnEditorActivateServer)
type=150 -> maxBudget=5      (real cap, populated after EOnEditorActivateServer)
```

`GetShownBudget()` successfully resolving to `PROPS` is itself positive proof the array **is**
populated — an empty array could not have produced that answer. Independently corroborated by
the fact that `main` runs this byte-identical provider config without issue.

**Also note `type=5`/`type=150` reading `0` before `EOnEditorActivateServer` and their real
values (`1000`/`5`) after** — that ordering is normal initialization, not a fault, and was
misread earlier in this investigation as suspicious.

**Net effect on the investigation:** the budget subsystem is exonerated end-to-end. The
`componentToUse=NULL` observation from UPDATE 3 also loses its significance — with no entry
carrying a max value, there is no component for `GetBudgetValue()` to hand back, so `NULL`
is likewise expected rather than a fault.

**What remains genuinely unexplained** (and is where a fresh session should start): between
`EOnEditorPostActivate` (menu populates, widgets create successfully) and `EOnEditorDeactivate`
(~40-70ms later), something calls `SCR_EditorManagerEntity.Close()` with **zero errors,
warnings, or exceptions logged in the gap**. Established hard facts about that gap, all
confirmed by repeated instrumented tests:
- `Toggle()` is **not** called a second time (hooked, never fires before the close).
- `Action_EditorToggle()` **never** fires at all (hooked; rules out any stray input binding).
- `CanClose()` **is** queried twice and returns `1` both times, immediately before the
  deactivate — so a close is being *requested and validated through the normal path*, not
  forced. **Finding who calls `Close()` is the single remaining question.**
- `RemoveProvider()` fires **after** the deactivate, as cleanup — it is not the trigger
  (already established in UPDATE 3).
- `SCR_EditorManagerEntity.Close()` is **not overridable** (confirmed by a real compile error
  earlier this session), which is precisely why the caller has never been caught. Suggested
  next approach: hook `SetCanClose`/`SetCanCloseOwner`, or the `GetOnCanClose()` script
  invoker (all public per `api_search`), to identify the requester at the moment `CanClose()`
  is consulted.

## UPDATE 6: ROOT CAUSE FOUND (call-stack proven) — the CAMERA component closes the editor

Obtained a real script call stack by calling `Debug.DumpStack(out string)` inside the
`CanClose()` override (`Debug.Trace` does not exist; `Debug.DumpStack` is the correct API).
`CanClose()` fires exactly twice right before every deactivate, so dumping the stack there
names the caller. **All 4 stacks across 2 independent runs are identical:**

```
CanClose()        Scripts/Game/UserActions/GRAD_BC_VehicleSpawnAction.c : 205
Close()           Scripts/Game/Editor/Entities/SCR_EditorManagerEntity.c : 216
CreateCamera()    Scripts/Game/Editor/Components/Editor/SCR_CampaignBuildingCameraEditorComponent.c : 15
TryCreateCamera() Scripts/Game/Editor/Components/Editor/SCR_CameraEditorComponent.c : 136
EOnFrame()        Scripts/Game/Editor/Components/Editor/SCR_CameraEditorComponent.c : 292
```

The second variant of the stack additionally shows the close travelling through COALITION's
own override on its way out:

```
ToggleServer()  Scripts/Game/!Systems/VanillaOverrides/Managers/COA_SCR_EditorManagerEntity.c : 86
Close()         Scripts/Game/Editor/Entities/SCR_EditorManagerEntity.c : 261
```

**What this means:** on the first frame after the building mode activates,
`SCR_CameraEditorComponent.EOnFrame()` calls `TryCreateCamera()`, which calls
`SCR_CampaignBuildingCameraEditorComponent.CreateCamera()`. That function calls
`Close()` on line 15 — i.e. it takes an early-exit/guard path and shuts the whole editor
down. The building menu therefore opens, populates, builds its widgets, and is killed on
the very next frame by the **camera** subsystem. Nothing in the content pipeline was ever
at fault.

`CreateCamera()` is protected and absent from the public API surface, which is why nothing
we hooked ever caught it — every previous probe was downstream of the decision.

**This retroactively explains the whole investigation.** Provider, faction, traits, budgets,
faction catalogs and the placing prefab list are all confirmed correct and were never
relevant. Timing fits exactly: widgets build, ~40-75ms later the next frame runs, camera
creation fails, editor closes.

### Everything now verified correct (do not re-investigate)

| Subsystem | Status |
|---|---|
| Provider component | present, `providerCount=1`, `hasProviderComponent=1` |
| Provider faction | `OPFOR`, correctly resolved |
| Provider traits | `VEHICLE_CAR/TRUCK/APC` |
| Placing component | `SCR_CampaignBuildingPlacingEditorComponent` (correct class) |
| Placing prefab list | **73 prefabs, 41 vehicles**, incl. `UAZ469.et` |
| Budget subsystem | exonerated — `-1` is correct ("no max value set") |
| Faction entity catalogs | irrelevant — never read during UI population |
| Faction rank info | fixed earlier, confirmed (`hasRankContainer=1`) |
| Errors between open and close | **none** |

### Retractions

- **UPDATE 4** (`m_aBudgetsToEvaluate` empty at runtime) — WRONG. `-1` is documented correct
  behaviour when no max value is set, and none of the truck's four entries define one.
  `GetShownBudget()` resolving to `PROPS` proves the array is populated.
- **The "placing list contains Game Master content / no vehicles" claim** — WRONG. It was
  inferred from a 5-of-73 sample. The full dump shows 41 vehicles including the exact
  prefab that was added by hand.
- **The provider-movement / physics-settling theory** — WRONG. `SetVehiclePhysics()`
  (handbrake + zeroed velocity) is confirmed called 50ms after spawn for both trucks.

### Next step

Investigate why `CreateCamera()` bails. Likely candidates, in order:
1. The player has no controlled entity / no valid camera target at that moment under
   COALITION's possession model (COALITION replaces the spawn/possession pipeline via
   `COA_SCR_PossessSpawnHandlerComponent` and `COA_PlayerCameraManager`).
2. `COA_PlayerCameraManager` (COALITION) already owns/controls the active camera and
   conflicts with the editor camera the building mode tries to create.
3. A missing camera prefab reference on `EditorModeBuilding.et`'s camera component.

Since `CreateCamera()` is protected, the practical approach is to hook the public
`SCR_CameraEditorComponent` surface (`TryCreateCamera` and neighbours) and log the camera
state/controlled entity at that exact moment, or to mod
`SCR_CampaignBuildingCameraEditorComponent` if it exposes an overridable entry point.

## UPDATE 7: the close is a SYMPTOM; empty population is the real bug (+ no mission header in WB)

The user pushed back on the `EOnFrame` suppression twice, correctly: *"i can imagine the self
closing of camera is also because its not populated"* and *"its not a real fix"*. Both correct.
The evidence now supports their causality, not mine.

### The `EOnFrame` override is a WORKAROUND, not a fix

Overriding `SCR_CameraEditorComponent.EOnFrame()` and skipping `super()` while in BUILDING mode
does stop the editor closing — the menu now stays open. But it repairs nothing; it just prevents
vanilla from reaching the code that closes. **Known cost: the editor camera is stuck/unmovable**,
because the camera's legitimate per-frame work is skipped too. It is marked in-code as
`!!! TEMPORARY WORKAROUND - NOT A FIX. DO NOT SHIP AS-IS. !!!` and should be DELETED if fixing
population makes the menu stay open on its own.

### Decisive measurement: the mode is empty BEFORE the camera ever runs

```
23:43:48.062  PostActivate summary: factionKey=OPFOR, placingPrefabs=73
23:43:48.062  ContentBrowser: filteredCount=0, blacklistedLabels(0)=      <-- empty already
23:43:48.069  CrateWidgets Mode_CampaignBuilding.layout                   <-- widgets build
23:43:48.100  CameraEditor frame 1: camera=NULL ... limited=1             <-- camera only now
```

`SCR_ContentBrowserEditorComponent.GetFilteredPrefabCount()` returns **0** while the placing
component holds **73** prefabs (41 vehicles), and **nothing is blacklisted**. The browser is
filtering 73 down to 0. This is measured one frame BEFORE the camera component first ticks, so
population fails first and the camera bails second — consistent with the user's reading that
`CreateCamera()` bails because there is nothing to show.

In-game confirmation (screenshot): menu open, left panel correctly populated
("SUPPLY TRUCK 1000/1000"), three category tabs present (car/truck/APC, matching the provider's
`VEHICLE_CAR`/`VEHICLE_TRUCK`/`VEHICLE_APC` traits), and **five empty slots**.

### NEW: there is no mission header at runtime (`haveHeader=0`)

```
CameraEditor frame 1: ... limited=1 haveHeader=0 armavisionMP=0 rplMode=0
```

`GetGame().GetMissionHeader()` returns **null**, and the log contains ZERO references to
`MissionHeader` or `BreakingContact_Kolgujev/Everon`. The world is being launched directly in
Workbench Play mode rather than through a mission `.conf`, so `GRAD_BC_MissionHeader` never
loads. **This resolves the UPDATE 6 wrinkle**: `m_bIsArmavisionAllowedInMP 1` is correctly set in
both `Missions/*.conf`, but cannot apply because no header exists in this session. Combined with
`limited=1`, the ArmaVision lock condition is satisfied.

**Caveat for future sessions: some of this may be a Workbench-only artifact.** On a dedicated
server launching via the mission `.conf`, the header would load and the flag would take effect.
Worth re-testing there before concluding the camera behaviour is a real gameplay bug.

### Correction to UPDATE 6

UPDATE 6 framed the camera as *the* root cause. That was premature. The camera is where the close
originates, but it is downstream of an empty content browser. The root question is now:
**why does the content browser filter 73 prefabs down to 0?**

### Next step

Find the filter predicate. `SCR_ContentBrowserEditorComponent` exposes `FilterEntries()`,
`IsMatchingToggledLabels(array<EEditableEntityLabel>)`, `GetValidBlackListedLabels()` and
`GetFilteredPrefabID(int)`. Blacklist is empty, so the rejection is label/trait matching:
the provider advertises traits `50 54 53` and the browser must match those against each prefab's
`EEditableEntityLabel` set. Prime suspect: the 73 placing prefabs do not carry the labels the
provider's traits require — i.e. the placing list and the provider traits disagree. Note this is
NOT the faction entity catalog (already ruled out — never read during UI population).

## UPDATE 8: CONFIRMED BY MEASUREMENT — the menu is empty because `m_aPlaceablePrefabs` is empty

```
BC Debug - PlaceablePrefabs: manager.GetPlaceablePrefabs() count=0
```

Measured directly on `SCR_CampaignBuildingManagerComponent` (BC's vendored copy) at
`EOnEditorPostActivate`. **Zero placeable prefabs.** This is the empty vehicle menu.

### The chain

`m_sPrefabsToBuildResource` is **UNSET** on the manager in `test_mode_2.layer` (verified: the
component block contains only `m_iCompositionRefundPercentage`, `m_iXpRewardTreshold`,
`m_iInitialRewardInterval`, `m_fRewardCurveIncrement`), and no `SCR_PlaceableEntitiesRegistry`
`.conf` exists anywhere in the repo. Therefore:

```
m_sPrefabsToBuildResource unset
  -> GetPrefabListFromConfig(): BaseContainerTools.LoadContainer("") fails
  -> returns SILENTLY (two unlogged `return`s at lines ~465-470)
  -> m_aPlaceablePrefabs stays empty
  -> GetCompositionResourceName(prefabID) has nothing to resolve
  -> menu renders with zero entries, no error anywhere
```

The silent returns are why every log looked clean throughout this entire investigation.

### The content browser was the WRONG COMPONENT all along

`SCR_ContentBrowserEditorComponent` is the **Game Master** browser, not the Campaign Building UI.
Proof: calling `OpenBrowserLabelConfigInstance()` opened the generic full-screen **"Entity Browser"**
overlay (screenshot), which itself showed "No results". The Campaign Building menu instead works in
**composition IDs** via `SCR_CampaignBuildingManagerComponent.GetCompositionResourceName(int)`.

Three attempts against that browser therefore failed, and are recorded in-code as do-not-retry:
1. `IsMatchingToggledLabels()` probing — showed `matches=0` for provider traits AND for an EMPTY
   label set. An empty set cannot mismatch, so this only ever proved the GM browser had no active
   selection — irrelevant to our UI.
2. `new SCR_EditorContentBrowserDisplayConfig(...)` + `OpenBrowserLabelConfigInstance()` — opened
   the wrong UI **and crashed** (NULL `#array` in `GetAlwaysActiveLabels`; the constructor needs
   wrapper objects, not raw `EEditableEntityLabel` arrays).
3. `SetBrowserState(0, false, true)` — completely inert, `filteredCount` stayed 0.

### Correction

This same finding was surfaced much earlier in the investigation and then dropped, on the reasoning
that `main` did not set `m_sPrefabsToBuildResource` either. That reasoning was wrong: `main` ran
`PS_GameModeCoop`, which may have populated this list by a route COALITION does not provide. It
should have been measured when first noticed instead of argued past.

### The fix

Author a `SCR_PlaceableEntitiesRegistry` `.conf` listing the purchasable vehicles and assign it to
`m_sPrefabsToBuildResource` on `SCR_CampaignBuildingManagerComponent` in `test_mode_2.layer`.
Per the attribute's own description it "has to be the same as on Editor Mode - placing editor
component" — i.e. it should correspond to the 73-prefab list already present on
`SCR_CampaignBuildingPlacingEditorComponent` (41 of which are vehicles).

Still open, and NOT to be conflated with this: whether populating the list also stops
`CreateCamera()` bailing. If it does, the `EOnFrame` workaround must be DELETED.

## UPDATE 9: BI developer confirms Campaign Building REQUIRES Campaign mode — this is the real answer

Two external pieces of intelligence (via Discord) resolve this investigation.

### 1. Bohemia developer (ArkensoR [ARMA], 28.10.2025), on why building refuses to consume supplies without `SCR_GameModeCampaign`:

> *"Yes because the campaign building system is for campaign mode ... it does not work without it
> as the data for it is on campaign gamemode and its components"*

**This is the root cause of the entire investigation.** Campaign Building is not a general-purpose
system; its data lives on `SCR_GameModeCampaign` and `SCR_CampaignMilitaryBaseComponent`. Under
`COA_Gamemode`, `SCR_GameModeCampaign.GetInstance()` returns null, so every campaign-gated path
silently no-ops — which is exactly the signature we chased all night: correct-looking config
everywhere, zero content, zero errors.

Corroborated in BC's own vendored `SCR_CampaignBuildingManagerComponent.c`:
- line 59: `map<SCR_CampaignMilitaryBaseComponent, ...> m_mCampaignBuildingComponents` — the core
  data structure is KEYED on campaign bases, which do not exist under COA_Gamemode (permanently empty)
- lines 248, 690: `SCR_GameModeCampaign.GetInstance()` — null under COA_Gamemode
- lines ~246-251: a previous author already hit this and **commented out** the campaign guard:
  ```
  //CampaignBuildingManagerComponent should not do anything if there is no campaign
  /* const SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance(); if (!campaign) return; */
  ```
  Commenting out the guard lets execution continue, but does not supply the missing campaign data.
- `RankCheck()`/`CooldownCheck()` are likewise gated behind `if (SCR_GameModeCampaign.GetInstance())`
  (noted much earlier in this doc, and never connected to the empty menu until now).

### 2. Community answer on how buildables are actually configured:

> *"To edit campaign (conflict) buildables you gotta override the gamemode editor_builder or
> buildmode whatever and add them to the arma registry or add a new one for your mod and add the
> items there, just make sure it has the editor component with the right flags"*

Confirmed against the vanilla game data (`addons/data/data005.pak`): registries live under
`Configs/Editor/PlaceableEntities/`, and Campaign Building uses a DEDICATED one —
`Configs/Editor/PlaceableEntities/Compositions/Compositions_FreeRoamBuilding.conf` — separate from
`Configs/Editor/PlaceableEntities/Vehicles/Vehicles.conf` (the Game Master vehicle list).

**This corrects UPDATE 8's conclusion.** The placing component's registries ARE the right mechanism;
the error was trying to READ vanilla's registries into the manager (which returned 1577 Game Master
entries) instead of ADDING a BC-specific registry. Note BC already owns
`Prefabs/Editor/Modes/EditorModeBuilding.et`, but it is an EMPTY inheritance stub — that is precisely
the override the community answer describes.

### Also retract from UPDATE 8

The `TryPopulatePlaceablePrefabsFromPlacingComponent()` fallback DID populate the list (0 -> 1577,
`filteredCount` 0 -> 1) but has been DISABLED because it also **mutated the placing component's own
registry list** as a side effect (`placingPrefabs` was a stable 73 across six sessions, jumped to
1650 in the single run where it executed). Do not re-enable.

### Where this leaves the feature

Making vanilla Campaign Building work under `COA_Gamemode` means supplying, by hand, the campaign
data the system assumes exists: a `SCR_GameModeCampaign` instance (or a convincing stand-in) plus
`SCR_CampaignMilitaryBaseComponent` bases for the composition map to key on. That is a substantially
larger undertaking than a config fix, and it is fighting the system's stated design.

**Note also `haveHeader=0`** — no mission header loads in Workbench Play mode, so some observed
behaviour (notably the ArmaVision camera lock) may not reproduce on a dedicated server launching via
`Missions/*.conf`. Worth verifying there before drawing further conclusions about the camera.

## UPDATE 10: CORRECTION to UPDATE 9 — it worked on `main` WITHOUT campaign mode, so it is doable

The user pushed back on UPDATE 9's pessimism: *"ok but it worked before on main branch. so its
definitely doable"*. Correct, and verified.

**PSCore contains ZERO campaign references.** Searched `PSCore_1337133713371337/data.pak`:
```
SCR_GameModeCampaign = 0 occurrences
CampaignBuilding     = 0 occurrences
```
So on `main` — running `PS_GameModeCoop` from PSCore — `SCR_GameModeCampaign.GetInstance()` was
ALSO null, exactly as it is now under `COA_Gamemode`. Vehicle buying nevertheless worked.

**Therefore the BI developer's "it does not work without campaign mode" cannot be the whole story.**
It is true that supply consumption / rank / cooldown paths are campaign-gated (that part matches
the code), but a populated, usable buy menu demonstrably ran without Campaign mode on `main`.
UPDATE 9's conclusion was too pessimistic and should not be used to abandon the feature.

### What is verified IDENTICAL between `main` and HEAD

- `Scripts/Game/Campaign/SCR_CampaignBuildingManagerComponent.c` — no diff (BC's vendored 951-line
  override of the vanilla manager, added in `5eba7e2`)
- `UI/Layouts/Editor/Modes/Mode_CampaignBuilding.layout` + `.meta` — no diff, same GUID
  `{F1279058C64D2815}`
- `SCR_CampaignBuildingManagerComponent` config block in `test_mode_2.layer` — same four tuning
  values, same component GUID `{6A0956CFB7A9F2D8}`
- `Ural4320_combox.et` provider config — only diff is an added `SCR_FactionAffiliationComponent`

### What actually changed (`git diff main HEAD -- addon.gproj`)

```
-  "58D0FB3206B6F859" "1337133713371337" "5EAF2B0473DB5A99" "60C4CE4888FF4621" ...
+  "58D0FB3206B6F859" "69EA43E490A50DBE" "60C4CE4888FF4621" ...
```
- REMOVED `1337133713371337` = **PSCore**
- REMOVED `5EAF2B0473DB5A99` = **Reforger Lobby**
- ADDED   `69EA43E490A50DBE` = **COALITION-Lobby**

Plus the gamemode entity swap in `test_mode_2.layer` (`PS_GameModeCoop` -> `COA_Gamemode`, 1105
lines changed).

### Open leads worth checking next (in this order)

1. **BC's custom building layout is ORPHANED.** `UI/Layouts/Editor/Modes/Mode_CampaignBuilding.layout`
   has GUID `{F1279058C64D2815}`, but the runtime loads the VANILLA layout
   `{1133DAC581B3D631}UI/layouts/Editor/Modes/Mode_CampaignBuilding.layout` (note lowercase
   `layouts` vs BC's `Layouts`). Nothing in the repo references `{F1279058C64D2815}` — grep found
   zero hits in `.et`/`.conf`/`.layer`/`.c`. On `main` something must have made BC's layout win.
   **If BC's custom UI was what rendered the vehicle list, this alone could explain the empty slots.**
   Worth finding how PSCore/Reforger Lobby caused BC's layout to be used.
2. **`Prefabs/Editor/Modes/EditorModeBuilding.et` is UNTRACKED (`??`) and an empty stub** — created
   during this migration, does nothing, and did not exist on `main`. Per the community answer this
   is the correct place to register a BC registry, but note it is NOT what made `main` work.
3. Compare how `PS_GameModeCoop` vs `COA_Gamemode` host `SCR_CampaignBuildingManagerComponent`.

## UPDATE 11: `SCR_EditorSettingsEntity` is in a state that never existed on `main` OR as committed

Digging into what `main` had that HEAD lost (per UPDATE 10's leads).

### Ruled out first: BC's custom layout is NOT the regression

`git grep` on `main` for BC's layout GUID `{F1279058C64D2815}` returns **nothing** — it was orphaned
on `main` too, exactly as it is now. And neither PSCore nor Reforger Lobby references either layout
GUID (searched both `data.pak` and `resourceDatabase.rdb`: 0 hits for `{1133DAC581B3D631}` and
`{F1279058C64D2815}`). So the vanilla layout was always the one loading, on `main` as well.
UPDATE 10's lead #1 is disproven — do not chase it further.

### The real difference: `main` had NO editor entity at all

```
main   test_mode_2.layer : grep -i "editor" -> ZERO matches
HEAD   test_mode_2.layer : SCR_EditorSettingsEntity at line 614
```

`SCR_EditorSettingsEntity` was ADDED during the COALITION migration. It did not exist on `main`,
where vehicle buying worked.

### Worse: its current state is one WE created mid-investigation

Committed state (`git diff main HEAD`):
```
+  SCR_EditorSettingsEntity {
+   ID "632771FB44BBD90B"
+   m_bOverrideBaseModes 1
+   m_BaseModes BUILDING
+  }
```
Current on-disk state:
```
  SCR_EditorSettingsEntity {
   ID "632771FB44BBD90B"
   m_bOverrideBaseModes 1        <-- override still ON
  }                              <-- m_BaseModes line GONE (empty list)
```

Earlier in this investigation `BUILDING` was unchecked from `Base Modes` to stop the eager
mode-entity creation crash. That deleted the line entirely, leaving **override enabled with an
empty mode list**.

Per the API doc for `EnableBaseOverride()`:
> *"When set to true, connecting players will receive editor modes **defined by these settings**,
> instead of use defaults from `SCR_EditorManagerCore`."*

So every connecting player now receives **ZERO editor modes**, rather than vanilla's defaults. This
is a state that existed neither on `main` (no entity at all) nor as committed (`BUILDING` present).

This is consistent with several unexplained observations: `CanOpen()`'s `m_Modes.IsEmpty()` gate in
COALITION's override, the building mode entity being spawned ad-hoc per interaction rather than
pre-registered, and the mode having no properly initialised content/camera state.

### Next actions (cheap, in order)

1. **Set `m_bOverrideBaseModes` to 0** (uncheck "Override Base Modes") so players get
   `SCR_EditorManagerCore`'s defaults, closest to `main`'s behaviour of having no override entity.
2. If that misbehaves, **delete `SCR_EditorSettingsEntity` outright** — `main` had no such entity
   and vehicle buying worked.
3. Only then revisit the registry work from UPDATE 9.

NOTE: the `EOnFrame` camera workaround and all TEMP DEBUG hooks are still active in
`GRAD_BC_VehicleSpawnAction.c` and must be removed before any real conclusion is drawn from a test.

## UPDATE 12: `SCR_EditorSettingsEntity` DELETED — no effect. Not the regression.

Tested with the entity fully removed from `test_mode_2.layer` (verified: zero grep hits), on a
verified-fresh build, with all debug scaffolding stripped and the vendored manager restored to its
committed state via `git checkout`.

```
BC Debug - MENU STATE: factionKey=OPFOR, placingPrefabs=73, placeableCount=0, filteredCount=0
```

**Identical to baseline on all four numbers.** UPDATE 11's hypothesis (override enabled + empty
`m_BaseModes` starving players of editor modes) is disproven. The menu also still closes itself.

### Running list of DISPROVEN causes — do not revisit

| Hypothesis | Verdict |
|---|---|
| Faction rank info missing | Real bug, fixed, but not this one |
| `m_aBudgetsToEvaluate` empty at runtime | WRONG — `-1` is correct when no max value is set |
| Provider/vehicle physics movement | WRONG — `SetVehiclePhysics()` runs 50ms after spawn |
| ArmaVision lock via `IsLimited` | WRONG — cleared it, `CreateCamera()` still bailed |
| Faction entity catalogs | Irrelevant — never read during UI population |
| Content browser label filtering | Wrong component — that is the Game Master browser |
| Content browser state index | Inert — `SetBrowserState(0)` changed nothing |
| Placing-component registries as the source | Reading them yields GM content AND mutates vanilla state |
| BC's custom `Mode_CampaignBuilding.layout` | Orphaned on `main` too — never loaded there either |
| `SCR_EditorSettingsEntity` base-modes override | **Disproven here — deleting it changed nothing** |

### What remains verified TRUE

- `main` worked with **no** Campaign mode (PSCore pak: 0 hits for `SCR_GameModeCampaign`), so the
  BI developer's "requires campaign mode" cannot be the whole explanation.
- The vendored manager script, the custom layout + GUID, the manager's layer config block, and the
  truck's provider config are all **byte-identical** to `main`.
- `m_aPlaceablePrefabs` is empty because `m_sPrefabsToBuildResource` is unset and
  `GetPrefabListFromConfig()` fails silently. **This was equally true on `main`.**
- Therefore something OTHER than the manager's own config populated the menu on `main`.

### The unexamined difference

Everything script- and config-side is identical to `main`. The remaining delta is the **gamemode
entity itself**: `PS_GameModeCoop` (from `PS_GameMode_Lobby.et`, PSCore) vs `COA_Lobby`
(`COA_Gamemode`). Since BC's `SCR_CampaignBuildingManagerComponent` is attached to that entity, and
its config is identical in both, the difference must lie in what the **host gamemode prefab**
provides — e.g. PSCore's gamemode may itself carry editor/building wiring that COALITION's does not.

NEXT: inspect `PS_GameMode_Lobby.et` (PSCore) for editor/building-related components and compare
against `COA_Lobby.et`.

## UPDATE 13: FOUND THE WORKING TEMPLATE — COALITION's own registry + editor-mode override

Comparing the two lobby addons for editor/building wiring revealed that **COALITION already ships
exactly the mechanism the community answer described**, and PSCore does not:

```
PlaceableEntities .conf paths:   PSCore = NONE      COALITION = 4 (per faction)
```

COALITION's registries:
```
Configs/Systems/Entities/PlaceableEntities/COA_PlacableEntitiesRegestry_BLUFOR.conf
Configs/Systems/Entities/PlaceableEntities/COA_PlacableEntitiesRegestry_OPFOR.conf
Configs/Systems/Entities/PlaceableEntities/COA_PlacableEntitiesRegestry_INDFOR.conf
Configs/Systems/Entities/PlaceableEntities/COA_PlacableEntitiesRegestry_CIV.conf
```

### Format 1 — the registry `.conf` (flat prefab list)

`COA_PlacableEntitiesRegestry_OPFOR.conf`:
```
SCR_PlaceableEntitiesRegistry {
 m_Prefabs {
  "{8AA6789CBC9922E8}Prefabs/Groups/OPFOR/Infantry/COA_OPFOR_RifleSquad_p.et"
  "{0AAB05F13990369D}PrefabsMissionMaking/Systems/SpawnPoints/Vehicles/OPFOR_MSP.et"
  "{106BD97D5BD966D2}Prefabs/Vehicles/OPFOR/OPFOR_COMMAND.et"
  ...
 }
}
```

### Format 2 — the editor-mode prefab override that CONSUMES them

`Prefabs/!Systems/Editor/Modes/EditorModeEdit.et` (COALITION):
```
SCR_EditorModeEntity : "{E56F54E533ACE527}Prefabs/Editor/Modes/EditorModeBase.et" {
 ID "54F0A77282A3AA3B"
 components {
  SCR_PlacingEditorComponent "{5145484399BAA2E7}" {
   m_Registries {
    SCR_PlaceableEntitiesRegistry "{64468232E859A47C}" : "{6D7BC9B4C66AE7B9}Configs/.../COA_PlacableEntitiesRegestry_BLUFOR.conf" { }
    SCR_PlaceableEntitiesRegistry "{64468234AF6E4F97}" : "{1FAFC87125B4C514}Configs/.../COA_PlacableEntitiesRegestry_OPFOR.conf" { }
    SCR_PlaceableEntitiesRegistry "{64468234AE745EAC}" : "{CE655A6827EFDD0F}Configs/.../COA_PlacableEntitiesRegestry_INDFOR.conf" { }
    SCR_PlaceableEntitiesRegistry "{64468234AD84BC4F}" : "{D9FDD2A390D771B0}Configs/.../COA_PlacableEntitiesRegestry_CIV.conf" { }
   }
  }
 }
}
```

Note COALITION overrides **EditorModeEdit**, not EditorModeBuilding — its own free-placement editor,
not the Campaign Building buy menu. So this is a template to copy, not something that already
covers BC's case.

### BC's equivalent file is an EMPTY STUB

`Prefabs/Editor/Modes/EditorModeBuilding.et` (BC, and note: **UNTRACKED in git**):
```
SCR_EditorModeEntity : "{E56F54E533ACE527}Prefabs/Editor/Modes/EditorModeBase.et" {
 ID "518A34314C7CB324"
}
```
Same base prefab, same override pattern — but **no `components` block at all**, so no registries.

### The concrete fix, by direct analogy

1. Author `Configs/Editor/PlaceableEntities/GRAD_BC_Vehicles_OPFOR.conf` (and a BLUFOR twin) in
   Format 1 above, listing the purchasable vehicles (BRDM2, BTR70, UAZ469 variants, etc.).
2. Add a `components` block to BC's `EditorModeBuilding.et` with `SCR_PlacingEditorComponent` and
   an `m_Registries` array pointing at those `.conf` files, per Format 2.
3. Per the community answer, ensure the vehicle prefabs carry the editor component with labels
   matching the provider's traits (`VEHICLE_CAR` / `VEHICLE_TRUCK` / `VEHICLE_APC`).

Easiest authoring route is Workbench (duplicate one of COALITION's registry `.conf` files and edit
the prefab list), which also guarantees correct `.meta` GUID generation.

## UPDATE 14: ROOT CAUSE FOUND — `m_sPrefabsToBuildResource` was INHERITED on `main`, not unset

A background agent resolved the paradox from UPDATE 10/12. The premise those updates rested on was
**false**, and that false premise is what derailed several hours of investigation.

### What actually happened

`main`'s gamemode entity inherited from **Reforger Lobby's** `PS_GameMode_Lobby.et`
(addon `5EAF2B0473DB5A99` — NOT PSCore, correcting UPDATE 10). That prefab carries a fully
configured manager component. Extracted verbatim from `ReforgerLobby_5EAF2B0473DB5A99/data.pak`:

```
PS_GameModeCoop : "{0F307326459A1395}Prefabs/MP/Modes/GameMode_Base.et" {
 components {
  SCR_CampaignBuildingManagerComponent "{61B4DDBF56BF1B48}" {
   m_sFreeRoamBuildingClientTrigger "{5E191CEAF4B95816}Prefabs/MP/FreeRoamBuildingClientTrigger.et"
   m_BudgetType CAMPAIGN
   m_iCompositionRefundPercentage 100
   m_OutlineManager SCR_CampaignBuildingCompositionOutlineManager "{5DF6FBB5913F3245}" : "{96A8B496A076F1C0}scripts/Game/Building/CampaignBuildingCompositionOutline.conf" { }
   m_sPrefabsToBuildResource "{D2527D9AA5B4A33E}Configs/Editor/PlaceableEntities/Compositions/Compositions_FreeRoamBuilding.conf"
  }
```

### Why it looked "unset on main too" — the trap

A `.layer` file records only **overrides**. `main`'s Kolgujev layer used the *same component ID*
as the prefab and overrode just four tuning numbers:

```
main  Worlds/MP/BC_kolgujev_Layers/test_mode_2.layer:18
  SCR_CampaignBuildingManagerComponent "{61B4DDBF56BF1B48}" {   <-- inherited instance
     m_iCompositionRefundPercentage 0 / m_iXpRewardTreshold 9999 / ... }

HEAD  Worlds/MP/BC_kolgujev_Layers/test_mode_2.layer:17
  SCR_CampaignBuildingManagerComponent "{6A0956CFB7A9F2D8}" {   <-- BRAND NEW instance
     (same four values) }
```

`{61B4DDBF56BF1B48}` is byte-identical to the prefab's component ID and is present in Reforger
Lobby's pak. `{6A0956CFB7A9F2D8}` returns **0 hits** in COALITION's pak — it is a fresh component
carrying only empty class defaults. So on HEAD `m_sPrefabsToBuildResource` really is empty,
`BaseContainerTools.LoadContainer("")` fails, and `GetPrefabListFromConfig()` returns silently at
line 466 — the exact path UPDATE 8 measured as `placeableCount=0`.

Corroborating: main's layer also carried the prefab's other sub-entity IDs
(`5ED9088735FF2D8B` SCR_FactionManager, `5ED9088735FF2DEE` RadioManagerEntity,
`5ED9088735FF2DD5` ItemPreviewManagerEntity), all present in Reforger Lobby's pak.

**Note: Kolgujev was the playable world on `main`** (per the user), so its layer is the reference.
Everon is broken the same way (`managers.layer` uses another fresh GUID `{6A0956CEF951B6F3}`).

### Corrections forced by this finding

- UPDATE 10/12: *"`m_sPrefabsToBuildResource` was equally unset on `main`"* — **FALSE**. Inherited,
  therefore invisible in the layer text. This false premise caused hours of misdirected work.
- UPDATE 10: `PS_GameMode_Lobby.et` attributed to PSCore — **wrong**, it is a Reforger Lobby asset.
- UPDATE 13: framing the fix around the placing component's `m_Registries` — **wrong axis**. The
  working mechanism on `main` was the MANAGER's `m_sPrefabsToBuildResource`. The experimental
  registry block added to BC's `EditorModeBuilding.et` has been REVERTED to an empty stub.
- COALITION provides no substitute: `grep -a -c "PrefabsToBuild"` in its `data.pak` = **0**.

### The fix (one component, four fields) — vanilla resources, no Reforger Lobby needed

On `COA_Lobby` -> `SCR_CampaignBuildingManagerComponent` `{6A0956CFB7A9F2D8}` in
`Worlds/MP/BC_kolgujev_Layers/test_mode_2.layer`:

| Field | Value |
|---|---|
| `m_sPrefabsToBuildResource` | `{D2527D9AA5B4A33E}Configs/Editor/PlaceableEntities/Compositions/Compositions_FreeRoamBuilding.conf` |
| `m_BudgetType` | `CAMPAIGN` |
| `m_OutlineManager` | `SCR_CampaignBuildingCompositionOutlineManager` from `{96A8B496A076F1C0}scripts/Game/Building/CampaignBuildingCompositionOutline.conf` |
| `m_sFreeRoamBuildingClientTrigger` | `{5E191CEAF4B95816}Prefabs/MP/FreeRoamBuildingClientTrigger.et` — **CAUTION: this path was read from Reforger Lobby's pak and may be an RL-owned asset. Verify it resolves without RL loaded; leave empty if not.** |

`Compositions_FreeRoamBuilding.conf` ships in **vanilla** `data005.pak` (the same value appears on
vanilla's Game Master gamemode, the Conflict gamemode, and vanilla `EditorModeBuilding.et`'s
placing component), so it does not depend on Reforger Lobby.

### UNRESOLVED — inference, flagged by the agent

The agent could not read `Compositions_FreeRoamBuilding.conf` (zlib-compressed in the pak), so what
`main`'s menu actually listed is **unverified**. Since it is vanilla's free-roam *composition*
registry (bunkers, ammo/fuel storage, tents), the menu on `main` may have shown COMPOSITIONS rather
than BC vehicles. If so, restoring this field reproduces `main` but not necessarily the desired
vehicle list — the follow-on is then UPDATE 13's plan, pointing
`m_sPrefabsToBuildResource` at a BC-authored `SCR_PlaceableEntitiesRegistry` `.conf` listing the
purchasable vehicles instead.

## UPDATE 15: two corrections to UPDATE 14's proposed fix

### (a) `m_BudgetType` — do NOT blindly set CAMPAIGN (user challenge, correct)

UPDATE 14 listed `m_BudgetType = CAMPAIGN` as part of the fix, copied from Reforger Lobby's prefab.
The user asked whether it should be `PROPS` instead. Checking rather than assuming:

`m_BudgetType` is used in exactly one place — the filter in `OnEntityCoreBudgetUpdated`
(`SCR_CampaignBuildingManagerComponent.c:231`):
```c
if (entityBudget != m_BudgetType)
    return;   // early return
```

Observed across all logs (`entityBudget: X, m_BudgetType: Y`):
```
1232x  entityBudget: 5,   m_BudgetType: 0
1220x  entityBudget: 6,   m_BudgetType: 0
1220x  entityBudget: 2,   m_BudgetType: 0
 252x  entityBudget: 0,   m_BudgetType: 0     <-- MATCHES, filter passes
 160x  entityBudget: 130, m_BudgetType: 0
```

`m_BudgetType` is already **0**, and `PROPS == 0` (proven earlier via `GetShownBudget()` returning 0
for a provider whose first `m_bShowBudgetInUI 1` entry is `PROPS`). Budget type 0 events DO arrive
and DO pass the filter. The truck provider declares both `PROPS` (shown in UI) and `CAMPAIGN`
(shown in UI) in `m_aBudgetsToEvaluate`.

**Conclusion: leave `m_BudgetType` at its current value (0 / PROPS) for the first test.** Changing it
to CAMPAIGN would alter which budget the manager reacts to, on no evidence beyond "Reforger Lobby's
prefab had it". If supply consumption misbehaves after the resource fix lands, revisit it then —
as a separate, single-variable change.

### (b) `Compositions_FreeRoamBuilding.conf` — vanilla AND COALITION, same GUID (override)

Checked because the two could have been different resources:

| Source | Path | GUID |
|---|---|---|
| Vanilla | `Configs/Editor/PlaceableEntities/Compositions/Compositions_FreeRoamBuilding.conf` | `{D2527D9AA5B4A33E}` |
| COALITION | `Configs/Systems/Compositions_FreeRoamBuilding.conf` | `{D2527D9AA5B4A33E}` |

**Same GUID, different path — COALITION overrides vanilla's resource.** Its content
(from the GitHub repo, readable):
```
SCR_PlaceableEntitiesRegistry {
 m_Prefabs +{
  "{1DE2CFFF2775C07C}Prefabs/Systems/Rallypoints/BLUFOR/COA_RallyPointBuildable_BLUFOR.et"
  "{6F48F93E3C7401EB}Prefabs/Systems/Rallypoints/OPFOR/COA_RallyPointBuildable_OPFOR.et"
 }
}
```
Note `m_Prefabs +{` — the **`+` denotes APPEND**, so COALITION adds two buildable rally points on
top of vanilla's composition list rather than replacing it.

**Implication:** pointing `m_sPrefabsToBuildResource` at `{D2527D9AA5B4A33E}` resolves to the
COALITION-overridden version = vanilla compositions + COALITION rally points. The GUID is correct
regardless of which addon is loaded. It also further supports the inference that `main`'s buy menu
listed COMPOSITIONS/fortifications, not BC vehicles.

## UPDATE 16: ROOT CAUSE FIXED AND CONFIRMED — `placeableCount` 0 -> 92

Applied the UPDATE 14/15 fix to Kolgujev (the world that was playable on `main`):

```
Worlds/MP/BC_kolgujev_Layers/test_mode_2.layer:17
  SCR_CampaignBuildingManagerComponent "{6A0956CFB7A9F2D8}" {
   m_iCompositionRefundPercentage 0
   m_iXpRewardTreshold 9999
   m_iInitialRewardInterval 9999
   m_fRewardCurveIncrement 9999
   m_sPrefabsToBuildResource "{D2527D9AA5B4A33E}Configs/Systems/Compositions_FreeRoamBuilding.conf"   <-- ADDED
  }
```

Measured result:

| | before | after |
|---|---|---|
| `placingPrefabs` | 73 | **165** |
| `placeableCount` | **0** | **92** |
| `filteredCount` | 0 | 0 |

**`placeableCount` 0 -> 92 confirms the root cause and the fix.** The empty buy menu was caused by
`m_sPrefabsToBuildResource` being unset on the NEW `{6A0956CFB7A9F2D8}` component created during
the COALITION migration, where on `main` it was INHERITED (invisibly, via component ID
`{61B4DDBF56BF1B48}`) from Reforger Lobby's `PS_GameMode_Lobby.et` prefab.

Only ONE field was set. `m_BudgetType` was deliberately left at its existing value (0 / PROPS) per
UPDATE 15(a) — the user's challenge to the CAMPAIGN suggestion was correct, and no evidence
supported changing it. `m_sFreeRoamBuildingClientTrigger` and `m_OutlineManager` were also left
unset, and are evidently NOT required for the list to populate.

No new script errors were introduced (the only exceptions in the log are the pre-existing COALITION
`[OPFOR ... GEARSCRIPT ERROR]` inventory warnings).

### What is still broken

1. **`filteredCount` remains 0** — 92 prefabs are loaded into the manager but the content browser /
   UI filter passes none of them, so the slots still render empty on screen. Next step is dumping
   what those 92 prefabs actually ARE (debug added) to determine whether they are vanilla
   compositions (bunkers/ammo storage/rally points) rather than vehicles. Per UPDATE 15(b), the
   resource resolves to COALITION's override of `{D2527D9AA5B4A33E}`, i.e. vanilla's composition
   list PLUS COALITION's two buildable rally points (`m_Prefabs +{` = append). **Strong expectation:
   these are compositions, not vehicles** — meaning `main`'s buy menu likely never listed BC
   vehicles either, and the vehicle catalogue is a separate feature still to be built.
2. **The menu still closes itself** (2 occurrences). Still masked by the `EOnFrame` camera
   workaround in `GRAD_BC_VehicleSpawnAction.c`, which remains a NOT-A-FIX and must be resolved or
   removed before shipping.

### Reproduce on Everon when ready

Everon has the same defect with its own fresh component GUID:
`Worlds/MP/BC_everon_Layers/managers.layer` component `{6A0956CEF951B6F3}` — apply the same single
field. Deliberately NOT done yet: Kolgujev was the playable world on `main`, so it is the reference,
and changing one world at a time keeps the variable isolated.

## UPDATE 17: the 92 prefabs are FORTIFICATIONS, not vehicles — `main` never had a vehicle menu

Dumped the contents of the now-populated list:

```
placeable[0]  = PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/E_CzechHedgehog_S_01_painted.et
placeable[1]  = .../E_Dragonsteeth_S_US_01.et
placeable[2]  = .../E_BarbedTapeKnifeRest_S_US_01.et
placeable[3]  = .../E_BarbedTapeTriple_S_USSR_01.et
placeable[4]  = .../E_SandbagRoundBurlap_S_USSR_01.et
placeable[5]  = .../E_SandbagRoundHighBurlap_S_USSR_01.et
placeable[6]  = .../E_SandbagLongBurlap_S_USSR_01.et
placeable[7]  = .../E_SandbagLongHighBurlap_S_USSR_01.et
placeable[8]  = .../E_SandbagWallSolidBurlap_S_USSR_01.et
placeable[9]  = .../E_SandbagWallBurlap_S_USSR_01.et
placeable[10] = .../E_CamoNetSmall_S_USSR_01.et
placeable[11] = .../E_CamoNetMedium_S_USSR_01.et
```

**Every entry is a fortification composition — czech hedgehogs, dragon's teeth, barbed tape,
sandbags, camo nets. Not one vehicle.**

This confirms the UPDATE 15(b)/16 inference and answers the central question of the whole
investigation: **`main`'s "buy vehicle" menu never listed vehicles.** It listed vanilla's free-roam
building fortifications, because `m_sPrefabsToBuildResource` pointed (via inheritance) at vanilla's
`Compositions_FreeRoamBuilding.conf`. The feature that "worked on main" was fortification building,
not vehicle purchasing.

**Consequence:** restoring `main`'s behaviour is DONE (UPDATE 16). Making the menu sell VEHICLES is
NEW work that was never implemented on `main` — it is a feature request, not a regression fix.

### Second observation: the counts grow between runs

```
run @ 00:40  placingPrefabs=73,  placeableCount=0
run @ 00:54  placingPrefabs=165, placeableCount=92
run @ 01:00  placingPrefabs=171, placeableCount=98
```
The last step is exactly +6, matching the six vehicle GUIDs briefly present in
`EditorModeBuilding.et`. That file is now an empty stub on disk (verified) and the vehicles still
appeared, so **Workbench is serving a cached in-memory copy of the prefab**. Not blocking, but it
means prefab edits may persist in a running Workbench session after being reverted on disk — treat
counts as unreliable until a full Workbench restart.

### How to make it sell vehicles (three options, evaluated)

BC depends on COALITION (`69EA43E490A50DBE` in `addon.gproj`), so **BC loads after COALITION and
its overrides win**.

1. **Override `{D2527D9AA5B4A33E}` in BC with `m_Prefabs +{ ... }` (append).** Mirrors COALITION's
   own idiom exactly (they append two rally points to vanilla's list). No script changes. Vehicles
   appear ALONGSIDE the fortifications. Risk: three-way GUID override (vanilla -> COALITION -> BC)
   is untested here, and Workbench may resist authoring a file with a GUID owned by another addon.
2. **Point `m_sPrefabsToBuildResource` at a fresh BC-only registry.** No GUID collision, simplest
   and safest. Trade-off: menu shows ONLY BC vehicles — fortifications and COALITION rally points
   disappear.
3. **Script injection via `SCR_PlaceableEntitiesRegistry.AddPrefabs()`.** Not recommended: BC
   already vendors the manager wholesale, and an earlier attempt at this style of injection mutated
   vanilla state as a side effect (UPDATE 8).

### BLOCKER before any of the above

`filteredCount` is **still 0** even with 92-98 valid prefabs loaded. Adding vehicles to a filter
that passes nothing will still render an empty menu. The filter rejection must be understood first
— note the provider advertises traits `VEHICLE_CAR`/`VEHICLE_TRUCK`/`VEHICLE_APC` (`50 54 53`)
while the loaded content is entirely fortification compositions, so a trait mismatch is the obvious
suspect: **the provider is asking for vehicles and the registry only contains fortifications.**

## UPDATE 18: config parity with `main` is COMPLETE; `filteredCount` still 0

Restored the last inherited field from Reforger Lobby's prefab. `Prefabs/MP/FreeRoamBuildingClientTrigger.et`
`{5E191CEAF4B95816}` is confirmed **vanilla** (2 hits in `data005.pak`, 0 in Reforger Lobby's own
`resourceDatabase.rdb`, 0 in COALITION), so it resolves without the removed addon. It is a PREFAB
REFERENCE spawned by the manager at runtime — it is NOT placed in the world by hand.

Kolgujev manager component now reads:
```
SCR_CampaignBuildingManagerComponent "{6A0956CFB7A9F2D8}" {
 m_sFreeRoamBuildingClientTrigger "{5E191CEAF4B95816}Prefabs/MP/FreeRoamBuildingClientTrigger.et"
 m_iCompositionRefundPercentage 0
 m_iXpRewardTreshold 9999
 m_iInitialRewardInterval 9999
 m_fRewardCurveIncrement 9999
 m_sPrefabsToBuildResource "{D2527D9AA5B4A33E}Configs/Systems/Compositions_FreeRoamBuilding.conf"
}
```

Result: **`placeableCount=98, filteredCount=0`** — unchanged. No FreeRoamBuilding trigger activity
in the log at all (0 mentions).

### Parity checklist vs `main` — all green

| Item | Status |
|---|---|
| `SCR_CampaignBuildingManagerComponent.c` (vendored script) | **byte-identical to main** (`git diff main` empty) |
| Gamemode component list | matches main (same 5 components) |
| `m_sPrefabsToBuildResource` | restored (root cause, UPDATE 16) |
| `m_sFreeRoamBuildingClientTrigger` | restored (this update) |
| `Ural4320_combox.et` provider | only diff = deliberate `SCR_FactionAffiliationComponent` |
| BC vehicle registry | working — 6 vehicles present at indices 92-97 |

`m_BudgetType` deliberately left at PROPS (UPDATE 15a) and `m_OutlineManager` left unset; neither
affected the list.

### NEW EVIDENCE: the content browser UI is never instantiated

- The building layout loads 3x per session, but the log contains **ZERO** references to any
  content-browser UI component.
- BC's `Mode_CampaignBuilding.layout` declares `SCR_AssetBrowserAccessEditorUIComponent`,
  `SCR_PlacingEditorUIComponent`, etc. — but **no content-browser UI component at all**.
- The layout that actually loads is **vanilla** `{1133DAC581B3D631}`; BC's `{F1279058C64D2815}` is
  orphaned (and was equally orphaned on `main`, so NOT the regression).

**Implication:** `filteredCount` (from `SCR_ContentBrowserEditorComponent`) may be measuring a
browser whose UI half never exists in this layout, i.e. it may be the wrong number to chase. The
slots are reached via `SCR_AssetBrowserAccessEditorUIComponent`, which exposes no scriptable API.

### Failed attempts against the browser — all four, do not retry

1. `IsMatchingToggledLabels()` probing — rejects even an EMPTY label set
2. `SCR_EditorContentBrowserDisplayConfig` + `OpenBrowserLabelConfigInstance()` — opened the wrong
   (Game Master) UI **and crashed** with a NULL `#array`
3. `SetBrowserState(0, false, true)` — completely inert
4. `SetLabel(trait, true)` x3 + `FilterEntries()` — `filtered` stayed 0

### Honest position

Config parity with `main` is complete and the root cause (UPDATE 16) is fixed and verified. What
remains is NOT a config regression. Two possibilities, and they need different work:
- the slots are fed by a path we have not identified (asset browser access), or
- `main`'s menu was ALSO effectively unusable and the feature never fully worked (recall UPDATE 17:
  the list contains only fortifications; nobody has confirmed `main` ever rendered them on screen).

**Before more script archaeology, worth confirming what `main` actually displayed on screen** — by
checking out `main` and running it. That single observation decides whether this is a regression to
chase or a feature to build.

## UPDATE 19: THE MISSING LINK — `main`'s factions inherited vanilla `USSR.conf`/`US.conf`; COALITION's do not

User states definitively: **"main had vehicles and they worked."** That is authoritative and rules
out UPDATE 18's speculation that the feature may never have worked. Chasing that speculation was a
mistake; the contradiction it created is what led to the actual answer.

### How `main` supplied vehicles

`main`'s Kolgujev layer, `SCR_FactionManager`:
```
SCR_Faction "{56DEAC40D2DBC8B1}" { }                  -> : "{5EB46557DF2AA24F}Configs/Factions/US.conf"
SCR_Faction "{56DEAC40D3C2E623}" { m_bIsPlayable 1 }  -> : "{09727032415AC39B}Configs/Factions/USSR.conf"
```
(inheritance confirmed by locating both GUIDs in vanilla `data*.pak`)

These inherit **vanilla's faction configs, which carry the faction ENTITY CATALOGS including the
VEHICLE catalog**. BC then filtered that inherited vehicle list via
`Configs/EntityCatalog/USSR/Vehicles_EntityCatalog_USSR.conf` — a sparse override enabling/disabling
specific `SCR_EntityCatalogEntry` GUIDs (`m_bEnabled 0`).

`Configs/Factions/USSR_Campaign.conf` (still present on HEAD, 72 bytes) is an EMPTY inheritance stub:
```
SCR_CampaignFaction : "{09727032415AC39B}Configs/Factions/USSR.conf" { }
```
It adds no data — it exists only to re-type vanilla's USSR faction as `SCR_CampaignFaction`.
**Nothing on HEAD references it** (grep: 0 hits outside the file itself).

### Why HEAD has no vehicles

COALITION's factions are standalone `SCR_Faction` entries that **do NOT inherit from vanilla's
faction configs**:
```
COALITION data.pak:  "Configs/Factions/USSR.conf" -> 0 hits
                     "Configs/Factions/US.conf"   -> 0 hits
SCR_Faction "{628C2D2BFC8C6447}" {     <-- no ": base.conf" inheritance
```
So they never receive vanilla's entity catalogs. This is directly corroborated by the user's earlier
Workbench screenshot of `FACTION_COA_OPFOR`: `Entity Catalogs > VEHICLE (0) > Entity Entry List (0)`
— genuinely empty before the user hand-added `UAZ469.et`.

### This also retracts an earlier dismissal

UPDATE 8 concluded faction entity catalogs were "irrelevant — never read during UI population",
based on observing zero catalog reads between `EOnEditorPostActivate` and widget creation. That was
**circular**: the catalogs were empty *because* of this bug, so naturally nothing read them
usefully. On `main` the catalogs were populated and were the vehicle source.

### Consequence for the fix

The `m_sPrefabsToBuildResource` restore (UPDATE 16) was still correct and necessary — it fixed
`placeableCount` 0 -> 98 (fortifications, the free-roam building compositions). But **vehicles came
from a DIFFERENT channel**: the faction's VEHICLE entity catalog. Two separate content sources:

| Channel | Feeds | Status |
|---|---|---|
| `m_sPrefabsToBuildResource` -> `SCR_PlaceableEntitiesRegistry` | fortifications/compositions | FIXED (UPDATE 16) |
| faction `Entity Catalogs > VEHICLE` | **vehicles** | still empty on COALITION factions |

### Options (NOT yet tested — do not present as solved)

1. **Populate COALITION's OPFOR/BLUFOR VEHICLE catalogs directly** in Workbench (user already added
   `UAZ469.et` this way, proving the field is editable). Simplest, no inheritance surgery.
2. **Make COALITION's factions inherit vanilla `USSR.conf`/`US.conf`** so they receive the full
   vanilla catalog, then keep BC's existing `Vehicles_EntityCatalog_USSR.conf` filter. Closest to
   `main`, but changes COALITION faction identity and may break its lobby/slotting.
3. **Point the truck provider at the vanilla-derived factions** rather than COALITION's — likely
   conflicts with COALITION's faction manager.

Option 1 is the safest first test. Note the user ALREADY added `UAZ469.et` to OPFOR's VEHICLE
catalog and it did not appear — so the catalog alone may be necessary but not sufficient, and that
needs re-testing now that `m_sPrefabsToBuildResource` is also restored.

## UPDATE 20: faction catalog is CORRECT — data pipeline complete, failure is display-only

Probed OPFOR's VEHICLE entity catalog through the provider's own `FactionAffiliationComponent`
(the same path vanilla uses), at `EOnEditorPostActivate`:

```
BC Debug - VEHICLE CATALOG: faction 'OPFOR' has 6 enabled entries
  {259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et
  {0B4DEA8078B78A9B}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_PKM.et
  {1FBB492E86002BF5}Prefabs/Vehicles/Wheeled/UAZ452/UAZ452_transport.et
  {D9B91FAB817A6033}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport_covered.et
  {254289B9C09904AB}Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et
  {C012BB3488BEA0C2}Prefabs/Vehicles/Wheeled/BTR70/BTR70.et
```

Each carries `m_iSupplyCostOverride` (30/60/40/40/150/200) and
`m_eMinimumRequiredRankOverride CAPTAIN`.

**This RULES OUT changing COALITION's faction inheritance.** The user proposed making COALITION's
factions inherit vanilla `USSR.conf`/`US.conf` to obtain the catalogs. Measurement shows the
catalog is already fully visible to the game, so that refactor would have fixed nothing while
risking COALITION's lobby, slotting, spawning and gearscript systems (all keyed on their factions).
Testing before refactoring saved that.

### Complete state of the data pipeline — ALL GREEN

| Element | Status |
|---|---|
| Provider component | present, `providerCount=1` |
| Provider faction | `OPFOR`, correctly resolved |
| Provider traits | `VEHICLE_CAR`/`TRUCK`/`APC` (50/54/53) |
| Faction rank info | fixed (CAPTAIN available) |
| Faction VEHICLE catalog | **6 vehicles, costs + rank gates, all enabled** |
| `m_sPrefabsToBuildResource` | restored -> 98 compositions |
| `m_sFreeRoamBuildingClientTrigger` | restored (vanilla prefab) |
| Manager script | byte-identical to `main` |
| Placing registries | 171 prefabs incl. BC's 6 vehicles at 92-97 |
| **On-screen result** | **EMPTY SLOTS** |

Every data channel is verified correct. Nothing in config or content remains to fix.

### The failure is isolated to the display path

`filteredCount` is stuck at 0 from `SCR_ContentBrowserEditorComponent`. Note also (UPDATE 18) that
the log contains ZERO references to any content-browser UI component, and BC's building layout
declares `SCR_AssetBrowserAccessEditorUIComponent` / `SCR_PlacingEditorUIComponent` but no
content-browser UI component — so `filteredCount` may be measuring a browser whose UI half is never
instantiated for this layout.

Four attempts against that browser have all failed (labels probe, display config + crash, browser
state, SetLabel+FilterEntries). Do not retry them.

### Recommended next step

Stop probing the browser blind. Two better options:

1. **Compare against a working vanilla Conflict session** — run stock Conflict, open a base's
   building menu, and capture the same measurements (`placeableCount`, `filteredCount`, catalog
   entries). That gives a known-good reference for what these numbers look like when the UI works,
   instead of inferring from a broken one.
2. **Check whether the UI needs the campaign context** the BI developer described (UPDATE 9) — the
   slots may be rendered by a component that early-outs without `SCR_GameModeCampaign`. This is
   consistent with everything observed: data complete, display absent.

## UPDATE 21: full git archaeology of the building system — what `main` actually had

Traced every commit touching the building system, oldest to newest.

### The three eras

**Era 1 — `1913e82` "Implement vehicle supply system for campaign spawners" (Jan 2)**
Vehicles were listed DIRECTLY ON THE PROVIDER via `m_aCompositions`:
```
SCR_CampaignBuildingProviderComponent {
 m_aCompositions {
  SCR_CampaignBuildingCompositionOutpost {
   m_sName "UAZ469"   m_sDescription "Light utility vehicle"
   m_sIconName "Car"  m_Prefab "{259EE7B78C51B624}...UAZ469.et"
   m_iSupplyCost 75
  }
  ... UAZ469 PKM (100), Ural-4320 (120), BTR-70 (200)
 }
 m_aBudgetsToEvaluate { ... m_eBudget SUPPLIES ... }
}
```
Self-contained: name, description, icon, prefab and cost per vehicle, on the truck itself.

**Era 2 — `5eba7e2` "Integrate custom building manager" (Jan 4)**
DELETED all of `m_aCompositions` and replaced it with **nothing** on the combox. Added the vendored
951-line `SCR_CampaignBuildingManagerComponent.c`, whose `m_sPrefabsToBuildResource` was intended to
supply the list instead. Also changed `m_eBudget SUPPLIES` -> the current PROPS/CAMPAIGN setup.

**Era 3 — `main` (223 commits later)**
Confirmed `git merge-base --is-ancestor 5eba7e2 main` = YES, so `main` INCLUDES the rewrite and has
NO `m_aCompositions`. Vehicles nevertheless worked.

### Critical finding: nobody ever assigned `m_sPrefabsToBuildResource`

`git log --all -S "m_sPrefabsToBuildResource"` returns exactly ONE commit — `5eba7e2` — and it only
adds the ATTRIBUTE DECLARATION in the script. **No commit ever set a value.** On `main` the value
came purely by inheritance from Reforger Lobby's prefab, pointing at vanilla's
`Compositions_FreeRoamBuilding.conf` — the FORTIFICATIONS registry (UPDATE 17 dumped its contents:
hedgehogs, dragons teeth, barbed tape, sandbags, camo nets).

### So where did `main`'s VEHICLES come from? The faction entity catalog

`Configs/EntityCatalog/USSR/Vehicles_EntityCatalog_USSR.conf` (GUID `{09C2ED5F617A0390}`) is a
`SCR_EntityCatalogMultiList` — a sparse override of the faction's vehicle catalog:
```
13 SCR_EntityCatalogEntry total
 9 with m_bEnabled 0
= 4 ENABLED vehicles on main
```
`main`'s factions inherited vanilla `USSR.conf`/`US.conf` (UPDATE 19), which carry the VEHICLE
entity catalog; this file then enabled/disabled specific entries. **That is the vehicle channel.**

### Why this does NOT immediately explain HEAD

The user has already populated COALITION's OPFOR faction VEHICLE catalog with 6 vehicles, and
UPDATE 20 measured that the game SEES all 6 (`faction 'OPFOR' has 6 enabled entries`, each listed,
with supply costs and CAPTAIN rank overrides). So HEAD now has MORE enabled vehicles in the
faction catalog than `main` had (6 vs 4), read through the same provider->faction path — and the
slots still render empty.

### Honest status

Both of `main`'s content channels are now reproduced on HEAD:
- fortifications via `m_sPrefabsToBuildResource` -> 98 entries (UPDATE 16)
- vehicles via the faction VEHICLE catalog -> 6 entries, verified visible (UPDATE 20)

`main` had 4 vehicles through the same catalog channel. HEAD has 6 and shows none. The remaining
difference is therefore NOT the content configuration, and git history contains no further
building-related change to recover.

### Also: the camera workaround has been REMOVED

`modded class SCR_CameraEditorComponent` (which skipped `super.EOnFrame()` in BUILDING mode) froze
the editor camera — the user correctly reported the UI was visible but unusable. Removed. If the
menu resumes closing itself, that is the original `CreateCamera() -> Close()` bug resurfacing
(UPDATE 6), not a new fault.

## UPDATE 22: `main` did NOT use Campaign mode — and a concrete missing component found

User asked whether `main` used the campaign base gamemode. **It did not** — verified:
```
main gamemode entity: PS_GameModeCoop : "{9C2001FE7A2F2281}Prefabs/MP/Modes/PS_GameMode_Lobby.et"
main SCR_GameModeCampaign references: ONLY inside the vendored manager script (2 lines, one of
   which is the guard a previous author commented out)
```
So Campaign mode was never involved, and vehicles worked regardless. **This closes the "add
SCR_GameModeCampaign" direction** (UPDATE 9 / the BI developer quote) — it is not what made `main`
work, and adding it would be expensive and wrong.

### Extracted `PS_GameModeCoop`'s full component list from Reforger Lobby's pak

```
SignalsManagerComponent / SCR_CommunicationSoundComponent
PS_CutsceneManager / PS_MissionDataManager / PS_MissionDescriptionManager
PS_ObjectiveManager / PS_PlayableManager / PS_VoNRoomsManager / PS_KillListManager
SCR_CampaignBuildingManagerComponent {61B4DDBF56BF1B48}
   m_OutlineManager ... CampaignBuildingCompositionOutline.conf
   m_sPrefabsToBuildResource "{D2527D9AA5B4A33E}Configs/Editor/PlaceableEntities/Compositions/Compositions_FreeRoamBuilding.conf"
SCR_CommandingManagerComponent {61B4DDBF56BF1B63}      <-- ABSENT from COALITION
SCR_GameModeSFManager / SCR_InitWeatherComponent / SCR_NametagConfigComponent
SCR_NotificationSenderComponent / SCR_PreGameGameModeStateComponent
SCR_RespawnSystemComponent
SCR_RestoreEditorModesComponent {61B68227D500DBF7}     <-- ABSENT from COALITION
SCR_TimeAndWeatherHandlerComponent / SCR_VotingManagerComponent / SCR_ReconnectComponent
SCR_FactionManager : "{4A188E44289B9A50}Prefabs/MP/Managers/Factions/FactionManager_Editor.et"
```

### THE LEAD: `SCR_RestoreEditorModesComponent`

```
HEAD test_mode_2.layer          : 0 occurrences
COALITION data.pak (whole addon): 0 hits
SCR_CommandingManagerComponent  : 0 hits in COALITION
```

Per the API it derives from `SCR_BaseGameModeComponent` and hooks `OnPlayerConnected`,
`OnPlayerAuditSuccess`, `OnGameModeStart`, `OnGameStateChanged` — i.e. **it restores/grants editor
modes to players as they connect.**

This is highly consistent with symptoms that were never explained:
- COALITION's `CanOpen()` override checks `m_Modes.IsEmpty()` (their own code, UPDATE 6)
- the building mode entity is SPAWNED AD-HOC on each interaction
  (`SpawnEntityPrefab EditorModeBuilding.et` appears per open in every log) rather than being
  pre-registered for the player
- `SCR_EditorSettingsEntity` base-modes experiments changed nothing (UPDATE 12) — plausibly because
  the component that ACTS on those modes was missing entirely

**Hypothesis (explicitly untested):** without `SCR_RestoreEditorModesComponent`, players are never
granted properly-registered editor modes, so the building mode is constructed ad hoc and its UI
half (content browser) never initialises — matching `filteredCount=0` with a fully populated data
pipeline (UPDATE 20).

### Cheap next test

Add `SCR_RestoreEditorModesComponent` to `COA_Lobby` in `Worlds/MP/BC_kolgujev_Layers/test_mode_2.layer`
(Workbench: select COA_Lobby -> Add Component). It is a vanilla component, so no addon dependency
is introduced. Optionally also `SCR_CommandingManagerComponent`, but add ONE AT A TIME to keep the
variable isolated.

If the slots populate, this was the missing piece. If not, it is one clean negative and the
remaining delta is the faction manager base prefab (`FactionManager_Editor.et` on `main` vs
COALITION's own).

## UPDATE 23: CORRECTION — the close bug is GONE; camera works; only empty slots remain

An earlier claim in this session that "the menu still closes itself" after removing the
`EOnFrame` camera workaround was **WRONG**. It came from a raw `grep -c "menu is closing"` = 2,
without checking context. The context shows both hits are at SESSION SHUTDOWN:

```
01:55:11.878  BC Debug - menu is closing
01:55:11.879  Cannot find editor component 'SCR_MenuLayoutEditorComponent', local instance of
              editor manager not found!
01:55:12.123  WORLD : Game::LoadEntities        <-- Play session tearing down
```

`Game::LoadEntities` immediately afterwards = the world is being unloaded. The menu closed because
the GAME closed, not because of the `CreateCamera() -> Close()` bug.

### Current confirmed state (user-observed, not inferred)

| Symptom | Status |
|---|---|
| Menu closes itself one frame after opening | **FIXED** — no longer occurs in gameplay |
| Editor camera frozen/unmovable | **FIXED** — user confirms camera rotates |
| Vehicles missing from the slot bar | **STILL BROKEN** — the only remaining issue |

The `modded class SCR_CameraEditorComponent` workaround has been REMOVED and is not needed. The
original `CreateCamera() -> Close()` close (UPDATE 6) evidently stopped occurring once the data
pipeline was completed (`m_sPrefabsToBuildResource` restored, UPDATE 16) — consistent with the
user's early hypothesis that the camera bailed BECAUSE the mode had no content. That hypothesis
now looks correct.

`SCR_RestoreEditorModesComponent` was added to `COA_Lobby` `{6A197D0605D36620}` and produced a
clean negative (`filteredCount` still 0, no component activity logged). Harmless; can stay or be
removed.

### The remaining lead: the slot bar is NOT the content browser

BC's `Mode_CampaignBuilding.layout` renders its slot bar from:
```
SizeLayoutWidgetClass "{684712E412CD05C0}" :
  "{C257411D3EEB089A}UI/layouts/Editor/Toolbar/ModeMenu/Building/BuidlingPanel.layout" {
   Name "ModeMenu_Content"
  }
VerticalLayoutWidgetClass "{6626BC0000000002}" { Name "VehicleSupplyDisplay" }
```

`BuidlingPanel.layout` (vanilla, note the typo in the vanilla filename) is a DIFFERENT path from
`SCR_ContentBrowserEditorComponent`, which is what `filteredCount` measures. This likely explains
why four separate attempts to manipulate that browser all did nothing — **it may never have fed
these slots at all**, making `filteredCount=0` a red herring.

Note BC's layout also has a custom `VehicleSupplyDisplay` widget beside it, so this layout was
purpose-built for vehicle buying.

NEXT: investigate what populates `BuidlingPanel.layout` / `ModeMenu_Content`, not the content
browser.

## UPDATE 24: `m_aCompositions` is DELETED FROM THE ENGINE — and the plan forward

### Why the era-1 mechanism can never be restored

Commit `1913e82` listed vehicles directly on the provider via `m_aCompositions` /
`SCR_CampaignBuildingCompositionOutpost`. Restoring that block produced **no change**, because:

```
SCR_CampaignBuildingCompositionOutpost : 0 hits across ALL TEN vanilla data*.pak
m_aCompositions                        : 0 hits in the Script API index
```

**Bohemia removed both from the game.** The engine parses the unknown class and silently drops it.
The dead block has been removed from `Ural4320_combox.et` again.

This is a significant framing change for the whole investigation: **BC's building system was
partly built against a vanilla API that no longer exists.** This is not purely a COALITION
migration problem — the vanilla foundation moved underneath the mod as well.

---

# PLAN: BC-owned vehicle purchase UI on top of the existing building mode

## Rationale

After ~24 updates of investigation, the split between working and broken is now precise:

**WORKS (verified by measurement, keep all of it):**
| Piece | Evidence |
|---|---|
| `SCR_CampaignBuildingStartUserAction` on the truck | menu opens on interaction |
| Building mode activation lifecycle | `EOnEditorActivate/PostActivate` clean, no errors |
| Menu stays open | UPDATE 23 — close bug gone once data pipeline completed |
| Editor camera | UPDATE 23 — works after removing the `EOnFrame` workaround |
| Provider -> faction resolution | `factionKey=OPFOR` correct |
| **Faction VEHICLE catalog** | **6 vehicles, costs 30-200, CAPTAIN gates, VERIFIED VISIBLE** |
| `GRAD_BC_VehicleSupplyComponent` | BC's own supply system, 1000/1000, already replicated |
| BC's custom `Mode_CampaignBuilding.layout` | exists, incl. a `VehicleSupplyDisplay` widget |

**BROKEN / GONE (replace only this):**
| Piece | Status |
|---|---|
| `m_aCompositions` | deleted from the engine |
| `SCR_ContentBrowserEditorComponent` path | 4 failed attempts; likely never fed these slots |
| Manager `m_sPrefabsToBuildResource` | works, but only yields fortifications |

**Only the slot list needs replacing.** Everything else already functions.

## Design

Add a modded `SCR_CampaignBuildingEditorComponent` in BC that, on `EOnEditorPostActivate`:

1. Resolves the provider's faction (code already proven in the UPDATE 20 probe):
   `provider.GetOwner() -> FactionAffiliationComponent -> SCR_Faction`
2. Reads `GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE)` and `GetEntityList(entries)` —
   returns the 6 configured vehicles, already confirmed working
3. For each entry, reads its `SCR_EntityCatalogSpawnerOverrideData` for
   `m_iSupplyCostOverride` and `m_eMinimumRequiredRankOverride`
4. Builds BC's own slot widgets into the existing building layout, replacing whatever currently
   drives `ModeMenu_Content` / `BuidlingPanel.layout`
5. On click: check `GRAD_BC_VehicleSupplyComponent.GetCurrentSupplies()` >= cost, check the
   player's rank, then spawn via `SCR_PlacingEditorComponent` (or `GetGame().SpawnEntityPrefab`
   with the placement transform) and deduct supplies

## Why this is lower-risk than it sounds

- No dependency on the content browser, `filteredCount`, registries, or `m_sPrefabsToBuildResource`
- No dependency on Campaign mode (`main` never used it — UPDATE 22)
- No changes to COALITION's factions, lobby, slotting or gearscripts (UPDATE 20 proved a faction
  refactor is unnecessary)
- The data source is already configured and verified
- BC already vendors the manager and mods several editor classes, so this is a familiar pattern

## Build order (each step independently testable)

1. **Read + log** — modded `SCR_CampaignBuildingEditorComponent` dumps the 6 catalog entries with
   their costs/ranks at post-activate. (Mostly written already as the UPDATE 20 probe.)
2. **Find the slot widget** — locate the widget in the loaded layout that should host the entries
   (`ModeMenu_Content`, from `BuidlingPanel.layout`); log its widget tree to confirm the anchor.
3. **Render** — create one widget per catalog entry showing name, icon and supply cost.
4. **Select** — clicking an entry sets it as the active placement prefab.
5. **Place + charge** — spawn on confirm, deduct from `GRAD_BC_VehicleSupplyComponent`, respect
   rank gate and insufficient-supply cases.
6. **Cleanup** — remove the remaining `BC Debug` probes from `GRAD_BC_VehicleSpawnAction.c`.

## Open questions to resolve at step 2

- Does BC's custom layout `{F1279058C64D2815}` actually load, or does vanilla's
  `{1133DAC581B3D631}` win? (Currently vanilla's loads — the BC one is orphaned on BOTH branches.)
  If BC's layout is needed, that orphaning must be fixed first.
- Is `SCR_PlacingEditorComponent.SetSelectedPrefab()` usable directly for the placement ghost, or
  does BC need its own preview?

## Do NOT revisit (all disproven, with evidence, in UPDATES 1-24)

faction rank info as the cause; `m_aBudgetsToEvaluate` being empty; vehicle physics/movement;
ArmaVision `IsLimited` camera lock; content-browser label filtering / display config / browser
state / `SetLabel`; reading placing-component registries into the manager; BC's custom layout as
the regression; `SCR_EditorSettingsEntity` base modes; adding `SCR_GameModeCampaign`; changing
COALITION faction inheritance; `m_aCompositions`.

## UPDATE 25: SOLVED (filter half) — a single active label `SLOT_STATIC` rejected all 170 infos

### Step 1 discriminating probe — result

Verified-fresh build (compile 12:55:27, full Play relaunch). Measured at `EOnEditorPostActivate`:

```
BC Debug - MENU STATE: placeableCount=98, filteredCount=0
BC Debug - PROBE: infos=171 nonNull=170 activeLabels=1 anyActive=1 blacklisted=0 filtered=0
BC Debug - PROBE activeLabel=0 (#AR-Editor_ContentBrowser_Filter_SLOT_STATIC_Name)
BC Debug - PROBE: filtered AFTER ResetAllLabels=170
```

**Unambiguous case (b).** `170 of 171` infos are non-null, the blacklist is empty, and exactly ONE
label is active: **`SLOT_STATIC`, enum value `0`**. With it active, `filteredCount=0`. Clearing it
via `ResetAllLabels(false)` + `FilterEntries()` immediately yields **`filteredCount=170`**.

Before -> after on one variable: **0 -> 170**.

### RETRACTIONS forced by this measurement

1. **Hypothesis (a) — "every info is null" — is DISPROVEN.** `nonNull=170`. The registry contents
   are perfectly valid editable entities with UIInfo and labels. The earlier reasoning that raw
   `Prefabs/Vehicles/...` entries yield null infos is still true in general, but it is NOT what is
   happening here: only 1 of 171 is null.
2. **The faction-label theory (task facts 6/7) is NOT the cause of this blockage.** The active
   label is `SLOT_STATIC` (value 0), not a `FACTION_*` label. Confirmed independently: the two
   COALITION `SCR_Faction` entities in `test_mode_2.layer` carry **no faction-label override at
   all** (grep for `FactionLabel`/`FACTION_` in the faction manager block returns nothing), so
   `GetFactionLabel()` is returning its default — evidently `0`/`SLOT_STATIC`.

   That makes facts 6+7 the *delivery mechanism* rather than a separate issue:
   `EOnEditorActivate()` calls `AddRemoveFactionLabel(faction, true)` and injects whatever
   `GetFactionLabel()` returns into **every** tab state (`stateIndex = -1`); `EOnEditorDeactivate()`
   fails to remove it because its `SCR_CampaignFaction.Cast()` returns null on COALITION's plain
   `SCR_Faction`. So an *unset* faction label is being injected as literal label `0` = `SLOT_STATIC`
   and then never cleaned up — and with `m_bUsePersistentBrowserStates` defaulting true, it is
   round-tripped to disk, which explains the bit-identical results across fresh sessions,
   `git checkout`, and the `SCR_EditorSettingsEntity` deletion.

### Why this was invisible for ~24 updates

`SLOT_STATIC = 0` is indistinguishable from "unset/default" in every earlier probe. The
`IsMatchingToggledLabels()` test in an earlier update rejected even an EMPTY label set — consistent
with exactly this: a stale active label already poisoning the group.

### Consequence for the plan

- **Step 2 is now justified and targeted**, not speculative. Setting a real `Faction Label` on the
  two COALITION factions (OPFOR -> `FACTION_USSR`, BLUFOR -> `FACTION_US`) stops label `0` being
  injected; disabling `m_bUsePersistentBrowserStates` stops the stale value persisting to disk; the
  `EOnEditorDeactivate` cast fix stops it accumulating.
- **Step 3 remains REQUIRED regardless.** `filteredCount=170` after clearing labels is 170
  *fortification compositions* — the loaded registry still contains zero vehicles (fact 10). Fixing
  the filter alone yields a menu full of sandbags and hedgehogs, not vehicles.

Next: Step 2 sub-steps, measured one variable at a time.

## UPDATE 26: Step 2(b) is INVALID — `FACTION_USSR`/`FACTION_US` do not exist; and the real mechanism

### The plan's sub-step (b) cannot be performed

User reported: **"there is no FACTION_USSR in the dropdown"**. Verified against vanilla data — the
only `FACTION_*` values shipped by vanilla are unrelated concepts:

```
FACTION_DEFEAT  FACTION_DRAW  FACTION_ITEMS  FACTION_NEUTRAL
FACTION_ONLY    FACTION_SPAWNPOINTS  FACTION_TASKS  FACTION_VICTORY
```

There is **no vanilla `FACTION_USSR` / `FACTION_US`**. The instruction to set them is not
actionable, and the premise behind it (that vanilla ships per-army faction labels) is wrong.

### What COALITION actually did

`FACTION_COA_OPFOR` / `FACTION_COA_BLUFOR` appear **0 times in all ten vanilla `data*.pak`** and
20/28 times in COALITION's pak. **COALITION defined these enum values themselves.** OPFOR's
`Faction Label` is currently `FACTION_COA_OPFOR` (user-confirmed in Workbench).

COALITION tags their OWN entities with it, e.g. from their pak:
```
m_sFaction "OPFOR"
m_aAuthoredLabels {
 ENTITYTYPE_GROUP FACTION_COA_OPFOR TRAIT_SUPPRESSIVE TRAIT_ARMORPIERCING TRAIT_EXPLOSIVE
}
```

### THE MECHANISM (this explains the empty menu)

1. `EOnEditorActivate()` injects the provider faction's label into every browser tab state via
   `AddRemoveFactionLabel(faction, true)` -> label = `FACTION_COA_OPFOR`.
2. The 170 loaded placeables are **vanilla fortification compositions**
   (`PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/...`). Vanilla content cannot possibly
   carry a COALITION-invented enum value.
3. The FACTION label group is therefore active with a value that **no loaded entry can match** ->
   `filteredCount = 0`.
4. `EOnEditorDeactivate()`'s `SCR_CampaignFaction.Cast()` fails on COALITION's plain `SCR_Faction`,
   so the label is never removed and the state persists.

This is consistent with every measurement, including UPDATE 25's `filtered 0 -> 170` on
`ResetAllLabels`.

### Is `GetFactionLabel` safe to change? Evidence says COALITION does not read it

```
COALITION pak : "GetFactionLabel"  -> 0 hits
COALITION GitHub source            -> 0 references (grep across all pulled COA_*.c)
vanilla paks  : "GetFactionLabel"  -> 0 plaintext hits (compressed; consumers are editor-side)
```
The label is consumed by the **editor content browser filter**, not by COALITION's lobby, slotting,
gearscripts or VoN. Those key off `FactionKey` (`"OPFOR"`), which is a DIFFERENT field
(`m_FactionLabel` vs `GetFactionKey()`) and must NOT be touched (task fact 9).

**So changing `Faction Label` is low-risk for COALITION systems** — but there is no correct value to
change it TO, because no vanilla per-army faction label exists.

### Revised options for the FACTION-label blockage

1. **Neutralise the injection (preferred).** Override `EOnEditorActivate` in BC's existing
   `modded class SCR_CampaignBuildingEditorComponent` to call `super`, then immediately
   `AddRemoveFactionLabel(faction, false)` to strip the just-injected faction label. Leaves
   COALITION's faction data untouched, no enum invention, and self-corrects each activation.
   Pairs with the `EOnEditorDeactivate` fix already added.
2. **Author BC vehicles WITH `FACTION_COA_OPFOR` in their `m_aAuthoredLabels`.** Works WITH the
   filter instead of against it, and is arguably the intended design — but requires editable
   vehicle variants BC controls (Step 3 authors these anyway).
3. Setting `Faction Label` to some other existing value — rejected: every available value is
   semantically wrong (`FACTION_NEUTRAL`, `FACTION_TASKS`, ...) and would still not match vanilla
   fortifications.

Option 1 is the smaller, reversible change and does not block option 2 later.

## UPDATE 27: VEHICLES VISIBLE IN MENU — placement fails because the tiles are vanilla FIA prefabs

### Progress: the menu now renders vehicle tiles

Screenshot confirms: three category tabs, UAZ-469 variants with prices (50 / 150 / 280), supply
counter 1000/1000, tooltips working. **This is the first time vehicle tiles have ever rendered.**

### Placement failure — exact cause

```
15:13:55.427 SCRIPT (E): Error when creating entity from prefab '{E72D78E7F45532EC}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_UK59_FIA.et' (id = 35)!
15:13:58.328 SCRIPT (E): Error when creating entity from prefab '{22B327C6752EC4D4}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_PKM_FIA.et'  (id = 34)!
15:14:01.594 SCRIPT (E): Error when creating entity from prefab '{F7E9AA0C813EABDA}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_FIA.et'      (id = 33)!
```

The prefabs being placed are **`*_FIA` variants** — they are NOT BC's configured vehicles. BC's
faction catalog contains:
```
UAZ469.et / UAZ469_PKM.et / UAZ452_transport.et
Ural4320_transport_covered.et / BRDM2.et / BTR70.et
```
No FIA variants anywhere in `test_mode_2.layer`. **The tiles come from the vanilla placing registry
(`Compositions_FreeRoamBuilding.conf` -> 99 placeables), not from BC's faction VEHICLE catalog.**

The prefab IDs (33/34/35) are indices into the placing component's list, confirming the click path
goes through `SCR_PlacingEditorComponent`, which resolves against the REGISTRY, not the catalog.

### RETRACTIONS

1. **`filteredCount` is NOT the instrument for these tiles.** Measured `filteredCount=0` while
   vehicle tiles were visibly rendered on screen. Task fact 1 ("filteredCount is the correct
   instrument") is disproven for this UI path. Stop using it as the success metric.
2. **The `EOnEditorActivate` label-strip fix did not do what was claimed.** It removed label
   `51871` (the real `OPFOR` faction label — the probe now confirms
   `faction 'OPFOR' factionLabel=51871 (OPFOR)`), but `SLOT_STATIC` (value 0) is STILL active
   afterwards (`activeLabels=1`). So the tiles appearing is NOT attributable to that fix, and
   UPDATE 26's mechanism was at best incomplete. What actually changed between the empty menu and
   the visible tiles was the accumulation of registry/catalog config, not the label strip.
3. `canSavePersistent=1` — persistent browser states ARE still enabled and round-tripping to user
   settings on disk. BC's `EditorModeBuilding.et` is still an empty stub with no content-browser
   component override.

### Unrelated noise, for the record

The 18 `Virtual Machine Exception` entries are pre-existing COALITION gearscript errors
(`UNABLE TO INSERT ITEM AMMO_ROCKET_PG7VR / NOT ENOUGH SPACE IN ENTITY`) fired during slotting at
15:13:31 — 20 seconds BEFORE the building menu opened. Not related.

### Next step

Point the placing registry at BC's vehicles so the tiles ARE the configured prefabs. This is Step 3
of the plan and is now clearly the remaining work: author a `SCR_PlaceableEntitiesRegistry` listing
BC's vehicles and set it on BOTH
- `SCR_CampaignBuildingManagerComponent.m_sPrefabsToBuildResource` (COA_Lobby `{6A0956CFB7A9F2D8}`)
- `SCR_CampaignBuildingPlacingEditorComponent.m_Registries` (BC's `EditorModeBuilding.et`)

Open question to resolve first: whether the `*_FIA` prefabs fail to spawn because they are
inherently non-spawnable in this context, or because of a placement/budget rule. If BC's own
vehicles spawn correctly once registered, the FIA failure is moot.

## UPDATE 28: the placing registries are CATALOG-DRIVEN, and `SCR_EntityCatalogManagerComponent` is missing

### Correction to UPDATE 27's proposed fix

I instructed the user to add `SCR_CampaignBuildingPlacingEditorComponent` and point its
`m_Registries` at `Compositions_FreeRoamBuilding.conf`. **That was wrong** — the user's screenshot
shows the registries on this component are not `.conf` references at all. They are
`SCR_PlaceableEntitiesRegistryFromCatalog` entries, which per the API
*"take entities from the catalog ... It fills in the Prefab list from the catalog"*.

Their fields are `Addon`, `Source Directory`, `Exposed`, `Prefabs`, `Editor Mode`,
`Catalog Faction Type`, `Catalog Types` — there is no `.conf` slot.

### What the component actually contains (from the screenshot)

```
[1] SCR_PlaceableEntitiesRegistry
[2] SCR_PlaceableEntitiesRegistry
[3] Catalog Getter (BUILDING) - GROUP & VEHICLE - FACTIONS_ONLY   <-- correctly configured
      Editor Mode = BUILDING, Catalog Faction Type = FACTIONS_ONLY, Catalog Types = (2)
[4] SCR_PlaceableEntitiesGameModeRegistry
[5] Catalog Getter (unknown) - MISSING CATALOGS! - FACTION_AND_FACTIONLESS   <-- BROKEN
      Editor Mode = 0 (should be BUILDING), Catalog Types = (0) EMPTY
```

Entry [3] is already set up to pull **VEHICLE catalogs from factions in BUILDING mode** — which is
exactly the intended path to BC's OPFOR vehicle catalog, and explains why vehicles rendered without
the user touching this component.

### THE MISSING DEPENDENCY

`SCR_PlaceableEntitiesRegistryFromCatalog.ProcessCatalog()` takes a
**`SCR_EntityCatalogManagerComponent`**. Measured:

```
SCR_EntityCatalogManagerComponent in test_mode_2.layer : 0
SCR_EntityCatalogManagerComponent in COALITION data.pak: 0
```

**It does not exist anywhere.** This is very likely the literal cause of the
`MISSING CATALOGS!` label on entry [5], and means the catalog-driven registries cannot resolve
faction catalogs into placeable prefabs.

Note: this component was investigated and dismissed much earlier in this document on the grounds
that it "does not exist in either branch's layer, therefore it was never the wiring mechanism".
That dismissal was **wrong reasoning** — its absence is the defect, not evidence of irrelevance.
On `main` the catalog path was never exercised because vehicles came through a different route.

### Next test (single variable)

Add `SCR_EntityCatalogManagerComponent` to `COA_Lobby` in `test_mode_2.layer` (Workbench:
select COA_Lobby -> Add Component). It is a vanilla component; no new addon dependency.

Expected if correct: entry [5] stops reporting `MISSING CATALOGS!`, and the catalog getter [3]
resolves BC's OPFOR VEHICLE catalog (6 vehicles, verified present and enabled in UPDATE 20) into
the placing list — making the clicked tiles BC's own prefabs instead of vanilla `*_FIA` variants.

If tiles still resolve to `*_FIA`, the catalog getter is not the source feeding them and the
registry entries [1]/[2] must be inspected instead.

## UPDATE 29: RETRACTION — `SCR_EntityCatalogManagerComponent` was ALREADY present

User correctly challenged UPDATE 28: *"but the component existed in my previous test already"*.

Verified — the component IS attached to `COA_Gamemode` (user screenshot shows it with 6 catalog
types: CHARACTER, VEHICLE, GROUP, WEAPONS_TRIPOD, ITEM, SUPPLY_CONTAINER_ITEM). It comes from the
**gamemode prefab**, not from `test_mode_2.layer`.

**UPDATE 28's central claim is therefore WRONG.** Its measurement was:
```
SCR_EntityCatalogManagerComponent in test_mode_2.layer : 0
SCR_EntityCatalogManagerComponent in COALITION data.pak: 0
```
Both numbers are real but both are MISLEADING:
- the `.layer` only records overrides, so an inherited component is invisible there (this is the
  SAME trap that produced the false "`m_sPrefabsToBuildResource` was unset on main" conclusion in
  UPDATE 10/12 — a `.layer` grep cannot prove absence of an inherited component)
- the packed prefab evidently stores the component by GUID rather than by class name, so a
  plaintext `grep` of `data.pak` returns 0 despite the component being present (COALITION's pak
  does contain 8 `SCR_EntityCatalogMultiList` entries, confirming catalog data is there)

**Standing lesson: grepping a `.layer` or a `.pak` for a class name CANNOT establish that a
component is absent.** Verify in Workbench's component list instead.

So `MISSING CATALOGS!` on registry entry [5] is NOT caused by a missing manager component. Its own
fields explain it directly:
```
Catalog Getter (unknown) - MISSING CATALOGS! - FACTION_AND_FACTIONLESS
  Editor Mode   = 0    (not BUILDING)
  Catalog Types = (0)  (EMPTY)
```
Entry [5] declares **no catalog types at all**, so it has nothing to fetch — self-explanatory, and
unrelated to the manager.

### Also do NOT author catalogs in the manager

Its class doc: *"Manager for **non-faction specific** entity catalogs as well as **getters for
faction specific catalogs**."* The `Entity Catalogs` list on the component is for NON-faction
catalogs. BC's vehicles live on the OPFOR **faction** catalog and are reached via
`GetFactionEntityCatalogOfType()`. Registry entry [3] is set to `FACTIONS_ONLY`, so it reads
faction catalogs and would ignore the manager's own list entirely.

The empty `VEHICLE (1) !!` / `NO PREFAB` stub visible in the screenshot was created by expanding the
list in Workbench. It is invalid (hence `!!`) and should be removed rather than filled.

### Where this leaves the placement bug

Still unexplained: clicking a tile places vanilla `*_FIA` prefabs (ids 33/34/35) which fail with
`Error when creating entity from prefab`. The tiles are NOT BC's six configured vehicles.

Since the manager exists and entry [3] is correctly configured for BUILDING/VEHICLE/FACTIONS_ONLY,
the next question is which registry entry actually supplied the rendered tiles — [1], [2], [3] or
[4] — and whether BC's faction vehicles are reaching the placing list at all. Registry entries
[1] and [2] are plain `SCR_PlaceableEntitiesRegistry` and have not been inspected.

## UPDATE 30: the FIA tiles come from VANILLA faction catalogs, not BC's

User observation (Workbench, `SCR_FactionManager` -> OPFOR faction): the OPFOR `Entity Catalogs`
contains `VEHICLE (6)` with exactly:
```
UAZ469.et  UAZ469_PKM.et  UAZ452_transport.et
Ural4320_transport_covered.et  BRDM2.et  BTR70.et
```
**No FIA variants.** Yet clicking tiles attempted to place `UAZ469_FIA`, `UAZ469_PKM_FIA`,
`UAZ469_UK59_FIA` (ids 33/34/35), all of which failed with
`Error when creating entity from prefab`.

### Where the FIA prefabs come from

```
BC repo         : UAZ469_FIA / UAZ469_PKM_FIA / UAZ469_UK59_FIA -> 0 references
COALITION pak   : 0 references
vanilla data005 : UAZ469_FIA x34   <-- SOURCE
```

They are **vanilla content**, reached because the placing component's catalog getter is configured:
```
Catalog Getter (BUILDING) - GROUP & VEHICLE - FACTIONS_ONLY
```
`FACTIONS_ONLY` pulls VEHICLE catalogs from **all factions present**, not only the provider's
faction. Vanilla's FIA/INDFOR faction ships a populated VEHICLE catalog, so its entries land in the
placing list alongside BC's six.

This explains the whole symptom cleanly:
- tiles render (catalog getter IS working - it resolves faction VEHICLE catalogs)
- but the tiles shown/clicked are FIA entries from vanilla's faction
- and those FIA prefabs fail to instantiate in this context

### Corrected understanding of the pipeline

The catalog path is functioning. BC's six vehicles ARE reachable (UPDATE 20 proved the OPFOR
catalog resolves correctly through the provider's faction). The defect is that the getter is not
scoped to the provider's faction, so foreign-faction vehicles are offered too.

### Candidate fixes (untested - single variable each)

1. **Scope the getter to the provider faction.** Change `Catalog Faction Type` on the BUILDING
   catalog getter from `FACTIONS_ONLY` to whatever value restricts to the requesting faction, if
   such a value exists in the `SCR_ECatalogFactionType` enum. Needs the enum values checked in
   Workbench's dropdown first.
2. **Remove/blank the vanilla FIA faction's VEHICLE catalog** in the layer, so nothing foreign is
   contributed. Heavier-handed; affects anything else reading that catalog.
3. **Filter at placement time** in BC's modded `SCR_CampaignBuildingEditorComponent`, rejecting
   prefabs not present in the provider faction's own catalog.

Option 1 is the smallest change if a suitable enum value exists. NOTE: the `!!`-marked invalid
`VEHICLE (1) / NO PREFAB` stub the user accidentally created on
`SCR_EntityCatalogManagerComponent` has since been removed; that component's own catalogs are
correctly empty (it manages NON-faction catalogs and is only a getter for faction ones).
