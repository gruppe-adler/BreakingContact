[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "Manager for civilian death events and UI triggers.")]
class GRAD_BC_TrafficHintManagerComponentClass : ScriptComponentClass {}

class GRAD_BC_TrafficHintManagerComponent : ScriptComponent
{
	protected RplComponent m_RplComponent;
	protected bool m_bSubscribed = false;

	// Cooldown tracking: recent event positions and their timestamps (server only)
	protected ref array<vector> m_aCooldownPositions;
	protected ref array<float> m_aCooldownTimes;

	static const float COOLDOWN_DISTANCE = 500.0;    // meters
	static const float MARKER_LIFETIME = 180.0;      // seconds
	static const float COOLDOWN_LIFETIME = 180.0;    // same as marker lifetime

	// Icon textures for map markers (.edds files loaded via LoadImageTexture)
	[Attribute("{AC1B5B57453D9134}UI/Textures/Icons/CivKilled.edds", UIWidgets.ResourcePickerThumbnail, desc: "Icon texture for civilian killed events", params: "edds")]
	protected ResourceName m_sIconKilled;

	[Attribute("{B790D8AC6330F4E4}UI/Textures/Icons/CivGunfight.edds", UIWidgets.ResourcePickerThumbnail, desc: "Icon texture for gunfight events", params: "edds")]
	protected ResourceName m_sIconGunfight;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.FRAME);

		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (!m_RplComponent)
			Print("GRAD_BC_TrafficHintManagerComponent: Warning - No RplComponent found", LogLevel.WARNING);

		m_aCooldownPositions = new array<vector>();
		m_aCooldownTimes = new array<float>();
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		PlayerController pc = GetGame().GetPlayerController();

		// On server (dedicated or listen), subscribe to traffic events immediately
		if (Replication.IsServer() && !m_bSubscribed)
		{
			SCR_TrafficEvents.OnCivilianEvent.Insert(OnCivilianEvent);
			m_bSubscribed = true;
			Print("GRAD_BC_TrafficHintManagerComponent: SERVER - Subscribed to traffic events", LogLevel.NORMAL);
			ClearEventMask(owner, EntityEvent.FRAME);
			return;
		}

		// On client, wait for local PlayerController to be available
		if (!Replication.IsServer() && pc && pc == owner && !m_bSubscribed)
		{
			m_bSubscribed = true;
			Print("GRAD_BC_TrafficHintManagerComponent: CLIENT - PlayerController ready", LogLevel.NORMAL);
			ClearEventMask(owner, EntityEvent.FRAME);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bSubscribed && Replication.IsServer())
		{
			SCR_TrafficEvents.OnCivilianEvent.Remove(OnCivilianEvent);
			Print("GRAD_BC_TrafficHintManagerComponent: Unsubscribed from traffic events", LogLevel.NORMAL);
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsOnCooldown(vector location)
	{
		float currentTime = System.GetTickCount() / 1000.0;

		// Clean up expired cooldown entries (iterate backwards for safe removal)
		for (int i = m_aCooldownTimes.Count() - 1; i >= 0; i--)
		{
			if (currentTime - m_aCooldownTimes[i] > COOLDOWN_LIFETIME)
			{
				m_aCooldownPositions.Remove(i);
				m_aCooldownTimes.Remove(i);
			}
		}

		// Check if any active cooldown position is within 500m
		foreach (vector pos : m_aCooldownPositions)
		{
			float dist = vector.Distance(location, pos);
			if (dist < COOLDOWN_DISTANCE)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void RegisterCooldown(vector location)
	{
		float currentTime = System.GetTickCount() / 1000.0;
		m_aCooldownPositions.Insert(location);
		m_aCooldownTimes.Insert(currentTime);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCivilianEvent(vector location, string eventtype)
	{
		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Civilian event detected at %1, type: %2", location, eventtype), LogLevel.NORMAL);

		// Only handle this on server
		if (!Replication.IsServer())
			return;

		// Check 500m proximity cooldown
		if (IsOnCooldown(location))
		{
			Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Event at %1 is on cooldown, skipping", location), LogLevel.NORMAL);
			return;
		}

		RegisterCooldown(location);

		// Broadcast event to all clients via RPC
		if (m_RplComponent)
		{
			Rpc(RpcAsk_BroadcastTrafficEvent, location, eventtype);
			Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Broadcasted RPC for event type '%1'", eventtype), LogLevel.NORMAL);
		}

		// Also trigger locally if server has a player controller (listen server)
		PlayerController pc = GetGame().GetPlayerController();
		if (pc)
			ShowTrafficEventUI(location, eventtype);
	}

	//------------------------------------------------------------------------------------------------
	// Format current time as HH:MM:SS for marker timestamp
	protected string FormatTimestamp()
	{
		int totalSeconds = System.GetTickCount() / 1000;
		int hours = (totalSeconds / 3600) % 24;
		int minutes = (totalSeconds / 60) % 60;
		int secs = totalSeconds % 60;

		string hh = hours.ToString();
		string mm = minutes.ToString();
		string ss = secs.ToString();

		if (hours < 10)
			hh = "0" + hh;
		if (minutes < 10)
			mm = "0" + mm;
		if (secs < 10)
			ss = "0" + ss;

		return string.Format("%1:%2:%3", hh, mm, ss);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcAsk_BroadcastTrafficEvent(vector location, string eventtype)
	{
		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: RPC received - event type '%1' at %2", eventtype, location), LogLevel.NORMAL);

		// Only handle on clients (server already handled it locally if needed)
		if (Replication.IsServer())
			return;

		ShowTrafficEventUI(location, eventtype);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowTrafficEventUI(vector location, string eventtype)
	{
		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: ShowTrafficEventUI called for type '%1'", eventtype), LogLevel.NORMAL);

		// Add custom icon to the map via GRAD_IconMarkerUI
		CreateTrafficMapIcon(location, eventtype);

		// Find the UI display via the HUD Manager
		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.Cast(GetGame().GetPlayerController().FindComponent(SCR_HUDManagerComponent));
		if (!hudManager)
		{
			Print("GRAD_BC_TrafficHintManagerComponent: No HUD manager found", LogLevel.WARNING);
			return;
		}

		e_currentTrafficDisplay displayType = e_currentTrafficDisplay.NONE;

		switch (eventtype)
		{
			case "killed":
				displayType = e_currentTrafficDisplay.KILLED;
				break;
			case "gunfight":
				displayType = e_currentTrafficDisplay.GUNFIGHT;
				break;
			default:
				displayType = e_currentTrafficDisplay.NONE;
				Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Unknown event type: %1", eventtype), LogLevel.ERROR);
				break;
		}

		GRAD_BC_Traffic display = GRAD_BC_Traffic.Cast(hudManager.FindInfoDisplay(GRAD_BC_Traffic));
		if (display)
		{
			GetGame().GetCallqueue().CallLater(
				display.showTrafficHint,
				5000,
				false,
				displayType,
				location
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateTrafficMapIcon(vector location, string eventtype)
	{
		GRAD_PlayerComponent playerComp = GRAD_PlayerComponent.GetInstance();
		if (!playerComp)
		{
			Print("GRAD_BC_TrafficHintManagerComponent: No GRAD_PlayerComponent found, cannot create map icon", LogLevel.WARNING);
			return;
		}

		GRAD_IconMarkerUI iconMarkerUI = playerComp.GetIconMarkerUI();
		if (!iconMarkerUI)
		{
			Print("GRAD_BC_TrafficHintManagerComponent: No GRAD_IconMarkerUI found, cannot create map icon", LogLevel.WARNING);
			return;
		}

		ResourceName iconTexture;
		string eventLabel;
		if (eventtype == "killed")
		{
			iconTexture = m_sIconKilled;
			eventLabel = "CIV KILLED";
		}
		else
		{
			iconTexture = m_sIconGunfight;
			eventLabel = "GUNFIGHT";
		}

		if (iconTexture.IsEmpty())
		{
			Print(string.Format("GRAD_BC_TrafficHintManagerComponent: No icon texture configured for event type '%1'", eventtype), LogLevel.WARNING);
			return;
		}

		string timestamp = FormatTimestamp();
		string label = string.Format("%1 [%2]", eventLabel, timestamp);

		// AddIcon uses world X/Z coordinates; pass same point for start and end (static icon, no rotation)
		int iconId = iconMarkerUI.AddIcon(location[0], location[2], location[0], location[2], iconTexture, -1, false, label);

		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Created map icon id=%1 for '%2' at %3", iconId, eventtype, location), LogLevel.NORMAL);

		// Schedule removal after MARKER_LIFETIME
		GetGame().GetCallqueue().CallLater(
			RemoveTrafficMapIcon,
			MARKER_LIFETIME * 1000,
			false,
			iconId
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveTrafficMapIcon(int iconId)
	{
		GRAD_PlayerComponent playerComp = GRAD_PlayerComponent.GetInstance();
		if (!playerComp)
			return;

		GRAD_IconMarkerUI iconMarkerUI = playerComp.GetIconMarkerUI();
		if (!iconMarkerUI)
			return;

		iconMarkerUI.RemoveIcon(iconId);
		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Removed expired traffic icon id=%1", iconId), LogLevel.NORMAL);
	}
}
