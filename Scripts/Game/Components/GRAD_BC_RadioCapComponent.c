[ComponentEditorProps(category: "Gruppe Adler", description: "Caps a character at one short-range and one long-range radio")]
class GRAD_BC_RadioCapComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Enforces the rule "at most two radios per character: one short-range, one long-range".
//!
//! WHY THIS EXISTS AT ALL:
//! Two systems grant radios independently and neither can see the other. BC's gearscript adds one
//! short-range radio in m_DefaultInventoryItems, which applies to EVERY role - that is deliberate,
//! since every player needs the squad net. COALITION separately grants leadership roles their own
//! radios from its own (packed) role config. The two stack, so a company commander ends up holding
//! R148 + R187 + R168.
//!
//! The config layer cannot fix this: m_RolesToSetCustomSettings can only ADD items to a role, never
//! remove what COALITION already granted, and deleting BC's default radio strips riflemen of the one
//! radio they are supposed to have. Trimming after both systems have run is the only place the rule
//! can actually be applied.
//!
//! CLASSIFICATION:
//! Short-range radios are typed EQUIPMENT and do not occupy the backpack slot; long-range radios are
//! typed RADIO_BACKPACK and do. That is the real distinction between the two bands, so the prefab
//! lists below are grouped by it rather than by faction - a character is capped the same way no
//! matter which side issued the radio, which also keeps a mixed-faction pickup from slipping through.
class GRAD_BC_RadioCapComponent : ScriptComponent
{
	//! Long enough for BC's gearscript and COALITION's role config to have both finished inserting.
	//! Both are done well inside a second; 2000ms leaves headroom on a loaded server without being
	//! long enough for a player to have started rearranging their own inventory.
	protected const int TRIM_DELAY_MS = 2000;

	protected static const string TAG = "BC RadioCap";

	//! Short-range (EQUIPMENT-typed) radios: the pair BC issues to every slot, plus the FIA variant.
	protected static ref array<string> s_aShortRangeRadios = {
		"Radio_ANPRC68",
		"Radio_R148",
		"Radio_R148_FIA"
	};

	//! Long-range (RADIO_BACKPACK-typed) radios, both era-appropriate and modern, plus INDFOR variants.
	//! ANPRC117/150/152 are modern-era and wrong for the 80s factions, but they are listed so that a
	//! character carrying one is still capped rather than silently exceeding the limit.
	protected static ref array<string> s_aLongRangeRadios = {
		"Radio_R107M",
		"Radio_R168",
		"Radio_R168_INDFOR",
		"Radio_R187",
		"Radio_R187_INDFOR",
		"Radio_ANPRC117",
		"Radio_ANPRC150",
		"Radio_ANPRC152"
	};

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// Inventory is authoritative on the server; trimming on a client would race replication and
		// could delete an entity the server still considers held.
		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(TrimRadios, TRIM_DELAY_MS, false, owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the first short-range and the first long-range radio found; deletes every further radio.
	protected void TrimRadios(IEntity owner)
	{
		if (!owner)
			return;

		InventoryStorageManagerComponent storageManager = InventoryStorageManagerComponent.Cast(
			owner.FindComponent(InventoryStorageManagerComponent));

		if (!storageManager)
			return;

		// Both purposes are collected: a radio may sit in cargo or in a worn/attached slot, and
		// querying only one is how the surplus would be missed.
		array<BaseInventoryStorageComponent> storages = {};
		storageManager.GetStorages(storages, EStoragePurpose.PURPOSE_DEPOSIT);

		array<BaseInventoryStorageComponent> attachmentStorages = {};
		storageManager.GetStorages(attachmentStorages, EStoragePurpose.PURPOSE_EQUIPMENT_ATTACHMENT);

		foreach (BaseInventoryStorageComponent attachmentStorage : attachmentStorages)
		{
			if (attachmentStorage && !storages.Contains(attachmentStorage))
				storages.Insert(attachmentStorage);
		}

		bool keptShortRange = false;
		bool keptLongRange = false;
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

				string prefab = GetPrefabName(item);
				if (prefab == string.Empty)
					continue;

				bool isShortRange = MatchesAny(prefab, s_aShortRangeRadios);
				bool isLongRange = false;

				// Only test the long-range list when the short-range one did not match, so a name that
				// is a substring of another cannot be counted in both bands.
				if (!isShortRange)
					isLongRange = MatchesAny(prefab, s_aLongRangeRadios);

				if (!isShortRange && !isLongRange)
					continue;

				bool keepThisOne = false;

				if (isShortRange && !keptShortRange)
				{
					keptShortRange = true;
					keepThisOne = true;
				}
				else if (isLongRange && !keptLongRange)
				{
					keptLongRange = true;
					keepThisOne = true;
				}

				if (keepThisOne)
					continue;

				// Removal alone leaves the entity loose in the world, so it is deleted explicitly -
				// the same pattern GRAD_BC_VehicleInventoryComponent.CleanInventory uses.
				if (!storageManager.TryRemoveItemFromStorage(item, storage))
				{
					Print(string.Format("%1 - could not remove surplus radio %2", TAG, prefab), LogLevel.WARNING);
					continue;
				}

				SCR_EntityHelper.DeleteEntityAndChildren(item);
				removed++;
			}
		}

		if (removed > 0 && GRAD_BC_BreakingContactManager.IsDebugMode())
		{
			Print(string.Format("%1 - removed %2 surplus radio(s) from %3 (short=%4 long=%5)",
				TAG, removed, owner, keptShortRange, keptLongRange), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Substring match on the prefab path. The names are distinctive enough that this is safe, and it
	//! avoids hardcoding GUIDs that would silently stop matching if a prefab were ever re-created.
	protected bool MatchesAny(string prefabName, notnull array<string> names)
	{
		foreach (string name : names)
		{
			if (prefabName.Contains(name))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected string GetPrefabName(IEntity item)
	{
		EntityPrefabData prefabData = item.GetPrefabData();
		if (!prefabData)
			return string.Empty;

		return prefabData.GetPrefabName();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(TrimRadios);

		super.OnDelete(owner);
	}
}
