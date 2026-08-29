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

		// AudioSystem.PlayEvent() now takes a TRANSFORM (vector[4]), not a plain position.
		vector soundTransform[4];
		Math3D.MatrixIdentity4(soundTransform);
		soundTransform[3] = location;
		AudioSystem.PlayEvent("{937A60765465B47D}sounds/BC_beam.acp", "beam", soundTransform);
	}
	
	protected ref GRAD_MapMarkerUI m_MapMarkerUI;
	protected ref GRAD_IconMarkerUI m_IconMarkerUI;

	GRAD_IconMarkerUI GetIconMarkerUI()
	{
		return m_IconMarkerUI;
	}

	protected bool m_bChoosingSpawn;
	protected bool m_bSpawnPositionReady = false; // Track if spawn calculation is complete

	// MAP LOCK (see GRAD_BC_M_SCR_MapGadgetComponent).
	//
	// While a commander is picking the spawn position the map must stay open, so they cannot walk
	// around before the team has spawned in. The lock is enforced in SCR_MapGadgetComponent.ModeClear.
	//
	// BC closes the map itself (ToggleMap(false) on phase change), and that close runs through the
	// exact same ModeClear the lock blocks - so every scripted close must raise this flag first, or
	// the game traps the player in the map it meant to close. ToggleMap() does that centrally; nothing
	// else should call SetGadgetMode on the map gadget directly.
	protected bool m_bScriptedMapClose = false;
	
	protected string m_faction;
	
	SCR_PlayerController m_playerController;
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());

		m_playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());

		if (gameMode && m_playerController) {
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
				
				string detectedFactionKey = ch.GetFactionKey();
				if (GRAD_BC_BreakingContactManager.IsOpforFactionKey(detectedFactionKey))
					m_faction = "USSR";
				else if (GRAD_BC_BreakingContactManager.IsBluforFactionKey(detectedFactionKey))
					m_faction = "US";
				else
					m_faction = detectedFactionKey;
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
		GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		if (BCM)
		{
			EBreakingContactPhase phase = BCM.GetBreakingContactPhase();

			// Spawn selection only makes sense during the two placement phases. Once the match is
			// running (GAME) or over (GAMEOVER/GAMEOVERDONE) the spawn has already happened and the
			// radio truck is placed, so re-picking a position must not be possible.
			//
			// This matters for JOIN IN PROGRESS: ForceOpenMap() sets m_bChoosingSpawn = true for
			// any "Opfor Commander"/"Blufor Commander" slot unconditionally, with no check for how
			// far the match has progressed. A commander joining mid-match therefore got spawn
			// selection re-enabled and could place the spawn marker again.
			if (phase != EBreakingContactPhase.OPFOR && phase != EBreakingContactPhase.BLUFOR)
			{
				return false;
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
		SetConfirmSpawnButtonVisible(choosing);

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


		string characterRole = "none";

		GRAD_CharacterRoleComponent characterRoleComponent = GRAD_CharacterRoleComponent.Cast(ch.FindComponent(GRAD_CharacterRoleComponent));
		if (characterRoleComponent)
			characterRole = characterRoleComponent.GetCharacterRole();

		// COALITION-Lobby path: BC's own GRAD_CharacterRoleComponent isn't set on
		// COA_GearscriptManager-spawned characters, so derive the same "Opfor Commander" /
		// "Blufor Commander" role strings from the player's slotted COA_EGearRole + faction.
		if (characterRole == "none")
		{
			COA_SlottingManager slottingManager = COA_SlottingManager.GetInstance();
			if (slottingManager)
			{
				int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(ch);
				if (playerId > 0)
				{
					COA_SlotData slotData = slottingManager.GetPlayerSlotData(playerId);
					if (slotData && slotData.GetSlotRole() == COA_EGearRole.COMPANY_COMMANDER)
					{
						string playerFactionKey = ch.GetFactionKey();
						if (GRAD_BC_BreakingContactManager.IsOpforFactionKey(playerFactionKey))
							characterRole = "Opfor Commander";
						else if (GRAD_BC_BreakingContactManager.IsBluforFactionKey(playerFactionKey))
							characterRole = "Blufor Commander";
					}
				}
			}
		}

		if (characterRole == "none") {
			Print(string.Format("no character role component for this slot - wait and retry in 5s"), LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(ForceOpenMap, 5000, false);
			return;
		}

		// JIP GUARD: only arm spawn selection while the match is actually in a placement phase.
		// A commander joining in progress (GAME / GAMEOVER) must not get spawn picking back -
		// the spawn has already happened and the radio truck is placed.
		bool inSpawnPhase = true;
		GRAD_BC_BreakingContactManager bcmPhase = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcmPhase)
		{
			EBreakingContactPhase currentPhase = bcmPhase.GetBreakingContactPhase();
			if (currentPhase != EBreakingContactPhase.OPFOR && currentPhase != EBreakingContactPhase.BLUFOR)
			{
				inSpawnPhase = false;
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("BC ForceOpenMap: JIP into phase %1 - not arming spawn selection",
						SCR_Enum.GetEnumName(EBreakingContactPhase, currentPhase)), LogLevel.WARNING);
			}
		}

		if (characterRole == "Opfor Commander")
		{
			m_faction = "USSR";
			GrantCommanderRank(ch);

			if (inSpawnPhase)
			{
				GetGame().GetInputManager().AddActionListener("GRAD_BC_ConfirmSpawn", EActionTrigger.DOWN, ConfirmSpawn);
				Print(string.Format("BC phase opfor - is opfor - add map key eh"), LogLevel.WARNING);
				m_bChoosingSpawn = true;
				// Defer button show — spectator menu widget tree is not ready until after ToggleMap opens it
				GetGame().GetCallqueue().CallLater(SetConfirmSpawnButtonVisible, 500, false, true);
			}
		}

		// blufor commander is NOT allowed to choose spawn, however can signal other players with a map marker some tactics or speculate
		if (characterRole == "Blufor Commander")
		{
			m_faction = "US";
			GrantCommanderRank(ch);

			if (inSpawnPhase)
				m_bChoosingSpawn = true;
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BC ForceOpenMap"), LogLevel.NORMAL);
		ToggleMap(true);

	}

	//------------------------------------------------------------------------------------------------
	// Commander needs at least Captain rank to pass SCR_CampaignBuildingStartUserAction's
	// access-rank check on the command trucks (BC has no XP/rank progression of its own).
	protected void GrantCommanderRank(IEntity ch)
	{
		if (Replication.IsServer())
		{
			DoGrantCommanderRank(ch);
			return;
		}

		Rpc(Ask_GrantCommanderRank);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void Ask_GrantCommanderRank()
	{
		if (!m_playerController)
			return;

		IEntity ch = m_playerController.GetControlledEntity();
		if (ch)
			DoGrantCommanderRank(ch);
	}

	//------------------------------------------------------------------------------------------------
	//! Client entry point: ask the server to switch the running scenario to s_aMaps[mapIndex].
	//! Called from GRAD_BC_COA_PreviewMenu's map dropdown after the admin confirms.
	void Ask_ChangeScenario(int mapIndex)
	{
		Rpc(RpcAsk_Server_ChangeScenario, mapIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side scenario switch. Security note: the caller's identity comes from GetOwner()
	//! - the PlayerController that owns this component - NOT from an RPC argument, which a
	//! modified client could set to any value. Do not refactor this to take a playerId param.
	//!
	//! Likewise this must not use m_playerController: GetGame().GetPlayerController() returns the
	//! LOCAL controller, which is null on a dedicated server.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Server_ChangeScenario(int mapIndex)
	{
		if (!Replication.IsServer())
			return;

		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
			return;

		int callerId = playerController.GetPlayerId();

		if (!GRAD_BC_MapSwitch.IsPlayerAdminServer(callerId))
		{
			Print(string.Format("BC Debug - MapSwitch: REJECTED scenario change from non-admin player %1", callerId), LogLevel.WARNING);
			return;
		}

		if (mapIndex < 0 || mapIndex >= GRAD_BC_MapSwitch.GetMapCount())
		{
			Print(string.Format("BC Debug - MapSwitch: REJECTED out-of-range map index %1 from player %2", mapIndex, callerId), LogLevel.WARNING);
			return;
		}

		if (GameStateTransitions.IsTransitionRequestedOrInProgress())
		{
			Print("BC Debug - MapSwitch: transition already requested or in progress, ignoring", LogLevel.WARNING);
			return;
		}

		ResourceName mission = GRAD_BC_MapSwitch.GetMission(mapIndex);

		Print(string.Format("BC Debug - MapSwitch: admin %1 switching scenario to %2", callerId, mission), LogLevel.NORMAL);

		// Empty addonList = keep the currently loaded addons. If clients get kicked for a mod
		// mismatch on switch, this is the first thing to change (semicolon-separated GUID list).
		bool requested = GameStateTransitions.RequestScenarioChangeTransition(mission, ResourceName.Empty, string.Empty);

		if (!requested)
			Print(string.Format("BC Debug - MapSwitch: RequestScenarioChangeTransition REFUSED for %1", mission), LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Client entry point: ask the server to move the calling admin into spectator.
	//! Works both from a slot and as the way back out of Game Master.
	void Ask_EnterSpectator()
	{
		// Close the editor first if it is open - while it is, the editor holds the player on its
		// own camera and the server-side possession would be overridden straight away.
		if (SCR_EditorManagerEntity.IsOpenedInstance())
			SCR_EditorManagerEntity.CloseInstance();

		Rpc(RpcAsk_Server_EnterSpectator);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side spectator switch for a single player. Same security model as
	//! RpcAsk_Server_ChangeScenario: the caller's identity comes from GetOwner(), never from an
	//! RPC argument, so a modified client cannot move a different player.
	//!
	//! Covers BOTH cases the feature needs:
	//!
	//!  1. Slotted player -> spectator. Clearing the slot makes COA_GamemodeManager.InitilizePlayer
	//!     take its spectator branch (it keys on IsPlayerInASlot), which spawns a spectator entity
	//!     and assigns it.
	//!
	//!  2. Already-unslotted player stuck in Game Master -> back to spectator. This is the case
	//!     the feature request came from: entering GM from spectator leaves no way back. Here the
	//!     slot is already clear, GetOrCreateSpectatorEntity returns the still-living spectator
	//!     entity, and the important part is that InitilizePlayer then re-runs
	//!     COA_PlayerHelper.AssignCharacterToPlayer -> RequestPossessSpawn, which hands control
	//!     back to that entity from whatever the GM camera left the player possessing.
	//!
	//! Closing the editor first matters: SCR_EditorManagerEntity keeps the player on its camera
	//! while open, so possession would be immediately overridden.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Server_EnterSpectator()
	{
		if (!Replication.IsServer())
			return;

		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
			return;

		int callerId = playerController.GetPlayerId();

		if (!GRAD_BC_MapSwitch.IsPlayerAdminServer(callerId))
		{
			Print(string.Format("BC Debug - Spectator: REJECTED spectator request from non-admin player %1", callerId), LogLevel.WARNING);
			return;
		}

		COA_SlottingManager slottingManager = COA_SlottingManager.GetInstance();
		COA_GamemodeManager gamemodeManager = COA_GamemodeManager.GetInstance();
		if (!slottingManager || !gamemodeManager)
		{
			Print("BC Debug - Spectator: COA_SlottingManager/COA_GamemodeManager not found, cannot move player to spectator", LogLevel.WARNING);
			return;
		}

		int slotId = slottingManager.GetPlayerSlotID(callerId);
		if (slotId != -1)
			slottingManager.UpdateSlotPlayerID(slotId, -1);

		gamemodeManager.InitilizePlayer(callerId);

		Print(string.Format("BC Debug - Spectator: admin %1 moved to spectator", callerId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void DoGrantCommanderRank(IEntity ch)
	{
		SCR_CharacterRankComponent rankComp = SCR_CharacterRankComponent.GetCharacterRankComponent(ch);
		if (!rankComp)
			return;

		if (SCR_CharacterRankComponent.GetCharacterRank(ch) < SCR_ECharacterRank.CAPTAIN)
			rankComp.SetCharacterRank(SCR_ECharacterRank.CAPTAIN, true);
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
			// SCR_HintManagerComponent.GetInstance().ShowCustomHint("Calculating spawn positions, please wait...", "Spawn Not Ready", 3, false);
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

				// Release the map lock here rather than waiting for the BLUFOR phase change. The
				// commander has finished choosing; leaving m_bChoosingSpawn set until the phase
				// advances would keep them locked in the map for that whole window.
				setChoosingSpawn(false);

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
		
		// SCR_HintManagerComponent.GetInstance().ShowCustomHint(text, "Spawn pos", 5, true);
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
		{
			gadgetManager.SetGadgetMode(mapEntity, EGadgetMode.IN_HAND, true);
			return;
		}

		// Raise the bypass so the map lock lets BC's own close through - without it, closing the map
		// at a phase change is rejected by the very lock that is supposed to end at that moment.
		m_bScriptedMapClose = true;
		gadgetManager.SetGadgetMode(mapEntity, EGadgetMode.IN_SLOT, false);
		m_bScriptedMapClose = false;
	}

	//------------------------------------------------------------------------------------------------
	//! True while a player-initiated map close must be refused. See the m_bScriptedMapClose comment.
	//! A scripted close (BC closing the map itself) is always allowed.
	bool IsMapCloseLocked()
	{
		if (m_bScriptedMapClose)
			return false;

		return IsChoosingSpawn();
	}

	//------------------------------------------------------------------------------------------------
	// NOTE: PSCore's PS_SpectatorMenu layout had an "OverlayFooter" widget BreakingContact
	// injected a "BC_ConfirmSpawn" button into. COA_SpectatorMenu's layout has no equivalent
	// footer widget, so this is currently a no-op under COALITION-Lobby until the confirm-spawn
	// button is given a home in COA_SpectatorMenu's own widget tree.
	protected void SetConfirmSpawnButtonVisible(bool visible)
	{
		if (!COA_SpectatorMenu.s_BCSpectatorMenu)
			return;

		Widget menuRoot = COA_SpectatorMenu.s_BCSpectatorMenu.GetRootWidget();
		if (!menuRoot)
			return;

		Widget overlayFooter = menuRoot.FindAnyWidget("OverlayFooter");
		if (!overlayFooter)
			return;

		Widget btn = overlayFooter.FindAnyWidget("BC_ConfirmSpawn");
		if (btn)
			btn.SetVisible(visible);
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
	override void OnDelete(IEntity owner)
	{
		// Clean up CallLater callbacks
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(EOnInit);
			GetGame().GetCallqueue().Remove(InitMapMarkerUI);
			GetGame().GetCallqueue().Remove(ForceOpenMap);
		}

		// Clean up input action listener
		if (GetGame() && GetGame().GetInputManager())
			GetGame().GetInputManager().RemoveActionListener("GRAD_BC_ConfirmSpawn", EActionTrigger.DOWN, ConfirmSpawn);

		// Clean up marker UIs and their static event subscriptions
		if (m_IconMarkerUI)
		{
			m_IconMarkerUI.Cleanup();
			m_IconMarkerUI = null;
		}
		if (m_MapMarkerUI)
		{
			m_MapMarkerUI.Cleanup();
			m_MapMarkerUI = null;
		}

		// Clear static instance if it's us
		if (m_instance == this)
			m_instance = null;

		super.OnDelete(owner);
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