[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "Manager for civilian death events and UI triggers.")]
class GRAD_BC_TrafficHintManagerComponentClass : ScriptComponentClass {}

// Tracks a single traffic marker with its metadata for fade-out
class GRAD_BC_TrafficMarkerData
{
	SCR_MapMarkerBase m_Marker;
	float m_fCreationTime;         // System.GetTickCount() / 1000.0 at creation
	string m_sEventType;           // "killed" or "gunfight"
	string m_sTimestamp;           // formatted time string for display
	vector m_vLocation;            // world position
	int m_iColorEntry;             // original color entry
	int m_iIconEntry;              // original icon entry
}

class GRAD_BC_TrafficHintManagerComponent : ScriptComponent
{
	protected RplComponent m_RplComponent;
	protected bool m_bSubscribed = false;
	
	// Cooldown tracking: recent event positions and their timestamps (server only)
	protected ref array<vector> m_aCooldownPositions;
	protected ref array<float> m_aCooldownTimes;
	
	// Active vanilla markers with metadata for fade-out (server only)
	protected ref array<ref GRAD_BC_TrafficMarkerData> m_aActiveMarkerData;
	
	static const float COOLDOWN_DISTANCE = 500.0;    // meters
	static const float MARKER_LIFETIME = 180.0;      // seconds
	static const float COOLDOWN_LIFETIME = 180.0;    // same as marker lifetime
	static const float FADE_START = 10.0;            // seconds before fade begins
	static const float FADE_UPDATE_INTERVAL = 15.0;  // base seconds between fade updates (decreases as marker ages)
	
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
		m_aActiveMarkerData = new array<ref GRAD_BC_TrafficMarkerData>();
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
		
		float creationTime = System.GetTickCount() / 1000.0;
		string timestamp = FormatTimestamp();
		
		string eventLabel;
		int iconEntry;
		int colorEntry;
		
		if (eventtype == "killed")
		{
			eventLabel = "CIV KILLED";
			iconEntry = ICON_KILLED;
			colorEntry = COLOR_KILLED;
		}
		else
		{
			eventLabel = "GUNFIGHT";
			iconEntry = ICON_GUNFIGHT;
			colorEntry = COLOR_GUNFIGHT;
		}
		
		// Build marker text with timestamp
		string markerText = string.Format("%1 [%2]", eventLabel, timestamp);
		
		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		
		// SetWorldPos expects [X, Z] - location[0] is X, location[2] is Z in world coordinates
		int worldPos[2];
		worldPos[0] = location[0];
		worldPos[1] = location[2];
		marker.SetWorldPos(worldPos[0], worldPos[1]);
		marker.SetCustomText(markerText);
		marker.SetIconEntry(iconEntry);
		marker.SetColorEntry(colorEntry);
		
		markerManager.InsertStaticMarker(marker, true, true);
		
		// Track marker data for fade-out
		GRAD_BC_TrafficMarkerData data = new GRAD_BC_TrafficMarkerData();
		data.m_Marker = marker;
		data.m_fCreationTime = creationTime;
		data.m_sEventType = eventtype;
		data.m_sTimestamp = timestamp;
		data.m_vLocation = location;
		data.m_iColorEntry = colorEntry;
		data.m_iIconEntry = iconEntry;
		m_aActiveMarkerData.Insert(data);
		
		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Created vanilla marker '%1' at %2", markerText, location), LogLevel.NORMAL);
		
		// Start periodic fade update after FADE_START seconds
		GetGame().GetCallqueue().CallLater(
			UpdateMarkerFade,
			FADE_START * 1000,
			false,
			data
		);
	}
	
	//------------------------------------------------------------------------------------------------
	// Calculates fade-out opacity using a power curve: starts slow, gets progressively faster
	// Returns 1.0 at FADE_START, decreases to 0.0 at MARKER_LIFETIME
	protected float CalculateFadeOpacity(float elapsedTime)
	{
		if (elapsedTime <= FADE_START)
			return 1.0;
		
		if (elapsedTime >= MARKER_LIFETIME)
			return 0.0;
		
		// Normalized progress from 0.0 (at FADE_START) to 1.0 (at MARKER_LIFETIME)
		float fadeProgress = (elapsedTime - FADE_START) / (MARKER_LIFETIME - FADE_START);
		
		// Power curve (exponent > 1): starts slow, accelerates toward the end
		float opacity = 1.0 - Math.Pow(fadeProgress, 2.5);
		
		return Math.Clamp(opacity, 0.0, 1.0);
	}
	
	//------------------------------------------------------------------------------------------------
	// Periodic update for marker fade-out
	protected void UpdateMarkerFade(GRAD_BC_TrafficMarkerData data)
	{
		if (!data || !data.m_Marker)
			return;
		
		float currentTime = System.GetTickCount() / 1000.0;
		float elapsed = currentTime - data.m_fCreationTime;
		
		// If past lifetime, remove the marker entirely
		if (elapsed >= MARKER_LIFETIME)
		{
			RemoveMarker(data);
			return;
		}
		
		float opacity = CalculateFadeOpacity(elapsed);
		
		// Build updated marker text with timestamp and elapsed time
		// Note: opacity value drives the update interval (faster updates as marker ages)
		// but cannot be applied as visual transparency since vanilla markers don't support per-marker opacity
		int elapsedSeconds = elapsed;
		int minutes = elapsedSeconds / 60;
		int seconds = elapsedSeconds % 60;
		
		string eventLabel;
		if (data.m_sEventType == "killed")
			eventLabel = "CIV KILLED";
		else
			eventLabel = "GUNFIGHT";
		
		string ageText;
		if (minutes > 0)
			ageText = string.Format("%1m %2s ago", minutes, seconds);
		else
			ageText = string.Format("%1s ago", seconds);
		
		string markerText = string.Format("%1 [%2] (%3)", eventLabel, data.m_sTimestamp, ageText);
		
		// Remove old marker and insert updated one to reflect fade state
		SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_MapMarkerManagerComponent));
		if (!markerManager)
			return;
		
		markerManager.RemoveStaticMarker(data.m_Marker);
		
		SCR_MapMarkerBase newMarker = new SCR_MapMarkerBase();
		newMarker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		
		int worldPos[2];
		worldPos[0] = data.m_vLocation[0];
		worldPos[1] = data.m_vLocation[2];
		newMarker.SetWorldPos(worldPos[0], worldPos[1]);
		newMarker.SetCustomText(markerText);
		newMarker.SetIconEntry(data.m_iIconEntry);
		newMarker.SetColorEntry(data.m_iColorEntry);
		
		markerManager.InsertStaticMarker(newMarker, true, true);
		data.m_Marker = newMarker;
		
		// Schedule next update - interval decreases as opacity drops (faster updates toward the end)
		// Scale interval: at full opacity use FADE_UPDATE_INTERVAL, at low opacity update more frequently
		float nextInterval = FADE_UPDATE_INTERVAL * opacity;
		if (nextInterval < 2.0)
			nextInterval = 2.0;
		
		GetGame().GetCallqueue().CallLater(
			UpdateMarkerFade,
			nextInterval * 1000,
			false,
			data
		);
		
		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Fade update - opacity: %1, age: %2s, next in: %3s", opacity, elapsedSeconds, nextInterval), LogLevel.VERBOSE);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void RemoveMarker(GRAD_BC_TrafficMarkerData data)
	{
		if (!data)
			return;
		
		if (data.m_Marker)
		{
			SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_MapMarkerManagerComponent));
			if (markerManager)
			{
				markerManager.RemoveStaticMarker(data.m_Marker);
				Print("GRAD_BC_TrafficHintManagerComponent: Removed expired traffic marker", LogLevel.NORMAL);
			}
		}
		
		m_aActiveMarkerData.RemoveItem(data);
	}
	
	//------------------------------------------------------------------------------------------------
	// Format current time as HH:MM:SS for marker timestamp
	protected string FormatTimestamp()
	{
		// Use system tick to derive a session-relative timestamp
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