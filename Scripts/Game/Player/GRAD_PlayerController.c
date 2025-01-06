//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController : PlayerController
{
	protected ref GRAD_MapMarkerUI m_MapMarkerUI;
	protected ref GRAD_IconMarkerUI m_IconMarkerUI;
	ref GRAD_BC_Logo m_BC_logo;
	
	protected bool m_bChoosingSpawn;
	
	SCR_PlayerController m_playerController;
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		
		m_BC_logo = new GRAD_BC_Logo;
		
		m_playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		
		if (gameMode) {
			if (gameMode.GetState() == SCR_EGameModeState.GAME) {
				Print(string.Format("SCR_PlayerController - EOninit"), LogLevel.NORMAL);
				GetGame().GetCallqueue().CallLater(InitMapMarkerUI, 3000, false);
				GetGame().GetCallqueue().CallLater(ForceOpenMap, 4000, false);
			
				return;
			}
		}
		
		// loop until LobbyMod decides its time to go to GAME
		GetGame().GetCallqueue().CallLater(EOnInit, 3000, false, owner);
    }
			
	bool IsChoosingSpawn() 
	{
		Print(string.Format("SCR_PlayerController - Choosing Spawn asked"), LogLevel.NORMAL);
		return m_bChoosingSpawn;
	}
	
	void setChoosingSpawn(bool choosing) {
		m_bChoosingSpawn = choosing;
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
			GetGame().GetInputManager().AddActionListener("GRAD_BC_ConfirmSpawn", EActionTrigger.DOWN, ConfirmSpawn);
			Print(string.Format("BC phase opfor - is opfor - add map key eh"), LogLevel.WARNING);
			m_bChoosingSpawn = true;
		}
		
		// blufor commander is NOT allowed to choose spawn, however can signal other players with a map marker some tactics or speculate
		if (characterRole == "Blufor Commander")
		{
			m_bChoosingSpawn = true;
		}
		
		Print(string.Format("BC ForceOpenMap"), LogLevel.NORMAL);
		ToggleMap(true);
		
	}
	
	// find BCM in favor of having own client instance
	GRAD_BC_BreakingContactManager FindBreakingContactManager()
	{
		IEntity GRAD_BCM = GetGame().GetWorld().FindEntityByName("GRAD_BCM");
		if (!GRAD_BCM) {
			Print("GRAD_BCM Entity missing", LogLevel.ERROR);
			return null	;
		}
		
	 	GRAD_BC_BreakingContactManager manager = GRAD_BC_BreakingContactManager.Cast(GRAD_BCM);
        if (manager)
        {
             Print("Found Server BCM!", LogLevel.NORMAL);
             return manager;
        }
	
	    Print("Server Breaking Contact Manager not found!", LogLevel.ERROR);
	    return null;
	}
	
	
	void ShowBCLogo() {
		if (!m_playerController) {
			Print(string.Format("GRAD Playercontroller ShowBCLogo - no playercontroller found"), LogLevel.WARNING);
			return;
		}
	
		GRAD_BC_Logo logo = GRAD_BC_Logo.Cast(m_playerController.FindComponent(GRAD_BC_Logo));
		// todo currently no logo found
		if (!m_BC_logo) {
			Print(string.Format("GRAD Playercontroller ShowBCLogo - no logo found"), LogLevel.WARNING);
			return;
		}
		m_BC_logo.SetVisible(true);
		GetGame().GetCallqueue().CallLater(HideBCLogo, 5000, false);
	}
	
	void HideBCLogo() {
		if (!m_playerController) {
			Print(string.Format("GRAD Playercontroller HideBCLogo - no playercontroller found"), LogLevel.WARNING);
			return;
		}
		GRAD_BC_Logo logo = GRAD_BC_Logo.Cast(m_playerController.FindComponent(GRAD_BC_Logo));
		if (!logo) {
			Print(string.Format("GRAD Playercontroller HideBCLogo - no logo found"), LogLevel.WARNING);
			return;
		}
		logo.SetVisible(false);
	}
	
	//------------------------------------------------------------------------------------------------
	void ConfirmSpawn()
	{
		if (Replication.IsServer()) {
			Print(string.Format("ConfirmSpawn executed on server too"), LogLevel.NORMAL);
		}
 
		if (!m_playerController) {
			Print(string.Format("ConfirmSpawn missing in playerController"), LogLevel.NORMAL);
			return;
		}
		
		GRAD_BC_BreakingContactManager BCM = FindBreakingContactManager();
		if (!BCM) {
			Print(string.Format("BCM missing in playerController"), LogLevel.NORMAL);
			return;
		}
		
		EBreakingContactPhase phase = BCM.GetBreakingContactPhase();
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(m_playerController.GetControlledEntity());
		if (!ch)  {
			Print(string.Format("SCR_ChimeraCharacter missing in m_playerController"), LogLevel.NORMAL);
			return;
		}
		
		string factionKey = ch.GetFactionKey();
		
		Print(string.Format("ConfirmSpawn: factionKey: %1 - phase: %2", factionKey, phase), LogLevel.NORMAL);
		
		if (factionKey == "USSR" && phase == EBreakingContactPhase.OPFOR) {
			vector spawnPosition = m_MapMarkerUI.GetSpawnCoords();
			RequestInitiateOpforSpawnLocal(spawnPosition);
			RemoveSpawnMarker();
			Print(string.Format("ConfirmSpawn: %1 - factionKey: %2 - phase: %3", spawnPosition, factionKey, phase), LogLevel.NORMAL);
		
			// remove key listener
			GetGame().GetInputManager().RemoveActionListener("GRAD_BC_ConfirmSpawn", EActionTrigger.DOWN, ConfirmSpawn);
			return;
		}
		
		if (factionKey == "US" && phase == EBreakingContactPhase.GAME) {
			Print(string.Format("Removing spawn marker for blufor"), LogLevel.NORMAL);
			RemoveSpawnMarker();
			return;
		}
		
	}
	
	//-----------------------------------------------------------------------
	void Rpc_AddTransmissionMarker(GRAD_BC_TransmissionPointComponent entity, float radius) {
	
		Print(string.Format("Breaking Contact - executing RPC Rpc_AddTransmissionMarker locally"), LogLevel.NORMAL);
	
		vector center = entity.GetOrigin();
		RplId rplId = Replication.FindId(RplComponent.Cast(entity.FindComponent(RplComponent)));
		
		int playerId = GetGame().GetPlayerController().GetPlayerId();
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));

		if (!playerController) {
			Print(string.Format("Breaking Contact - no playerController found in Rpc_AddTransmissionMarker"), LogLevel.ERROR);
			return;
		}
	
		playerController.AddCircleMarker(
			center[0] - radius,
			center[2] + radius,
			center[0] + radius,
			center[2] + radius,
			rplId
		);
		
		playerController.SetCircleMarkerActive(rplId); // if a new transmission point is created, its active by default
		
		playerController.AddIconMarker(
			center[0] - (radius / 12),
			center[2] + (radius / 12),
			center[0] + (radius / 12),
			center[2] + (radius / 12),
			"{534DF45C06CFB00C}UI/Textures/Map/transmission_active.edds",
			rplId
		);
	}
	
	
	//------------------------------------------------------------------------------------------------
	void InsertMarker(SCR_MapMarkerBase marker)
	{
		Rpc(RpcDo_Owner_InsertMarker, marker);
	}
	
	//------------------------------------------------------------------------------------------------
	void RequestInitiateOpforSpawnLocal(vector spawnPos) {
		Print(string.Format("Breaking Contact - RequestInitiateOpforSpawnLocal"), LogLevel.NORMAL);
		
		Rpc(RequestInitiateOpforSpawn,spawnPos);
	}
	
	//------------------------------------------------------------------------------------------------
	// this needs to be inside player controller to work, dont switch component during rpc? i guess
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RequestInitiateOpforSpawn(vector spawnPos) {
		GRAD_BC_BreakingContactManager BCM = FindBreakingContactManager();
		Print(string.Format("Breaking Contact - RequestInitiateOpforSpawn"), LogLevel.NORMAL);
		
		BCM.Rpc_RequestInitiateOpforSpawn(spawnPos);
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
	void AddIconMarker(float startX, float startY, float endX, float endY, string sType, RplId rplId)
	{
		Rpc(RpcDo_Owner_AddIconMarker, startX, startY, endX, endY, sType, rplId);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_AddIconMarker(float startX, float startY, float endX, float endY, string sType, RplId rplId)
	{
		m_IconMarkerUI.AddIcon(startX, startY, endX, endY, sType, rplId);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetIconMarker(string sType, RplId rplId)
	{
		Rpc(RpcDo_Owner_SetIconMarker, sType, rplId);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_SetIconMarker(string sType, RplId rplId)
	{
		m_IconMarkerUI.SetIcon(sType, rplId);
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
	void TeleportPlayer(vector pos)
	{
		Rpc(RpcDo_Owner_TeleportPlayer, pos);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_TeleportPlayer(vector pos)
	{
		// executed locally on players machine
		
		// Close map before creating marker
		ToggleMap(false);
		
		if(SCR_Global.TeleportLocalPlayer(pos, SCR_EPlayerTeleportedReason.DEFAULT))
			Print(string.Format("PlayerController - Player with ID %1 successfully teleported to position %2", GetPlayerId(), pos), LogLevel.NORMAL);
		else
			Print(string.Format("PlayerController - Player with ID %1 NOT successfully teleported to position %2", GetPlayerId(), pos), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	void TeleportPlayerToMapPos(int playerId, vector spawnPos)
	{
		Rpc(Rpc_Do_Owner_TeleportPlayerToMapPos, playerId, spawnPos);
	}
		
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void Rpc_Do_Owner_TeleportPlayerToMapPos(int playerId, vector spawnPos)
	{
		// executed locally on players machine
		
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		
		if (!playerEntity)
			return;
		
		Print(string.Format("GRAD PlayerController - Player with ID %1 has position %2", playerId, playerEntity.GetOrigin()), LogLevel.NORMAL);
		
		bool teleportSuccessful;
		bool spawnEmpty;
		int spawnSearchLoop = 0;
		
		vector newWorldPos;
		
		while ((!teleportSuccessful || !spawnEmpty) && spawnSearchLoop < 10)
		{
			int radius = 3 + spawnSearchLoop; // increasing each loop
			Math.Randomize(-1);
            float randomDistanceX = Math.RandomFloat( -radius, radius );
            float randomDistanceY = Math.RandomFloat( -radius, radius );
			
			vector spawnPosFinal = {spawnPos[0] + randomDistanceX, GetGame().GetWorld().GetSurfaceY(spawnPos[0] + randomDistanceX, spawnPos[2] + randomDistanceY), spawnPos[2] + randomDistanceY};
			spawnSearchLoop = spawnSearchLoop + 1;
			spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(spawnPosFinal, spawnPosFinal, radius, 2);
			teleportSuccessful = SCR_Global.TeleportLocalPlayer(spawnPosFinal, SCR_EPlayerTeleportedReason.DEFAULT);
			Print(string.Format("GRAD PlayerController - spawnSearchLoop %1", spawnSearchLoop), LogLevel.NORMAL);
		}
		Print(string.Format("GRAD PlayerController - teleport %1", teleportSuccessful), LogLevel.NORMAL);
		
	}
};
