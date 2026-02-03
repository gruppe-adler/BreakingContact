[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "Manager for civilian death events and UI triggers.")]
class GRAD_BC_TrafficHintManagerComponentClass : ScriptComponentClass {}

class GRAD_BC_TrafficHintManagerComponent : ScriptComponent
{
	protected RplComponent m_RplComponent;
	protected bool m_bSubscribed = false;
	
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
	protected void OnCivilianEvent(vector location, string eventtype)
	{
		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Civilian event detected at %1, type: %2", location, eventtype), LogLevel.NORMAL);
		
		// Only handle this on server
		if (!Replication.IsServer())
		{
			Print("GRAD_BC_TrafficHintManagerComponent: Not server, ignoring event", LogLevel.VERBOSE);
			return;
		}
		
		Print("GRAD_BC_TrafficHintManagerComponent: Server handling event, broadcasting to all clients", LogLevel.NORMAL);
		
		// Broadcast event to all clients via RPC
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
			// Schedule hint and marker to appear after 5-second delay
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