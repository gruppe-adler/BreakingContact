//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController : PlayerController
{
	protected ref GRAD_MapMarkerUI m_MapMarkerUI;
	protected ref GRAD_IconMarkerUI m_IconMarkerUI;
	
	protected bool m_bChoosingSpawn;
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		InitMapMarkerUI();
		GetGame().GetCallqueue().CallLater(ForceOpenMap, 5000, false);
		m_bChoosingSpawn = true;
    }
			
	bool IsChoosingSpawn() 
	{
		Print(string.Format("SCR_PlayerController - Choosing Spawn asked"), LogLevel.NORMAL);
		return m_bChoosingSpawn;
	}
	
	//------
	void ForceOpenMap() 
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		// try again
		if (!playerController) {
			GetGame().GetCallqueue().CallLater(ForceOpenMap, 5000, false);
			return;
		}
		
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		// try again
		if (!ch) {
			GetGame().GetCallqueue().CallLater(ForceOpenMap, 5000, false);
			return;
		}
		
		bool isOpfor = ch.GetFactionKey() == "USSR";
		
		// InputManager m_InputManager = GetGame().GetInputManager();
		
		
		// GRAD_CharacterRoleComponent characterRoleComponent = GRAD_CharacterRoleComponent.Cast(ch.FindComponent(GRAD_CharacterRoleComponent));
		// string characterRole = characterRoleComponent.GetCharacterRole();
		
		/*
		if (characterRole != "BC Commander")
		{
			Print(string.Format("BC - Wrong role for marker. Current Role '%1'", characterRole), LogLevel.NORMAL);
			return;
		}
		*/
		GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		string phase = (SCR_Enum.GetEnumName(EBreakingContactPhase, BCM.GetBreakingContactPhase()));
				
		if (phase == "OPFOR" && isOpfor) {
			GetGame().GetInputManager().AddActionListener("GRAD_BC_ConfirmSpawn", EActionTrigger.DOWN, ConfirmSpawn);
			Print(string.Format("BC phase opfor - is opfor - add map key eh"), LogLevel.WARNING);
		}
		
		if (phase == "BLUFOR" && !isOpfor) {
			GetGame().GetInputManager().AddActionListener("GRAD_BC_ConfirmSpawn", EActionTrigger.DOWN, ConfirmSpawn);
			Print(string.Format("BC phase blufor - is blufor - add map key eh"), LogLevel.WARNING);
		}
		
		Print(string.Format("BC ForceOpenMap"), LogLevel.NORMAL);
		m_bChoosingSpawn = true;
		ToggleMap(true);
		
	}
	
	//------------------------------------------------------------------------------------------------
	void PhaseChange(string phase) 
	{
		
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController) {
			return;
		}
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		
		bool isOpfor = ch.GetFactionKey() == "USSR";
		
		// close map for opfor
		if (phase == "BLUFOR" && isOpfor) {
			ToggleMap(false);
			m_bChoosingSpawn = false;
			Print(string.Format("GRAD Playercontroller PhaseChange - closing map - opfor done"), LogLevel.NORMAL);
		}
		
		// close map for blufor
		if (phase == "GAME" && !isOpfor) {
			ToggleMap(false);
			m_bChoosingSpawn = false;
			Print(string.Format("GRAD Playercontroller PhaseChange - closing map - blufor done"), LogLevel.NORMAL);
		}
		
		if (phase == "GAME") {
			Print(string.Format("GRAD Playercontroller PhaseChange - game started, show logo"), LogLevel.NORMAL);
			// ShowBCLogo();
		}
	
	}
	
	void ShowBCLogo() {
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController) {
			Print(string.Format("GRAD Playercontroller ShowBCLogo - no playercontroller found"), LogLevel.WARNING);
			return;
		}
		GRAD_BC_Logo logo = GRAD_BC_Logo.Cast(playerController.FindComponent(GRAD_BC_Logo));
		if (!logo) {
			Print(string.Format("GRAD Playercontroller ShowBCLogo - no logo found"), LogLevel.WARNING);
			return;
		}
		logo.SetVisible(true);
		GetGame().GetCallqueue().CallLater(HideBCLogo, 5000, false);
	}
	
	void HideBCLogo() {
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController) {
			Print(string.Format("GRAD Playercontroller HideBCLogo - no playercontroller found"), LogLevel.WARNING);
			return;
		}
		GRAD_BC_Logo logo = GRAD_BC_Logo.Cast(playerController.FindComponent(GRAD_BC_Logo));
		if (!logo) {
			Print(string.Format("GRAD Playercontroller HideBCLogo - no logo found"), LogLevel.WARNING);
			return;
		}
		logo.SetVisible(false);
	}
	
	//------------------------------------------------------------------------------------------------
	void ConfirmSpawn()
	{
		RemoveSpawnMarker();
		Print(string.Format("BC PlayerController ConfirmSpawn"), LogLevel.NORMAL);
		GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		
		vector spawnPosition = m_MapMarkerUI.GetSpawnCoords();
		
		
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController) {
			return;
		}
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		
		bool isOpfor = ch.GetFactionKey() == "USSR";
		if (isOpfor) {
			BCM.InitiateOpforSpawn(spawnPosition);
		} else {
			BCM.InitiateBluforSpawn(spawnPosition);
		}
		
		// remove key listener
		GetGame().GetInputManager().RemoveActionListener("GRAD_BC_ConfirmSpawn", EActionTrigger.DOWN, ConfirmSpawn);
	}

	
	
	//------------------------------------------------------------------------------------------------
	void InsertMarker(SCR_MapMarkerBase marker)
	{
		Rpc(RpcDo_Owner_InsertMarker, marker);
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
		
		mapMarkerManager.InsertStaticMarker(newMarker);
	}
		
	//------------------------------------------------------------------------------------------------
	void ToggleMap(bool open)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc) return;
		
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());
		if (!ch) return;
		
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(ch.FindComponent(SCR_GadgetManagerComponent));
		if (!gadgetManager) return;
		
		if (!gadgetManager.GetGadgetByType(EGadgetType.MAP)) return;
		
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
		Rpc(RpcDo_Owner_AddCircleMarker, startX, startY, endX, endY, rplId, spawnMarker);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_AddCircleMarker(float startX, float startY, float endX, float endY, RplId rplId, bool spawnMarker)
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
			Print(string.Format("OTF - Player with ID %1 successfully teleported to position %2", GetPlayerId(), pos), LogLevel.NORMAL);
		else
			Print(string.Format("OTF - Player with ID %1 NOT successfully teleported to position %2", GetPlayerId(), pos), LogLevel.WARNING);
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
		int spawnSearchLoop;
		
		vector newWorldPos;
		
		while ((!teleportSuccessful || !spawnEmpty) && spawnSearchLoop < 10)
		{
			int radius = 3;
			Math.Randomize(-1);
            int randomDistanceX = Math.RandomInt( -radius, radius );
            int randomDistanceY = Math.RandomInt( -radius, radius );
			
			vector spawnPosFinal = {spawnPos[0] + randomDistanceX, GetGame().GetWorld().GetSurfaceY(spawnPos[0] + randomDistanceX, spawnPos[2] + randomDistanceY), spawnPos[2] + randomDistanceY};
			spawnSearchLoop = spawnSearchLoop + 1;
			spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(spawnPosFinal, spawnPosFinal, 2, 2);
			teleportSuccessful = SCR_Global.TeleportLocalPlayer(spawnPosFinal, SCR_EPlayerTeleportedReason.DEFAULT);
			Print(string.Format("GRAD PlayerController - spawnSearchLoop %1", spawnSearchLoop), LogLevel.NORMAL);
		}
		Print(string.Format("GRAD PlayerController - teleport %1", teleportSuccessful), LogLevel.NORMAL);
		
	}
};
