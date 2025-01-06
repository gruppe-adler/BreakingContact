enum ETransmissionState
{
	OFF,
	TRANSMITTING,
	INTERRUPTED,
	DISABLED,
	DONE
}

[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "")]
class GRAD_BC_TransmissionPointComponentClass : GenericEntityClass
{
}

// entity to be able to work without spawning an actual antenna
class GRAD_BC_TransmissionPointComponent : GenericEntity
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "MinDistance", params: "", category: "Breaking Contact - Transmission Point")];
	protected int m_TransmissionPointMinDistance;

	[RplProp()]
	protected ETransmissionState m_eTransmissionState;

	[RplProp()]
	protected float m_iTransmissionProgress;

	static float m_iTransmissionDuration = 30.0; // todo make param, 120s for debug
	static float m_iTransmissionUpdateTickSize = 1.0 /m_iTransmissionDuration;

	private SCR_MapDescriptorComponent m_mapDescriptorComponent;

	private RplComponent m_RplComponent;

	protected bool m_bTransmissionActive;
	
	GRAD_BC_TransmissionPointComponent m_TPC;

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		//Print("BC Debug - OnPostInit()", LogLevel.NORMAL);
		
		m_TPC = GRAD_BC_TransmissionPointComponent.Cast(owner);

		m_mapDescriptorComponent = SCR_MapDescriptorComponent.Cast(owner.FindComponent(SCR_MapDescriptorComponent));

		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));

		//PrintFormat("BC Debug - IsMaster(): %1", m_RplComponent.IsMaster()); // IsMaster() does not mean Authority
		//PrintFormat("BC Debug - IsProxy(): %1", m_RplComponent.IsProxy());
		//PrintFormat("BC Debug - IsOwner(): %1", m_RplComponent.IsOwner());

		// Initially set transmission state to off and disable the map marker
		//SetTransmissionState(m_eTransmissionState);
		GetGame().GetCallqueue().CallLater(SetTransmissionState, 5000, false, m_eTransmissionState);

		if (m_RplComponent.IsMaster()) {
			GetGame().GetCallqueue().CallLater(MainLoop, 1000, true, owner);	
		}
		
		// m_transmissionPoint.GetParent().GetParent().RemoveChild(owner, false); // disable attachment hierarchy to radiotruck (?!)
		
	}
	
	
	//------------------------------------------------------------------------------------------------
	float GetTransmissionDuration()
	{
		//RpcAsk_Authority_SyncVariables();

		return m_iTransmissionProgress;
	}

	//------------------------------------------------------------------------------------------------
	ETransmissionState GetTransmissionState()
	{
		return m_eTransmissionState;
	}
	
	//------------------------------------------------------------------------------------------------
	bool GetTransmissionActive()
	{
		return m_bTransmissionActive;
	}

	//------------------------------------------------------------------------------------------------
	private void SetTransmissionState(ETransmissionState transmissionState)
	{
		if (m_eTransmissionState != transmissionState) {
			m_eTransmissionState = transmissionState;
			
			Replication.BumpMe();
		
			/*
			int playerId = GetGame().GetDataCollector().GetPlayerData(GetGame().GetPlayerController().GetPlayerId());
			if (playerId) {
				SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			*/
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController) return;
		SCR_HUDManagerComponent hudmanager = SCR_HUDManagerComponent.Cast(playerController.FindComponent(SCR_HUDManagerComponent));
		
		if (!hudmanager) {
			PrintFormat("TPC - No hudmanager found");
			return;
		}
		
		GRAD_BC_Transmission info = GRAD_BC_Transmission.Cast(hudmanager.FindInfoDisplay(GRAD_BC_Transmission));
		
		if (!info) {
			PrintFormat("TPC - No Info Panel found");
			return;
		}

		switch (m_eTransmissionState)
			{
				case ETransmissionState.TRANSMITTING: {
					info.TransmissionStarted();
					break;
				}

				case ETransmissionState.INTERRUPTED: {
					info.TransmissionInterrupted();
					break;
				}

				case ETransmissionState.DISABLED: {
					info.TransmissionInterrupted();
					break;
				}

				case ETransmissionState.DONE: {
					info.TransmissionDone();
					break;
				}

				default: {
					Print(string.Format("Breaking Contact TPC - No known ETransmissionState %1", m_eTransmissionState), LogLevel.ERROR);
					break;
				}
			}
		}
		Print(string.Format("Breaking Contact TPC - Set Transmissionstate to %1", m_eTransmissionState), LogLevel.NORMAL);
	}


	//------------------------------------------------------------------------------------------------
	
	void SetTransmissionActive(bool setState) {
		Rpc(Rpc_SetTransmissionActive, setState);
	}
	
	// each client on its own
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void Rpc_SetTransmissionActive(bool setState) {
		if (m_bTransmissionActive != setState) {
			m_bTransmissionActive = setState;
			
			RplId entityId = Replication.FindId(m_RplComponent);
		
			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(SCR_PlayerController.GetLocalPlayerId()));
				
			
			if (m_bTransmissionActive) {
				playerController.SetIconMarker(
					"{534DF45C06CFB00C}UI/Textures/Map/transmission_active.edds",
					entityId
				);
				playerController.SetCircleMarkerActive(entityId);
			} else {
				playerController.SetIconMarker(
					"{97BB746698125B85}UI/Textures/Map/transmission_default.edds",
					entityId
				);
				playerController.SetCircleMarkerInactive(entityId);
			}
				
			
			if (m_bTransmissionActive &&
				(
					GetTransmissionState() == ETransmissionState.OFF ||
					GetTransmissionState() == ETransmissionState.INTERRUPTED
				)
			) {
				SetTransmissionState(ETransmissionState.TRANSMITTING);
			};

			if (!setState &&
				(
					GetTransmissionState() == ETransmissionState.TRANSMITTING
				)
			) {
				SetTransmissionState(ETransmissionState.INTERRUPTED);
			};
			PrintFormat("TPC - SetTransmissionActive : %1", GetTransmissionState());

		}
	}


	//------------------------------------------------------------------------------------------------
	void SyncVariables()
	{
		Rpc(RpcAsk_Authority_SyncVariables);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Authority_SyncVariables()
	{
		//Print("BC Debug - RpcAsk_Authority_SyncTransmissionDuration()", LogLevel.NORMAL);

		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected void MainLoop(IEntity owner)
	{
		// this function runs on server-side only
	
		ETransmissionState currentState = GetTransmissionState();

		if (currentState == ETransmissionState.DONE) {
			Print("TPC - skipping mainloop due to transmission state done", LogLevel.NORMAL);
			return;
		}
		
		if (m_mapDescriptorComponent) {
			MapItem item;
			item = m_mapDescriptorComponent.Item();
			MapDescriptorProps props = item.GetProps();
			
	        float currentProgress = Math.Floor(m_iTransmissionProgress * 100);
			string progressString = string.Format("Antenna: %1\% ...", currentProgress); // % needs to be escaped
			
			// 
			if (currentProgress >= 100) {
				SetTransmissionState(ETransmissionState.DONE);
				currentState = GetTransmissionState(); // update to reflect this change

				GRAD_BC_BreakingContactManager BCM = FindBreakingContactManager();
				BCM.AddTransmissionPointDone(m_TPC);
			};
			 
			switch (currentState)
			{
		   		 case ETransmissionState.TRANSMITTING: {
			        m_iTransmissionProgress += m_iTransmissionUpdateTickSize;
			        PrintFormat("m_iTransmissionDuration: %1", m_iTransmissionDuration);
			        PrintFormat("m_iTransmissionUpdateTickSize: %1", m_iTransmissionUpdateTickSize);
			        PrintFormat("m_iTransmissionProgress: %1", m_iTransmissionProgress);
			        item.SetDisplayName(progressString);
			        item.SetInfoText("Transmitting");
			        props.SetIconVisible(true);
			        props.SetBackgroundColor(Color.Red);
			        props.SetFont("{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt");
			        props.SetImageDef("transmission_active");
					props.SetIconVisible(true);
			        props.SetFrontColor(Color.FromRGBA(0, 0, 0, 1));
			        props.SetOutlineColor(Color.Black);
			        props.SetTextColor(Color.Red);
			        props.SetTextSize(60.0, 30.0, 60.0);
			        props.SetIconSize(32, 0.3, 0.3);
			        props.Activate(true);
			        item.SetProps(props);
		        	break;
				}
						
				case ETransmissionState.DISABLED: {
					progressString = string.Format("Antenna: %1\%", currentProgress); // % needs to be escaped
			        props.SetIconVisible(true);
			        item.SetDisplayName(progressString);
			        item.SetInfoText("Transmission disabled");
			        props.SetBackgroundColor(Color.Black);
			        props.SetFont("{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt");
			        props.SetImageDef("transmission_interrupted");
			        props.SetFrontColor(Color.FromRGBA(0, 0, 0, 1));
			        props.SetTextColor(Color.Black);
			        props.SetTextSize(60.0, 30.0, 60.0);
			        props.SetIconSize(32, 0.3, 0.3);
			        props.Activate(true);
			        item.SetProps(props);
					break;
				}
		
			    case ETransmissionState.INTERRUPTED: {
					progressString = string.Format("Antenna: %1\%", currentProgress); // % needs to be escaped
			        props.SetIconVisible(true);
			        item.SetDisplayName(progressString);
			        item.SetInfoText("Transmission interrupted");
			        props.SetBackgroundColor(Color.Black);
			        props.SetFont("{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt");
			        props.SetImageDef("transmission_interrupted");
			        props.SetFrontColor(Color.FromRGBA(0, 0, 0, 1));
			        props.SetTextColor(Color.Black);
			        props.SetTextSize(60.0, 30.0, 60.0);
			        props.SetIconSize(32, 0.3, 0.3);
			        props.Activate(true);
			        item.SetProps(props);
					break;
				}
		        
		
		    	case ETransmissionState.DONE: {
					progressString = "Transmission Done";
			        props.SetIconVisible(true);
			        item.SetInfoText("Transmission Done");
			        item.SetDisplayName("100\%");
			        props.SetFont("{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt");
			        props.SetImageDef("transmission_default");
			        props.SetFrontColor(Color.FromRGBA(0, 0, 0, 1));
			        props.SetTextColor(Color.Green);
			        props.SetTextSize(60.0, 30.0, 60.0);
			        props.SetIconSize(32, 0.3, 0.3);
			        props.Activate(true);
			        item.SetProps(props);
			        break;
				}
		
		   	 	// Add more cases as needed
		
		   		 default: {
		        	// Handle any other state if necessary
		        	break;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SetTransmissionPointMarkerVisibility(bool enableMarkerVisibility)
	{
		// this function runs on server-side only

		if (!m_mapDescriptorComponent)
		{
			Print("BC Debug - m_mapDescriptorComponent is null", LogLevel.ERROR);
			return;
		}

		if (enableMarkerVisibility && !(m_mapDescriptorComponent.Item().IsVisible()))
		{
			m_mapDescriptorComponent.Item().SetVisible(true);
			//Rpc(RpcAsk_Authority_SetMarkerVisibility, true);
			//SCR_HintManagerComponent.GetInstance().ShowCustomHint("MARKER ON");
			Print("BC Debug - marker on", LogLevel.NORMAL);
		}
		else if (!enableMarkerVisibility && (m_mapDescriptorComponent.Item().IsVisible()))
		{
			m_mapDescriptorComponent.Item().SetVisible(false);
			//Rpc(RpcAsk_Authority_SetMarkerVisibility, false);
			//SCR_HintManagerComponent.GetInstance().ShowCustomHint("MARKER OFF");
			Print("BC Debug - marker off", LogLevel.NORMAL);
		}
	}
	
	// find the instance on the server
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

}
