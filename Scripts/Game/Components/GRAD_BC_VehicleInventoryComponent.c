//------------------------------------------------------------------------------------------------
//! Cleans a freshly spawned vehicle's cargo and restocks it with a small, faction-appropriate
//! ammo reserve so crews cannot run dry mid-match.
//!
//! Vanilla vehicle prefabs ship with assorted random cargo (loot, supplies, occasionally weapons).
//! BC wants deterministic contents, so every vehicle is stripped first and then given exactly the
//! magazines listed below for its faction.
//!
//! This is a static utility rather than a ScriptComponent: BC's buyable vehicles are all VANILLA
//! prefabs, so there is no BC-owned prefab to attach a component to. Call ApplyToVehicle() from
//! wherever a faction vehicle is spawned - currently
//! SCR_CampaignBuildingPlacingEditorComponent.OnEntityCreatedServer in
//! Scripts/Game/UserActions/GRAD_BC_VehicleSpawnAction.c.
//!
//! NOTE: ambient/civilian traffic (GRAD_BC_AmbientVehicleManager) is deliberately NOT processed -
//! civilian cars must not carry military ammunition.
//!
//! Server-authoritative: inventory changes are replicated from the server, so every call site must
//! already be on the server. ApplyToVehicle() enforces this itself.
class GRAD_BC_VehicleInventory
{
	//------------------------------------------------------------------------------------------------
	// Magazines match what each faction's gearscript actually issues, so resupply fits the weapons
	// players carry. Keep in sync with Configs/Gearscripts/GRAD_BC_GS_{US80s,USSR80s}.conf.
	//
	// USSR: AK74 (5.45x39), RPK (5.45x39 45rnd), PK (7.62x54 100rnd box)
	protected static const ref array<string> AMMO_OPFOR = {
		"{0A84AA5A3884176F}Prefabs/Weapons/Magazines/Magazine_545x39_AK_30rnd_Last_5Tracer.et",
		"{D78C667F59829717}Prefabs/Weapons/Magazines/Magazine_545x39_RPK_45rnd_4Ball_1Tracer.et",
		"{E5E9C5897CF47F44}Prefabs/Weapons/Magazines/Box_762x54_PK_100rnd_4Ball_1Tracer.et"
	};

	// US: M16/STANAG (5.56x45), M249 (5.56x45 200rnd box), M60 (7.62x51 100rnd box)
	protected static const ref array<string> AMMO_BLUFOR = {
		"{D8F2CA92583B23D3}Prefabs/Weapons/Magazines/Magazine_556x45_STANAG_30rnd_M855_M856_Last_5Tracer.et",
		"{4FCBBDF274FD2157}Prefabs/Weapons/Magazines/Box_556x45_M249_200rnd_Ball.et",
		"{AAF51CFA75A9CF8B}Prefabs/Weapons/Magazines/Box_762x51_M60_100rnd_4AP_1Tracer.et"
	};

	// How many of each magazine above to insert. Rifle mags are the common case, so they get the
	// larger share; belt boxes are bulky and only a couple fit alongside them.
	protected static const int COUNT_RIFLE = 12;
	protected static const int COUNT_SUPPORT = 4;
	protected static const int COUNT_MG = 3;

	//------------------------------------------------------------------------------------------------
	//! Strip the vehicle's cargo and insert the faction ammo reserve.
	//! \param vehicle the freshly spawned vehicle
	//! \param factionKey owning faction ("USSR"/"OPFOR" or "US"/"BLUFOR"); when empty it is read
	//!        off the vehicle's own FactionAffiliationComponent
	static void ApplyToVehicle(IEntity vehicle, string factionKey = "")
	{
		if (!vehicle)
			return;

		// Inventory edits must originate on the server or they will not replicate.
		if (!Replication.IsServer())
			return;

		if (factionKey.IsEmpty())
			factionKey = ResolveFactionKey(vehicle);

		array<string> ammo = {};
		if (!GetAmmoForFaction(factionKey, ammo))
		{
			// Civilian or unresolved faction - clean nothing, add nothing. Better to leave the
			// vehicle untouched than to strip a car BC does not own.
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_VehicleInventory: skipping '%1' - unhandled faction '%2'",
					GetPrefabName(vehicle), factionKey), LogLevel.NORMAL);
			return;
		}

		InventoryStorageManagerComponent storageManager = InventoryStorageManagerComponent.Cast(
			vehicle.FindComponent(InventoryStorageManagerComponent));
		if (!storageManager)
		{
			// Not every vehicle carries a cargo inventory (some light jeeps have none).
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_VehicleInventory: '%1' has no inventory manager, nothing to do",
					GetPrefabName(vehicle)), LogLevel.NORMAL);
			return;
		}

		int removed = CleanInventory(storageManager);
		int inserted = InsertAmmo(storageManager, ammo);

		Print(string.Format("GRAD_BC_VehicleInventory: %1 [%2] cleaned=%3 insertedTypes=%4",
			GetPrefabName(vehicle), factionKey, removed, inserted), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Prefab path for logging. Entities spawned at runtime always have prefab data, but scene-placed
	//! ones may not, so this never dereferences a null.
	protected static string GetPrefabName(notnull IEntity entity)
	{
		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData)
			return "<no prefab>";

		return prefabData.GetPrefabName();
	}

	//------------------------------------------------------------------------------------------------
	//! Read the faction key off the vehicle, walking up the parent chain - on some vehicles the
	//! faction component sits on the root while the spawned entity is a child part.
	protected static string ResolveFactionKey(IEntity vehicle)
	{
		IEntity current = vehicle;
		while (current)
		{
			FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(
				current.FindComponent(FactionAffiliationComponent));
			if (fac)
			{
				Faction f = fac.GetAffiliatedFaction();
				if (!f)
					f = fac.GetDefaultAffiliatedFaction();

				if (f)
					return f.GetFactionKey();
			}

			current = current.GetParent();
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Pick the magazine list for a faction. Returns false for civilian/unknown factions, which are
	//! left completely alone.
	protected static bool GetAmmoForFaction(string factionKey, out array<string> ammo)
	{
		if (GRAD_BC_BreakingContactManager.IsOpforFactionKey(factionKey))
		{
			foreach (string o : AMMO_OPFOR)
			{
				ammo.Insert(o);
			}
			return true;
		}

		if (GRAD_BC_BreakingContactManager.IsBluforFactionKey(factionKey))
		{
			foreach (string b : AMMO_BLUFOR)
			{
				ammo.Insert(b);
			}
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Remove every item from the vehicle's cargo storages and delete it.
	//!
	//! TryRemoveItemFromStorage only detaches the item - it does NOT destroy the entity, which would
	//! leave the removed cargo loose in the world. Each item is therefore deleted explicitly after
	//! removal.
	//!
	//! Only PURPOSE_DEPOSIT storages are touched: that is the cargo compartment. Attachment slots
	//! (wheels, doors, mounted weapons) live in other storages and must be left intact.
	//! \return how many items were removed
	protected static int CleanInventory(notnull InventoryStorageManagerComponent storageManager)
	{
		array<BaseInventoryStorageComponent> storages = {};
		storageManager.GetStorages(storages, EStoragePurpose.PURPOSE_DEPOSIT);

		int removed = 0;

		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!storage)
				continue;

			array<IEntity> items = {};
			storage.GetAll(items, false);

			foreach (IEntity item : items)
			{
				if (!item)
					continue;

				if (!storageManager.TryRemoveItemFromStorage(item, storage))
					continue;

				SCR_EntityHelper.DeleteEntityAndChildren(item);
				removed++;
			}
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! Insert the configured ammo reserve.
	//!
	//! TrySpawnPrefabToStorage takes a count, so each magazine type is one call. A false return means
	//! the vehicle could not take that batch (typically full); we move on to the next type rather
	//! than aborting, since a partially stocked vehicle is still useful.
	//! \return how many magazine types were successfully inserted
	protected static int InsertAmmo(notnull InventoryStorageManagerComponent storageManager, notnull array<string> ammo)
	{
		int inserted = 0;

		for (int i = 0; i < ammo.Count(); i++)
		{
			int count = CountForIndex(i);

			if (storageManager.TrySpawnPrefabToStorage(ammo[i], null, -1, EStoragePurpose.PURPOSE_DEPOSIT, null, count))
			{
				inserted++;
			}
			else if (GRAD_BC_BreakingContactManager.IsDebugMode())
			{
				Print(string.Format("GRAD_BC_VehicleInventory: could not insert %1x %2",
					count, ammo[i]), LogLevel.NORMAL);
			}
		}

		return inserted;
	}

	//------------------------------------------------------------------------------------------------
	//! How many of the magazine at this index to insert. Index order matches the AMMO_* arrays:
	//! 0 = rifle, 1 = support/automatic rifle, 2 = machinegun belt.
	protected static int CountForIndex(int index)
	{
		if (index == 0)
			return COUNT_RIFLE;

		if (index == 1)
			return COUNT_SUPPORT;

		return COUNT_MG;
	}
}
