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
		// THE VEHICLE CHANNEL - never measured until now. On `main` vehicles came from the FACTION's
		// VEHICLE entity catalog (inherited from vanilla USSR.conf), NOT from m_sPrefabsToBuildResource.
		// COALITION's factions do not inherit vanilla's configs, so their catalog started empty; the
		// user has since hand-added 6 vehicles with supply costs + CAPTAIN rank gates.
		// Read that catalog through the provider's own faction and report what the game actually sees.
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
					SCR_EntityCatalog vehCatalog = scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, false);
					if (!vehCatalog)
					{
						Print(string.Format("BC Debug - VEHICLE CATALOG: faction '%1' has NO VEHICLE catalog",
							scrFaction.GetFactionKey()), LogLevel.WARNING);
					}
					else
					{
						array<SCR_EntityCatalogEntry> entries = {};
						vehCatalog.GetEntityList(entries);
						Print(string.Format("BC Debug - VEHICLE CATALOG: faction '%1' has %2 enabled entries",
							scrFaction.GetFactionKey(), entries.Count()), LogLevel.WARNING);

						foreach (SCR_EntityCatalogEntry e : entries)
							Print(string.Format("BC Debug -   catalog entry: %1", e.GetPrefab()), LogLevel.WARNING);
					}
				}
			}
		}
	}

	protected override void EOnEditorDeactivate()
	{
		Print("BC Debug - menu is closing", LogLevel.WARNING);
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
