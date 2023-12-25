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
class GRAD_BreakingContactManagerClass : GenericEntityClass
{
}

// This class is server-only code

class GRAD_BreakingContactManager : GenericEntity
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

    protected vector m_vOpforSpawnPos;
    protected vector m_vBluforSpawnPos;

    [RplProp()]
	protected int m_iBreakingContactPhase;

    protected array<TransmissionPointComponent> m_transmissionPoints = {};
    

    //------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		// execute only on the server
		if (!Replication.IsServer())
			return;
		
		// check win conditions every second
        GetGame().GetCallqueue().CallLater(mainLoop, m_iCheckInterval, true);
    }


    //------------------------------------------------------------------------------------------------
	bool factionEliminated(string factionName)
	{
		return (GetAlivePlayersOfSide(factionName) == 0);
	}


    //------------------------------------------------------------------------------------------------
	void mainLoop()
	{
		if (m_skipWinConditions || !(GameModeStarted()))
        {
			Print(string.Format("Breaking Contact - Game not started yet"), LogLevel.NORMAL);
			return;
		}
		
        // skip win conditions if active
		if (GameModeStarted() && !(GameModeOver())) {
			CheckWinConditions();
            Print(string.Format("Breaking Contact - Checking Win Conditions..."), LogLevel.NORMAL);
		}

        Print(string.Format("Breaking Contact -  Main Loop Tick"), LogLevel.NORMAL);
	}


    //------------------------------------------------------------------------------------------------
	void CheckWinConditions(int phase)
	{
		// in debug mode we want to test alone without ending the game
		bool bluforEliminated = factionEliminated("US") && !m_debug;
		bool opforEliminated = factionEliminated("USSR") && !m_debug;
        bool isOver;
		
		if (bluforEliminated) {
			isOver = true;
			m_winnerSide = "opfor";
			Print(string.Format("Breaking Contact - Blufor eliminated"), LogLevel.NORMAL);
		}
		
		if (opforEliminated || m_bluforCaptured) {
			isOver = true;
			m_winnerSide = "blufor";
			Print(string.Format("Breaking Contact - Opfor eliminated"), LogLevel.NORMAL);
		}
		
		// needs to be on last position as would risk to be overwritten
		if (bluforEliminated && opforEliminated) {
			isOver = true;
			m_winnerSide = "draw";
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
        m_iBreakingContactPhase == EBreakingContactPhase.GAME
    }


    //------------------------------------------------------------------------------------------------
    protected bool GameModeOver()
    {
        m_iBreakingContactPhase == EBreakingContactPhase.GAMEOVER
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
	protected bool spawnTransmissionPoint(vector center, int radius)
	{		
		vector newCenter = findSpawnPoint(center, radius);
		
		EntitySpawnParams params = new EntitySpawnParams();
        params.Transform[3] = newCenter;
		
        // create antenna that serves as component holder for transmission point
        Resource ressource = Resource.Load("{5B8922E61D8DF345}Prefabs/Props/Military/Antennas/Antenna_R161_01.et");
        IEntity transmissionPoint = GetGame().SpawnEntityPrefab(ressource, GetGame().GetWorld(), params);

        addTransmissionPoint(transmissionPoint); // add to array
		
		return true;
	}


    //------------------------------------------------------------------------------------------------
    protected vector findSpawnPoint(vector center, int radius)
    {
        bool foundSpawnPoint;

        while (!foundSpawnPoint) {
            Math.Randomize(-1);
            int randomDistanceX = Math.RandomInt( radius, radius );
            int randomDistanceY = Math.RandomInt( radius, radius );
            center[0] += randomDistanceX;
            center[2] += randomDistanceY;
            
            center = MapPosToWorldPos(center[0], center[2]); // add surfaceY on that position
            bool spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(center, center, 2, 2);

            if (spawnEmpty) {
                foundSpawnPoint = true;
            }
            Sleep(50);
        }

        return center;
    }


    //------------------------------------------------------------------------------------------------
	protected void addTransmissionPoint(IEntity transmissionPoint)
	{
        m_transmissionPoints.Insert(transmissionPoint);		
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
	void TeleportFactionToMapPos(Faction faction, string factionName, vector worldPos[3], bool isdebug)
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
				Print(string.Format("Breaking Contact - Player with ID %1 is Member of Faction %2 and will be teleported to %3", playerId, playerFaction.GetFactionKey(), mapPos), LogLevel.NORMAL);
				playerController.TeleportPlayerToMapPos(playerId, mapPos);
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
}
