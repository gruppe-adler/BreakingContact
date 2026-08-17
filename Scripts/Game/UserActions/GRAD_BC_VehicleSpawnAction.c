modded class SCR_CampaignBuildingStartUserAction
{
	// Building menu access is not blocked by vehicle supplies, but only the company
	// commander may open it - vehicle purchasing is a commander-only decision.
	//
	// NOTE: CanBeShownScript/CanBePerformedScript run locally on the client, so this is a
	// UI-level gate, not an authoritative one. That is intentional and sufficient here; the
	// server-side chokepoint would be SCR_CampaignBuildingPlacingEditorComponent.CanPlaceEntityServer.
	override bool CanBeShownScript(IEntity user)
	{
		if (!GRAD_BC_BreakingContactManager.IsCommanderEntity(user))
			return false;

		return super.CanBeShownScript(user);
	}

	override bool CanBePerformedScript(IEntity user)
	{
		if (!GRAD_BC_BreakingContactManager.IsCommanderEntity(user))
			return false;

		return super.CanBePerformedScript(user);
	}
}

//------------------------------------------------------------------------------------------------
// MINIMAL DIAGNOSTIC. One line, so the log stays readable.
//   placeableCount - m_aPlaceablePrefabs on the manager. Was 0 (empty menu); after restoring
//                    m_sPrefabsToBuildResource it is 98, including the 6 BC vehicles at 92-97.
//   filteredCount  - what the content browser passes to the UI. Still 0 = slots render empty.
modded class SCR_CampaignBuildingEditorComponent
{
	//------------------------------------------------------------------------------------------------
	// THE FIX (UPDATE 26, option 1): strip the faction label that vanilla injects into the content
	// browser filter on activation.
	//
	// Vanilla EOnEditorActivate() calls AddRemoveFactionLabel(faction, true), which writes the
	// provider faction's EEditableEntityLabel into EVERY browser tab state (stateIndex = -1).
	// COALITION's factions carry FACTION_COA_OPFOR / FACTION_COA_BLUFOR - enum values COALITION
	// DEFINED THEMSELVES (0 hits in all ten vanilla data*.pak). The placeables actually loaded are
	// vanilla fortification compositions, which cannot carry a COALITION-invented label, so the
	// FACTION label group goes active with a value nothing can match -> filteredCount = 0.
	//
	// Measured: UPDATE 25 showed filtered 0 -> 170 the instant labels were cleared.
	//
	// Removing the label immediately after super() leaves COALITION's faction data untouched (their
	// lobby/slotting/gearscripts/VoN key off FactionKey, a DIFFERENT field - and GetFactionLabel is
	// read 0 times anywhere in their pak or source). Reversible, and does not block later tagging
	// BC-owned vehicle prefabs with FACTION_COA_OPFOR to work WITH the filter instead.
	protected override void EOnEditorActivate()
	{
		super.EOnEditorActivate();

		SCR_CampaignBuildingProviderComponent provider = GetProviderComponent();
		if (!provider)
			return;

		FactionAffiliationComponent facComp = FactionAffiliationComponent.Cast(
			provider.GetOwner().FindComponent(FactionAffiliationComponent));
		if (!facComp)
			return;

		SCR_Faction scrFaction = SCR_Faction.Cast(facComp.GetAffiliatedFaction());
		if (!scrFaction)
			return;

		Print(string.Format("BC Debug - ACTIVATE: stripping injected faction label %1 for '%2'",
			scrFaction.GetFactionLabel(), scrFaction.GetFactionKey()), LogLevel.WARNING);

		AddRemoveFactionLabel(scrFaction, false);
	}

	protected override void EOnEditorPostActivate()
	{
		super.EOnEditorPostActivate();

		int placeableCount = -1;
		SCR_CampaignBuildingManagerComponent buildingManager = SCR_CampaignBuildingManagerComponent.Cast(
			GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent));
		if (buildingManager)
		{
			array<ResourceName> placeable = buildingManager.GetPlaceablePrefabs();
			if (placeable)
				placeableCount = placeable.Count();
		}

		int filteredCount = -1;
		SCR_ContentBrowserEditorComponent browser = SCR_ContentBrowserEditorComponent.Cast(
			SCR_ContentBrowserEditorComponent.GetInstance(SCR_ContentBrowserEditorComponent));
		if (browser)
			filteredCount = browser.GetFilteredPrefabCount();

		Print(string.Format("BC Debug - MENU STATE: placeableCount=%1, filteredCount=%2",
			placeableCount, filteredCount), LogLevel.WARNING);

		//--------------------------------------------------------------------------------------------
		// STEP 1 DISCRIMINATING PROBE.
		//
		// From the real vanilla source, FilterEntries() applies exactly three gates: null-info skip,
		// IsMatchingToggledLabels(), and the blacklist. It never consults budgets. And
		// IsMatchingToggledLabels() returns TRUE immediately when no labels are active.
		//
		// Blacklist is already measured as empty. Therefore filteredCount=0 means EITHER
		//   (a) every m_aInfos entry is null  -> the registry holds non-editable prefabs
		//                                        (raw Prefabs/... has no UIInfo; only
		//                                         PrefabsEditable/Auto/... carries labels), OR
		//   (b) labels ARE active and a label group rejects everything.
		//
		// Clearing labels is the discriminating operation. Adding labels was tried before and is
		// inert - it can only add constraints to a group that is already failing.
		if (browser)
		{
			array<EEditableEntityLabel> active = {};
			browser.GetActiveLabels(active);

			int total = browser.GetInfoCount();
			int nonNull = 0;
			for (int i = 0; i < total; i++)
			{
				if (browser.GetInfo(i))
					nonNull++;
			}

			array<EEditableEntityLabel> blacklisted = {};
			int blacklistCount = browser.GetValidBlackListedLabels(blacklisted);

			// canSavePersistent reports the EFFECTIVE runtime value of m_bUsePersistentBrowserStates.
			// BC's EditorModeBuilding.et is an empty stub on disk (no ContentBrowser component), so
			// whatever Workbench shows is INHERITED from vanilla EditorModeBase.et, not a BC override.
			// If this reports 1, the poisoned label set is still being round-tripped to user settings
			// on disk and BC must add its own component override to turn it off.
			Print(string.Format("BC Debug - PROBE: infos=%1 nonNull=%2 activeLabels=%3 anyActive=%4 blacklisted=%5 filtered=%6 canSavePersistent=%7",
				total, nonNull, active.Count(), browser.IsAnyLabelActive(), blacklistCount,
				browser.GetFilteredPrefabCount(), browser.CanSavePersistentBrowserStates()), LogLevel.WARNING);

			foreach (EEditableEntityLabel activeLabel : active)
			{
				Print(string.Format("BC Debug - PROBE activeLabel=%1 (%2)",
					activeLabel, browser.GetLabelName(activeLabel)), LogLevel.WARNING);
			}

			// What label is the provider's faction actually contributing? UPDATE 25 showed the
			// leaked label is 0 (SLOT_STATIC) = the factions' UNSET default. After setting a real
			// Faction Label in Workbench this must report FACTION_USSR / FACTION_US instead.
			SCR_CampaignBuildingProviderComponent probeProvider = GetProviderComponent();
			if (probeProvider)
			{
				FactionAffiliationComponent probeFac = FactionAffiliationComponent.Cast(
					probeProvider.GetOwner().FindComponent(FactionAffiliationComponent));
				if (probeFac)
				{
					SCR_Faction probeFaction = SCR_Faction.Cast(probeFac.GetAffiliatedFaction());
					if (probeFaction)
					{
						Print(string.Format("BC Debug - PROBE: faction '%1' factionLabel=%2 (%3)",
							probeFaction.GetFactionKey(), probeFaction.GetFactionLabel(),
							browser.GetLabelName(probeFaction.GetFactionLabel())), LogLevel.WARNING);
					}
				}
			}

			// NOTE: ResetAllLabels() is deliberately NOT called here any more. With the
			// EOnEditorActivate fix above, `filtered` on the FIRST line should already be non-zero.
			// Clearing labels here would mask whether the fix actually worked.
			// If `filtered` is still 0 AND activeLabels > 0, the strip did not take effect - the
			// printed activeLabel names whatever is still blocking.

			//----------------------------------------------------------------------------------------
			// UPDATE 34 - WHERE DO THE MENU TILES ACTUALLY COME FROM?
			//
			// Observed: the menu shows a GENERIC vehicle list - vanilla FIA, US MERDC, Conflict
			// variants, and even CIVILIAN vehicles - in an OPFOR provider's menu. Only 3 of the 17
			// prefabs ever seen overlap BC's six, and BC's BRDM2 / BTR70 / Ural4320_transport NEVER
			// appear at all. So the tiles are NOT coming from
			// m_sPrefabsToBuildResource -> Compositions_FreeRoamBuilding.conf.
			//
			// Two lists are in play and they disagree:
			//   A) the MANAGER list  - GetPlaceablePrefabs(), fed by m_sPrefabsToBuildResource
			//   B) whatever the BROWSER renders
			// placeableCount reports A. filteredCount reports 0 while tiles are visibly rendered,
			// so the UI is NOT reading the filtered view of A either.
			//
			// This dumps BOTH lists so the feed can be identified by comparison. Civilian entries
			// appearing in B would confirm a catalog-wide source that ignores the provider faction.
			// FEED dump REMOVED - it did its job (proved the browser list and the manager list are
			// different arrays with different orderings, which is why prefab IDs did not resolve)
			// and cost 274 log lines per menu open. Restore from git history if that comparison is
			// ever needed again.
		}
	}

	//------------------------------------------------------------------------------------------------
	// STEP 2 SUB-STEP (c): fix the label leak.
	//
	// Vanilla EOnEditorActivate() injects the provider faction's label into EVERY tab state via
	//   AddRemoveFactionLabel(SCR_Faction.Cast(buildingFaction), true)
	// but EOnEditorDeactivate() tries to remove it with SCR_CampaignFaction.Cast(...), which returns
	// NULL for COALITION's plain SCR_Faction. The label is therefore never removed, and with
	// m_bUsePersistentBrowserStates defaulting true it is saved to user settings on disk - which is
	// why the poisoned state survived fresh sessions, git checkout and entity deletion.
	//
	// UPDATE 25 measured the leaked label as value 0 (SLOT_STATIC), i.e. the factions' UNSET default,
	// and it alone rejected all 170 valid infos.
	//
	// AddRemoveFactionLabel() is public and takes the BASE SCR_Faction type, so we can do the removal
	// correctly here without touching the broken cast.
	protected override void EOnEditorDeactivate()
	{
		SCR_CampaignBuildingProviderComponent provider = GetProviderComponent();
		if (provider)
		{
			FactionAffiliationComponent facComp = FactionAffiliationComponent.Cast(
				provider.GetOwner().FindComponent(FactionAffiliationComponent));
			if (facComp)
			{
				SCR_Faction scrFaction = SCR_Faction.Cast(facComp.GetAffiliatedFaction());
				if (scrFaction)
				{
					Print(string.Format("BC Debug - DEACTIVATE: removing faction label %1 for '%2'",
						scrFaction.GetFactionLabel(), scrFaction.GetFactionKey()), LogLevel.WARNING);
					AddRemoveFactionLabel(scrFaction, false);
				}
			}
		}

		super.EOnEditorDeactivate();
	}
}

//------------------------------------------------------------------------------------------------
// !!! TEMPORARY WORKAROUND - NOT A FIX. DO NOT SHIP. !!!
//
// Call-stack proven (Debug.DumpStack, 4/4 stacks over 2 runs):
//   EOnFrame -> TryCreateCamera -> SCR_CampaignBuildingCameraEditorComponent.CreateCamera()
//   -> SCR_EditorManagerEntity.Close()
// CreateCamera() is protected, so its guard cannot be fixed directly. EOnFrame is public, so
// skipping super() in BUILDING mode stops the chain. Cost: the editor camera is stuck/unmovable.
//
// Likely a SYMPTOM of the mode having no content. If the menu populates and stays open by itself,
// DELETE THIS CLASS.
// REMOVED. It froze the editor camera (skipping super.EOnFrame() also skips the camera's real
// per-frame work), which is worse than the close it suppressed. If the menu starts closing itself
// again after this removal, that is the ORIGINAL CreateCamera() -> Close() bug resurfacing, not a
// new problem - see the call stack above.

//------------------------------------------------------------------------------------------------
// UPDATE 35 - THE REJECTION FOUND IN VANILLA SOURCE.
//
// SCR_CampaignBuildingPlacingEditorComponent.CanPlaceEntityServer() ends with:
//
//   SCR_EditableEntityUIInfo prefabUIInfo =
//       SCR_EditableEntityUIInfo.ExtractEditableUIInfoFromPrefab(prefabData.GetPrefab(prefabID));
//   if (!prefabUIInfo)
//       return false;
//   array<EEditableEntityLabel> entityLabels = {};
//   prefabUIInfo.GetEntityLabels(entityLabels);
//   return AreLabelsMatching(entityLabels);
//
// and AreLabelsMatching() gates on:
//
//   if (!entityLabels.Contains(providerFaction.GetFactionLabel())) // faction label must match
//       return false;
//
// MEASURED: the provider faction is COALITION's OPFOR with GetFactionLabel() == 51871
// (FACTION_COA_OPFOR - a COALITION-invented enum value, 0 hits in all ten vanilla data*.pak).
// The placeables are VANILLA prefabs whose UIInfo carries VANILLA labels (FACTION_USSR etc).
// A vanilla prefab can therefore NEVER contain 51871, so this returns false for EVERY prefab -
// BC's six, vanilla FIA, US MERDC, Conflict variants alike. That is exactly the observed symptom.
//
// This also CORRECTS the earlier "fails below every script hook" conclusion: the rejection IS in
// script, but in CanPlaceEntityServer - which runs BEFORE OnBeforeEntityCreatedServer. Our probes
// on the latter were simply downstream of the gate, which is why they never fired.
//
// NOTE the existing EOnEditorActivate fix (AddRemoveFactionLabel(faction, false)) does NOT help
// here: that strips the label from the CONTENT BROWSER FILTER, while AreLabelsMatching reads
// providerFaction.GetFactionLabel() directly from the faction object.
//
// FIX: drop only the faction-label gate, keep every other check vanilla enforces (provider traits,
// blacklist, SERVICE_HQ handling). Re-implemented rather than super()-ed because the faction check
// sits in the middle of the method and cannot be skipped otherwise.
//
// SECOND KNOWN BLOCKER (not addressed here): prefabID indexes prefabData.GetPrefab(prefabID) - the
// PLACING component's list - while the browser tile indices come from a DIFFERENT list (measured:
// browser has 174 entries with vehicles at 32-75; the manager list has 99 with BC's six at 93-98).
// If placement still fails after this, that index mismatch is the next thing to attack.
modded class SCR_CampaignBuildingPlacingEditorComponent
{
	override protected bool AreLabelsMatching(notnull array<EEditableEntityLabel> entityLabels)
	{
		if (!m_Provider)
			return false;

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(
			m_Provider.FindComponent(FactionAffiliationComponent));
		if (!fac)
		{
			// Construction trucks: the provider is the truck's back, the faction component is on
			// the truck itself, so walk up the parent chain (same as vanilla).
			IEntity parent = m_Provider.GetParent();
			while (parent && fac == null)
			{
				fac = FactionAffiliationComponent.Cast(parent.FindComponent(FactionAffiliationComponent));
				parent = parent.GetParent();
			}

			if (!fac)
				return false;
		}

		SCR_Faction providerFaction = SCR_Faction.Cast(fac.GetAffiliatedFaction());
		if (!providerFaction)
		{
			providerFaction = SCR_Faction.Cast(fac.GetDefaultAffiliatedFaction());
			if (!providerFaction)
				return false;
		}

		// VANILLA DOES:
		//   if (!entityLabels.Contains(providerFaction.GetFactionLabel()))
		//       return false;
		// SKIPPED - see block comment above. COALITION's faction label can never appear on a
		// vanilla prefab, so this gate rejects everything under COA_Gamemode.
		Print(string.Format("BC Debug - LABELGATE: skipping faction-label check (providerLabel=%1, entityLabels=%2)",
			providerFaction.GetFactionLabel(), entityLabels.Count()), LogLevel.WARNING);

		SCR_CampaignBuildingProviderComponent providerComp = SCR_CampaignBuildingProviderComponent.Cast(
			m_Provider.FindComponent(SCR_CampaignBuildingProviderComponent));
		if (!providerComp)
			return false;

		array<EEditableEntityLabel> providerLabels = providerComp.GetAvailableTraits();
		if (providerLabels.Contains(EEditableEntityLabel.SERVICE_HQ)
			&& !SCR_CampaignBuildingProviderComponent.CanBeUsedToEstablishBase(m_Provider, m_Manager.GetPlayerID()))
		{
			providerLabels.RemoveItem(EEditableEntityLabel.SERVICE_HQ);
		}

		bool matchingLabel;
		foreach (EEditableEntityLabel providerLabel : providerLabels)
		{
			if (!entityLabels.Contains(providerLabel))
				continue;

			matchingLabel = true;
			break;
		}

		if (!matchingLabel)
		{
			Print("BC Debug - LABELGATE: REJECTED - no provider trait matched", LogLevel.WARNING);
			return false;
		}

		SCR_ContentBrowserEditorComponent browserComp = SCR_ContentBrowserEditorComponent.Cast(
			m_Owner.FindComponent(SCR_ContentBrowserEditorComponent));
		if (!browserComp)
			return false;

		array<EEditableEntityLabel> validBlackListLabels = {};
		browserComp.GetValidBlackListedLabels(validBlackListLabels);
		foreach (EEditableEntityLabel blacklistedLabel : validBlackListLabels)
		{
			if (entityLabels.Contains(blacklistedLabel))
			{
				Print("BC Debug - LABELGATE: REJECTED - blacklisted label", LogLevel.WARNING);
				return false;
			}
		}

		Print("BC Debug - LABELGATE: PASSED", LogLevel.WARNING);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolve the cost for the prefab about to be placed. This override DOES receive prefabID, so
	//! it is where the pending cost gets established for the gate in AreLabelsMatching().
	override protected bool CanPlaceEntityServer(IEntityComponentSource editableEntitySource, out EEditableEntityBudget blockingBudget, bool updatePreview, bool showNotification, int prefabID = -1, int playerID = -1, SCR_EditorPreviewParams params = null)
	{
		m_iBC_PendingCost = BC_GetVehicleCost(editableEntitySource);

		if (m_iBC_PendingCost > 0)
		{
			Print(string.Format("BC Debug - BUDGET: pending cost=%1 for prefabID=%2",
				m_iBC_PendingCost, prefabID), LogLevel.WARNING);
		}

		return super.CanPlaceEntityServer(editableEntitySource, blockingBudget, updatePreview,
			showNotification, prefabID, playerID, params);
	}

	//------------------------------------------------------------------------------------------------
	// BUDGET DIAGNOSTIC.
	//
	// Symptom: ~20 vehicles can be spawned from a truck holding 1000 supplies, i.e. no budget is
	// ever actually deducted or enforced.
	//
	// Suspected cause: the truck's SCR_CampaignBuildingProviderComponent.m_aBudgetsToEvaluate lists
	// PROPS, COOLDOWN, RANK_CAPTAIN and CAMPAIGN - but NOT whatever budget the vehicles themselves
	// declare. IsThereEnoughBudgetToSpawn() delegates to providerComponent.IsThereEnoughBudgetToSpawn
	// (budgetCosts), and a budget type absent from m_aBudgetsToEvaluate is simply not evaluated.
	//
	// Earlier BC debug output showed vehicles reporting entityBudget 5 and 6 while the manager sat
	// on 0 (PROPS), every one hitting "Early return: entityBudget != m_BudgetType".
	//
	// This logs the budget types/values each placement actually reports, so the correct entry can be
	// added to m_aBudgetsToEvaluate in Workbench rather than guessed at.
	//! Cost of the placement currently being evaluated, so OnEntityCreatedServer can deduct exactly
	//! what IsThereEnoughBudgetToSpawn approved. Set on every check, read once on success.
	protected int m_iBC_PendingCost;

	//------------------------------------------------------------------------------------------------
	//! Pull the vehicle's real supply cost out of its declared budgets.
	//! Measured: a BTR70 reports type=120 value=2 (CAMPAIGN), type=5 value=200 (VEHICLES),
	//! type=6 value=1. Type 5 / VEHICLES carries the number configured as m_iSupplyCostOverride on
	//! the faction catalog entry (BTR70 = 200), so that is the one to charge.
	protected int BC_GetVehicleCost(IEntityComponentSource entitySource)
	{
		if (!m_BudgetManager || !entitySource)
			return 0;

		array<ref SCR_EntityBudgetValue> budgetCosts = {};
		m_BudgetManager.GetBudgetCostsDontDiscardCampaignBudget(entitySource, budgetCosts);

		foreach (SCR_EntityBudgetValue cost : budgetCosts)
		{
			if (cost.GetBudgetType() == EEditableEntityBudget.VEHICLES)
				return cost.GetBudgetValue();
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Find BC's own supply component on the provider (or up its parent chain).
	protected GRAD_BC_VehicleSupplyComponent BC_GetSupplyComponent()
	{
		if (!m_Provider)
			return null;

		GRAD_BC_VehicleSupplyComponent supply = GRAD_BC_VehicleSupplyComponent.Cast(
			m_Provider.FindComponent(GRAD_BC_VehicleSupplyComponent));
		if (supply)
			return supply;

		IEntity parent = m_Provider.GetParent();
		while (parent)
		{
			supply = GRAD_BC_VehicleSupplyComponent.Cast(
				parent.FindComponent(GRAD_BC_VehicleSupplyComponent));
			if (supply)
				return supply;

			parent = parent.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	// BUDGET ENFORCEMENT VIA BC'S OWN SUPPLY COMPONENT.
	//
	// Vanilla's budget path cannot gate this. Measured on every placement:
	//   BUDGETCOST:  type=120 value=2 | type=5 value=200 | type=6 value=1
	//   BUDGETSTATE: PROPS=0  CAMPAIGN=0
	//   BUDGETCHECK: result=1
	// i.e. the provider's configured budgets are BOTH ZERO, so nothing can ever be exceeded, and the
	// budget carrying the real cost (VEHICLES / type 5) was not even in m_aBudgetsToEvaluate.
	// SCR_CampaignBuildingBudgetToEvaluateData exposes only Budget / UseMasterProviderBudget /
	// ShowBudgetInUI - there is NO limit field on it, so the ceiling is not authorable there; the
	// maximum is a runtime value (SCR_BudgetEditorComponent.DEFAULT_MAX_BUDGET / the 29500 seen in
	// the UI) unrelated to BC's supplies.
	//
	// So BC enforces it directly against GRAD_BC_VehicleSupplyComponent, which already holds the
	// right numbers (1000 supplies) and is already replicated. The vanilla check still runs first so
	// cooldown/rank/obstruction gating is preserved.
	override bool IsThereEnoughBudgetToSpawn(IEntityComponentSource entitySource)
	{
		if (!super.IsThereEnoughBudgetToSpawn(entitySource))
			return false;

		m_iBC_PendingCost = BC_GetVehicleCost(entitySource);

		// Not a costed vehicle (compositions etc.) - let vanilla's verdict stand.
		if (m_iBC_PendingCost <= 0)
			return true;

		GRAD_BC_VehicleSupplyComponent supply = BC_GetSupplyComponent();
		if (!supply)
		{
			Print("BC Debug - BUDGET: no GRAD_BC_VehicleSupplyComponent found, not enforcing",
				LogLevel.WARNING);
			return true;
		}

		bool affordable = supply.HasSupplies(m_iBC_PendingCost);

		Print(string.Format("BC Debug - BUDGET: cost=%1 available=%2 affordable=%3",
			m_iBC_PendingCost, supply.GetCurrentSupplies(), affordable), LogLevel.WARNING);

		return affordable;
	}

	//------------------------------------------------------------------------------------------------
	//! Charge the approved cost once the entity actually exists. Deducting here rather than in the
	//! check means a placement that fails later (obstruction, clipping) is never billed.
	//! Server-side only - this is the server creation callback.
	override protected void OnEntityCreatedServer(array<SCR_EditableEntityComponent> entities)
	{
		super.OnEntityCreatedServer(entities);

		if (m_iBC_PendingCost <= 0)
			return;

		if (!entities || entities.IsEmpty())
		{
			m_iBC_PendingCost = 0;
			return;
		}

		GRAD_BC_VehicleSupplyComponent supply = BC_GetSupplyComponent();
		if (supply)
		{
			supply.DeductSupplies(m_iBC_PendingCost);

			Print(string.Format("BC Debug - BUDGET: deducted %1, remaining=%2",
				m_iBC_PendingCost, supply.GetCurrentSupplies()), LogLevel.WARNING);
		}

		m_iBC_PendingCost = 0;
	}
}

//------------------------------------------------------------------------------------------------
// UPDATE 36 - SCOPE THE BROWSER TO THE PROVIDER'S FACTION.
//
// Why the menu lists OPFOR + FIA + CIV together, from vanilla source:
//
//   SCR_PlaceableEntitiesRegistryFromCatalog.ProcessCatalog()
//     FACTIONS_ONLY -> catalogManager.GetFilteredEditorPrefabsOfAllFactions(..., getFactionLessPrefabs: false)
//
// "OfAllFactions" is literal - the registry is all-factions BY DESIGN, and SCR_ECatalogFactionType
// offers only FACTION_AND_FACTIONLESS / FACTIONS_ONLY / FACTIONLESS_ONLY (confirmed in the
// Workbench dropdown). There is NO per-faction option. Vanilla Campaign relies on the faction-LABEL
// gate in AreLabelsMatching to reject other factions' items at placement time - the same gate that
// cannot work here, because COALITION's GetFactionLabel() is 51871 (FACTION_COA_OPFOR) and no
// vanilla prefab carries that value.
//
// So the scoping has to move to the BROWSER, and it must key off something that is NOT the broken
// label enum. SCR_EditableEntityUIInfo exposes GetFactionKey() -> a FactionKey ("USSR", "FIA",
// "CIV", "US"), which is real data present on vanilla prefabs and unrelated to EEditableEntityLabel.
//
// FilterEntries() is public on SCR_ContentBrowserEditorComponent and is what populates the visible
// list, so we let vanilla filter first and then remove anything whose faction key is not allowed
// for the current provider.
//
// Factionless prefabs (empty faction key) are KEPT - that is what the fortification compositions
// are, and they must stay placeable.
modded class SCR_ContentBrowserEditorComponent
{
	//------------------------------------------------------------------------------------------------
	// EXPLICIT PREFAB WHITELIST - replaces the faction-key filtering entirely.
	//
	// Why faction filtering was abandoned: matching SCR_EditableEntityUIInfo.GetFactionKey() against
	// the provider's faction required mapping COALITION's OPFOR/BLUFOR/INDFOR onto vanilla's
	// USSR/US/FIA, and even when the log showed FIA/US/CIV being rejected correctly on every pass
	// (removed=2..18), FIA vehicles were still visible in the menu. It also scoped to "every USSR
	// vehicle in the game" (visible=8..14) rather than to BC's curated six.
	//
	// These lists ARE the intent. No faction semantics, no key namespaces, no label enums - just
	// "these are the vehicles BC sells", per side.
	//
	// KEEP IN SYNC with Configs/Systems/Compositions_FreeRoamBuilding.conf. A prefab must be in
	// BOTH: the .conf makes it reachable, these lists make it visible for the right faction.
	//
	// Prices live on the prefabs themselves (m_EntityBudgetCost / m_Value), see the pricing table
	// in docs/vehicle-buying-investigation.md.
	protected static const ref array<string> BC_ALLOWED_PREFABS_OPFOR = {
		"Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et",                    // 50  light jeep
		"Prefabs/Vehicles/Wheeled/UAZ452/UAZ452_transport.et",          // 75  light transport
		"Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et",      // 150 heavy transport
		"Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_PKM.et",                // 300 armed jeep
		"Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et",                      // 450 armed recon
		"Prefabs/Vehicles/Wheeled/BTR70/BTR70.et"                       // 650 heavy armed APC
	};

	protected static const ref array<string> BC_ALLOWED_PREFABS_BLUFOR = {
		"Prefabs/Vehicles/Wheeled/M151A2/M151A2.et",                    // 50  light jeep
		"Prefabs/Vehicles/Wheeled/M998/M998_covered_long.et",           // 75  light transport
		"Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport_covered.et",  // 150 heavy transport
		"Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et",          // 150 heavy transport (open)
		"Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et",            // 300 armed jeep
		"Prefabs/Vehicles/Wheeled/LAV25/LAV25.et",                      // 650 heavy armed IFV
		"Prefabs/Vehicles/Helicopters/UH1H/UH1H.et"                     // 800 air
	};

	//------------------------------------------------------------------------------------------------
	//! Pick the list for the faction of the provider whose menu is open. Falls back to BOTH lists
	//! when the faction cannot be resolved, so a lookup failure never empties the menu.
	protected void BC_GetAllowedPrefabs(out array<string> allowed)
	{
		FactionKey key;

		SCR_CampaignBuildingEditorComponent buildingComp = SCR_CampaignBuildingEditorComponent.Cast(
			SCR_CampaignBuildingEditorComponent.GetInstance(SCR_CampaignBuildingEditorComponent));

		if (buildingComp)
		{
			IEntity provider = buildingComp.GetProviderEntity();
			if (provider)
			{
				FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(
					provider.FindComponent(FactionAffiliationComponent));

				// Construction trucks keep the faction component on the truck, not on the combox.
				if (!fac)
				{
					IEntity parent = provider.GetParent();
					while (parent && fac == null)
					{
						fac = FactionAffiliationComponent.Cast(
							parent.FindComponent(FactionAffiliationComponent));
						parent = parent.GetParent();
					}
				}

				if (fac)
				{
					Faction f = fac.GetAffiliatedFaction();
					if (!f)
						f = fac.GetDefaultAffiliatedFaction();

					if (f)
						key = f.GetFactionKey();
				}
			}
		}

		if (GRAD_BC_BreakingContactManager.IsOpforFactionKey(key))
		{
			foreach (string o : BC_ALLOWED_PREFABS_OPFOR)
			{
				allowed.Insert(o);
			}
			return;
		}

		if (GRAD_BC_BreakingContactManager.IsBluforFactionKey(key))
		{
			foreach (string b : BC_ALLOWED_PREFABS_BLUFOR)
			{
				allowed.Insert(b);
			}
			return;
		}

		// Unresolved faction - offer everything rather than nothing.
		foreach (string o2 : BC_ALLOWED_PREFABS_OPFOR)
		{
			allowed.Insert(o2);
		}

		foreach (string b2 : BC_ALLOWED_PREFABS_BLUFOR)
		{
			allowed.Insert(b2);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True if the prefab is one BC offers to the CURRENT provider's faction. Compared on the PATH
	//! portion so the leading {GUID} on the stored ResourceName does not have to be reproduced here.
	protected bool BC_IsAllowedPrefab(ResourceName prefab, notnull array<string> allowed)
	{
		if (prefab.IsEmpty())
			return false;

		foreach (string a : allowed)
		{
			if (prefab.IndexOf(a) != -1)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Strip everything not on the whitelist. Returns how many entries were removed.
	//! Safe to call repeatedly on an already-stripped list.
	protected int BC_StripToWhitelist()
	{
		int removed = 0;

		// Resolved once per pass, not per entry.
		array<string> allowed = {};
		BC_GetAllowedPrefabs(allowed);

		for (int i = m_aFilteredPrefabIDs.Count() - 1; i >= 0; i--)
		{
			int prefabID = m_aFilteredPrefabIDs[i];

			// GetResourceNamePrefabID() resolves through m_PlacingManagerData.GetPrefab(prefabID),
			// which is the SAME list placement indexes into - so a survivor here is guaranteed to
			// resolve to the same prefab when clicked.
			ResourceName prefab = GetResourceNamePrefabID(prefabID);

			if (BC_IsAllowedPrefab(prefab, allowed))
				continue;

			m_aFilteredPrefabIDs.Remove(i);
			removed++;
		}

		m_iFilteredPrefabIDsCount = m_aFilteredPrefabIDs.Count();
		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! FilterExtendedSlots() rebuilds m_aFilteredPrefabIDs from the extended-entity cache and sets
	//! m_iFilteredPrefabIDsCount WITHOUT going through FilterEntries(), so the strip must be
	//! re-applied here too.
	override int FilterExtendedSlots()
	{
		super.FilterExtendedSlots();

		int stripped = BC_StripToWhitelist();
		if (stripped > 0)
		{
			Print(string.Format("BC Debug - WHITELIST(extended): removed=%1 visible=%2",
				stripped, m_iFilteredPrefabIDsCount), LogLevel.WARNING);
		}

		return m_iFilteredPrefabIDsCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Post-process vanilla's filtered list, dropping entries belonging to other factions.
	//!
	//! Vanilla FilterEntries() builds m_aFilteredPrefabIDs (protected, so reachable here) as indices
	//! into m_aInfos, then applies the search on top and finally sets m_iFilteredPrefabIDsCount and
	//! invokes Event_OnBrowserEntriesFiltered. We let all of that run, then strip disallowed entries
	//! and re-sync the count. m_aLocalizationKeys is NOT touched: it is only consumed inside
	//! FilterEntries() itself for the search pass, which has already completed by then.
	override void FilterEntries()
	{
		super.FilterEntries();

		// Extended-slot mode bails out of vanilla FilterEntries() early and uses a separate list
		// built by FilterExtendedSlots(). Leave that path completely alone.
		if (GetExtendedEntity())
		{
			Print("BC Debug - FACTIONFILTER: extended-entity mode, filter skipped", LogLevel.WARNING);
			return;
		}

		int removed = BC_StripToWhitelist();

		Print(string.Format("BC Debug - WHITELIST: removed=%1 visible=%2",
			removed, m_iFilteredPrefabIDsCount), LogLevel.WARNING);

		// Vanilla FilterEntries() fires Event_OnBrowserEntriesFiltered.Invoke() at its END - i.e.
		// inside super(), BEFORE the strip above - so the UI component
		// (SCR_ContentBrowserEditorUIComponent.OnBrowserEntriesFiltered) is told "filtering done"
		// while the list still holds everything. Re-invoke so it re-reads the stripped list.
		if (removed > 0)
			Event_OnBrowserEntriesFiltered.Invoke();
	}
}
