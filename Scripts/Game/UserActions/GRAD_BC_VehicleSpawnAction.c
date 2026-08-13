modded class SCR_CampaignBuildingStartUserAction
{
	// No overrides needed - building menu access should not be blocked by vehicle supplies
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
