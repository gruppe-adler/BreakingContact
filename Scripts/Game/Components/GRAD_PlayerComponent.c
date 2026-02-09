[ComponentEditorProps(category: "Gruppe Adler/Breaking Contact", description: "Attach to a character. Handles stuff")]
class GRAD_PlayerComponentClass : ScriptComponentClass
{
}

class GRAD_PlayerComponent : ScriptComponent
{
	static protected GRAD_PlayerComponent m_instance;
	static GRAD_PlayerComponent GetInstance()
	{		
		if (m_instance == null)
		{
			PlayerController playerController = GetGame().GetPlayerController();
			if (playerController == null)
				return null;
			
			m_instance = GRAD_PlayerComponent.Cast(playerController.FindComponent(GRAD_PlayerComponent));
		}
		
		return m_instance;
	}
	
	protected PlayerManager m_PlayerManager;
	
	protected PlayerManager GetPlayerManager()
	{
		if (m_PlayerManager == null)
			m_PlayerManager = GetGame().GetPlayerManager();
		
		return m_PlayerManager;
	}
	
	//------------------------------------------------------------------------------------------------
	void Ask_TeleportPlayer(vector location)
	{
		Rpc(RpcDo_Owner_TeleportPlayer, location);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_TeleportPlayer(vector location)
	{
		SCR_Global.TeleportLocalPlayer(location, SCR_EPlayerTeleportedReason.DEFAULT);
		AudioSystem.PlayEvent("{937A60765465B47D}sounds/BC_beam.acp", "beam", location);
	}
	
	protected ref GRAD_MapMarkerUI m_MapMarkerUI;
	protected ref GRAD_IconMarkerUI m_IconMarkerUI;

	GRAD_IconMarkerUI GetIconMarkerUI()
	{
		return m_IconMarkerUI;
	}

	protected bool m_bChoosingSpawn;
	protected bool m_bSpawnPositionReady = false; // Track if spawn calculation is complete
	
	protected string m_faction;
	
	SCR_PlayerController m_playerController;
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		
		m_playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		
		if (gameMode) {
			if (gameMode.GetState() == SCR_EGameModeState.GAME) {
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("SCR_PlayerController - EOninit"), LogLevel.NORMAL);
				GetGame().GetCallqueue().CallLater(InitMapMarkerUI, 1000, false);
				GetGame().GetCallqueue().CallLater(ForceOpenMap, 1500, false);
				
				SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(m_playerController.GetControlledEntity());
				if (!ch)  {
					if (GRAD_BC_BreakingContactManager.IsDebugMode())
						Print(string.Format("SCR_ChimeraCharacter missing in m_playerController"), LogLevel.NORMAL);
					return;
				}
				
				m_faction = ch.GetFactionKey();
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("faction detected: %1", m_faction), LogLevel.NORMAL);
			
				return;
			}
		}
		
		// loop until LobbyMod decides its time to go to GAME
		GetGame().GetCallqueue().CallLater(EOnInit, 1000, false, owner);
    }
			
	bool IsChoosingSpawn() 
	{
		// Don't allow spawn selection during GAMEOVER phase (replay mode)
		GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		if (BCM)
		{
			EBreakingContactPhase phase = BCM.GetBreakingContactPhase();
			if (phase == EBreakingContactPhase.GAMEOVER)
			{
				return false; // Disable spawn selection during replay
			}
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("SCR_PlayerController - Choosing Spawn asked"), LogLevel.NORMAL);
		return m_bChoosingSpawn;
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsSpawnPositionReady()
	{
		return m_bSpawnPositionReady;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetSpawnPositionReady(bool ready)
	{
		m_bSpawnPositionReady = ready;
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("PlayerComponent: Spawn position ready set to %1", ready), LogLevel.NORMAL);
	}
	
	void setChoosingSpawn(bool choosing) {
		m_bChoosingSpawn = choosing;
		
		// Reset spawn ready flag when starting to choose spawn
		if (choosing)
		{
			m_bSpawnPositionReady = false;
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("PlayerComponent: Started choosing spawn, resetting spawn ready flag", LogLevel.NORMAL);
		}
	}
	
	//------
	void ForceOpenMap()
	{
		// try again
		if (!m_playerController) {
			GetGame().GetCallqueue().CallLater(ForceOpenMap, 5000, false);
			Print(string.Format("no playerController - wait and retry in 5s"), LogLevel.WARNING);
			return;
		}

		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(m_playerController.GetControlledEntity());
		// try again
		if (!ch) {
			GetGame().GetCallqueue().CallLater(ForceOpenMap, 5000, false);
			Print(string.Format("no chimera - wait and retry in 5s"), LogLevel.WARNING);
			return;
		}


		GRAD_CharacterRoleComponent characterRoleComponent = GRAD_CharacterRoleComponent.Cast(ch.FindComponent(GRAD_CharacterRoleComponent));
		if (!characterRoleComponent) {
			Print(string.Format("no character role component for this slot - wait and retry in 5s"), LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(ForceOpenMap, 5000, false);
			return;
		}

		string characterRole = "none";

		if (characterRoleComponent) {
			characterRole = characterRoleComponent.GetCharacterRole();
		} else {
			Print(string.Format("BC phase opfor - no commander found"), LogLevel.WARNING);
		}

		if (characterRole == "Opfor Commander")
		{
			m_faction = "USSR";
			GetGame().GetInputManager().AddActionListener("GRAD_BC_ConfirmSpawn", EActionTrigger.DOWN, ConfirmSpawn);
			Print(string.Format("BC phase opfor - is opfor - add map key eh"), LogLevel.WARNING);
			m_bChoosingSpawn = true;
		}

		// blufor commander is NOT allowed to choose spawn, however can signal other players with a map marker some tactics or speculate
		if (characterRole == "Blufor Commander")
		{
			m_faction = "US";
			m_bChoosingSpawn = true;
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BC ForceOpenMap"), LogLevel.NORMAL);
		ToggleMap(true);

	}
	
	//------------------------------------------------------------------------------------------------
	void ConfirmSpawn()
	{
		if (Replication.IsServer()) {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("ConfirmSpawn executed on server too"), LogLevel.NORMAL);
		}
 
		if (!m_playerController) {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("ConfirmSpawn missing in playerController"), LogLevel.NORMAL);
			return;
		}
		
		// Check if spawn position calculation is complete
		if (!m_bSpawnPositionReady)
		{
			Print("ConfirmSpawn: Spawn position not ready yet, please wait for calculation to complete", LogLevel.WARNING);
			SCR_HintManagerComponent.GetInstance().ShowCustomHint("Calculating spawn positions, please wait...", "Spawn Not Ready", 3, false);
			return;
		}
		
		GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		if (!BCM) {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("BCM missing in playerController"), LogLevel.NORMAL);
			return;
		}
		
		EBreakingContactPhase phase = BCM.GetBreakingContactPhase();
		
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("ConfirmSpawn: factionKey: %1 - phase: %2 - SpawnReady: %3", m_faction, phase, m_bSpawnPositionReady), LogLevel.NORMAL);
		
		if (m_faction == "USSR") {
			if (phase != EBreakingContactPhase.OPFOR) {
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("ConfirmSpawn: Not in opfor phase but ussr player"), LogLevel.NORMAL);
				return;
			}
			if (phase == EBreakingContactPhase.OPFOR) {				
				// remove key listener
				GetGame().GetInputManager().RemoveActionListener("GRAD_BC_ConfirmSpawn", EActionTrigger.DOWN, ConfirmSpawn);
				
				RequestInitiateOpforSpawnLocal();
				RemoveSpawnMarker();
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("ConfirmSpawn - m_faction: %1 - phase: %2 - Removing spawn marker for opfor.", m_faction, phase), LogLevel.NORMAL);
				return;
			}
		}
		
		if (m_faction == "US" && phase == EBreakingContactPhase.GAME) {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("Removing spawn marker for blufor"), LogLevel.NORMAL);
			RemoveSpawnMarker();
			return;
		}
		
	}
	
	//------------------------------------------------------------------------------------------------
	void SetOpforSpawn(vector worldPos)
	{
		// Reset spawn ready flag when new position is being calculated
		m_bSpawnPositionReady = false;
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("PlayerComponent: Setting new spawn position, marking as not ready", LogLevel.NORMAL);
		
		int playerId = GetGame().GetPlayerController().GetPlayerId();
		Rpc(RpcDo_SetOpforSpawn, worldPos, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcDo_SetOpforSpawn(vector spawnPoint, int requesterPlayerId)
	{
		GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		
		GRAD_SpawnPointResponse result = BCM.SetSpawnPositions(spawnPoint);
		Rpc(RcpResp_SetOpforSpawn, result);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RcpResp_SetOpforSpawn(GRAD_SpawnPointResponse result)
	{
		string text;
		switch(result)
		{
			case GRAD_SpawnPointResponse.OK:
				text = "OK - Spawn ready";
				// Mark spawn position as ready when calculation succeeds
				m_bSpawnPositionReady = true;
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print("PlayerComponent: Spawn position calculation complete and READY", LogLevel.NORMAL);
				break;
			case GRAD_SpawnPointResponse.OPFOR_NOTFOUND:
				text = "Couldn't find suitable OPFOR spawn pos";
				m_bSpawnPositionReady = false;
				break;
			case GRAD_SpawnPointResponse.BLUFOR_NOTFOUND:
				text = "Couldn't find suitable BLUFOR spawn pos";
				m_bSpawnPositionReady = false;
				break;
		}
		
		SCR_HintManagerComponent.GetInstance().ShowCustomHint(text, "Spawn pos", 5, true);
	}
	
	//------------------------------------------------------------------------------------------------
	void InsertMarker(SCR_MapMarkerBase marker)
	{
		Rpc(RpcDo_Owner_InsertMarker, marker);
	}
	
	//------------------------------------------------------------------------------------------------
	void RequestInitiateOpforSpawnLocal() {
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Breaking Contact - RequestInitiateOpforSpawnLocal"), LogLevel.NORMAL);
		
		Rpc(RequestInitiateOpforSpawn);
	}
	
	//------------------------------------------------------------------------------------------------
	// this needs to be inside player controller to work, dont switch component during rpc? i guess
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RequestInitiateOpforSpawn() {
		GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Breaking Contact - RequestInitiateOpforSpawn"), LogLevel.NORMAL);
		
		if (!BCM) {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("PANIC, no BCM in PC");
			return;
		}
		BCM.Rpc_RequestInitiateOpforSpawn();
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_InsertMarker(SCR_MapMarkerBase marker)
	{
		// executed locally on players machine
		
		// Open map before creating marker
		ToggleMap(true);
		
		// create marker
		GetGame().GetCallqueue().CallLater(SetMarker, 1000, false, marker); // 1s delay until map is open
	}

	//------------------------------------------------------------------------------------------------
	protected void SetMarker(SCR_MapMarkerBase marker)
	{
		SCR_MapMarkerManagerComponent mapMarkerManager = SCR_MapMarkerManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_MapMarkerManagerComponent));
		
		SCR_MapMarkerBase newMarker = new SCR_MapMarkerBase();
		newMarker.SetType(marker.GetType());
		int worldPos[2];
		marker.GetWorldPos(worldPos);
		newMarker.SetWorldPos(worldPos[0], worldPos[1]);
		newMarker.SetMarkerConfigID(marker.GetMarkerConfigID());
		newMarker.SetCustomText(marker.GetCustomText());
		newMarker.SetColorEntry(marker.GetColorEntry());
		newMarker.SetIconEntry(marker.GetIconEntry());
		
		mapMarkerManager.InsertStaticMarker(newMarker, true, true);
	}
		
	//------------------------------------------------------------------------------------------------
	void ToggleMap(bool open)
	{
		if (!m_playerController) {
			Print(string.Format("No SCR_PlayerController in ToggleMap"), LogLevel.ERROR);
			return;
		}
		
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(m_playerController.GetControlledEntity());
		if (!ch) {
			Print(string.Format("No SCR_ChimeraCharacter in ToggleMap"), LogLevel.ERROR);
			return;
		}
		
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(ch.FindComponent(SCR_GadgetManagerComponent));
		if (!gadgetManager) {
			Print(string.Format("No gadgetManager in ToggleMap"), LogLevel.ERROR);
			return;
		}
		
		if (!gadgetManager.GetGadgetByType(EGadgetType.MAP)) {
			Print(string.Format("No EGadgetType.MAP in ToggleMap"), LogLevel.ERROR);
			return;
		}
		
		IEntity mapEntity = gadgetManager.GetGadgetByType(EGadgetType.MAP);
		
		if (open)
			gadgetManager.SetGadgetMode(mapEntity, EGadgetMode.IN_HAND, true);
		else
			gadgetManager.SetGadgetMode(mapEntity, EGadgetMode.IN_SLOT, false);
	}

	
	//------------------------------------------------------------------------------------------------
	protected void InitMapMarkerUI()
	{
		if (!m_MapMarkerUI)
		{
			m_MapMarkerUI = new GRAD_MapMarkerUI();
			m_MapMarkerUI.Init();
		}
		
		if (!m_IconMarkerUI)
		{
			m_IconMarkerUI = new GRAD_IconMarkerUI();
			m_IconMarkerUI.Init();
		}
	}
	
	
	//------------------------------------------------------------------------------------------------
	void RemoveSpawnMarker()
	{
		Rpc(RpcDo_Owner_RemoveSpawnMarker);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_RemoveSpawnMarker()
	{
		m_MapMarkerUI.RemoveSpawnMarker();
	}
	
	//------------------------------------------------------------------------------------------------
	void AddCircleMarker(float startX, float startY, float endX, float endY, RplId rplId, bool spawnMarker = false)
	{
		m_MapMarkerUI.AddCircle(startX, startY, endX, endY, rplId, spawnMarker);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("AddCircleMarker in GRAD_PlayerComponent");
	}
	
	//------------------------------------------------------------------------------------------------
	void SetCircleMarkerActive(RplId rplId)
	{
		Rpc(RpcDo_Owner_SetCircleMarkerActive, rplId);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_SetCircleMarkerActive(RplId rplId)
	{
		m_MapMarkerUI.SetCircleActive(rplId);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetCircleMarkerInactive(RplId rplId)
	{
		Rpc(RpcDo_Owner_SetCircleMarkerInactive, rplId);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_SetCircleMarkerInactive(RplId rplId)
	{
		m_MapMarkerUI.SetCircleInactive(rplId);
	}
	
	//------------------------------------------------------------------------------------------------
	void ShowHint(string message, string title, int duration, bool isSilent)
	{
		Rpc(RpcDo_Owner_ShowHint, message, title, duration, isSilent);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_ShowHint(string message, string title, int duration, bool isSilent)
	{
		// executed locally on players machine
		
		SCR_HintManagerComponent.GetInstance().ShowCustomHint(message, title, duration, isSilent);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    protected void Rpc_ShowBCLogo_Local()
    {
        // Find the HUD display and call ShowLogo() on it
        array<BaseInfoDisplay> infoDisplays = {};
        GetGame().GetPlayerController().GetHUDManagerComponent().GetInfoDisplays(infoDisplays);

        // Search for our GRAD_BC_Logo instance
        GRAD_BC_Logo logoDisplay = null;
        foreach (BaseInfoDisplay baseDisp : infoDisplays)
        {
            GRAD_BC_Logo candidate = GRAD_BC_Logo.Cast(baseDisp);
            if (candidate)
            {
                logoDisplay = candidate;
                break;
            }
        }

        if (!logoDisplay)
        {
            Print("SCR_PlayerController: could not find GRAD_BC_Logo in HUD", LogLevel.ERROR);
            return;
        }

        // Show the logo immediately
        logoDisplay.ShowLogo();
    }
	
	// RPC wrapper for BC Logo
	void ShowBCLogoRPC()
    {
        Rpc(Rpc_ShowBCLogo_Local);
    }
	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    protected void Rpc_ShowTransmissionHint_Local(ETransmissionState state)
    {
		// Don't show transmission hints during replay or gameover
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm)
		{
			EBreakingContactPhase currentPhase = bcm.GetBreakingContactPhase();
			if (currentPhase == EBreakingContactPhase.GAMEOVER || currentPhase == EBreakingContactPhase.GAMEOVERDONE)
			{
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print("GRAD_PlayerComponent: Skipping transmission hint - in replay/gameover phase", LogLevel.NORMAL);
				return;
			}
		}

		// Check if replay is active
		GRAD_BC_ReplayManager replayManager = GRAD_BC_ReplayManager.GetInstance();
		if (replayManager && replayManager.IsPlayingBack())
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_PlayerComponent: Skipping transmission hint - replay is playing", LogLevel.NORMAL);
			return;
		}

		// Get current faction from controlled character (not cached m_faction which may be stale)
		// This fixes race condition where m_faction could be wrong due to initialization timing
		string currentFaction = "";
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(m_playerController.GetControlledEntity());
		if (ch)
			currentFaction = ch.GetFactionKey();

		// Fallback to cached faction if character not available
		if (currentFaction == "")
			currentFaction = m_faction;

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_PlayerComponent: Transmission hint - using faction: %1 (cached: %2)", currentFaction, m_faction), LogLevel.NORMAL);

        // Find the HUD display and call ShowLogo() on it
        array<BaseInfoDisplay> infoDisplays = {};
        GetGame().GetPlayerController().GetHUDManagerComponent().GetInfoDisplays(infoDisplays);

        // Search for our GRAD_BC_Transmission instance
        GRAD_BC_Transmission transmissionDisplay = null;
        foreach (BaseInfoDisplay baseDisp : infoDisplays)
        {
            GRAD_BC_Transmission candidate = GRAD_BC_Transmission.Cast(baseDisp);
            if (candidate)
            {
                transmissionDisplay = candidate;
                break;
            }
        }

        if (!transmissionDisplay)
        {
            Print("SCR_PlayerController: could not find GRAD_BC_Transmission in HUD", LogLevel.ERROR);
            return;
        }

        // Show the transmission hint with fresh faction
        transmissionDisplay.showTransmissionHint(currentFaction, state);
    }
	
	// RPC wrapper for BC Logo
	void ShowTransmissionHintRPC(ETransmissionState state)
    {
        Rpc(Rpc_ShowTransmissionHint_Local, state);
    }
}