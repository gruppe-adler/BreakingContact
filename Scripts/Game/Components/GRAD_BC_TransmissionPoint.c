[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "adds transmission point management")]
class GRAD_BC_TransmissionComponentClass : ScriptComponentClass
{
}

// entity to be able to work without spawning an actual antenna
class GRAD_BC_TransmissionComponent : ScriptComponent
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "MinDistance", params: "", category: "Breaking Contact - Transmission Point")];
	protected int m_TransmissionPointMinDistance;

	[RplProp()]
	protected ETransmissionState m_eTransmissionState;

	[RplProp()]
	protected float m_iTransmissionProgress;

	static float m_iTransmissionDuration = 30.0; // todo make param, 120s for debug
	static float m_iTransmissionUpdateTickSize = 1.0 /m_iTransmissionDuration;

	private RplComponent m_RplComponent;

	protected bool m_bTransmissionActive;
	
	[RplProp()]
	vector m_position;

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		//Print("BC Debug - OnPostInit()", LogLevel.NORMAL);
		
		m_position = owner.GetOrigin();
		Replication.BumpMe();

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
	
	vector GetPosition() 
	{
		return m_position;
	}
	
	void SetPosition(vector center) 
	{
		m_position = center;
	}

	
		// todo move into playercontroller
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
	
	void SetTransmissionActive(bool setState) {
		if (m_bTransmissionActive != setState) {
			m_bTransmissionActive = setState;
			
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
	protected void MainLoop(IEntity owner)
	{
		// this function runs on server-side only
	
		ETransmissionState currentState = GetTransmissionState();

		if (currentState == ETransmissionState.DONE) {
			Print("TPC - skipping mainloop due to transmission state done", LogLevel.NORMAL);
			return;
		}
		
		// todo make string stuff in mapmanager
        float currentProgress = Math.Floor(m_iTransmissionProgress * 100);
		string progressString = string.Format("Antenna: %1\% ...", currentProgress); // % needs to be escaped
		
		// 
		if (currentProgress >= 100) {
			SetTransmissionState(ETransmissionState.DONE);
			currentState = GetTransmissionState(); // update to reflect this change

			GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		};
		 
		switch (currentState)
		{
	   		 case ETransmissionState.TRANSMITTING: {
		        m_iTransmissionProgress += m_iTransmissionUpdateTickSize;
				PrintFormat("m_iTransmissionProgress %1", m_iTransmissionProgress);
	        	break;
			}
					
			case ETransmissionState.DISABLED: {
				progressString = string.Format("Antenna: %1\%", currentProgress); // % needs to be escaped
				break;
			}
	
		    case ETransmissionState.INTERRUPTED: {
				progressString = string.Format("Antenna: %1\%", currentProgress); // % needs to be escaped
				break;
			}
	        
	
	    	case ETransmissionState.DONE: {
				progressString = "Transmission Done";
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
