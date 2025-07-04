// no GAMEMASTER phase as everything should be run out of the box self explaining
enum EBreakingContactPhase
{
	LOADING,
	PREPTIME,
	OPFOR,
	BLUFOR,
	GAME,
    GAMEOVER
}

[EntityEditorProps(category: "Gruppe Adler", description: "Breaking Contact Gamemode Manager")]
class GRAD_BC_BreakingContactManagerClass : ScriptComponentClass
{
}

// This class is server-only code

class GRAD_BC_BreakingContactManager : ScriptComponent
{
    [Attribute(defvalue: "3", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How many transmissions are needed to win.", category: "Breaking Contact - Parameters", params: "1 3 1")]
	protected int m_iTransmissionCount;
	
	[Attribute(defvalue: "600", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How long one transmission needs to last.", category: "Breaking Contact - Parameters", params: "1 600 1")]
	protected int m_TransmissionDuration;
	
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How far away BLUFOR spawns from OPFOR.", category: "Breaking Contact - Parameters", params: "700 3000 1000")]
	protected int m_iBluforSpawnDistance;
	
	[Attribute(defvalue: "10", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How long in seconds the notifications should be displayed", category: "Breaking Contact - Parameters", params: "1 30 1")]
	protected int m_iNotificationDuration;
	

    protected bool m_bluforCaptured;
    protected bool m_skipWinConditions;
	protected bool m_debug = true;
	
	protected string m_sWinnerSide;

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

	static float m_iMaxTransmissionDistance = 500.0;

    protected ref array<GRAD_TransmissionPoint> m_transmissionPoints = {};
	
	protected IEntity m_radioTruck;
	protected IEntity m_westCommandVehicle;
	
	protected bool choosingBluforSpawn;
	
	// not possible to replicate the IEntity itself!
	[RplProp()]
    protected RplId radioTruckRplId;
	[RplProp()]
    protected RplId westCommandVehRplId;
	
	protected bool m_bIsTransmittingCache;
	
	protected PlayerManager m_PlayerManager;
	
	protected PlayerManager GetPlayerManager()
	{
		if (m_PlayerManager == null)
			m_PlayerManager = GetGame().GetPlayerManager();
		
		return m_PlayerManager;
	}
	
	protected static GRAD_BC_BreakingContactManager m_instance;
	
	static GRAD_BC_BreakingContactManager GetInstance()
	{
		if (!m_instance)
		{
			m_instance = GRAD_BC_BreakingContactManager.Cast(GetGame().GetGameMode().FindComponent(GRAD_BC_BreakingContactManager));
			Print(string.Format("Breaking Contact BCM - m_instance got filled %1", m_instance), LogLevel.NORMAL);	
		}
		
		return m_instance;
	}
	
    //------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		Print(string.Format("Breaking Contact BCM -  main init -"), LogLevel.NORMAL);					
		
		// execute only on the server
		if (Replication.IsServer()) {
			m_iNotificationDuration = 10;
			
			// check win conditions every second
			GetGame().GetCallqueue().CallLater(mainLoop, 10000, true);
			GetGame().GetCallqueue().CallLater(setPhaseInitial, 11000, false);
		}
    }
	
	
	void OnBreakingContactPhaseChanged()
	{
		Print(string.Format("Client: Notifying player of phase change: %1", SCR_Enum.GetEnumName(EBreakingContactPhase, m_iBreakingContactPhase)), LogLevel.NORMAL);
		
		string factionKey = GetPlayerFactionKey();
		
		string title = string.Format("New phase '%1' entered.", SCR_Enum.GetEnumName(EBreakingContactPhase, m_iBreakingContactPhase));
		string message = "Breaking Contact";
		
		switch (m_iBreakingContactPhase) {
			case EBreakingContactPhase.PREPTIME :
			{
				message = "Pretime still running.";
				break;
			}
			case EBreakingContactPhase.OPFOR :
			{
				message = "Opfor has to spawn now.";
				break;
			}
			case EBreakingContactPhase.BLUFOR :
			{
				message = "Blufor will spawn now.";
				break;
			}
			case EBreakingContactPhase.GAME :
			{
				message = "Blufor spawned, Game begins now.";
				break;
			}
			case EBreakingContactPhase.GAMEOVER :
			{
				message = "Game is over.";
				break;
			}
		}
		
		const int duration = 10;
		bool isSilent = false;
		
		
		GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
		if (playerComponent == null)
			return;
		
		// no rpc needed here, logs already on client
		SCR_HintManagerComponent.GetInstance().ShowCustomHint(message, title, duration, isSilent);
		Print(string.Format("Notifying player about phase %1", m_iBreakingContactPhase), LogLevel.NORMAL);
		
		// close map for opfor
		if (m_iBreakingContactPhase == EBreakingContactPhase.BLUFOR && factionKey == "USSR") {
			playerComponent.ToggleMap(false);
			playerComponent.setChoosingSpawn(false);
			Print(string.Format("GRAD Playercontroller PhaseChange - closing map - opfor done"), LogLevel.NORMAL);
		}
		
		// close map for blufor
		if (m_iBreakingContactPhase == EBreakingContactPhase.GAME && factionKey == "US") {
			playerComponent.ToggleMap(false);
			playerComponent.setChoosingSpawn(false);
			Print(string.Format("GRAD Playercontroller PhaseChange - closing map - blufor done"), LogLevel.NORMAL);
		}
		
		// Retrieve the HUD‐display and call ShowLogo on it:
	    SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
	    if (!hud) {
	        Print("Cannot find SCR_HUDManagerComponent!", LogLevel.ERROR);
	        return;
	    }
	
	    array<BaseInfoDisplay> infoDisplays = {};
		GetGame().GetPlayerController().GetHUDManagerComponent().GetInfoDisplays(infoDisplays);
		GRAD_BC_Logo logoDisplay = null;
		foreach (BaseInfoDisplay baseDisp : infoDisplays)
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
		
		// show logo for all
		if (m_iBreakingContactPhase == EBreakingContactPhase.GAME) {
			Print(string.Format("GRAD Playercontroller PhaseChange - game started, show logo"), LogLevel.NORMAL);
		
		    // Now bump the counter on the server so that all clients show it.
		    // (ShowLogo() itself only increments on Listen/Dedicated, so it is safe
		    // to call here from a dedicated server or listen‐server host.)
		    logoDisplay.ShowLogo();
		}
			
		// show logo for all
		if (m_iBreakingContactPhase == EBreakingContactPhase.GAMEOVER) {
			Print(string.Format("GRAD Playercontroller PhaseChange - game started, show logo"), LogLevel.NORMAL);
			logoDisplay.ShowLogo();
		}

	}
	
	//-----
	string GetPlayerFactionKey() {
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
				
		if (!playerController) {
			Print(string.Format("No playerController found in RpcAsk_Authority_NotifyLocalPlayerOnPhaseChange"), LogLevel.NORMAL);
			return "";
		}
		
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		if (!ch)  {
			Print(string.Format("SCR_ChimeraCharacter missing in playerController"), LogLevel.NORMAL);
			return "";
		}
		
		string factionKey = ch.GetFactionKey();
		
		return factionKey;
	}
	
	//------------------------------------------------------------------------------------------------
	void setPhaseInitial() 
	{
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
		Print(string.Format("Breaking Contact BCM -  -------------------------------------------------"), LogLevel.NORMAL);
        Print(string.Format("Breaking Contact BCM -  Main Loop Tick ----------------------------------"), LogLevel.NORMAL);
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
			Print(string.Format("Breaking Contact - choosingBluforSpawn upcoming"), LogLevel.NORMAL);
			choosingBluforSpawn = true;
			InitiateBluforSpawn();
		};
		
		if (m_skipWinConditions || !(GameModeStarted()))
        {
			Print(string.Format("Breaking Contact - Game not started yet"), LogLevel.NORMAL);
			return;
		};
		
		ManageMarkers();
		
        // skip win conditions if active
		if (GameModeStarted() && !(GameModeOver())) {
			CheckWinConditions();
			Print(string.Format("Breaking Contact - Checking Win Conditions..."), LogLevel.NORMAL);
		};
	}

	//------------------------------------------------------------------------------------------------
	int GetTransmissionsDoneCount()
	{
		int count = 0;
		
		if (!m_transmissionPoints)
        	return 0;
		
		foreach (ref GRAD_TransmissionPoint transmissionPoint : m_transmissionPoints)
		{
			if (!transmissionPoint)
            	continue;
			
			if (transmissionPoint.GetTransmissionState() == ETransmissionState.DONE) {
				count = count + 1;
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
		Print(string.Format("Breaking Contact BCM -  Manage markers..."), LogLevel.NORMAL);
		
		if (!m_radioTruck) {
			Print(string.Format("Breaking Contact BCM -  No radio truck found"), LogLevel.NORMAL);
			return;
		};
		
		GRAD_BC_RadioTruckComponent RTC = GRAD_BC_RadioTruckComponent.Cast(m_radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
		if (!RTC) {
			Print(string.Format("Breaking Contact BCM -  No radio truck component found"), LogLevel.NORMAL);
			return;
		}
		
		bool isTransmitting = RTC.GetTransmissionActive();
		bool stateChanged = (m_bIsTransmittingCache != isTransmitting);
		m_bIsTransmittingCache = isTransmitting;
		
		GRAD_TransmissionPoint nearestTPCAntenna = GetNearestTransmissionPoint(m_radioTruck.GetOrigin(), isTransmitting);
		array<GRAD_TransmissionPoint> transmissionPoints = GetTransmissionPoints();
		
		if (!nearestTPCAntenna) {
			Print(string.Format("Breaking Contact RTC -  No Transmission Point found"), LogLevel.NORMAL);
			return;
		}
		
		GRAD_TransmissionPoint activeTPC = nearestTPCAntenna;
		if (activeTPC) {
			if (stateChanged && isTransmitting) {
				activeTPC.SetTransmissionActive(true);
				
				Print(string.Format("Breaking Contact RTC - activating active TPC: %1 - Component: %2", nearestTPCAntenna, activeTPC), LogLevel.NORMAL);
				
				foreach (ref GRAD_TransmissionPoint singleTPCAntenna : transmissionPoints)
				{
					// disable all others
					if (nearestTPCAntenna != singleTPCAntenna) {
						GRAD_TransmissionPoint singleTPC = singleTPCAntenna;
						singleTPC.SetTransmissionActive(false);
						Print(string.Format("Breaking Contact RTC -  Disabling Transmission at: %1", singleTPCAntenna), LogLevel.NORMAL);
					};
				};
				
			} else if (stateChanged && !isTransmitting) {
				foreach (ref GRAD_TransmissionPoint singleTPCAntenna : transmissionPoints)
				{
					// disable all
					GRAD_TransmissionPoint singleTPC = singleTPCAntenna;
					singleTPC.SetTransmissionActive(false);
					Print(string.Format("Breaking Contact RTC -  Disabling Transmission at: %1", singleTPCAntenna), LogLevel.NORMAL);
				};
			} else {
			//	Print(string.Format("Breaking Contact RTC - No state change"), LogLevel.NORMAL);
			};
		} else {
			Print(string.Format("Breaking Contact RTC - No GRAD_TransmissionPoint found"), LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	GRAD_TransmissionPoint GetNearestTransmissionPoint(vector center, bool isTransmitting)
	{
		
			auto transmissionPoints = GetTransmissionPoints();
			PrintFormat(
			  "Breaking Contact GetNearestTransmissionPoint — currently have %1 TPCs in array",
			  transmissionPoints.Count(),
			  LogLevel.NORMAL
			);
		
			Print(string.Format("Breaking Contact RTC - GetNearestTransmissionPoint"), LogLevel.NORMAL);

			// if transmission points exist, find out which one is the nearest
			if (transmissionPoints && transmissionPoints.Count() > 0) 
			{
				float bestDist  = m_iMaxTransmissionDistance;
				GRAD_TransmissionPoint closest = null;

				// Scan through all existing points, find the one closest but still < maxDistance
				foreach (ref GRAD_TransmissionPoint TPCAntenna : transmissionPoints)
				{
					if (!TPCAntenna) {
						Print(string.Format("Breaking Contact RTC - TPCAntenna is null"), LogLevel.ERROR);
						continue;
					}
				
					float distance = vector.Distance(TPCAntenna.GetPosition(), center);

					// check if distance is in reach of radiotruck
					if (distance < bestDist) {
						bestDist = distance;
						closest = TPCAntenna;
					}
				}
			
				// If we already found an existing TPC within range, return that:
				if (closest)
	        		return closest;
			
				// Otherwise, if we are currently transmitting, spawn a new one
				if (!closest && isTransmitting)
		        {
					closest = this.SpawnTransmissionPoint(center);
		            Print("Breaking Contact RTC - Created New Transmission Point (in-range none found)", LogLevel.NORMAL);
		        }
				return closest;
			}
			// if no TPC exist, create a new
			if (transmissionPoints.Count() == 0 && isTransmitting)
		    {
				GRAD_TransmissionPoint selectedPoint = this.SpawnTransmissionPoint(center);
		        Print("Breaking Contact RTC - SpawnTransmissionPoint called (no existing points)", LogLevel.NORMAL);
				return selectedPoint;
		    }
			return null;
	}

	
	//------------------------------------------------------------------------------------------------
	void SpawnSpawnVehicleWest()
	{
		EntitySpawnParams params = new EntitySpawnParams();
        params.Transform[3] = m_vBluforSpawnPos;
		params.TransformMode = ETransformMode.WORLD;
		
        // create antenna that serves as component holder for transmission point
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
			Print(string.Format("BCM - West Command Truck has rplComponent"), LogLevel.NORMAL);
        }
		
		SetVehiclePhysics(m_westCommandVehicle);
		
		Print(string.Format("BCM - West Command Truck spawned: %1 at %2", m_westCommandVehicle, params), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void SpawnSpawnVehicleEast()
	{		
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		
		params.Transform[3] = m_vOpforSpawnPos + Vector(0, 0.5, 0); // lift it 0.5m
		
        // create radiotruck
        Resource ressource = Resource.Load("{1BABF6B33DA0AEB6}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_command.et");
        m_radioTruck = GetGame().SpawnEntityPrefab(ressource, GetGame().GetWorld(), params);
		
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
			 Print(string.Format("BCM - East Radio Truck has rplComponent"), LogLevel.NORMAL);
        }
		// SetVehiclePhysics(m_radioTruck);
	
		Print(string.Format("BCM - East Radio Truck spawned: %1 at %2", m_radioTruck, params), LogLevel.NORMAL);
	}
	
    //------------------------------------------------------------------------------------------------
	void CheckWinConditions()
	{
		// in debug mode we want to test alone without ending the game
		bool bluforEliminated = factionEliminated("US") && !m_debug;
		bool opforEliminated = factionEliminated("USSR") && !m_debug;
        bool isOver;

		bool finishedAllTransmissions = (GetTransmissionsDoneCount() >= m_iTransmissionCount);
		
		if (bluforEliminated) {
			isOver = true;
			m_sWinnerSide = "opfor";
			Print(string.Format("Breaking Contact - Blufor eliminated"), LogLevel.NORMAL);
		}

		if (finishedAllTransmissions) {
			isOver = true;
			m_sWinnerSide = "opfor";
			Print(string.Format("Breaking Contact - All transmissions done"), LogLevel.NORMAL);
		}
		
		if (opforEliminated) {
			isOver = true;
			m_sWinnerSide = "blufor";
			Print(string.Format("Breaking Contact - Opfor eliminated"), LogLevel.NORMAL);
		}

		if (m_bluforCaptured) {
			isOver = true;
			m_sWinnerSide = "blufor";
			Print(string.Format("Breaking Contact - Blufor captured radio truck"), LogLevel.NORMAL);
		}
		
		// needs to be on last position as would risk to be overwritten
		if (bluforEliminated && opforEliminated) {
			isOver = true;
			m_sWinnerSide = "draw";
			Print(string.Format("Breaking Contact - Both sides eliminated"), LogLevel.NORMAL);
		}
		
		if (isOver) {
			// show game over screen with a 20s delay
			// local stuff is managed by breaking contact phase handler
	        GetGame().GetCallqueue().CallLater(SetBreakingContactPhase, 20000, false, EBreakingContactPhase.GAMEOVER);
			Print(string.Format("Breaking Contact - Game Over Screen TODO"), LogLevel.NORMAL);
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
	EBreakingContactPhase GetBreakingContactPhase()
	{
		Print(string.Format("GetBreakingContactPhase - Phase '%1'", m_iBreakingContactPhase, LogLevel.NORMAL));
		return m_iBreakingContactPhase;
	}
	
	//------------------------------------------------------------------------------------------------
	void Rpc_RequestInitiateOpforSpawn()
	{
		Print(string.Format("Breaking Contact - Rpc_RequestInitiateOpforSpawn"), LogLevel.NORMAL);
	    TeleportFactionToMapPos("USSR");
		SetBreakingContactPhase(EBreakingContactPhase.BLUFOR);
	}
	
	 //------------------------------------------------------------------------------------------------
	// called by InitiateOpforSpawn - server side
	void SetBreakingContactPhase(EBreakingContactPhase phase)
    {
        m_iBreakingContactPhase = phase;
        Replication.BumpMe();
		
		OnBreakingContactPhaseChanged(); // call on server too for debug in SP test

        Print(string.Format("Breaking Contact - Phase %1 entered - %2 -", SCR_Enum.GetEnumName(EBreakingContactPhase, phase), phase), LogLevel.NORMAL);
    }	
	
	//------------------------------------------------------------------------------------------------
	void InitiateBluforSpawn() 
	{
		SetBreakingContactPhase(EBreakingContactPhase.GAME);
        TeleportFactionToMapPos("US");
	}

	//------------------------------------------------------------------------------------------------
	GRAD_TransmissionPoint SpawnTransmissionPoint(vector center)
	{	
		// create antenna that serves as component holder for transmission point
        // Resource ressource = Resource.Load("{5B8922E61D8DF345}Prefabs/Props/Military/Antennas/Antenna_R161_01.et");
        // GRAD_TransmissionPoint transmissionPoint = GRAD_TransmissionPoint.Cast(GetGame().SpawnEntity(GRAD_TransmissionPoint, GetWorld(), params));
		// RemoveChild(transmissionPoint, false); // disable attachment hierarchy to radiotruck (?!)
		
		GRAD_TransmissionPoint transmissionPoint = new GRAD_TransmissionPoint();
		transmissionPoint.SetPosition(center);
		
		if (transmissionPoint) {
        	m_transmissionPoints.Insert(transmissionPoint);
		}
		
		Print(string.Format("BCM - Transmission Point spawned: %1 at %2", transmissionPoint, center), LogLevel.NORMAL);
		
		return transmissionPoint;
	}

	// Method to find the closest road position
	protected vector FindSpawnPointOnRoad(vector position)
	{
	    array<vector> roadPoints = GetNearestRoadPos(position, 10);
	    if (roadPoints.IsEmpty())
	        return vector.Zero; // No suitable spawn found
	    
	    // Find the closest point from the road points
	    vector closestPos = position;
	    float minDistance = float.MAX;
	    
	    foreach (vector roadPoint : roadPoints)
	    {
	        float distance = vector.Distance(position, roadPoint);
	        if (distance < minDistance)
	        {
	            minDistance = distance;
	            closestPos = roadPoint;
	        }
	    }
	    
	    return closestPos;
	}
	
	// Method to find the second closest road position for e.g. road direction
	protected vector FindNextSpawnPointOnRoad(vector position)
	{
	    array<vector> roadPoints = GetNearestRoadPos(position,100);
	    if (roadPoints.IsEmpty())
	        return position; // no road points at all
	
	    // Track best and runner-up
	    float   bestDist    = float.MAX;
	    float   secondDist  = float.MAX;
	    vector  bestPos     = position;
	    vector  secondPos   = position;
	
	    foreach (vector roadPoint : roadPoints)
	    {
	        float d = vector.Distance(position, roadPoint);
			Print("d: %1" + d);
	
	        if (d < bestDist)
	        {
	            // shift best → second, then update best
	            secondDist = bestDist;
	            secondPos  = bestPos;
	
	            bestDist   = d;
	            bestPos    = roadPoint;
				Print("secondDist: %1" + secondDist);
				Print("bestDist: %1" + bestDist);
	        }
	        else if (d < secondDist)
	        {
	            // it's worse than best but better than current second
	            secondDist = d;
	            secondPos  = roadPoint;
				Print("secondDist: %1" + secondDist);
	        }
	    }
	
	    // If we never found a true second, fall back to the nearest
	    if (secondDist < float.MAX)
	    {
			Print("secondDist found! %1" + secondPos);
	        return secondPos;
	    }
	    else
	    {
			Print("no second best found! %1" + bestPos);
	        return bestPos;
	    }
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
	    
	    // Debug outputs
	    Print("BCM - Center: " + center.ToString());
	    Print("BCM - aabbMin: " + aabbMin.ToString());
	    Print("BCM - aabbMax: " + aabbMax.ToString());
	    Print("BCM - Result Code: " + result);
	    
	    // Output the retrieved roads
	    for (int i = 0; i < emptyRoads.Count(); i++)
	    {
	        Print("Road " + i + ": " + emptyRoads[i].ToString());
	    }
	    
	    if (result > 0)
	    {
	        BaseRoad emptyRoad = emptyRoads[0];
	        emptyRoad.GetPoints(outPoints);
	        PrintFormat("BCM - found road %1 - outPoints after %2", emptyRoad, outPoints);
	        
	        return outPoints;
	    }
	    
		// in case we dont find return null
		PrintFormat("BCM - Unable to determine spawn pos for position %1 and search radius", center, searchRadius, level: LogLevel.WARNING);
	    return new array<vector>();
	}
	
	//------------------------------------------------------------------------------------------------
	array<GRAD_TransmissionPoint> GetTransmissionPoints()
	{
		Print(string.Format("BCM - GetTransmissionPoints '%1'.", m_transmissionPoints.Count()), LogLevel.NORMAL);	
        return m_transmissionPoints;		
    }


    //------------------------------------------------------------------------------------------------
    protected array<vector> findBluforPosition(vector opforPosition) {
	    bool foundPositionOnLand = false;
	    int loopCount = 0;
	    vector roadPosition;
	    
	    // Define min and max distance bounds
	    float minDistance = m_iBluforSpawnDistance - 100;
	    float maxDistance = m_iBluforSpawnDistance + 100;
	    
	    while (!foundPositionOnLand && loopCount < 100) {
	        loopCount++;
	        
	        // Generate random direction
	        int degrees = Math.RandomIntInclusive(0, 360);
	        
	        // Generate random distance within bounds
	        float randomDistance = Math.RandomFloatInclusive(minDistance, maxDistance);
	        
	        // Get initial point on circle
	        vector candidatePos = GetPointOnCircle(opforPosition, randomDistance, degrees);
	        
	        // Find nearest road position within a reasonable search radius
	        array<vector> roadPositions = GetNearestRoadPos(candidatePos, 200); // 200m search radius
	        
	        if (roadPositions && !roadPositions.IsEmpty()) {
	            // Use the closest road position
	            roadPosition = roadPositions[0];
	            
	            // Verify the position meets distance requirements and is on land
	            float distanceToOpfor = vector.Distance(roadPosition, opforPosition);
	            
	            if (!SurfaceIsWater(roadPosition) && 
	                distanceToOpfor >= minDistance && 
	                distanceToOpfor <= maxDistance) {
	                Print(string.Format("BCM - Found valid Blufor position after %1 loops - distance: %2 - pos: %3 - opfor: %4 - degrees: %5", 
	                    loopCount, distanceToOpfor, roadPosition, opforPosition, degrees), LogLevel.NORMAL);
	                foundPositionOnLand = true;
	            }
	        }
	        
	        if (!foundPositionOnLand && loopCount >= 100) {
	            Print(string.Format("BCM - findBluforPosition failed to find valid road position after %1 loops", loopCount), LogLevel.ERROR);
	            // roadPosition to original position if no valid road position found
	            roadPosition = GetPointOnCircle(opforPosition, m_iBluforSpawnDistance, degrees);
	            foundPositionOnLand = true;
	        }
	    }
		
		vector roadPosition2 = FindNextSpawnPointOnRoad(roadPosition);
		
		vector direction = vector.Direction(roadPosition, roadPosition2);
		vector midpoint = vector.Lerp(roadPosition, roadPosition2, 0.5);
		
		array<vector> output = new array<vector>();
		output.Insert(midpoint);
		output.Insert(direction);
		
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
	void NotifyPlayerWrongRole(int playerId, string neededRole)
	{

		const string title = "Breaking Contact";
		string message = string.Format("You have the wrong role to create a teleport marker. You need to have the '%1' role.", neededRole);
		int duration = m_iNotificationDuration;
		bool isSilent = false;
	
		GRAD_PlayerComponent.GetInstance().ShowHint(message, title, duration, isSilent);
	}


    //------------------------------------------------------------------------------------------------
	void NotifyFactionWrongPhaseForMarker(Faction faction)
	{
		array<int> playerIds = {};
		GetPlayerManager().GetAllPlayers(playerIds);

		const string title = "Breaking Contact";
		const string message = "You can't create the marker in this phase.";
		int duration = m_iNotificationDuration;
		bool isSilent = false;
		
		foreach (int playerId : playerIds)
		{
			if (SCR_FactionManager.SGetPlayerFaction(playerId) == faction)
			{			
				GRAD_PlayerComponent.GetInstance().ShowHint(message, title, duration, isSilent);
			}
		}
	}


    //------------------------------------------------------------------------------------------------
	void TeleportFactionToMapPos(string factionName)
	{
		array<vector> availablePositions = new array<vector>();
		
		if (factionName == "USSR")
		{	
			Print(string.Format("Breaking Contact - Opfor spawn is done"), LogLevel.NORMAL);	
			SpawnSpawnVehicleEast();
			availablePositions = FindAllEmptyTerrainPositions(m_vOpforSpawnPos);
		}
		
		if (factionName == "US") {	
			Print(string.Format("Breaking Contact - Blufor spawn is done"), LogLevel.NORMAL);
			SpawnSpawnVehicleWest();
			availablePositions = FindAllEmptyTerrainPositions(m_vBluforSpawnPos);
		}
		
		if (factionName != "US" && factionName != "USSR") {
			Print(string.Format("Breaking Contact - PANIC, faction is %1", factionName), LogLevel.NORMAL);
			return;
		}
		
		PS_PlayableManager m_PlayableManager = PS_PlayableManager.GetInstance();
		array<PS_PlayableContainer> playables = m_PlayableManager.GetPlayablesSorted();
		
		int index = 0;
		foreach (int idx, PS_PlayableContainer playableCont : playables)
		{			
			PS_PlayableComponent playableComp = playableCont.GetPlayableComponent();
			IEntity owner = playableComp.GetOwnerCharacter();
			int playerId = GetPlayerManager().GetPlayerIdFromControlledEntity(owner);
			if (playerId == 0) // This isn't a real player so we can skip it
				continue;
			
			string playerFactionName = playableComp.GetFactionKey();		
			Print(string.Format("BCM - playerFactionName %1 - factionName %2", playerFactionName, factionName), LogLevel.NORMAL);
			
			if (factionName == playerFactionName)
			{								
				GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.Cast(GetPlayerManager().GetPlayerController(playerId).FindComponent(GRAD_PlayerComponent));
				if (playerComponent == null)
				{
					Print("Unable to find GRAD_PlayerComponent", LogLevel.ERROR);
					return;
				}
				
				GetGame().GetCallqueue().CallLater(playerComponent.Ask_TeleportPlayer, 1000, false, availablePositions[index]);
				
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
		
		while (availablePositions.Count() <= minCount || range < 1000)
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
			simulation.SetBreak(true, true);	
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
		m_vOpforSpawnPos = vector.Zero;
		m_vOpforSpawnDir = vector.Zero;
		m_vBluforSpawnPos = vector.Zero;
		m_vBluforSpawnDir = vector.Zero;
		
		array<vector> opforSpawnPos = GetSpawnPos(spawnPos);		
		if (opforSpawnPos.Count() != 2)
		{
			return GRAD_SpawnPointResponse.OPFOR_NOTFOUND;
		}
		
		array<vector> bluforSpawnPos = findBluforPosition(opforSpawnPos[0]);
		if (bluforSpawnPos.Count() != 2)
		{
			return GRAD_SpawnPointResponse.BLUFOR_NOTFOUND;
		}
		
		// If we get both then we can set them and do replication	
		m_vOpforSpawnPos = opforSpawnPos[0];
		m_vOpforSpawnDir = opforSpawnPos[1];
		
		m_vBluforSpawnPos = bluforSpawnPos[0];
		m_vBluforSpawnDir = bluforSpawnPos[1];
		
		Replication.BumpMe();
		
		// Manually call change on the server since replication is not happening there
		if (Replication.IsServer())
			OnOpforPositionChanged();
		
		return GRAD_SpawnPointResponse.OK;
	}
	
	protected array<vector> GetSpawnPos(vector spawnPos)
	{
		vector roadPosition = FindSpawnPointOnRoad(spawnPos);
		if (roadPosition == vector.Zero)
		{
			return new array<vector>();
		}
		
		vector roadPosition2 = FindNextSpawnPointOnRoad(roadPosition);
		
		vector direction = vector.Direction(roadPosition, roadPosition2);
		vector midpoint = vector.Lerp(roadPosition, roadPosition2, 0.5);
		
		array<vector> output = new array<vector>();
		output.Insert(midpoint);
		output.Insert(direction);
		
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
			Print(string.Format("SCR_ChimeraCharacter missing in playerController"), LogLevel.NORMAL);
			return;
		}
		
		string factionKey = ch.GetFactionKey();
		
		if (factionKey != "USSR")
			return;
		
		GRAD_PlayerComponent.GetInstance().AddCircleMarker(
			m_vOpforSpawnPos[0] - 500.0,
			m_vOpforSpawnPos[2] + 500.0,
			m_vOpforSpawnPos[0] + 500.0,
			m_vOpforSpawnPos[2] + 500.0,
			-1,
			true);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}
}

enum GRAD_SpawnPointResponse
{
	OK,
	OPFOR_NOTFOUND,
	BLUFOR_NOTFOUND
}