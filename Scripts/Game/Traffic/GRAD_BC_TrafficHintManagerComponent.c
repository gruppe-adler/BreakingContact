[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "Manager for civilian death events and UI triggers.")]
class GRAD_BC_TrafficHintManagerComponentClass : ScriptComponentClass {}

class GRAD_BC_TrafficHintManagerComponent : ScriptComponent
{
	protected RplComponent m_RplComponent;
	protected bool m_bSubscribed = false;
	
	// Cooldown tracking: recent event positions and their timestamps (server only)
	protected ref array<vector> m_aCooldownPositions;
	protected ref array<float> m_aCooldownTimes;
	
	// Active vanilla markers for scheduled removal (server only)
	protected ref array<ref SCR_MapMarkerBase> m_aActiveMarkers;
	
	static const float COOLDOWN_DISTANCE = 500.0;    // meters
	static const float MARKER_LIFETIME = 180.0;      // seconds
	static const float COOLDOWN_LIFETIME = 180.0;    // same as marker lifetime
	
	// Placeholder icon entries - TODO: replace with actual resource IDs when known
	static const int ICON_KILLED = 8;       // placeholder icon index for killed events
	static const int ICON_GUNFIGHT = 9;     // placeholder icon index for gunfight events
	static const int COLOR_KILLED = 4;      // placeholder color index for killed (red-ish)
	static const int COLOR_GUNFIGHT = 5;    // placeholder color index for gunfight (yellow-ish)
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		// Enable frame updates so we can check every frame
   		SetEventMask(owner, EntityEvent.FRAME);
		
		// Get RPC component for network communication
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (!m_RplComponent)
			Print("GRAD_BC_TrafficHintManagerComponent: Warning - No RplComponent found", LogLevel.WARNING);
		else
			Print("GRAD_BC_TrafficHintManagerComponent: RplComponent found successfully", LogLevel.NORMAL);
		
		m_aCooldownPositions = new array<vector>();
		m_aCooldownTimes = new array<float>();
		m_aActiveMarkers = new array<ref SCR_MapMarkerBase>();
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
		// Cleanup subscription only if we subscribed
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
		{
			Print("GRAD_BC_TrafficHintManagerComponent: Not server, ignoring event", LogLevel.VERBOSE);
			return;
		}
		
		// Check 500m proximity cooldown
		if (IsOnCooldown(location))
		{
			Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Event at %1 is on cooldown (within %2m of recent event), skipping", location, COOLDOWN_DISTANCE), LogLevel.NORMAL);
			return;
		}
		
		// Register cooldown for this location
		RegisterCooldown(location);
		
		Print("GRAD_BC_TrafficHintManagerComponent: Server handling event, broadcasting to all clients", LogLevel.NORMAL);
		
		// Create vanilla map marker on server (visible to all players via replication)
		CreateVanillaMapMarker(location, eventtype);
		
		// Broadcast event to all clients via RPC (for HUD notification)
		if (m_RplComponent)
		{
			Rpc(RpcAsk_BroadcastTrafficEvent, location, eventtype);
			Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Broadcasted RPC for event type '%1'", eventtype), LogLevel.NORMAL);
		}
		else
		{
			Print("GRAD_BC_TrafficHintManagerComponent: No RplComponent, cannot broadcast", LogLevel.ERROR);
		}
		
		// Also trigger locally if server has a player controller
		PlayerController pc = GetGame().GetPlayerController();
		if (pc)
		{
			Print("GRAD_BC_TrafficHintManagerComponent: Server has local player, showing event locally", LogLevel.NORMAL);
			ShowTrafficEventUI(location, eventtype);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CreateVanillaMapMarker(vector location, string eventtype)
	{
		SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_MapMarkerManagerComponent));
		if (!markerManager)
		{
			Print("GRAD_BC_TrafficHintManagerComponent: No SCR_MapMarkerManagerComponent found", LogLevel.ERROR);
			return;
		}
		
		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		
		// SetWorldPos expects [X, Z] - location[0] is X, location[2] is Z in world coordinates
		int worldPos[2];
		worldPos[0] = location[0];
		worldPos[1] = location[2];
		marker.SetWorldPos(worldPos[0], worldPos[1]);
		
		string markerText;
		int iconEntry;
		int colorEntry;
		
		if (eventtype == "killed")
		{
			markerText = "CIV KILLED";
			iconEntry = ICON_KILLED;
			colorEntry = COLOR_KILLED;
		}
		else
		{
			markerText = "GUNFIGHT";
			iconEntry = ICON_GUNFIGHT;
			colorEntry = COLOR_GUNFIGHT;
		}
		
		marker.SetCustomText(markerText);
		marker.SetIconEntry(iconEntry);
		marker.SetColorEntry(colorEntry);
		
		markerManager.InsertStaticMarker(marker, true, true);
		m_aActiveMarkers.Insert(marker);
		
		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Created vanilla marker '%1' at %2", markerText, location), LogLevel.NORMAL);
		
		// Schedule marker removal after MARKER_LIFETIME seconds
		GetGame().GetCallqueue().CallLater(
			RemoveMarker,
			MARKER_LIFETIME * 1000,
			false,
			marker
		);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void RemoveMarker(SCR_MapMarkerBase marker)
	{
		if (!marker)
			return;
		
		SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_MapMarkerManagerComponent));
		if (markerManager)
		{
			markerManager.RemoveStaticMarker(marker);
			Print("GRAD_BC_TrafficHintManagerComponent: Removed expired traffic marker", LogLevel.NORMAL);
		}
		
		m_aActiveMarkers.RemoveItem(marker);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcAsk_BroadcastTrafficEvent(vector location, string eventtype)
	{
		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: RPC received - event type '%1' at %2", eventtype, location), LogLevel.NORMAL);
		
		// Only handle on clients (server already handled it locally if needed)
		if (Replication.IsServer())
		{
			Print("GRAD_BC_TrafficHintManagerComponent: RPC on server, skipping", LogLevel.VERBOSE);
			return;
		}
		
		Print("GRAD_BC_TrafficHintManagerComponent: Client handling RPC event", LogLevel.NORMAL);
		ShowTrafficEventUI(location, eventtype);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void ShowTrafficEventUI(vector location, string eventtype)
	{
		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: ShowTrafficEventUI called for type '%1'", eventtype), LogLevel.NORMAL);
		
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
			Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Found traffic display, scheduling hint for type %1", displayType), LogLevel.NORMAL);
			// Schedule hint to appear after 5-second delay (HUD only, no custom map marker)
			GetGame().GetCallqueue().CallLater(
				display.showTrafficHint,
				5000,  // 5 seconds delay
				false,
				displayType,
				location
			);
		}
		else
		{
			Print("GRAD_BC_TrafficHintManagerComponent: No traffic display found", LogLevel.ERROR);
		}
	}
}