// GAMEMASTER will be automatically skipped in the future
enum EBreakingContactPhase
{
	GAMEMASTER,
	OPFOR,
	BLUFOR,
	GAME,
    GAMEOVER
}

[EntityEditorProps(category: "Gruppe Adler", description: "Breaking Contact Gamemode Manager")]
class GRAD_BC_BreakingContactManagerClass : GenericEntityClass
{
}

// This class is server-only code

class GRAD_BC_BreakingContactManager : GenericEntity
{
    [Attribute(defvalue: "3", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How many transmissions are needed to win.", category: "Breaking Contact - Parameters", params: "1 3 1")]
	protected int m_iTransmissionCount;
	
	[Attribute(defvalue: "600", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How long one transmission needs to last.", category: "Breaking Contact - Parameters", params: "1 600 1")]
	protected int m_TransmissionDuration;
	
	[Attribute(defvalue: "3000", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How far away BLUFOR spawns from OPFOR.", category: "Breaking Contact - Parameters", params: "1 3000 1")]
	protected int m_iBluforSpawnDistance;
	
	[Attribute(defvalue: "10", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How long in seconds the notifications should be displayed", category: "Breaking Contact - Parameters", params: "1 30 1")]
	protected int m_iNotificationDuration;


    protected bool m_bluforCaptured;
    protected bool m_skipWinConditions;
	protected bool m_debug;
	
	protected string m_sWinnerSide;

    protected vector m_vOpforSpawnPos;
    protected vector m_vBluforSpawnPos;


    [RplProp()]
	protected int m_iBreakingContactPhase;
	

	static float m_iMaxTransmissionDistance = 500.0;

    protected ref array<IEntity> m_transmissionPoints = {};
	protected ref array<IEntity> m_iTransmissionsDone = {};
	
	protected static GRAD_BC_BreakingContactManager s_Instance;
	protected IEntity m_radioTruck;
	protected bool m_bIsTransmittingCache;
	
	//------------------------------------------------------------------------------------------------
	static GRAD_BC_BreakingContactManager GetInstance()
	{
		return s_Instance;
	}
	
    //------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		Print(string.Format("Breaking Contact BCM -  main init -"), LogLevel.NORMAL);
		
		// execute only on the server
		if (!Replication.IsServer())
			return;
		
		// check win conditions every second
        GetGame().GetCallqueue().CallLater(mainLoop, 1000, true);
		
		// Find radio truck east; Init order of entities unknown; therefore it could be missing
		// I also added the check in main loop.
		// TODO: @nomisum --> Please decide what fit's your needs
		GetGame().GetCallqueue().CallLater(FindRadioTruckEast, 5 * 1000, false);
    }

    //------------------------------------------------------------------------------------------------
	protected void FindRadioTruckEast()
	{
		if (!m_radioTruck)
		{
			m_radioTruck = GetGame().GetWorld().FindEntityByName("radioTruckEast");
		}
		
		InitRadioTruckMarker(m_radioTruck);
	} 
	
	void InitRadioTruckMarker(IEntity entity)
	{
		RplId rplId = Replication.FindId(entity.FindComponent(RplComponent));
		
		Print(string.Format("Breaking Contact BCM - init icon marker for rplId %1", rplId), LogLevel.NORMAL);
		
		array<int> allPlayers = {};
		
		GetGame().GetPlayerManager().GetPlayers(allPlayers);
		foreach(int playerId : allPlayers)
		{
			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			// todo do this on all clients
			playerController.SetIconMarker(
				"{243D963F2E18E435}UI/Textures/Map/radiotruck_active.edds",
				rplId
			);
		};
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
		GetGame().GetPlayerManager().GetPlayers(allPlayers);
		foreach(int playerId : allPlayers)
		{
			// null check bc of null pointer crash
			PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
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

		Print(string.Format("Breaking Contact BCM -  -------------------------------------------------"), LogLevel.NORMAL);
        Print(string.Format("Breaking Contact BCM -  Main Loop Tick ----------------------------------"), LogLevel.NORMAL);
		Print(string.Format("Breaking Contact BCM -  -------------------------------------------------"), LogLevel.NORMAL);
		
		// todo move behind game mode started
		ManageMarkers();		
		
		if (m_skipWinConditions || !(GameModeStarted()))
        {
			Print(string.Format("Breaking Contact - Game not started yet"), LogLevel.NORMAL);
			return;
		};
		
        // skip win conditions if active
		if (GameModeStarted() && !(GameModeOver())) {
			CheckWinConditions();
            Print(string.Format("Breaking Contact - Checking Win Conditions..."), LogLevel.NORMAL);
		};
	}

	//------------------------------------------------------------------------------------------------
	int GetTransmissionsDoneCount()
	{
		return (m_iTransmissionsDone.Count());
	} 

	//------------------------------------------------------------------------------------------------
	void AddTransmissionPointDone(IEntity transmissionPoint) 
	{
		m_iTransmissionsDone.Insert(transmissionPoint);
	}
	
	//------------------------------------------------------------------------------------------------
	void ManageMarkers() 
	{
		Print(string.Format("Breaking Contact BCM -  Manage markers..."), LogLevel.NORMAL);
		
		if (!m_radioTruck) {
			FindRadioTruckEast();
			if (!m_radioTruck) {
				Print(string.Format("Breaking Contact BCM -  No radio truck found"), LogLevel.NORMAL);
				return;
			};
		};
		
		GRAD_BC_RadioTruckComponent RTC = GRAD_BC_RadioTruckComponent.Cast(m_radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
		if (!RTC) {
			Print(string.Format("Breaking Contact BCM -  No radio truck component found"), LogLevel.NORMAL);
			return;
		}
		
		bool isTransmitting = RTC.GetTransmissionActive();
		bool stateChanged = (m_bIsTransmittingCache != isTransmitting);
		m_bIsTransmittingCache = isTransmitting;
		
		IEntity nearestTPCAntenna = GetNearestTransmissionPoint(m_radioTruck.GetOrigin(), isTransmitting);
		array<IEntity> transmissionPoints = GetTransmissionPoints();
		
		if (!nearestTPCAntenna) {
			Print(string.Format("Breaking Contact RTC -  No Transmission Point found"), LogLevel.NORMAL);
			return;
		}
		
		GRAD_BC_TransmissionPointComponent activeTPC = GRAD_BC_TransmissionPointComponent.Cast(nearestTPCAntenna.FindComponent(GRAD_BC_TransmissionPointComponent));
		if (activeTPC) {
			if (stateChanged && isTransmitting) {
				activeTPC.SetTransmissionActive(true);
				
				Print(string.Format("Breaking Contact RTC - activating active TPC: %1 - Component: %2", nearestTPCAntenna, activeTPC), LogLevel.NORMAL);
				
				foreach (IEntity singleTPCAntenna : transmissionPoints)
				{
					// disable all others
					if (nearestTPCAntenna != singleTPCAntenna) {
						GRAD_BC_TransmissionPointComponent singleTPC = GRAD_BC_TransmissionPointComponent.Cast(singleTPCAntenna.FindComponent(GRAD_BC_TransmissionPointComponent));
						singleTPC.SetTransmissionActive(false);
						Print(string.Format("Breaking Contact RTC -  Disabling Transmission at: %1", singleTPCAntenna), LogLevel.NORMAL);
					};
				};
				
			} else if (stateChanged && !isTransmitting) {
				foreach (IEntity singleTPCAntenna : transmissionPoints)
				{
					// disable all
					GRAD_BC_TransmissionPointComponent singleTPC = GRAD_BC_TransmissionPointComponent.Cast(singleTPCAntenna.FindComponent(GRAD_BC_TransmissionPointComponent));
					singleTPC.SetTransmissionActive(false);
					Print(string.Format("Breaking Contact RTC -  Disabling Transmission at: %1", singleTPCAntenna), LogLevel.NORMAL);
				};
			} else {
			//	Print(string.Format("Breaking Contact RTC - No state change"), LogLevel.NORMAL);
			};
		} else {
			Print(string.Format("Breaking Contact RTC - No GRAD_BC_TransmissionPointComponent found"), LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected IEntity GetNearestTransmissionPoint(vector center, bool isTransmitting)
	{
		
			array<IEntity> transmissionPoints = GetTransmissionPoints();
			IEntity selectedPoint = null;

			// if transmission points exist, find out which one is the nearest
			if (transmissionPoints.Count() > 0) {
				float distanceMaxTemp;

				foreach (IEntity TPCAntenna : transmissionPoints)
				{
					float distance = vector.Distance(TPCAntenna.GetOrigin(), center);

					// check if distance is in reach of radiotruck
					if (distance < m_iMaxTransmissionDistance) {
						distanceMaxTemp = distance;
						selectedPoint = TPCAntenna;
					}
				}
				if (!selectedPoint && isTransmitting) {
					selectedPoint = CreateTransmissionPoint(center);
				}
				return selectedPoint;
			}
			if (isTransmitting) {
				selectedPoint = CreateTransmissionPoint(center);
			}
			return selectedPoint;
		}

	
	//------------------------------------------------------------------------------------------------
	IEntity CreateTransmissionPoint(vector center) {
		// if no transmission point exists, create one
		IEntity TPCAntenna = SpawnTransmissionPoint(center, 10);
		Print(string.Format("Breaking Contact RTC -  Create TransmissionPoint: %1", TPCAntenna), LogLevel.NORMAL);
	
		
		
		GetGame().GetCallqueue().CallLater(WaitForAntennaRplId, 1000, false, TPCAntenna, 1);
		
		return TPCAntenna;
	}
	
	void WaitForAntennaRplId(IEntity entity, int timeToWaitForCheck)
	{
		RplId rplId = Replication.FindId(entity.FindComponent(RplComponent));
		
		Print(string.Format("Breaking Contact BCM - waited for antenna %1 s, its %2", timeToWaitForCheck, rplId), LogLevel.NORMAL);
		
		AddTransmissionMarker(entity, m_iMaxTransmissionDistance);
	}
	
	//------------------------------------------------------------------------------------------------
	void AddTransmissionMarker(IEntity TPCAntenna, float radius)
	{

		vector center = TPCAntenna.GetOrigin();
		RplId rplId = Replication.FindId(TPCAntenna.FindComponent(RplComponent));

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetAllPlayers(playerIds);

		foreach (int playerId : playerIds)
		{

			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));

			if (!playerController)
				return;
			
			Print(string.Format("Breaking Contact AddTransmissionMarker - Replication Id of Antenna is %1", rplId), LogLevel.WARNING);
		
			
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
	        // GetGame().GetCallqueue().CallLater(showGameOver, 20000, false, m_winnerSide);
            SetBreakingContactPhase(EBreakingContactPhase.GAMEOVER);
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
	int GetBreakingContactPhase()
	{
		return m_iBreakingContactPhase;
	}


    //------------------------------------------------------------------------------------------------
	void SetBreakingContactPhase(int phase)
	{
		Rpc(RpcAsk_Authority_SetBreakingContactPhase, phase);
	}


    //------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void 	RpcAsk_Authority_SetBreakingContactPhase(int phase)
	{
		m_iBreakingContactPhase = phase;
		
		Replication.BumpMe();
		
		NotifyAllOnPhaseChange(phase);
		
		Print(string.Format("Breaking Contact - Phase '%1' entered (%2)", SCR_Enum.GetEnumName(EBreakingContactPhase, phase), phase), LogLevel.NORMAL);
	}


	//------------------------------------------------------------------------------------------------
	IEntity SpawnTransmissionPoint(vector center, int radius)
	{		
		vector newCenter = findSpawnPoint(center, radius);
		
		EntitySpawnParams params = new EntitySpawnParams();
        params.Transform[3] = newCenter;
		
        // create antenna that serves as component holder for transmission point
        Resource ressource = Resource.Load("{5B8922E61D8DF345}Prefabs/Props/Military/Antennas/Antenna_R161_01.et");
        IEntity transmissionPoint = GetGame().SpawnEntityPrefab(ressource, GetGame().GetWorld(), params);
		
		RemoveChild(transmissionPoint, false); // disable attachment hierarchy to radiotruck (?!)
		
        addTransmissionPoint(transmissionPoint); // add to array
		
		Print(string.Format("BCM - Transmission Point spawned: %1 at %2", transmissionPoint, params), LogLevel.NORMAL);
		
		return transmissionPoint;
	}


    //------------------------------------------------------------------------------------------------
    protected vector findSpawnPoint(vector center, int radius)
    {
        bool foundSpawnPoint;
		int loopCount;

        while (!foundSpawnPoint) {
            Math.Randomize(-1);
            int randomDistanceX = Math.RandomInt( -radius, radius );
            int randomDistanceY = Math.RandomInt( -radius, radius );
			
			vector worldPos = {center[0] + randomDistanceX, GetGame().GetWorld().GetSurfaceY(center[0] + randomDistanceX, center[2] + randomDistanceY), center[2] + randomDistanceY};
            bool spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(worldPos, worldPos, 2, 2);

			loopCount = loopCount + 1;	
			Print(string.Format("BCM - spawn point loop '%1 at %2, success is %3 .", loopCount, worldPos, spawnEmpty), LogLevel.NORMAL);
			
            if (spawnEmpty) {
                foundSpawnPoint = true;
				center = worldPos;
				
				Print(string.Format("BCM - spawn point found after '%1 loops'.", loopCount), LogLevel.NORMAL);
            }
			
			if (loopCount > 100)
				return center;
        }

        return center;
    }


    //------------------------------------------------------------------------------------------------
	protected void addTransmissionPoint(IEntity transmissionPoint)
	{
        m_transmissionPoints.Insert(transmissionPoint);		
    }
	
	
	//------------------------------------------------------------------------------------------------
	array<IEntity> GetTransmissionPoints()
	{
        return m_transmissionPoints;		
    }
	
	//
	void OnCreateMarker(SCR_MapMarkerBase marker)
	{
		Print(string.Format("Breaking Contact - OnCreateMarker"), LogLevel.WARNING);
	}


    //------------------------------------------------------------------------------------------------
	void OnSynchedMarkerAdded(SCR_MapMarkerBase marker)
	{
		if (marker.GetType() != SCR_EMapMarkerType.PLACED_CUSTOM)
			return; // only custom markers will have marker text that we need
		
		string markerText = marker.GetCustomText();
		Print(string.Format("Breaking Contact - Custom Marker '%1' placed.", markerText), LogLevel.NORMAL);
		markerText.ToLower();
		if (!(markerText == "debug" || markerText == "opfor" || markerText == "blufor"))
			return; // we are only interested in these special markers
		
		int markerOwnerId = marker.GetMarkerOwnerID();
		
		// only characters with the role 'Breaking Contact Commander' either BLUFOR or OPFOR are allowed to create teleport markers
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(markerOwnerId));
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());
		GRAD_CharacterRoleComponent characterRoleComponent = GRAD_CharacterRoleComponent.Cast(ch.FindComponent(GRAD_CharacterRoleComponent));
		string characterRole = characterRoleComponent.GetCharacterRole();
		if (characterRole != "Breaking Contact Commander")
		{
			Print(string.Format("Breaking Contact - Wrong role for marker. Current Role '%1'", characterRole), LogLevel.NORMAL);
			NotifyPlayerWrongRole(markerOwnerId, "Breaking Contact Commander");
			GRAD_BC_Logo logo = GRAD_BC_Logo.Cast(pc.FindComponent(GRAD_BC_Logo));
			logo.SetVisible(true);
			return;
		}
		
		int markerPos[2];
		marker.GetWorldPos(markerPos);
        vector opforSpawnPos = MapPosToWorldPos(markerPos);
		
		Faction markerOwnerFaction = SCR_FactionManager.SGetPlayerFaction(markerOwnerId);
		
		switch (markerText)
		{
			case "opfor":
				OpforMarkerCreated(marker, opforSpawnPos, markerOwnerFaction);
				GRAD_BC_Logo logo = GRAD_BC_Logo.Cast(pc.FindComponent(GRAD_BC_Logo));
				logo.SetVisible(true);
				break;
		}
	}


    //------------------------------------------------------------------------------------------------
	void OpforMarkerCreated(SCR_MapMarkerBase marker, vector opforSpawnPos, Faction markerOwnerFaction)
	{
		// manage opfor marker placement
		if (m_iBreakingContactPhase != EBreakingContactPhase.OPFOR) {
			NotifyFactionWrongPhaseForMarker(markerOwnerFaction);
			return;
		}
			
		if (markerOwnerFaction.GetFactionKey() == "USSR") {	
            teleportOpfor(opforSpawnPos, markerOwnerFaction);
            teleportBlufor(opforSpawnPos, markerOwnerFaction);
		}
	}


    //------------------------------------------------------------------------------------------------
    void teleportOpfor(vector opforSpawnPos, Faction markerOwnerFaction) {
        TeleportFactionToMapPos(markerOwnerFaction, markerOwnerFaction.GetFactionKey(), opforSpawnPos, false);
    }

    //------------------------------------------------------------------------------------------------
    void teleportBlufor(vector opforSpawnPos, Faction markerOwnerFaction) {

        vector bluforSpawnPos = findBluforPosition(opforSpawnPos);
        TeleportFactionToMapPos(markerOwnerFaction, markerOwnerFaction.GetFactionKey(), bluforSpawnPos, false);
    }


    //------------------------------------------------------------------------------------------------
    vector findBluforPosition(vector opforSpawnPos) {
		vector bluforSpawnPos = "0 0 0"; // todo
		return bluforSpawnPos;
    }


    //------------------------------------------------------------------------------------------------
	void NotifyAllOnPhaseChange(int phase)
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetAllPlayers(playerIds);

		string title = "Breaking Contact";
		string message = string.Format("New phase '%1' entered.", SCR_Enum.GetEnumName(EBreakingContactPhase, phase));
		int duration = m_iNotificationDuration;
		bool isSilent = false;
		
		foreach (int playerId : playerIds)
		{
			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			
			if (!playerController)
				return;
		
			playerController.ShowHint(message, title, duration, isSilent);
		}
	}
	
	
	//------------------------------------------------------------------------------------------------
	void NotifyPlayerWrongRole(int playerId, string neededRole)
	{

		string title = "On The Fly";
		string message = string.Format("You have the wrong role to create a teleport marker. You need to have the '%1' role.", neededRole);
		int duration = m_iNotificationDuration;
		bool isSilent = false;
		
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		
		if (!playerController)
			return;
	
		playerController.ShowHint(message, title, duration, isSilent);
	}


    //------------------------------------------------------------------------------------------------
	void NotifyFactionWrongPhaseForMarker(Faction faction)
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetAllPlayers(playerIds);

		string title = "Breaking Contact";
		string message = "You can't create the marker in this phase.";
		int duration = m_iNotificationDuration;
		bool isSilent = false;
		
		foreach (int playerId : playerIds)
		{
			if (SCR_FactionManager.SGetPlayerFaction(playerId) == faction)
			{
				SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
				
				if (!playerController)
					return;
			
				playerController.ShowHint(message, title, duration, isSilent);
			}
		}
	}


    //------------------------------------------------------------------------------------------------
	void TeleportFactionToMapPos(Faction faction, string factionName, vector worldPos, bool isdebug)
	{
		if (factionName == "USSR" || isdebug)
		{	
			m_vOpforSpawnPos = worldPos;
			Print(string.Format("Breaking Contact - Opfor spawn is done"), LogLevel.NORMAL);
						
			// enter debug mode
			if (isdebug) {
				m_debug = true;
			}			
		} else {
			Print(string.Format("Breaking Contact - Blufor spawn is done"), LogLevel.NORMAL);
		}
		
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetAllPlayers(playerIds);
		
		foreach (int playerId : playerIds)
		{
			Faction playerFaction = SCR_FactionManager.SGetPlayerFaction(playerId);
			
			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			
			if (!playerController)
				return;
			
			if (playerFaction == faction)
			{
				Print(string.Format("Breaking Contact - Player with ID %1 is Member of Faction %2 and will be teleported to %3", playerId, playerFaction.GetFactionKey(), worldPos), LogLevel.NORMAL);
				playerController.TeleportPlayerToMapPos(playerId, worldPos);
			}
		} 
	}


    //------------------------------------------------------------------------------------------------
	vector MapPosToWorldPos(int mapPos[2])
	{	
        // get surfaceY of position mapPos X(Z)Y
		vector worldPos = {mapPos[0], GetGame().GetWorld().GetSurfaceY(mapPos[0], mapPos[1]), mapPos[1]};
		return worldPos;
	}

    //------------------------------------------------------------------------------------------------	
	void GRAD_BC_BreakingContactManager(IEntitySource src, IEntity parent)
	{
		if (s_Instance)
		{
			Print("Breaking Contact - Only one instance of GRAD_BC_BreakingContactManager is allowed in the world!", LogLevel.WARNING);
			delete this;
			return;
		}

		s_Instance = this;

		// rest of the init code
		
		SetEventMask(EntityEvent.INIT);
	}
}
