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
	}
	
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
		
		GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
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
		
		if (factionKey == "USSR") {
			if (phase != EBreakingContactPhase.OPFOR) {
				Print(string.Format("ConfirmSpawn: Not in opfor phase but ussr player"), LogLevel.NORMAL);
				return;
			}
			if (phase == EBreakingContactPhase.OPFOR) {				
				// remove key listener
				GetGame().GetInputManager().RemoveActionListener("GRAD_BC_ConfirmSpawn", EActionTrigger.DOWN, ConfirmSpawn);
				
				RequestInitiateOpforSpawnLocal();
				RemoveSpawnMarker();
				Print(string.Format("ConfirmSpawn - factionKey: %1 - phase: %2 - Removing spawn marker for opfor.", factionKey, phase), LogLevel.NORMAL);
				return;
			}
		}
		
		if (factionKey == "US" && phase == EBreakingContactPhase.GAME) {
			Print(string.Format("Removing spawn marker for blufor"), LogLevel.NORMAL);
			RemoveSpawnMarker();
			return;
		}
		
	}
	
	//------------------------------------------------------------------------------------------------
	void SetOpforSpawn(vector worldPos)
	{
		Rpc(RpcDo_SetOpforSpawn, worldPos);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcDo_SetOpforSpawn(vector spawnPoint)
	{
		GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		
		BCM.SetOpforSpawnPos(spawnPoint);
	}
	
	//------------------------------------------------------------------------------------------------
	void InsertMarker(SCR_MapMarkerBase marker)
	{
		Rpc(RpcDo_Owner_InsertMarker, marker);
	}
	
	//------------------------------------------------------------------------------------------------
	void RequestInitiateOpforSpawnLocal() {
		Print(string.Format("Breaking Contact - RequestInitiateOpforSpawnLocal"), LogLevel.NORMAL);
		
		Rpc(RequestInitiateOpforSpawn);
	}
	
	//------------------------------------------------------------------------------------------------
	// this needs to be inside player controller to work, dont switch component during rpc? i guess
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RequestInitiateOpforSpawn() {
		GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		Print(string.Format("Breaking Contact - RequestInitiateOpforSpawn"), LogLevel.NORMAL);
		
		if (!BCM) {
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
}