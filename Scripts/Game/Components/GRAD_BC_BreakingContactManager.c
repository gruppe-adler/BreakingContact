// no GAMEMASTER phase as everything should be run out of the box self explaining
enum EBreakingContactPhase
{
	LOADING,
	PREPTIME,
	OPFOR,
	BLUFOR,
	GAME,
	GAMEOVER,
	GAMEOVERDONE
}

[EntityEditorProps(category: "Gruppe Adler", description: "Breaking Contact Gamemode Manager")]
class GRAD_BC_BreakingContactManagerClass : ScriptComponentClass
{
}

// This class is server-only code

class GRAD_BC_BreakingContactManager : ScriptComponent
{
    [Attribute(defvalue: "2", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How many transmissions are needed to win.", category: "Breaking Contact - Parameters", params: "1 3 1")]
	protected int m_iTransmissionCount;
	
	[Attribute(defvalue: "900", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How long one transmission needs to last.", category: "Breaking Contact - Parameters", params: "1 600 1")]
	protected int m_TransmissionDuration;
	
	[Attribute(defvalue: "3000", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How far away BLUFOR spawns from OPFOR.", category: "Breaking Contact - Parameters", params: "1000 10000 100")]
	protected int m_iBluforSpawnDistance;
	
	[Attribute(defvalue: "10", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How long in seconds the notifications should be displayed", category: "Breaking Contact - Parameters", params: "1 30 1")]
	protected int m_iNotificationDuration;
	

    protected bool m_bluforCaptured;
    protected bool m_skipWinConditions;
	
	protected int m_spawnLock = 0;
	
	// Radio truck destruction tracking
	protected bool m_bRadioTruckDestroyed = false;
	protected string m_sRadioTruckDestroyerFaction = "";
	
	// Replay system
	protected GRAD_BC_ReplayManager m_replayManager;
	
	// Debounce phase change notifications
	protected float m_fLastPhaseNotification = 0;

	[RplProp(onRplName: "OnOpforPositionChanged")]
    protected vector m_vOpforSpawnPos;
	
	[RplProp()]
    protected vector m_vBluforSpawnPos;
	
	[RplProp()]
    protected vector m_vOpforSpawnDir;
	
	[RplProp()]
    protected vector m_vBluforSpawnDir;

	[RplProp(onRplName: "OnBreakingContactPhaseChanged")]
    protected EBreakingContactPhase m_iBreakingContactPhase = EBreakingContactPhase.LOADING;	

	static float m_iMaxTransmissionDistance = 1000.0;

    protected ref array<GRAD_BC_TransmissionComponent> m_aTransmissionComps = {};

	[RplProp(onRplName: "OnTransmissionIdsChanged")]
	protected ref array<RplId> m_aTransmissionIds = {};

	// Replicated transmission marker data - allows clients to show markers even when entities are out of streaming distance
	// These arrays are parallel: index 0 of each array corresponds to the same transmission point
	[RplProp(onRplName: "OnTransmissionMarkerDataChanged")]
	protected ref array<vector> m_aTransmissionPositions = {};

	[RplProp(onRplName: "OnTransmissionMarkerDataChanged")]
	protected ref array<int> m_aTransmissionStates = {};  // ETransmissionState as int for replication

	[RplProp(onRplName: "OnTransmissionMarkerDataChanged")]
	protected ref array<float> m_aTransmissionProgress = {};
	
	// Replicated radio truck marker data - position and transmitting state for blue pulse marker
	[RplProp(onRplName: "OnTransmissionMarkerDataChanged")]
	protected vector m_vRadioTruckMarkerPos;
	
	[RplProp(onRplName: "OnTransmissionMarkerDataChanged")]
	protected bool m_bRadioTruckTransmitting = false;
	
	protected IEntity m_radioTruck;
	protected IEntity m_westCommandVehicle;
	
	protected bool choosingBluforSpawn;
	
	// not possible to replicate the IEntity itself!
	[RplProp()]
    protected RplId radioTruckRplId;
	[RplProp()]
    protected RplId westCommandVehRplId;
	
	protected bool m_bIsTransmittingCache;
	
	// Track destroyed transmission locations to prevent immediate respawning
	protected ref array<vector> m_aDestroyedTransmissionPositions = {};
	protected ref array<float> m_aDestroyedTransmissionTimes = {};
	protected static float DESTROYED_TRANSMISSION_COOLDOWN = 300000.0; // 5 minutes in milliseconds (GetWorldTime returns milliseconds)
	protected static float DESTROYED_TRANSMISSION_MIN_DISTANCE = 1000.0; // Minimum distance from destroyed transmission
	
	// Track disabled transmission components for re-enabling after cooldown
	protected ref array<GRAD_BC_TransmissionComponent> m_aDisabledTransmissionComponents = {};
	protected ref array<float> m_aDisabledTransmissionTimes = {};
	
	protected PlayerManager m_PlayerManager;

	// Group naming
	protected static const ref array<string> GROUP_NATO_NAMES = {"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel"};
	protected ref map<string, int> m_mGroupNameCounters = new map<string, int>();

	// Endscreen text storage - replicated to clients
	[RplProp()]
	protected string m_sLastEndscreenTitle;
	[RplProp()]
	protected string m_sLastEndscreenSubtitle;
	[RplProp()]
	protected string m_sWinnerSide;
	
	protected PlayerManager GetPlayerManager()
	{
		if (m_PlayerManager == null)
			m_PlayerManager = GetGame().GetPlayerManager();
		
		return m_PlayerManager;
	}
	
	int GetTransmissionDuration() {
		return m_TransmissionDuration;
	}
	
	protected static GRAD_BC_BreakingContactManager m_instance;
	
	static GRAD_BC_BreakingContactManager GetInstance()
	{
    if (!m_instance)
    {
        BaseGameMode gameMode = GetGame().GetGameMode();
        if (!gameMode) {
            Print("Breaking Contact BCM - No game mode found!", LogLevel.WARNING);
            return null;
        }

        m_instance = GRAD_BC_BreakingContactManager.Cast(gameMode.FindComponent(GRAD_BC_BreakingContactManager));
        if (!m_instance) {
            Print("Breaking Contact BCM - Could not find BCM component in game mode!", LogLevel.WARNING);
            return null;
        }

        if (GRAD_BC_BreakingContactManager.IsDebugMode())
        	Print(string.Format("Breaking Contact BCM - m_instance initialized: %1", m_instance), LogLevel.NORMAL);
        
        // Initialize replay manager
        GRAD_BC_ReplayManager replayManager = GRAD_BC_ReplayManager.GetInstance();
        if (!replayManager)
        {
            Print("Breaking Contact BCM - Warning: No replay manager found", LogLevel.WARNING);
        }
    }

    return m_instance;
	}

	// Cached debug flag from mission header - avoids repeated lookups
	protected static int m_iDebugModeCache = -1; // -1 = not cached, 0 = off, 1 = on

	static bool IsDebugMode()
	{
		if (m_iDebugModeCache >= 0)
			return m_iDebugModeCache == 1;

		m_iDebugModeCache = 0;
		MissionHeader header = GetGame().GetMissionHeader();
		if (header)
		{
			GRAD_BC_MissionHeader bcHeader = GRAD_BC_MissionHeader.Cast(header);
			if (bcHeader && bcHeader.IsDebugLogsEnabled())
				m_iDebugModeCache = 1;
		}

		return m_iDebugModeCache == 1;
	}

	// Cached skip-faction-elimination flag from mission header
	protected static int m_iSkipFactionEliminationCache = -1; // -1 = not cached, 0 = off, 1 = on

	static bool IsSkipFactionElimination()
	{
		if (m_iSkipFactionEliminationCache >= 0)
			return m_iSkipFactionEliminationCache == 1;

		m_iSkipFactionEliminationCache = 0;
		MissionHeader header = GetGame().GetMissionHeader();
		if (header)
		{
			GRAD_BC_MissionHeader bcHeader = GRAD_BC_MissionHeader.Cast(header);
			if (bcHeader && bcHeader.IsSkipFactionElimination())
				m_iSkipFactionEliminationCache = 1;
		}

		return m_iSkipFactionEliminationCache == 1;
	}
	
    //------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Breaking Contact BCM -  main init -"), LogLevel.NORMAL);					
		
		// execute only on the server
		if (Replication.IsServer()) {
			m_iNotificationDuration = 10;
			
			// check win conditions every second
			GetGame().GetCallqueue().CallLater(mainLoop, 1000, true);
			GetGame().GetCallqueue().CallLater(setPhaseInitial, 1100, false);
		}
    }
	
	void OnBreakingContactPhaseChanged()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Client: Notifying player of phase change: %1", SCR_Enum.GetEnumName(EBreakingContactPhase, m_iBreakingContactPhase)), LogLevel.NORMAL);
		
		string factionKey = GetPlayerFactionKey();
		
		string title = string.Format("New phase '%1' entered.", SCR_Enum.GetEnumName(EBreakingContactPhase, m_iBreakingContactPhase));
		string message = "Breaking Contact";
		string customSound = "";
		string customSoundGUID = "";
		
		switch (m_iBreakingContactPhase) {
			case EBreakingContactPhase.PREPTIME :
			{
				message = "Preptime still running.";
				break;
			}
			case EBreakingContactPhase.OPFOR :
			{
				message = "Opfor has to spawn now.";
				customSound = "gong_1";
				customSoundGUID = "{0A59B476EEEDFA86}sounds/BC_gong_1.acp";
				break;
			}
			case EBreakingContactPhase.BLUFOR :
			{
				message = "Blufor will spawn now.";
				customSound = "gong_2";
				customSoundGUID = "{93470DEFF30ACB16}sounds/BC_gong_2.acp";
				
				break;
			}
			case EBreakingContactPhase.GAME :
			{
				message = "Blufor spawned, Game begins now.";
				customSound = "gong_3";
				customSoundGUID = "{E44D656707A82466}sounds/BC_gong_3.acp";
				break;
			}
			case EBreakingContactPhase.GAMEOVER :
			{
				message = "Game is over. Replay loading.";
				customSound = "gong_3";
				customSoundGUID = "{E44D656707A82466}sounds/BC_gong_3.acp";
				break;
			}
			case EBreakingContactPhase.GAMEOVERDONE :
			{
				message = string.Format("Replay is over. %1 wins.", m_sWinnerSide);
				customSound = "gong_3";
				customSoundGUID = "{E44D656707A82466}sounds/BC_gong_3.acp";
				break;
			}
		}
		
		const int duration = 10;
		bool isSilent = false;
		
		
		GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
		if (playerComponent == null)
			return;
		
		// no rpc needed here, logs already on client
		// SCR_HintManagerComponent.GetInstance().ShowCustomHint(message, title, duration, isSilent);
		if (customSound != "") {
			vector location = playerComponent.GetOwner().GetOrigin();
			AudioSystem.PlayEvent(customSoundGUID, customSound, location);
		}
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Notifying player about phase %1", m_iBreakingContactPhase), LogLevel.NORMAL);
		
		// close map for opfor
		if (m_iBreakingContactPhase == EBreakingContactPhase.BLUFOR && factionKey == "USSR") {
			playerComponent.ToggleMap(false);
			playerComponent.setChoosingSpawn(false);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD Playercontroller PhaseChange - closing map - opfor done"), LogLevel.NORMAL);
		}
		
		// close map for blufor
		if (m_iBreakingContactPhase == EBreakingContactPhase.GAME && factionKey == "US") {
			playerComponent.ToggleMap(false);
			playerComponent.setChoosingSpawn(false);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD Playercontroller PhaseChange - closing map - blufor done"), LogLevel.NORMAL);
		}
		
		// Retrieve the HUD‐display and call ShowLogo on it:
	    SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
	    if (!hud) {
	        Print("Cannot find SCR_HUDManagerComponent!", LogLevel.ERROR);
	        return;
	    }
	
		// BC LOGO
	    array<BaseInfoDisplay> infoDisplaysLogo = {};
		GetGame().GetPlayerController().GetHUDManagerComponent().GetInfoDisplays(infoDisplaysLogo);
		GRAD_BC_Logo logoDisplay = null;
		foreach (BaseInfoDisplay baseDisp : infoDisplaysLogo)
		{
		    // Try casting *each* BaseInfoDisplay directly to GRAD_BC_Logo
		    GRAD_BC_Logo candidate = GRAD_BC_Logo.Cast(baseDisp);
		    if (candidate)
		    {
		        logoDisplay = candidate;
		        break;
		    }
		}
	    if (!logoDisplay) {
	        Print("InfoDisplay found was not a GRAD_BC_Logo!", LogLevel.ERROR);
	        return;
	    }
		
		// BC GAMESTATE
		array<BaseInfoDisplay> infoDisplaysGameState = {};
		GetGame().GetPlayerController().GetHUDManagerComponent().GetInfoDisplays(infoDisplaysGameState);
		GRAD_BC_Gamestate gamestateDisplay = null;
		foreach (BaseInfoDisplay baseDisp : infoDisplaysGameState)
		{
		    // Try casting *each* BaseInfoDisplay directly to gamestateDisplay
		    GRAD_BC_Gamestate candidate = GRAD_BC_Gamestate.Cast(baseDisp);
		    if (candidate)
		    {
		        gamestateDisplay = candidate;
		        break;
		    }
		}
	    if (!gamestateDisplay) {
	        Print("InfoDisplay found was not a gamestateDisplay!", LogLevel.ERROR);
	        return;
	    }
		
		gamestateDisplay.ShowText(message);
		
	// For ALL JIP players who join as spectators (no controlled entity)
	// Hide UI elements after brief delay to prevent them showing forever
	SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
	if (playerController)
	{
		IEntity controlledEntity = playerController.GetControlledEntity();
		// If no controlled entity, player is in spectator mode
		if (!controlledEntity)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC: JIP spectator detected, scheduling UI hide", LogLevel.NORMAL);
			GetGame().GetCallqueue().CallLater(HideUIForSpectators, 5000, false, logoDisplay, gamestateDisplay);
			return; // Don't show the logos/text for spectators
		}
		
		// show logo for all
		if (m_iBreakingContactPhase == EBreakingContactPhase.GAME) {
			// Debounce to prevent multiple calls
			float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
			if (currentTime - m_fLastPhaseNotification < 2.0)
				return;
			m_fLastPhaseNotification = currentTime;
			
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD Playercontroller PhaseChange - game started, show logo"), LogLevel.NORMAL);
		
		    // Now bump the counter on the server so that all clients show it.
		    // (ShowLogo() itself only increments on Listen/Dedicated, so it is safe
		    // to call here from a dedicated server or listen‐server host.)
		    logoDisplay.ShowLogo();
		}
			
		// show logo for all
		if (m_iBreakingContactPhase == EBreakingContactPhase.GAMEOVER) {
			// Debounce to prevent multiple calls
			float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
			if (currentTime - m_fLastPhaseNotification < 2.0)
				return;
			m_fLastPhaseNotification = currentTime;
			
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD Playercontroller PhaseChange - game started, show logo"), LogLevel.NORMAL);
			logoDisplay.ShowLogo();
		}

	}
	}
	
	//------------------------------------------------------------------------------------------------
	void HideUIForSpectators(GRAD_BC_Logo logoDisplay, GRAD_BC_Gamestate gamestateDisplay)
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC: Hiding UI elements for spectator", LogLevel.NORMAL);
		if (logoDisplay)
		{
			logoDisplay.Show(false, 1.0, EAnimationCurve.EASE_OUT_QUART);
		}
		if (gamestateDisplay)
		{
			gamestateDisplay.Show(false, 1.0, EAnimationCurve.EASE_OUT_QUART);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Called on clients when m_aTransmissionIds is replicated from server
	void OnTransmissionIdsChanged()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Client: Transmission IDs changed, count: %1", m_aTransmissionIds.Count()), LogLevel.NORMAL);

		// Notify all listeners (e.g., map marker manager) that transmission points have changed
		NotifyTransmissionPointListeners();
	}

	//------------------------------------------------------------------------------------------------
	// Called on clients when marker data is replicated from server
	void OnTransmissionMarkerDataChanged()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Client: Transmission marker data changed, count: %1 positions, %2 states",
				m_aTransmissionPositions.Count(), m_aTransmissionStates.Count()), LogLevel.NORMAL);

		// Notify all listeners that marker data has changed
		NotifyTransmissionPointListeners();
	}
	
	//-----
	string GetPlayerFactionKey() {
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
				
		if (!playerController) {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("No playerController found in RpcAsk_Authority_NotifyLocalPlayerOnPhaseChange"), LogLevel.NORMAL);
			return "";
		}
		
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		if (!ch)  {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("SCR_ChimeraCharacter missing in playerController"), LogLevel.NORMAL);
			return "";
		}
		
		string factionKey = ch.GetFactionKey();
		
		return factionKey;
	}
	

	
	//------------------------------------------------------------------------------------------------
	void setPhaseInitial() 
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("setPhaseInitial executed"), LogLevel.NORMAL);
		SetBreakingContactPhase(EBreakingContactPhase.PREPTIME);
	}

    //------------------------------------------------------------------------------------------------
	bool factionEliminated(string factionName)
	{
		return (GetAlivePlayersOfSide(factionName) == 0);
	}
	
	
	//------------------------------------------------------------------------------------------------
	int GetAlivePlayersOfSide(string factionName)
	{
		array<int> alivePlayersOfSide = {};
		
		array<int> allPlayers = {};
		GetPlayerManager().GetPlayers(allPlayers);
		foreach(int playerId : allPlayers)
		{
			// null check bc of null pointer crash
			PlayerController pc = GetPlayerManager().GetPlayerController(playerId);
			if (!pc) continue;
			
			// Game Master is also a player but perhaps with no controlled entity
			IEntity controlled = pc.GetControlledEntity();
			if (!controlled) continue;
			
			// null check bc of null pointer crash
			SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(controlled);
			if (!ch) continue;
			
			CharacterControllerComponent ccc = ch.GetCharacterController();
			if (factionName != ch.GetFactionKey() || ccc.IsDead()) continue;
			
			alivePlayersOfSide.Insert(playerId);
		}
		
		return alivePlayersOfSide.Count();
	}


    //------------------------------------------------------------------------------------------------
	protected void mainLoop()
	
	{

		/* 
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Breaking Contact BCM -  -------------------------------------------------"), LogLevel.NORMAL);
        if (GRAD_BC_BreakingContactManager.IsDebugMode())
        	Print(string.Format("Breaking Contact BCM -  Main Loop Tick ----------------------------------"), LogLevel.NORMAL);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Breaking Contact BCM -  -------------------------------------------------"), LogLevel.NORMAL);
		*/
			
		// set opfor phase as soon as players leave lobby
		PS_GameModeCoop psGameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		EBreakingContactPhase currentPhase = GetBreakingContactPhase();
		
		if (psGameMode && currentPhase == EBreakingContactPhase.PREPTIME)
		{
			if (psGameMode.GetState() == SCR_EGameModeState.GAME)
				SetBreakingContactPhase(EBreakingContactPhase.OPFOR);
		};
		
		if (currentPhase == EBreakingContactPhase.BLUFOR && !choosingBluforSpawn) {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("Breaking Contact - choosingBluforSpawn upcoming"), LogLevel.NORMAL);
			choosingBluforSpawn = true;
			InitiateBluforSpawn();
		};
		
		// Skip mainLoop during LOADING/PREPTIME or if game is over
		if (m_skipWinConditions || currentPhase == EBreakingContactPhase.LOADING || currentPhase == EBreakingContactPhase.PREPTIME)
        {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("Breaking Contact - Game not started yet"), LogLevel.NORMAL);
			return;
		};
		
		// Skip win conditions and marker management during GAMEOVER
		if (currentPhase == EBreakingContactPhase.GAMEOVER)
		{
			return;
		};
		
		ManageMarkers();
		
		// Clean up expired destroyed transmission positions
		CleanupExpiredDestroyedTransmissions();
		
		// Clean up expired disabled transmission components and re-enable them
		CleanupExpiredDisabledTransmissions();
		
        // skip win conditions if active
		if (GameModeStarted() && !(GameModeOver())) {
			CheckWinConditions();
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("Breaking Contact - Checking Win Conditions..."), LogLevel.NORMAL);
		};
	}

	//------------------------------------------------------------------------------------------------
	int GetTransmissionsDoneCount()
	{
		int count = 0;
		
		if (!m_aTransmissionComps )
        	return 0;
		
		foreach (ref GRAD_BC_TransmissionComponent tpc : m_aTransmissionComps )
		{
			if (!tpc)
            	continue;
			
			if (tpc) {			
				if (tpc.GetTransmissionState() == ETransmissionState.DONE) {
					count = count + 1;
				}
			}
		}
		return count;
	}
	
	
	//------------------------------------------------------------------------------------------------
	IEntity GetRadioTruck() 
	{
		return m_radioTruck;
	}
	
	
	//------------------------------------------------------------------------------------------------
	void ManageMarkers() 
	{
        // Print(string.Format("Breaking Contact BCM -  Manage markers..."), LogLevel.NORMAL);
        
        if (!m_radioTruck) {
            // Print(string.Format("Breaking Contact BCM -  No radio truck found"), LogLevel.NORMAL);
            return;
        };
        
        GRAD_BC_RadioTruckComponent RTC = GRAD_BC_RadioTruckComponent.Cast(m_radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
        if (!RTC) {
            // Print(string.Format("Breaking Contact BCM -  No radio truck component found"), LogLevel.NORMAL);
            return;
        }
        
        bool isTransmitting = RTC.GetTransmissionActive();
        bool stateChanged = (m_bIsTransmittingCache != isTransmitting);
        m_bIsTransmittingCache = isTransmitting;
        
        GRAD_BC_TransmissionComponent nearestTPCAntenna = GetNearestTransmissionPoint(m_radioTruck.GetOrigin(), isTransmitting);
        array<GRAD_BC_TransmissionComponent> transmissionPoints = GetTransmissionPoints();
        
        if (!nearestTPCAntenna) {
            // Print(string.Format("Breaking Contact RTC -  No Transmission Point found"), LogLevel.NORMAL);
            return;
        }
        
        GRAD_BC_TransmissionComponent activeTPC = nearestTPCAntenna;
        if (activeTPC) {
            if (stateChanged && isTransmitting) {
                activeTPC.SetTransmissionActive(true);
                
                // Print(string.Format("Breaking Contact RTC - activating active TPC: %1 - Component: %2", nearestTPCAntenna, activeTPC), LogLevel.NORMAL);
                
                foreach (ref GRAD_BC_TransmissionComponent singleTPCAntenna : transmissionPoints)
                {
                    // disable all others
                    if (nearestTPCAntenna != singleTPCAntenna) {
                        GRAD_BC_TransmissionComponent singleTPC = singleTPCAntenna;
                        singleTPC.SetTransmissionActive(false);
                        // Print(string.Format("Breaking Contact RTC -  Disabling Transmission at: %1", singleTPCAntenna), LogLevel.NORMAL);
                    };
                };
                
            } else if (stateChanged && !isTransmitting) {
                foreach (ref GRAD_BC_TransmissionComponent singleTPCAntenna : transmissionPoints)
                {
                    // disable all
                    GRAD_BC_TransmissionComponent singleTPC = singleTPCAntenna;
                    singleTPC.SetTransmissionActive(false);
                    // Print(string.Format("Breaking Contact RTC -  Disabling Transmission at: %1", singleTPCAntenna), LogLevel.NORMAL);
                };
            } else {
            //  Print(string.Format("Breaking Contact RTC - No state change"), LogLevel.NORMAL);
            };
        } else {
            // Print(string.Format("Breaking Contact RTC - No GRAD_BC_TransmissionComponent found"), LogLevel.NORMAL);
        }
		
		// Always update radio truck position for blue pulse marker (truck moves)
		UpdateTransmissionMarkerData();
    }
	
	// --- Transmission Point Event System ---
	private ref array<ref ScriptInvoker> m_TransmissionPointListeners = {};

	void AddTransmissionPointListener(ScriptInvoker listener)
	{
	    if (!m_TransmissionPointListeners) m_TransmissionPointListeners = {};
	    m_TransmissionPointListeners.Insert(listener);
	}
	void RemoveTransmissionPointListener(ScriptInvoker listener)
	{
	    if (!m_TransmissionPointListeners) return;
	    m_TransmissionPointListeners.RemoveItem(listener);
	}
	void NotifyTransmissionPointListeners()
	{
	    if (!m_TransmissionPointListeners) return;
	    foreach (ScriptInvoker inv : m_TransmissionPointListeners) {
	        if (inv) inv.Invoke();
	    }
	}
	
	void RegisterTransmissionComponent(GRAD_BC_TransmissionComponent comp)
	{
		if (!comp) return;

		// Already known?
		if (m_aTransmissionComps.Find(comp) != -1) return;

		m_aTransmissionComps.Insert(comp);

		//clients need to know the list, store the RplId as well
		RplComponent rpl = RplComponent.Cast(comp.GetOwner().FindComponent(RplComponent));
		if (rpl)
		{
			RplId transmissionRplId = Replication.FindId(rpl);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				PrintFormat("BCM - RegisterTransmissionComponent: Adding RplId %1 for entity %2", transmissionRplId, comp.GetOwner());
			m_aTransmissionIds.Insert(transmissionRplId);
		}

		// Update replicated marker data
		UpdateTransmissionMarkerData();
		Replication.BumpMe();
		NotifyTransmissionPointListeners();
	}


void UnregisterTransmissionComponent(GRAD_BC_TransmissionComponent comp)
	{
		// Remove the strong pointer
		int idx = m_aTransmissionComps.Find(comp);
		if (idx != -1)
			m_aTransmissionComps.Remove(idx);

		// Clean the replicated Id list
		for (int i = m_aTransmissionIds.Count() - 1; i >= 0; --i)
		{
			IEntity ent = IEntity.Cast(Replication.FindItem(m_aTransmissionIds[i]));

			// 1) Id is dead  → drop it
			if (!ent)
			{
				m_aTransmissionIds.Remove(i);
				continue;
			}

			// 2) Id belongs to the component being removed → drop it
			if (ent.FindComponent(GRAD_BC_TransmissionComponent) == comp)
				m_aTransmissionIds.Remove(i);
		}

		// Update replicated marker data
		UpdateTransmissionMarkerData();
		Replication.BumpMe();
		NotifyTransmissionPointListeners();
	}
	
	//------------------------------------------------------------------------------------------------
	void RegisterDestroyedTransmissionPosition(vector position)
	{
		if (!m_aDestroyedTransmissionPositions)
			m_aDestroyedTransmissionPositions = {};
		if (!m_aDestroyedTransmissionTimes)
			m_aDestroyedTransmissionTimes = {};
			
		float currentTime = GetGame().GetWorld().GetWorldTime();
		m_aDestroyedTransmissionPositions.Insert(position);
		m_aDestroyedTransmissionTimes.Insert(currentTime);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - Registered destroyed transmission at position %1 at time %2", position.ToString(), currentTime), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void RegisterDisabledTransmissionComponent(GRAD_BC_TransmissionComponent component)
	{
		if (!m_aDisabledTransmissionComponents)
			m_aDisabledTransmissionComponents = {};
		if (!m_aDisabledTransmissionTimes)
			m_aDisabledTransmissionTimes = {};
			
		float currentTime = GetGame().GetWorld().GetWorldTime();
		m_aDisabledTransmissionComponents.Insert(component);
		m_aDisabledTransmissionTimes.Insert(currentTime);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - Registered disabled transmission component at time %1", currentTime), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void CleanupExpiredDestroyedTransmissions()
	{
		if (!m_aDestroyedTransmissionPositions || !m_aDestroyedTransmissionTimes)
			return;
			
		float currentTime = GetGame().GetWorld().GetWorldTime();
		
		for (int i = m_aDestroyedTransmissionTimes.Count() - 1; i >= 0; i--)
		{
			float destroyedTime = m_aDestroyedTransmissionTimes[i];
			float timeDiff = currentTime - destroyedTime;
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("BCM - Cleanup check: current time %1, destroyed time %2, diff %3, cooldown %4", currentTime, destroyedTime, timeDiff, DESTROYED_TRANSMISSION_COOLDOWN), LogLevel.NORMAL);
			
			if (timeDiff > DESTROYED_TRANSMISSION_COOLDOWN)
			{
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("BCM - Removing expired destroyed transmission at %1 (time diff: %2 > %3)", m_aDestroyedTransmissionPositions[i].ToString(), timeDiff, DESTROYED_TRANSMISSION_COOLDOWN), LogLevel.NORMAL);
				m_aDestroyedTransmissionPositions.Remove(i);
				m_aDestroyedTransmissionTimes.Remove(i);
			}
			else
			{
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("BCM - Keeping destroyed transmission at %1 (time diff: %2 < %3)", m_aDestroyedTransmissionPositions[i].ToString(), timeDiff, DESTROYED_TRANSMISSION_COOLDOWN), LogLevel.NORMAL);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CleanupExpiredDisabledTransmissions()
	{
		if (!m_aDisabledTransmissionComponents || !m_aDisabledTransmissionTimes)
			return;
			
		float currentTime = GetGame().GetWorld().GetWorldTime();
		
		for (int i = m_aDisabledTransmissionTimes.Count() - 1; i >= 0; i--)
		{
			float disabledTime = m_aDisabledTransmissionTimes[i];
			float timeDiff = currentTime - disabledTime;
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("BCM - Cleanup check for disabled: current time %1, disabled time %2, diff %3, cooldown %4", currentTime, disabledTime, timeDiff, DESTROYED_TRANSMISSION_COOLDOWN), LogLevel.NORMAL);
			
			if (timeDiff > DESTROYED_TRANSMISSION_COOLDOWN)
			{
				GRAD_BC_TransmissionComponent component = m_aDisabledTransmissionComponents[i];
				if (component)
				{
					if (GRAD_BC_BreakingContactManager.IsDebugMode())
						Print(string.Format("BCM - Re-enabling disabled transmission component (time diff: %1 > %2)", timeDiff, DESTROYED_TRANSMISSION_COOLDOWN), LogLevel.NORMAL);
					
					// Show the antenna model again
					ReShowAntennaModel(component);
					
					// Reset the transmission state to OFF so it can be activated again
					component.SetTransmissionState(ETransmissionState.OFF);
					component.SetTransmissionActive(true);
				}
				
				m_aDisabledTransmissionComponents.Remove(i);
				m_aDisabledTransmissionTimes.Remove(i);
			}
			else
			{
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("BCM - Keeping disabled transmission component (time diff: %1 < %2)", timeDiff, DESTROYED_TRANSMISSION_COOLDOWN), LogLevel.NORMAL);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ReShowAntennaModel(GRAD_BC_TransmissionComponent component)
	{
		if (!component)
		{
			Print("BCM - Cannot re-show antenna: component is null", LogLevel.ERROR);
			return;
		}
		
		IEntity owner = component.GetOwner();
		if (!owner)
		{
			Print("BCM - Cannot re-show antenna: owner entity is null", LogLevel.ERROR);
			return;
		}
		
		// Show the visual representation of the antenna
		owner.SetFlags(EntityFlags.VISIBLE, true);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BCM - Antenna model re-shown", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsNearDestroyedTransmission(vector position)
	{
		if (!m_aDestroyedTransmissionPositions)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("BCM - No destroyed transmission positions tracked", LogLevel.NORMAL);
			return false;
		}
			
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - Checking position %1 against %2 destroyed transmissions", position.ToString(), m_aDestroyedTransmissionPositions.Count()), LogLevel.NORMAL);
		
		foreach (vector destroyedPos : m_aDestroyedTransmissionPositions)
		{
			float distance = vector.Distance(position, destroyedPos);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("BCM - Distance to destroyed transmission at %1: %2m (limit: %3m)", destroyedPos.ToString(), distance, DESTROYED_TRANSMISSION_MIN_DISTANCE), LogLevel.NORMAL);
			if (distance < DESTROYED_TRANSMISSION_MIN_DISTANCE)
			{
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("BCM - Position %1 is too close to destroyed transmission at %2 (distance: %3)", position.ToString(), destroyedPos.ToString(), distance), LogLevel.NORMAL);
				return true;
			}
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BCM - Position is not near any destroyed transmissions", LogLevel.NORMAL);
		return false;
	}

	
	//------------------------------------------------------------------------------------------------
	GRAD_BC_TransmissionComponent GetNearestTransmissionPoint(vector center, bool isTransmitting)
	{
			auto transmissionPoints = GetTransmissionPoints();
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("BCM - GetNearestTransmissionPoint called at %1, isTransmitting: %2, existing points: %3", center.ToString(), isTransmitting, transmissionPoints.Count()), LogLevel.NORMAL);
			
			GRAD_BC_TransmissionComponent closest;

			// if transmission points exist, find out which one is the nearest
			if (transmissionPoints && transmissionPoints.Count() > 0) 
			{
				float bestDist  = m_iMaxTransmissionDistance;

				// Scan through all existing points, find the one closest but still < maxDistance
				foreach (ref GRAD_BC_TransmissionComponent tpc : transmissionPoints)
				{
					if (!tpc) {
						Print(string.Format("Breaking Contact RTC - tpc is null"), LogLevel.ERROR);
						continue;
					}
				
					float distance = vector.Distance(tpc.GetPosition(), center);
					if (GRAD_BC_BreakingContactManager.IsDebugMode())
						Print(string.Format("BCM - Found existing transmission at %1, distance: %2m", tpc.GetPosition().ToString(), distance), LogLevel.NORMAL);

					// check if distance is in reach of radiotruck
					if (distance < bestDist) {
						bestDist = distance;
						closest = tpc;
					}
				}
			
				// If we already found an existing TPC within range, return that:
				if (closest)
				{
					if (GRAD_BC_BreakingContactManager.IsDebugMode())
						Print(string.Format("BCM - Returning existing closest transmission at distance %1", bestDist), LogLevel.NORMAL);
	        		return closest;
				}
			
				if (isTransmitting)
				{
				    // cooldown prevents double spawn
				    if (m_spawnLock > 0)
				    {
				        if (GRAD_BC_BreakingContactManager.IsDebugMode())
				        	Print(string.Format("BCM - Spawn locked, countdown: %1", m_spawnLock), LogLevel.NORMAL);
				        m_spawnLock--;
				        return null;
				    }
				
				    if (GRAD_BC_BreakingContactManager.IsDebugMode())
				    	Print(string.Format("BCM - Checking if can spawn new transmission at %1", center.ToString()), LogLevel.NORMAL);
				    // Check if position is too close to a destroyed transmission
				    if (IsNearDestroyedTransmission(center))
				    {
				        if (GRAD_BC_BreakingContactManager.IsDebugMode())
				        	Print("BCM - Cannot spawn transmission near destroyed transmission site", LogLevel.NORMAL);
				        return null;
				    }
				
				    // ready to drop a new antenna
				    if (GRAD_BC_BreakingContactManager.IsDebugMode())
				    	Print("BCM - Spawning new transmission point", LogLevel.NORMAL);
				    SpawnTransmissionPoint(center);
				    m_spawnLock = 3;        // three main-loop ticks ≈ 3 s
				}
				return null;
			}
			// if no TPC exist, create a new
			if (transmissionPoints.Count() == 0 && isTransmitting)
	    {
	    		if (GRAD_BC_BreakingContactManager.IsDebugMode())
	    			Print(string.Format("BCM - No existing transmissions, checking if can spawn first at %1", center.ToString()), LogLevel.NORMAL);
	    		// Check if position is too close to a destroyed transmission
	    		if (IsNearDestroyedTransmission(center))
	    		{
	    			if (GRAD_BC_BreakingContactManager.IsDebugMode())
	    				Print("BCM - Cannot spawn first transmission near destroyed transmission site", LogLevel.NORMAL);
	    			return null;
	    		}
	    		
	    		if (GRAD_BC_BreakingContactManager.IsDebugMode())
	    			Print("BCM - Spawning first transmission point", LogLevel.NORMAL);
				this.SpawnTransmissionPoint(center);
	        // Print("Breaking Contact RTC - SpawnTransmissionPoint called (no existing points)", LogLevel.NORMAL);
				// By returning null, we wait one frame for the component to initialize. fixing race condition
				return null;
	    }
	    if (GRAD_BC_BreakingContactManager.IsDebugMode())
	    	Print("BCM - No transmission spawning conditions met", LogLevel.NORMAL);
		return null;
	}

	
	//------------------------------------------------------------------------------------------------
	void SpawnSpawnVehicleWest()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - SpawnSpawnVehicleWest called (IsServer: %1)", Replication.IsServer()), LogLevel.NORMAL);
		
		// Validate spawn position is set - NEVER spawn at 0,0,0
		if (m_vBluforSpawnPos == vector.Zero)
		{
			Print("BCM - ERROR: Cannot spawn West vehicle at vector.Zero! Spawn position not set!", LogLevel.ERROR);
			Print(string.Format("BCM - DEBUG: Current phase: %1, m_vOpforSpawnPos: %2", 
				SCR_Enum.GetEnumName(EBreakingContactPhase, m_iBreakingContactPhase), m_vOpforSpawnPos.ToString()), LogLevel.ERROR);
			return;
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - Spawning West vehicle at validated position: %1", m_vBluforSpawnPos.ToString()), LogLevel.NORMAL);
		
		EntitySpawnParams params = new EntitySpawnParams();
        params.Transform[3] = m_vBluforSpawnPos;
		params.TransformMode = ETransformMode.WORLD;
		
        // create spawn vehicle
        Resource ressource = Resource.Load("{36BDCC88B17B3BFA}Prefabs/Vehicles/Wheeled/M923A1/M923A1_command.et");
        m_westCommandVehicle = GetGame().SpawnEntityPrefab(ressource, GetGame().GetWorld(), params);
	    m_westCommandVehicle.SetYawPitchRoll(m_vBluforSpawnDir.VectorToAngles());
		
		if (!m_westCommandVehicle) {
			Print(string.Format("BCM - West Command Truck failed to spawn: %1", params), LogLevel.ERROR);
			return;
		}

		RplComponent rplComponent = RplComponent.Cast(m_westCommandVehicle.FindComponent(RplComponent));
        if (rplComponent)
        {
             westCommandVehRplId = Replication.FindId(rplComponent);
             Replication.BumpMe(); // Replicate the RplId
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("BCM - West Command Truck has rplComponent"), LogLevel.NORMAL);
        }
		
		// Defer physics setup slightly to reduce spawn freeze
		GetGame().GetCallqueue().CallLater(SetVehiclePhysics, 50, false, m_westCommandVehicle);
		
		vector finalPos = m_westCommandVehicle.GetOrigin();
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - West Command Truck spawned successfully at final position: %1 (requested: %2)", finalPos.ToString(), m_vBluforSpawnPos.ToString()), LogLevel.NORMAL);

		// --- BC MOD: Register BLUFOR truck with replay manager ---
		if (m_westCommandVehicle)
		{
			Vehicle vehicle = Vehicle.Cast(m_westCommandVehicle);
			if (vehicle)
			{
				GRAD_BC_ReplayManager replayMgr = GRAD_BC_ReplayManager.GetInstance();
				if (replayMgr)
				{
					replayMgr.RegisterTrackedVehicle(vehicle);
					if (GRAD_BC_BreakingContactManager.IsDebugMode())
						Print("BC Debug - Registered BLUFOR truck with replay manager", LogLevel.NORMAL);
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void SpawnSpawnVehicleEast()
	{		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - SpawnSpawnVehicleEast called (IsServer: %1)", Replication.IsServer()), LogLevel.NORMAL);
		
		// Validate spawn position is set - NEVER spawn at 0,0,0
		if (m_vOpforSpawnPos == vector.Zero)
		{
			Print("BCM - ERROR: Cannot spawn East vehicle at vector.Zero! Spawn position not set!", LogLevel.ERROR);
			Print(string.Format("BCM - DEBUG: Current phase: %1, m_vBluforSpawnPos: %2", 
				SCR_Enum.GetEnumName(EBreakingContactPhase, m_iBreakingContactPhase), m_vBluforSpawnPos.ToString()), LogLevel.ERROR);
			return;
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - Spawning East vehicle at validated position: %1", m_vOpforSpawnPos.ToString()), LogLevel.NORMAL);
		
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		
		params.Transform[3] = m_vOpforSpawnPos + Vector(0, 0.5, 0); // lift it 0.5m
		
        // create radiotruck
        // Resource ressource = Resource.Load("{1BABF6B33DA0AEB6}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_command.et");
		Resource ressource = Resource.Load("{924B84AA3252B2DB}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_command_armored.et");
		
        m_radioTruck = GetGame().SpawnEntityPrefab(ressource, GetGame().GetWorld(), params);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("m_vOpforSpawnDir.VectorToAngles(): %1", m_vOpforSpawnDir.VectorToAngles()), LogLevel.VERBOSE);
			
		m_radioTruck.SetYawPitchRoll(m_vOpforSpawnDir.VectorToAngles());
		
		if (!m_radioTruck) {
			Print(string.Format("BCM - East Radio Truck failed to spawn: %1", params), LogLevel.ERROR);
			return;
		}
		
		RplComponent rplComponent = RplComponent.Cast(m_radioTruck.FindComponent(RplComponent));
        if (rplComponent)
        {
             radioTruckRplId = Replication.FindId(rplComponent);
             Replication.BumpMe(); // Replicate the RplId
			 if (GRAD_BC_BreakingContactManager.IsDebugMode())
			 	Print(string.Format("BCM - East Radio Truck has rplComponent"), LogLevel.NORMAL);
        }
		// Defer physics setup slightly to reduce spawn freeze
		GetGame().GetCallqueue().CallLater(SetVehiclePhysics, 50, false, m_radioTruck);
	
		vector finalPos = m_radioTruck.GetOrigin();
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - East Radio Truck spawned successfully at final position: %1 (requested: %2)", finalPos.ToString(), m_vOpforSpawnPos.ToString()), LogLevel.NORMAL);

		// --- BC MOD: Register OPFOR radio truck with replay manager ---
		if (m_radioTruck)
		{
			Vehicle vehicle = Vehicle.Cast(m_radioTruck);
			if (vehicle)
			{
				GRAD_BC_ReplayManager replayMgr = GRAD_BC_ReplayManager.GetInstance();
				if (replayMgr)
				{
					replayMgr.RegisterTrackedVehicle(vehicle);
					if (GRAD_BC_BreakingContactManager.IsDebugMode())
						Print("BC Debug - Registered OPFOR radio truck with replay manager", LogLevel.NORMAL);
				}
			}
		}
	}
	
    //------------------------------------------------------------------------------------------------
	void CheckWinConditions()
	{
		// Prevent win conditions if spawns not properly set up (timing/race condition protection)
		// This can happen when phases advance too quickly in local playtest before spawn markers are processed
		if (m_vOpforSpawnPos == vector.Zero || m_vBluforSpawnPos == vector.Zero)
		{
			Print(string.Format("Breaking Contact - Spawn positions not set yet (OPFOR: %1, BLUFOR: %2), skipping win conditions", 
				m_vOpforSpawnPos != vector.Zero, m_vBluforSpawnPos != vector.Zero), LogLevel.WARNING);
			return;
		}
		
		// skip faction elimination when configured in mission header (for solo testing on dedicated server)
		// also skip in singleplayer workbench testing
		bool skipElimination = GRAD_BC_BreakingContactManager.IsSkipFactionElimination();
		#ifdef WORKBENCH
			skipElimination = true;
		#endif

		bool bluforEliminated = factionEliminated("US") && !skipElimination;
		bool opforEliminated = factionEliminated("USSR") && !skipElimination;
        bool isOver;

		bool finishedAllTransmissions = (GetTransmissionsDoneCount() >= m_iTransmissionCount);
		
		// Validate transmission count - if set to 0, transmissions are disabled/infinite
		// Prevent instant game over when transmission count is misconfigured
		if (m_iTransmissionCount <= 0)
		{
			finishedAllTransmissions = false;
			if (m_iTransmissionCount == 0)
			{
				Print("Breaking Contact - WARNING: Transmission count is 0! Transmissions disabled. Set to 1-3 in gamemode prefab.", LogLevel.WARNING);
			}
		}
		
		// Debug logging for transmission tracking
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Breaking Contact - Win condition check: transmissions done=%1/%2, BLUFOR eliminated=%3, OPFOR eliminated=%4", 
				GetTransmissionsDoneCount(), m_iTransmissionCount, bluforEliminated, opforEliminated), LogLevel.VERBOSE);
		
		// Check for radio truck destruction first (highest priority)
		if (m_bRadioTruckDestroyed) {
			isOver = true;
			// m_sWinnerSide already set in SetRadioTruckDestroyed
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("Breaking Contact - Radio truck destroyed, winner: %1", m_sWinnerSide), LogLevel.NORMAL);
		}
		else if (bluforEliminated) {
			isOver = true;
			m_sWinnerSide = "opfor";
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("Breaking Contact - Blufor eliminated"), LogLevel.NORMAL);
		}
		else if (finishedAllTransmissions) {
			isOver = true;
			m_sWinnerSide = "opfor";
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("Breaking Contact - All transmissions done"), LogLevel.NORMAL);
		}
		else if (opforEliminated) {
			isOver = true;
			m_sWinnerSide = "blufor";
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("Breaking Contact - Opfor eliminated"), LogLevel.NORMAL);
		}
		else if (m_bluforCaptured) {
			isOver = true;
			m_sWinnerSide = "blufor";
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("Breaking Contact - Blufor captured radio truck"), LogLevel.NORMAL);
		}
		
		// needs to be on last position as would risk to be overwritten
		if (bluforEliminated && opforEliminated) {
			isOver = true;
			m_sWinnerSide = "draw";
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("Breaking Contact - Both sides eliminated"), LogLevel.NORMAL);
		}
		
		if (isOver) {
            // Check if we are already skipping (already scheduled)
            if (m_skipWinConditions || GameModeOver())
            {
                return; 
            }
            
            // Lock it immediately so the next mainLoop tick (1 sec later) exits early
            m_skipWinConditions = true;

            // Schedule game over
            GetGame().GetCallqueue().CallLater(SetBreakingContactPhase, 5000, false, EBreakingContactPhase.GAMEOVER);
        }
	}
	
	void SetBluforWin()
	{
        if (m_skipWinConditions || GameModeOver())
            return; 
            
        // Lock immediately
        m_skipWinConditions = true;
			
		m_sWinnerSide = "blufor";
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Breaking Contact - BLUFOR wins: Radio truck disabled"), LogLevel.NORMAL);
		
		// Notify all players about the radio truck being disabled
		NotifyAllPlayersRadioTruckDisabled();
		
		// Immediately end the game
		GetGame().GetCallqueue().CallLater(SetBreakingContactPhase, 5000, false, EBreakingContactPhase.GAMEOVER);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetRadioTruckDestroyed(string destroyerFaction)
	{
        if (m_skipWinConditions || GameModeOver())
            return; 
        
        // Lock immediately
        m_skipWinConditions = true;
		
		m_bRadioTruckDestroyed = true;
		m_sRadioTruckDestroyerFaction = destroyerFaction;
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Breaking Contact - Radio truck destroyed by faction: %1", destroyerFaction), LogLevel.NORMAL);
		
		// The faction that destroyed the radio truck loses
		if (destroyerFaction == "US")
		{
			m_sWinnerSide = "opfor";
			NotifyAllPlayersRadioTruckDestroyed("BLUFOR destroyed the radio truck! OPFOR wins!");
		}
		else if (destroyerFaction == "USSR")
		{
			m_sWinnerSide = "blufor";
			NotifyAllPlayersRadioTruckDestroyed("OPFOR destroyed the radio truck! BLUFOR wins!");
		}
		else if (destroyerFaction == "DISABLED")
		{
			// Truck disabled/immobilized - BLUFOR wins
			m_sWinnerSide = "blufor";
			NotifyAllPlayersRadioTruckDestroyed("Radio truck disabled! BLUFOR wins!");
		}
		else
		{
			// Unknown or neutral destruction - treat as draw or no effect
			Print(string.Format("Breaking Contact - Radio truck destroyed by unknown faction: %1", destroyerFaction), LogLevel.WARNING);
			return;
		}
		
		// Immediately end the game
		GetGame().GetCallqueue().CallLater(SetBreakingContactPhase, 5000, false, EBreakingContactPhase.GAMEOVER);
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsRadioTruckDestroyed()
	{
		return m_bRadioTruckDestroyed;
	}
	
	//------------------------------------------------------------------------------------------------
	string GetRadioTruckDestroyerFaction()
	{
		return m_sRadioTruckDestroyerFaction;
	}
	
	//------------------------------------------------------------------------------------------------
	void NotifyAllPlayersRadioTruckDisabled()
	{
		array<int> playerIds = {};
		GetPlayerManager().GetAllPlayers(playerIds);

		const string title = "Breaking Contact";
		const string message = "BLUFOR has disabled the OPFOR radio truck! BLUFOR wins!";
		int duration = m_iNotificationDuration;
		bool isSilent = false;
		
		foreach (int playerId : playerIds)
		{
			IEntity playerEntity = GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!playerEntity) continue;
			
			// Get player component to show hint
			GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.Cast(playerEntity.FindComponent(GRAD_PlayerComponent));
			if (playerComponent)
			{
				playerComponent.ShowHint(message, title, duration, isSilent);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void NotifyAllPlayersRadioTruckDestroyed(string message)
	{
		array<int> playerIds = {};
		GetPlayerManager().GetAllPlayers(playerIds);

		const string title = "Breaking Contact";
		int duration = m_iNotificationDuration;
		bool isSilent = false;
		
		foreach (int playerId : playerIds)
		{
			IEntity playerEntity = GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!playerEntity) continue;
			
			// Get player component to show hint
			GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.Cast(playerEntity.FindComponent(GRAD_PlayerComponent));
			if (playerComponent)
			{
				playerComponent.ShowHint(message, title, duration, isSilent);
			}
		}
	}

    //------------------------------------------------------------------------------------------------
    protected bool GameModeStarted()
    {
        return (m_iBreakingContactPhase == EBreakingContactPhase.GAME);
    }


    //------------------------------------------------------------------------------------------------
    protected bool GameModeOver()
    {
        return (m_iBreakingContactPhase == EBreakingContactPhase.GAMEOVER);
    }


	//------------------------------------------------------------------------------------------------
	// Show the game over screen with faction-specific content
	void ShowGameOverScreen()
	{
		if (!Replication.IsServer())
		{
			Print("BCM - ShowGameOverScreen called on client, ignoring", LogLevel.WARNING);
			return;
		}
			
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BCM - Showing game over screen", LogLevel.NORMAL);
		
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			Print("BCM - Cannot show endscreen - no game mode found", LogLevel.ERROR);
			return;
		}
		
		// Determine title and description based on victory condition
		string title = "";
		string subtitle = "";
		string description = "";
		
		// Get first player's faction to determine which message to show
		array<int> playerIds = {};
		GetPlayerManager().GetAllPlayers(playerIds);
		string playerFactionKey = "USSR"; // default
		
		if (playerIds.Count() > 0)
		{
			Faction playerFaction = SCR_FactionManager.SGetPlayerFaction(playerIds[0]);
			if (playerFaction)
				playerFactionKey = playerFaction.GetFactionKey();
		}
		
		// Determine if player's faction won
		bool playerWon = false;
		if ((m_sWinnerSide == "opfor" && playerFactionKey == "USSR") ||
		    (m_sWinnerSide == "blufor" && playerFactionKey == "US"))
		{
			playerWon = true;
		}
		
		// Set title based on win/loss
		if (m_sWinnerSide == "draw")
		{
			title = "#AR-GameOver_Title_Draw";
		}
		else if (playerWon)
		{
			title = "#AR-GameOver_Title_Victory";
		}
		else
		{
			title = "#AR-GameOver_Title_Defeat";
		}
		
		// Set subtitle and description based on how the game ended
		if (m_bRadioTruckDestroyed)
		{
			subtitle = "Radio Truck Destroyed";
			string destroyerFaction = m_sRadioTruckDestroyerFaction;
			
			if (destroyerFaction == "USSR")
			{
				if (playerFactionKey == "USSR")
					description = "Your team accidentally destroyed the radio truck. BLUFOR wins by default.";
				else
					description = "OPFOR accidentally destroyed their own radio truck. Your team wins by default.";
			}
			else if (destroyerFaction == "US")
			{
				if (playerFactionKey == "US")
					description = "Your team destroyed the radio truck. OPFOR wins by default.";
				else
					description = "BLUFOR destroyed the radio truck. Your team wins by default.";
			}
		}
		else if (m_bluforCaptured)
		{
			subtitle = "Radio Truck Disabled";
			if (playerFactionKey == "US")
				description = "Your team successfully disabled the OPFOR radio truck before all transmissions were completed.";
			else
				description = "BLUFOR disabled your radio truck before all transmissions were completed.";
		}
		else if (GetTransmissionsDoneCount() >= m_iTransmissionCount)
		{
			subtitle = "All Transmissions Completed";
			if (playerFactionKey == "USSR")
				description = string.Format("Your team successfully completed all %1 transmissions.", m_iTransmissionCount);
			else
				description = string.Format("OPFOR completed all %1 transmissions before you could stop them.", m_iTransmissionCount);
		}
		else if (factionEliminated("US"))
		{
			subtitle = "Enemy Eliminated";
			if (playerFactionKey == "USSR")
				description = "All BLUFOR forces have been eliminated.";
			else
				description = "All your forces have been eliminated.";
		}
		else if (factionEliminated("USSR"))
		{
			subtitle = "Enemy Eliminated";
			if (playerFactionKey == "US")
				description = "All OPFOR forces have been eliminated.";
			else
				description = "All your forces have been eliminated.";
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - Endscreen: Won=%1, Title=%2, Subtitle=%3", playerWon, title, subtitle), LogLevel.NORMAL);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - Endscreen: Won=%1, Title=%2, Subtitle=%3", playerWon, title, subtitle), LogLevel.NORMAL);
		
		// Store endscreen text for retrieval
		m_sLastEndscreenTitle = title;
		m_sLastEndscreenSubtitle = subtitle;
		Replication.BumpMe(); // Replicate endscreen data to clients
		
		// Determine which game over screen to show based on win condition
		EGameOverTypes gameOverType = EGameOverTypes.END1; // Default
		
		if (m_bRadioTruckDestroyed)
		{
			if (m_sRadioTruckDestroyerFaction == "USSR")
				gameOverType = EGameOverTypes.END5; // Blufor wins - Opfor destroyed the truck
			else if (m_sRadioTruckDestroyerFaction == "US")
				gameOverType = EGameOverTypes.END4; // Opfor wins - Blufor destroyed the truck
		}
		else if (m_bluforCaptured)
		{
			gameOverType = EGameOverTypes.END2; // Blufor wins by disabling the radio truck
		}
		else if (GetTransmissionsDoneCount() >= m_iTransmissionCount)
		{
			gameOverType = EGameOverTypes.END6; // Opfor wins by completing all transmissions
		}
		else if (factionEliminated("US"))
		{
			gameOverType = EGameOverTypes.END3; // Opfor wins by elimination
		}
		else if (factionEliminated("USSR"))
		{
			gameOverType = EGameOverTypes.END1; // Blufor wins by elimination
		}
		
		// Create endscreen data
		SCR_GameModeEndData endData = SCR_GameModeEndData.CreateSimple(gameOverType);
		
		gameMode.EndGameMode(endData);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BCM - Game ended, endscreen should show", LogLevel.NORMAL);
	}

    //------------------------------------------------------------------------------------------------
	EBreakingContactPhase GetBreakingContactPhase()
	{
		// Print(string.Format("GetBreakingContactPhase - Phase '%1'", m_iBreakingContactPhase, LogLevel.NORMAL));
		return m_iBreakingContactPhase;
	}
	
	//------------------------------------------------------------------------------------------------
	void Rpc_RequestInitiateOpforSpawn()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("Breaking Contact - Rpc_RequestInitiateOpforSpawn"), LogLevel.NORMAL);
		SpawnSpawnVehicleEast();
	    TeleportFactionToMapPos("USSR");
		SetBreakingContactPhase(EBreakingContactPhase.BLUFOR);
	}
	
	 //------------------------------------------------------------------------------------------------
	// called by InitiateOpforSpawn - server side
	void SetBreakingContactPhase(EBreakingContactPhase phase)
    {
        m_iBreakingContactPhase = phase;
        Replication.BumpMe();
		
		// Interrupt all running transmissions when game ends
		if (phase == EBreakingContactPhase.GAMEOVER && Replication.IsServer())
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("BCM: GAMEOVER phase - interrupting all running transmissions", LogLevel.NORMAL);
			array<GRAD_BC_TransmissionComponent> transmissions = GetTransmissionPoints();
			if (transmissions)
			{
				foreach (GRAD_BC_TransmissionComponent tpc : transmissions)
				{
					if (tpc && tpc.GetTransmissionState() == ETransmissionState.TRANSMITTING)
					{
						if (GRAD_BC_BreakingContactManager.IsDebugMode())
							Print(string.Format("BCM: Interrupting transmission at %1", tpc.GetPosition()), LogLevel.NORMAL);
						tpc.SetTransmissionActive(false);
					}
				}
			}
		}
		
		OnBreakingContactPhaseChanged(); // call on server too for debug in SP test

        if (GRAD_BC_BreakingContactManager.IsDebugMode())
        	Print(string.Format("Breaking Contact - Phase %1 entered - %2 -", SCR_Enum.GetEnumName(EBreakingContactPhase, phase), phase), LogLevel.NORMAL);
    }	
	
	//------------------------------------------------------------------------------------------------
	void InitiateBluforSpawn() 
	{
		SpawnSpawnVehicleWest();
		SetBreakingContactPhase(EBreakingContactPhase.GAME);
        TeleportFactionToMapPos("US");
	}

	//------------------------------------------------------------------------------------------------
	void SpawnTransmissionPoint(vector center)
	{
		// Only server can spawn replicated entities
		if (!Replication.IsServer())
		{
			Print("BCM - SpawnTransmissionPoint called on client, ignoring (server-side only)", LogLevel.WARNING);
			return;
		}

		bool spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(center, center, 15, 3);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - Transmission Point position search starts at %1", center), LogLevel.NORMAL);
		
		if (!spawnEmpty) {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("BCM - Transmission Point position dangerous, didnt find a free spot!"), LogLevel.NORMAL);
		}
		
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3]  = center;
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - Transmission Point position transformed to %1", center), LogLevel.NORMAL);
		
		Resource ressource = Resource.Load("{55B73CF1EE914E07}Prefabs/Props/Military/Compositions/USSR/Antenna_02_USSR.et");
        IEntity transmissionPoint = GetGame().SpawnEntityPrefab(ressource, GetGame().GetWorld(), params);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - Transmission Point spawned: %1 at %2", transmissionPoint, center), LogLevel.NORMAL);
		
		// Force replication to clients
		RplComponent rpl = RplComponent.Cast(transmissionPoint.FindComponent(RplComponent));
		if (rpl) {
			Replication.BumpMe();
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("BCM - Transmission Point entity marked for replication to clients", LogLevel.NORMAL);
		}
		
		GRAD_BC_TransmissionComponent tpc = GRAD_BC_TransmissionComponent.Cast(transmissionPoint.FindComponent(GRAD_BC_TransmissionComponent));
	    if (tpc) {
	        // 1. Manually set its position to bypass any EOnInit timing issues.
	        tpc.SetPosition(center);
	        
	        // 2. Register it directly. 'this' is guaranteed to be a valid manager instance here.
	        this.RegisterTransmissionComponent(tpc);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("BCM - Transmission Point Component registered and setpossed"), LogLevel.NORMAL);
	    } else {
			Print(string.Format("BCM - Transmission Point Component not found!"), LogLevel.ERROR);
		}
		
	}

	// Method to find the closest road position
	protected vector FindSpawnPointOnRoad(vector position)
	{
		// Progressive search to prioritize closer roads
		// 20m -> 50m -> 100m -> 200m
		array<int> searchRadii = {20, 50, 100, 200};
		
		foreach (int radius : searchRadii)
		{
			array<vector> roadPoints = GetNearestRoadPos(position, radius);
			
			if (roadPoints && !roadPoints.IsEmpty())
			{
				// Find the closest point from the road points
				vector closestPos = roadPoints[0];
				float minDistance = vector.Distance(position, closestPos);
				
				for (int i = 1, count = roadPoints.Count(); i < count; i++)
				{
					float distance = vector.Distance(position, roadPoints[i]);
					if (distance < minDistance)
					{
						minDistance = distance;
						closestPos = roadPoints[i];
					}
				}
				
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("BCM - Found road position %1m from original (Search Radius: %2m)", minDistance, radius), LogLevel.VERBOSE);
				return closestPos;
			}
		}
		
		Print(string.Format("BCM - No road found near %1 (Max Radius: 200m), using original position", position.ToString()), LogLevel.WARNING);
		return position; // Use player-selected position as absolute fallback
	}
	// Method to get road points using RoadNetworkManager
	protected array<vector> GetNearestRoadPos(vector center, int searchRadius)
	{
	    vector mins, maxs;
	    GetGame().GetWorldEntity().GetWorldBounds(mins, maxs);
	    vector worldCenter = vector.Lerp(mins, maxs, 0.5);
	    
	    auto worldCenterArray = new array<vector>();
	    worldCenterArray.Insert(worldCenter);
	    worldCenterArray.Insert(worldCenter);
	    
	    const float halfSize = searchRadius; // Adjust as needed for your AABB size
	    vector aabbMin = center - Vector(halfSize, halfSize, halfSize); 
	    vector aabbMax = center + Vector(halfSize, halfSize, halfSize); 
	
	    SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
	    if (!aiWorld)
	        return worldCenterArray; // Fallback if AIWorld is not available
	        
	    RoadNetworkManager roadNetworkManager = aiWorld.GetRoadNetworkManager();
	    if (!roadNetworkManager)
	        return worldCenterArray; // Fallback if RoadNetworkManager is not available
	    
	    auto outPoints = new array<vector>();
	    auto emptyRoads = new array<BaseRoad>();
	    int result = roadNetworkManager.GetRoadsInAABB(aabbMin, aabbMax, emptyRoads);
	    
	    if (result > 0)
	    {
	        BaseRoad emptyRoad = emptyRoads[0];
	        emptyRoad.GetPoints(outPoints);
	        return outPoints;
	    }
	    
		// in case we dont find return null
		return null;
	}

	// Find the closest point on a road and the road's heading direction at that point
	// Uses consecutive ordered road points for accurate direction (not second-closest point)
	protected bool GetRoadPointAndDir(vector targetPos, float searchRadius, out vector outPos, out vector outDir)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return false;

		RoadNetworkManager roadMgr = aiWorld.GetRoadNetworkManager();
		if (!roadMgr)
			return false;

		vector aabbMin = targetPos - Vector(searchRadius, searchRadius, searchRadius);
		vector aabbMax = targetPos + Vector(searchRadius, searchRadius, searchRadius);

		array<BaseRoad> roads = {};
		int roadCount = roadMgr.GetRoadsInAABB(aabbMin, aabbMax, roads);

		if (roadCount <= 0 || roads.IsEmpty())
			return false;

		// Search all roads to find the globally closest point
		float globalClosestDist = float.MAX;
		int globalClosestIdx = 0;
		array<vector> globalClosestPoints;

		foreach (BaseRoad road : roads)
		{
			array<vector> points = {};
			road.GetPoints(points);

			if (points.Count() < 2)
				continue;

			for (int i = 0; i < points.Count(); i++)
			{
				float d = vector.DistanceSq(targetPos, points[i]);
				if (d < globalClosestDist)
				{
					globalClosestDist = d;
					globalClosestIdx = i;
					globalClosestPoints = points;
				}
			}
		}

		if (!globalClosestPoints || globalClosestPoints.Count() < 2)
			return false;

		outPos = globalClosestPoints[globalClosestIdx];

		// Direction from consecutive road points: look forward, or backward at end
		if (globalClosestIdx < globalClosestPoints.Count() - 1)
			outDir = vector.Direction(globalClosestPoints[globalClosestIdx], globalClosestPoints[globalClosestIdx + 1]);
		else
			outDir = vector.Direction(globalClosestPoints[globalClosestIdx - 1], globalClosestPoints[globalClosestIdx]);

		outDir.Normalize();

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - GetRoadPointAndDir: pos=%1, dir=%2 (idx %3/%4)",
				outPos.ToString(), outDir.ToString(), globalClosestIdx, globalClosestPoints.Count()), LogLevel.VERBOSE);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	array<GRAD_BC_TransmissionComponent> GetTransmissionPoints()
	{
		// On server, return the direct array
		if (Replication.IsServer())
		{
			return m_aTransmissionComps;
		}

		// On clients, resolve from replicated RplIds
		array<GRAD_BC_TransmissionComponent> clientComps = new array<GRAD_BC_TransmissionComponent>();

		foreach (RplId rplId : m_aTransmissionIds)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				PrintFormat("BCM - GetTransmissionPoints (client): Trying to resolve RplId %1", rplId);
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(rplId));
			if (!rpl) {
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					PrintFormat("BCM - GetTransmissionPoints (client): FindItem returned null for RplId %1", rplId);
				continue;
			}

			IEntity entity = rpl.GetEntity();
			if (!entity) {
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					PrintFormat("BCM - GetTransmissionPoints (client): RplComponent has no entity for RplId %1", rplId);
				continue;
			}

			GRAD_BC_TransmissionComponent comp = GRAD_BC_TransmissionComponent.Cast(entity.FindComponent(GRAD_BC_TransmissionComponent));
			if (comp)
				clientComps.Insert(comp);
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("BCM - GetTransmissionPoints (client): Found %1 transmission points from %2 RplIds", clientComps.Count(), m_aTransmissionIds.Count());
		return clientComps;
	}

	//------------------------------------------------------------------------------------------------
	// SERVER ONLY: Update the replicated marker data arrays from current transmission components
	// This should be called whenever transmission state changes
	void UpdateTransmissionMarkerData()
	{
		if (!Replication.IsServer())
			return;

		// Ensure arrays exist
		if (!m_aTransmissionPositions)
			m_aTransmissionPositions = {};
		if (!m_aTransmissionStates)
			m_aTransmissionStates = {};
		if (!m_aTransmissionProgress)
			m_aTransmissionProgress = {};

		// Clear and rebuild
		m_aTransmissionPositions.Clear();
		m_aTransmissionStates.Clear();
		m_aTransmissionProgress.Clear();

		foreach (GRAD_BC_TransmissionComponent comp : m_aTransmissionComps)
		{
			if (!comp)
				continue;

			m_aTransmissionPositions.Insert(comp.GetPosition());
			m_aTransmissionStates.Insert(comp.GetTransmissionState());  // ETransmissionState is an enum, stores as int
			m_aTransmissionProgress.Insert(comp.GetTransmissionDuration());
		}

		// Update radio truck marker data
		if (m_radioTruck)
		{
			m_vRadioTruckMarkerPos = m_radioTruck.GetOrigin();
			GRAD_BC_RadioTruckComponent rtc = GRAD_BC_RadioTruckComponent.Cast(m_radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
			if (rtc)
				m_bRadioTruckTransmitting = rtc.GetTransmissionActive();
			else
				m_bRadioTruckTransmitting = false;
		}
		else
		{
			m_bRadioTruckTransmitting = false;
		}

		// Replicate the updated arrays to clients
		Replication.BumpMe();

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("BCM - UpdateTransmissionMarkerData: Updated %1 markers", m_aTransmissionPositions.Count());
	}

	//------------------------------------------------------------------------------------------------
	// Get transmission marker data for map display - works on both server and client
	// Returns the count of markers, and fills the output arrays with data
	// This method uses replicated data that works regardless of entity streaming distance
	int GetTransmissionMarkerData(out array<vector> outPositions, out array<ETransmissionState> outStates, out array<float> outProgress)
	{
		// On server, read directly from components for most up-to-date data
		if (Replication.IsServer())
		{
			outPositions = new array<vector>();
			outStates = new array<ETransmissionState>();
			outProgress = new array<float>();

			foreach (GRAD_BC_TransmissionComponent comp : m_aTransmissionComps)
			{
				if (!comp)
					continue;
				outPositions.Insert(comp.GetPosition());
				outStates.Insert(comp.GetTransmissionState());
				outProgress.Insert(comp.GetTransmissionDuration());
			}
			return outPositions.Count();
		}

		// On clients, use replicated data arrays (works even when entities are out of streaming distance)
		outPositions = new array<vector>();
		outStates = new array<ETransmissionState>();
		outProgress = new array<float>();

		if (!m_aTransmissionPositions || !m_aTransmissionStates || !m_aTransmissionProgress)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				PrintFormat("BCM - GetTransmissionMarkerData (client): Replicated arrays not initialized yet");
			return 0;
		}

		int count = Math.Min(m_aTransmissionPositions.Count(),
					Math.Min(m_aTransmissionStates.Count(), m_aTransmissionProgress.Count()));

		for (int i = 0; i < count; i++)
		{
			outPositions.Insert(m_aTransmissionPositions[i]);
			outStates.Insert(m_aTransmissionStates[i]);  // Cast int back to enum
			outProgress.Insert(m_aTransmissionProgress[i]);
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("BCM - GetTransmissionMarkerData (client): Returning %1 markers from replicated data", count);
		return count;
	}

	//------------------------------------------------------------------------------------------------
	// Get radio truck marker data for map display - works on both server and client
	// Returns true if the radio truck is transmitting and fills outPosition with its location
	bool GetRadioTruckMarkerData(out vector outPosition, out bool outTransmitting)
	{
		if (Replication.IsServer())
		{
			if (m_radioTruck)
			{
				outPosition = m_radioTruck.GetOrigin();
				GRAD_BC_RadioTruckComponent rtc = GRAD_BC_RadioTruckComponent.Cast(m_radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
				if (rtc)
					outTransmitting = rtc.GetTransmissionActive();
				else
					outTransmitting = false;
				return true;
			}
			outTransmitting = false;
			return false;
		}

		// On clients, use replicated data
		outPosition = m_vRadioTruckMarkerPos;
		outTransmitting = m_bRadioTruckTransmitting;
		return (outPosition != vector.Zero);
	}


    //------------------------------------------------------------------------------------------------
    protected array<vector> findBluforPosition(vector opforPosition) {
	    bool foundPositionOnLand = false;
	    int loopCount = 0;
	    vector roadPosition;
	    
	    // Define min and max distance bounds
	    // Start with strict adherence to the configured distance (default 3000m)
	    float minDistance = m_iBluforSpawnDistance - 200;
	    float maxDistance = m_iBluforSpawnDistance + 200;
	    
	    // Max 100 iterations to find valid position
	    while (!foundPositionOnLand && loopCount < 100) {
	        loopCount++;
	        
	        // If we struggle to find a position, relax the constraints
	        // User requirement: allow down to 2km or further away if 3km isn't possible
	        if (loopCount == 25)
	        {
	            minDistance = 2000.0; // Minimum 2km
	            maxDistance = m_iBluforSpawnDistance + 2000.0; // Allow much further away
	            if (GRAD_BC_BreakingContactManager.IsDebugMode())
	            	Print(string.Format("BCM - Relaxing BLUFOR spawn search radius: %1m - %2m", minDistance, maxDistance), LogLevel.NORMAL);
	        }
	        
	        // Generate random direction and distance
	        int degrees = Math.RandomIntInclusive(0, 360);
	        float randomDistance = Math.RandomFloatInclusive(minDistance, maxDistance);
	        
	        // Get initial point on circle
	        vector candidatePos = GetPointOnCircle(opforPosition, randomDistance, degrees);
	        
	        // Find road position + direction using consecutive road points
	        vector roadDir;
	        if (!GetRoadPointAndDir(candidatePos, 500, roadPosition, roadDir))
	            continue;

	        // Quick checks first (cheaper operations)
	        float distanceToOpfor = vector.Distance(roadPosition, opforPosition);

	        // Ensure we respect the minimum distance
	        if (distanceToOpfor < minDistance)
	            continue;

	        // Water check (more expensive)
	        if (SurfaceIsWater(roadPosition))
	            continue;

	        vector finalSpawnPos = roadPosition;

	        // Basic collision check using trace
	        bool hasCollision = false;
	        if (GetGame().GetWorld())
	        {
	            autoptr TraceParam trace = new TraceParam();
	            trace.Start = finalSpawnPos + Vector(0, 5, 0);
	            trace.End = finalSpawnPos + Vector(0, -2, 0);
	            trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
	            trace.LayerMask = EPhysicsLayerPresets.Projectile;

	            float traceResult = GetGame().GetWorld().TraceMove(trace, null);
	            hasCollision = (traceResult < 0.95);
	        }

	        if (!hasCollision)
	        {
	            if (GRAD_BC_BreakingContactManager.IsDebugMode())
	            	Print(string.Format("BCM - Found valid Blufor position after %1 loops - distance: %2m",
		                loopCount, distanceToOpfor), LogLevel.NORMAL);

	            array<vector> output = new array<vector>();
	            output.Insert(finalSpawnPos);
	            output.Insert(roadDir);

	            return output;
	        }
	    }
	    
	    // Failed to find valid position after max iterations
	    Print(string.Format("BCM - findBluforPosition failed to find valid road position after %1 loops", loopCount), LogLevel.ERROR);
	    
			// Fallback to original position if no valid road position found
			int fallbackAttempts = 0;
			vector fallbackPos = vector.Zero;
			while (fallbackAttempts < 20)
			{
				fallbackPos = GetPointOnCircle(opforPosition, m_iBluforSpawnDistance, Math.RandomIntInclusive(0, 359));
				if (!SurfaceIsWater(fallbackPos))
					break;
				fallbackAttempts++;
			}

			// If all attempts fail, use last fallbackPos (may be in water, but we tried)
			vector fallbackDir = Vector(1, 0, 0);
			array<vector> output = new array<vector>();
			output.Insert(fallbackPos);
			output.Insert(fallbackDir);
			Print(string.Format("BCM - Fallback BLUFOR spawn position after %1 attempts: %2 (in water: %3)", fallbackAttempts, fallbackPos.ToString(), SurfaceIsWater(fallbackPos)), LogLevel.WARNING);
			return output;
	}
	
	// helper to find point on circle
	protected vector GetPointOnCircle(vector center, float radius, float degrees)
	{
	    // Convert degrees to radians
	    float radians = degrees * Math.DEG2RAD;
	    
	    // Calculate offsets using cosine and sine (radians)
	    float offsetX = radius * Math.Cos(radians - 0.5 * Math.PI);
	    float offsetZ = radius * Math.Sin(radians - 0.5 * Math.PI);
	    
	    float terrainX = center[0] + offsetX;
		float terrainZ = center[2] + offsetZ;
		float terrainY = GetGame().GetWorld().GetSurfaceY(terrainX, terrainZ);
		
		return Vector(terrainX, terrainY, terrainZ);
	}
	
	//------------------------------------------------------------------------------------------------
	void TeleportFactionToMapPos(string factionName)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		if (!playableManager)
		{
			Print("Unable to get PS_PlayableManager instance", LogLevel.ERROR);
			return;
		}
		array<PS_PlayableContainer> playables = playableManager.GetPlayablesSorted();
		
		array<vector> availablePositions = {};
		
		if (factionName == "USSR")
			availablePositions = FindAllEmptyTerrainPositions(m_vOpforSpawnPos, 25);
		
		if (factionName == "US")
			availablePositions = FindAllEmptyTerrainPositions(m_vBluforSpawnPos, 25);
		
		int index = 0;
		foreach (int idx, PS_PlayableContainer playableCont : playables)
		{			
			PS_PlayableComponent playableComp = playableCont.GetPlayableComponent();
			IEntity owner = playableComp.GetOwnerCharacter();
			int playerId = GetPlayerManager().GetPlayerIdFromControlledEntity(owner);
			if (playerId == 0) // This isn't a real player so we can skip it
				continue;
			
			string playerFactionName = playableComp.GetFactionKey();		
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("BCM - playerFactionName %1 - factionName %2", playerFactionName, factionName), LogLevel.NORMAL);
			
			if (factionName == playerFactionName)
			{								
				GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.Cast(GetPlayerManager().GetPlayerController(playerId).FindComponent(GRAD_PlayerComponent));
				if (playerComponent == null)
				{
					Print("Unable to find GRAD_PlayerComponent", LogLevel.ERROR);
					return;
				}
				
				// Stagger teleports by 100ms each to reduce load spike
				GetGame().GetCallqueue().CallLater(playerComponent.Ask_TeleportPlayer, 1000 + (index * 100), false, availablePositions[index]);
				
				index = index + 1;
				
				// In case we ran out of positions start over
				// Not ideal that they spawn exact same location but better than not spawning at all...
				if (index >= availablePositions.Count())
					index = 0;
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> FindAllEmptyTerrainPositions(vector location, int minCount = 100)
	{
		array<vector> availablePositions = new array<vector>();
		int range = 200;
		
		while (availablePositions.Count() <= minCount && range < 1000)
		{
			SCR_WorldTools.FindAllEmptyTerrainPositions(availablePositions, location, range, 3, 5, 100);
			range = range + 100;
		}
		
		return availablePositions;
	}

	//----
	void SetVehiclePhysics(IEntity vehicle) {
		// handbrake first, saw this in BI method		
		CarControllerComponent carController = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
		// Activate handbrake so the vehicles don't go downhill on their own when spawned
		// not entirely sure why there are two functions for this, probably one for unsimulated/frozen and one for simulated
		if (carController)
			carController.SetPersistentHandBrake(true);
		
		VehicleWheeledSimulation simulation = carController.GetSimulation();
		if (simulation) {
			simulation.SetBreak(1.0, true);	
		}
		
		Physics physicsComponent = vehicle.GetPhysics();
		if (physicsComponent)
		{
			physicsComponent.SetActive(ActiveState.ACTIVE);
			physicsComponent.SetVelocity(vector.Zero);
			physicsComponent.SetVelocity("0 -1 0");
			physicsComponent.SetAngularVelocity(vector.Zero);
		}
	}
	
	GRAD_SpawnPointResponse SetSpawnPositions(vector spawnPos)
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - SetSpawnPositions called with spawnPos: %1 (IsServer: %2)", spawnPos.ToString(), Replication.IsServer()), LogLevel.NORMAL);
		
		m_vOpforSpawnPos = vector.Zero;
		m_vOpforSpawnDir = vector.Zero;
		m_vBluforSpawnPos = vector.Zero;
		m_vBluforSpawnDir = vector.Zero;
		
		array<vector> opforSpawnPos = GetSpawnPos(spawnPos);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - GetSpawnPos returned %1 elements for OPFOR", opforSpawnPos.Count()), LogLevel.NORMAL);
		if (opforSpawnPos.Count() != 2)
		{
			Print(string.Format("BCM - ERROR: OPFOR spawn position not found! Count: %1", opforSpawnPos.Count()), LogLevel.ERROR);
			return GRAD_SpawnPointResponse.OPFOR_NOTFOUND;
		}
		
		array<vector> bluforSpawnPos = findBluforPosition(opforSpawnPos[0]);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - findBluforPosition returned %1 elements for BLUFOR", bluforSpawnPos.Count()), LogLevel.NORMAL);
		if (bluforSpawnPos.Count() != 2)
		{
			Print(string.Format("BCM - ERROR: BLUFOR spawn position not found! Count: %1", bluforSpawnPos.Count()), LogLevel.ERROR);
			return GRAD_SpawnPointResponse.BLUFOR_NOTFOUND;
		}
		
		// If we get both then we can set them and do replication	
		m_vOpforSpawnPos = opforSpawnPos[0];
		m_vOpforSpawnDir = opforSpawnPos[1];
		
		m_vBluforSpawnPos = bluforSpawnPos[0];
		m_vBluforSpawnDir = bluforSpawnPos[1];
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - Spawn positions set successfully - OPFOR: %1, BLUFOR: %2", 
				m_vOpforSpawnPos.ToString(), m_vBluforSpawnPos.ToString()), LogLevel.NORMAL);
		
		Replication.BumpMe();
		
		// Manually call change on the server since replication is not happening there
		if (Replication.IsServer())
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("BCM - Server calling OnOpforPositionChanged manually", LogLevel.NORMAL);
			OnOpforPositionChanged();
		}
		
		return GRAD_SpawnPointResponse.OK;
	}
	
	protected array<vector> GetSpawnPos(vector spawnPos)
	{
		// Progressive search for road position + direction using consecutive road points
		vector roadPosition;
		vector direction;
		bool foundRoad = false;

		array<int> searchRadii = {20, 50, 100, 200};
		foreach (int radius : searchRadii)
		{
			if (GetRoadPointAndDir(spawnPos, radius, roadPosition, direction))
			{
				foundRoad = true;
				break;
			}
		}

		if (!foundRoad)
		{
			Print(string.Format("BCM - WARNING: No road found near %1, using original position with default direction", spawnPos.ToString()), LogLevel.WARNING);
			roadPosition = spawnPos;
			direction = Vector(1, 0, 0);
		}

		if (roadPosition == vector.Zero)
		{
			Print(string.Format("BCM - WARNING: roadPosition is vector.Zero, using original spawn position %1 as fallback", spawnPos.ToString()), LogLevel.ERROR);
			roadPosition = spawnPos;
		}

		// Basic collision check
		bool hasCollision = false;
		if (GetGame().GetWorld())
		{
			autoptr TraceParam trace = new TraceParam();
			trace.Start = roadPosition + Vector(0, 5, 0);
			trace.End = roadPosition + Vector(0, -2, 0);
			trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			trace.LayerMask = EPhysicsLayerPresets.Projectile;

			float traceResult = GetGame().GetWorld().TraceMove(trace, null);
			if (traceResult < 0.95)
			{
				hasCollision = true;
				Print(string.Format("BCM - OPFOR position has collision (trace: %1), using anyway as fallback", traceResult), LogLevel.WARNING);
			}
		}

		array<vector> output = new array<vector>();
		output.Insert(roadPosition);
		output.Insert(direction);

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM - GetSpawnPos returning position: %1, direction: %2", roadPosition.ToString(), direction.ToString()), LogLevel.NORMAL);
		return output;
	}

    //------------------------------------------------------------------------------------------------
	vector MapPosToWorldPos(int mapPos[2])
	{	
        // get surfaceY of position mapPos X(Z)Y
		vector worldPos = {mapPos[0], GetGame().GetWorld().GetSurfaceY(mapPos[0], mapPos[1]), mapPos[1]};
		return worldPos;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool SurfaceIsWater(vector pos)
	{
	    pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
	    vector outWaterSurfacePoint;
	    EWaterSurfaceType outType;
	    vector transformWS[4];
	    vector obbExtents;
	    ChimeraWorldUtils.TryGetWaterSurface(GetGame().GetWorld(), pos, outWaterSurfacePoint, outType, transformWS, obbExtents);
	    return outType != EWaterSurfaceType.WST_NONE;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnOpforPositionChanged()
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(SCR_PlayerController.GetLocalPlayerId()));
		if (!playerController) {
			Print(string.Format("GRAD CirclemarkerUI: playerController is false"), LogLevel.WARNING);	
			return;
		}
		
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		if (!ch)  {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("SCR_ChimeraCharacter missing in playerController"), LogLevel.NORMAL);
			return;
		}
		
		string factionKey = ch.GetFactionKey();
		
		if (factionKey != "USSR")
			return;
		
		// avoid the log spam by delaying the call by one frame
		GetGame().GetCallqueue().CallLater(GRAD_PlayerComponent.GetInstance().AddCircleMarker, 0, false,
			m_vOpforSpawnPos[0] - 500.0,
			m_vOpforSpawnPos[2] + 500.0,
			m_vOpforSpawnPos[0] + 500.0,
			m_vOpforSpawnPos[2] + 500.0,
			-1,
			true);
	}
	
	//------------------------------------------------------------------------------------------------
	void GetEndscreenText(out string title, out string subtitle)
	{
		title = m_sLastEndscreenTitle;
		subtitle = m_sLastEndscreenSubtitle;
	}
	
	//------------------------------------------------------------------------------------------------
	// Called by replay manager to show end screen after replay finishes
	void ShowPostReplayGameOverScreen()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BCM: ShowPostReplayGameOverScreen() called", LogLevel.NORMAL);
		
		// Only server should initiate the broadcast
		if (!Replication.IsServer())
		{
			Print("BCM: Not server, ignoring call", LogLevel.WARNING);
			return;
		}
		
		// Ensure endscreen data is set
		if (m_sLastEndscreenTitle.IsEmpty())
		{
			Print("BCM: No endscreen data found, using defaults", LogLevel.WARNING);
			m_sLastEndscreenTitle = "Game Over";
			m_sLastEndscreenSubtitle = string.Format("%1 wins!", m_sWinnerSide);
			Replication.BumpMe();
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM: Broadcasting gameover screen - Title: %1, Subtitle: %2", m_sLastEndscreenTitle, m_sLastEndscreenSubtitle), LogLevel.NORMAL);
		
		// Broadcast to all clients (including server if it has a player)
		Rpc(RpcDo_ShowGameOverScreen);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcDo_ShowGameOverScreen()
	{
		string location;
		if (Replication.IsServer())
			location = "Server";
		else
			location = "Client";
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM: RpcDo_ShowGameOverScreen received on %1", location), LogLevel.NORMAL);
		
		// Only show on clients with player controller (skip on pure dedicated server)
		if (!GetGame().GetPlayerController())
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("BCM: No player controller, skipping UI (pure dedicated server)", LogLevel.NORMAL);
			return;
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BCM: Showing endscreen - Title: %1, Subtitle: %2, Winner: %3", m_sLastEndscreenTitle, m_sLastEndscreenSubtitle, m_sWinnerSide), LogLevel.NORMAL);
		
		// Show the game over screen using the standard Arma Reforger method
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			Print("BCM: Cannot show endscreen - no game mode found", LogLevel.ERROR);
			return;
		}
		
		ShowGameOverScreen();
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnPlayableGroupCreated(SCR_AIGroup group)
	{
		if (!group)
			return;

		Faction faction = group.GetFaction();
		if (!faction)
			return;

		string factionKey = faction.GetFactionKey();

		if (!m_mGroupNameCounters.Contains(factionKey))
			m_mGroupNameCounters.Set(factionKey, 0);

		int idx = m_mGroupNameCounters.Get(factionKey);

		// Check if this group contains a commander character
		bool isCommandGroup = false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			IEntity controlledEntity = agent.GetControlledEntity();
			if (!controlledEntity)
				continue;

			GRAD_CharacterRoleComponent roleComp = GRAD_CharacterRoleComponent.Cast(controlledEntity.FindComponent(GRAD_CharacterRoleComponent));
			if (roleComp && roleComp.GetCharacterRole().Contains("Commander"))
			{
				isCommandGroup = true;
				break;
			}
		}

		string groupName;
		if (isCommandGroup)
		{
			groupName = "Command";
		}
		else
		{
			if (idx < GROUP_NATO_NAMES.Count())
			{
				groupName = GROUP_NATO_NAMES[idx];
			}
			else
			{
				groupName = string.Format("Squad %1", idx + 1);
			}
			m_mGroupNameCounters.Set(factionKey, idx + 1);
		}

		// Reassign callsign so main title in deploy menu uses the name
		SCR_CallsignGroupComponent callsignComp = SCR_CallsignGroupComponent.Cast(group.FindComponent(SCR_CallsignGroupComponent));
		if (callsignComp)
			callsignComp.ReAssignGroupCallsign(idx, 0, 0);

		// Set custom description (prominent display in deploy menu)
		group.SetCustomDescription(groupName, 0);

		// Set custom name (subtitle in deploy menu)
		group.SetCustomName(groupName, 0);

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BC Debug - Named %1 group: %2 (command: %3)", factionKey, groupName, isCommandGroup), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(mainLoop);
			GetGame().GetCallqueue().Remove(setPhaseInitial);
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);

		if (Replication.IsServer())
		{
			SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
			if (groupsManager)
			{
				groupsManager.GetOnPlayableGroupCreated().Insert(OnPlayableGroupCreated);
			}
			else
			{
				Print("BC Debug - SCR_GroupsManagerComponent not found, group naming disabled", LogLevel.WARNING);
			}
		}
	}
}

enum GRAD_SpawnPointResponse
{
	OK,
	OPFOR_NOTFOUND,
	BLUFOR_NOTFOUND
}