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
			if (buildingManager)
			{
				array<ResourceName> managerList = buildingManager.GetPlaceablePrefabs();
				if (managerList)
				{
					Print(string.Format("BC Debug - FEED: manager list has %1 entries", managerList.Count()),
						LogLevel.WARNING);

					// Print every entry with its index. The failing placements report an id which is an
					// index - this is what lets us check whether that id indexes into THIS list.
					for (int mi = 0; mi < managerList.Count(); mi++)
					{
						Print(string.Format("BC Debug - FEED manager[%1] = %2", mi, managerList[mi]),
							LogLevel.WARNING);
					}
				}
			}

			// The browser's own info list - this is what backs the rendered tiles.
			for (int bi = 0; bi < total; bi++)
			{
				SCR_UIInfo info = browser.GetInfo(bi);
				if (!info)
					continue;

				Print(string.Format("BC Debug - FEED browser[%1] = '%2'", bi, info.GetName()),
					LogLevel.WARNING);
			}
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
	//! Faction keys the current provider may place. Empty => no filtering (fail open, never hide
	//! everything just because the provider could not be resolved).
	protected void BC_GetAllowedFactionKeys(out array<FactionKey> allowedKeys)
	{
		SCR_CampaignBuildingEditorComponent buildingComp = SCR_CampaignBuildingEditorComponent.Cast(
			SCR_CampaignBuildingEditorComponent.GetInstance(SCR_CampaignBuildingEditorComponent));
		if (!buildingComp)
			return;

		IEntity provider = buildingComp.GetProviderEntity();
		if (!provider)
			return;

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(
			provider.FindComponent(FactionAffiliationComponent));
		if (!fac)
		{
			// Construction trucks keep the faction component on the truck, not on the back.
			IEntity parent = provider.GetParent();
			while (parent && fac == null)
			{
				fac = FactionAffiliationComponent.Cast(parent.FindComponent(FactionAffiliationComponent));
				parent = parent.GetParent();
			}
		}

		if (!fac)
			return;

		Faction providerFaction = fac.GetAffiliatedFaction();
		if (!providerFaction)
			providerFaction = fac.GetDefaultAffiliatedFaction();

		if (!providerFaction)
			return;

		FactionKey providerKey = providerFaction.GetFactionKey();
		if (providerKey.IsEmpty())
			return;

		allowedKeys.Insert(providerKey);

		// KEY NAMESPACE MISMATCH.
		//
		// COALITION's factions use the keys OPFOR / BLUFOR / INDFOR / CIV, but the placeable
		// prefabs are VANILLA assets whose SCR_EditableEntityUIInfo.GetFactionKey() returns the
		// VANILLA keys USSR / US / FIA / CIV. Matching COALITION's key against a vanilla prefab
		// therefore never succeeds, and the first run of this filter stripped almost everything
		// (measured: allowed='OPFOR' -> visible=0 on most tabs, 3-5 on the rest).
		//
		// So the provider's key is expanded to the vanilla key(s) that mean the same side. Both are
		// kept in the allow-list, so BC-owned prefabs tagged either way still match.
		if (providerKey == "OPFOR")
		{
			allowedKeys.Insert("USSR");
		}
		else if (providerKey == "BLUFOR")
		{
			allowedKeys.Insert("US");
		}
		else if (providerKey == "INDFOR")
		{
			allowedKeys.Insert("FIA");
		}
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
			return;

		array<FactionKey> allowedKeys = {};
		BC_GetAllowedFactionKeys(allowedKeys);

		// Fail open: if the provider faction cannot be resolved, keep vanilla's result rather than
		// risk emptying the menu.
		if (allowedKeys.IsEmpty())
		{
			Print("BC Debug - FACTIONFILTER: provider faction unresolved, filter skipped",
				LogLevel.WARNING);
			return;
		}

		int removed = 0;
		int keptFactionless = 0;

		// Which keys actually got rejected - so a namespace mismatch is visible in one line instead
		// of being inferred from a low `visible` count.
		string rejectedKeys;

		// Walk backwards so removal does not shift indices we have yet to visit.
		for (int i = m_aFilteredPrefabIDs.Count() - 1; i >= 0; i--)
		{
			SCR_EditableEntityUIInfo info = GetInfo(m_aFilteredPrefabIDs[i]);
			if (!info)
				continue;

			FactionKey entryKey = info.GetFactionKey();

			// STRICT FACTION-ONLY (user decision 2026-08-14): anything without a faction key is
			// stripped, no exceptions.
			//
			// Rationale: a US LAV and FIA vehicles kept appearing under an OPFOR provider on passes
			// that logged removed=0 rejectedKeys=[] - they carry NO faction key, so any "factionless
			// is allowed" rule lets them through. Trying to distinguish factionless-vehicle from
			// factionless-composition needs GetEntityTypex() to be reliable for every prefab, which
			// is not established. Stripping all factionless is unambiguous.
			//
			// CONSEQUENCE: the ~91 fortification compositions (sandbags, camo nets, barbed wire,
			// hedgehogs) in Compositions_FreeRoamBuilding.conf are factionless and WILL disappear
			// from the menu, leaving faction-tagged vehicles only. If those need to come back, the
			// non-vehicle exemption is the thing to restore here.
			if (entryKey.IsEmpty())
			{
				keptFactionless++;
				m_aFilteredPrefabIDs.Remove(i);
				removed++;
				continue;
			}

			if (allowedKeys.Contains(entryKey))
				continue;

			if (!rejectedKeys.Contains(entryKey))
				rejectedKeys = rejectedKeys + entryKey + " ";

			m_aFilteredPrefabIDs.Remove(i);
			removed++;
		}

		// Re-sync the count vanilla set from the pre-strip list.
		m_iFilteredPrefabIDsCount = m_aFilteredPrefabIDs.Count();

		string allowedStr;
		foreach (FactionKey k : allowedKeys)
		{
			allowedStr = allowedStr + k + " ";
		}

		Print(string.Format("BC Debug - FACTIONFILTER: allowed=[%1] rejectedKeys=[%2] removed=%3 factionless=%4 visible=%5",
			allowedStr, rejectedKeys, removed, keptFactionless, m_iFilteredPrefabIDsCount),
			LogLevel.WARNING);
	}
}
