[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "adds transmission point management")]
class GRAD_BC_TransmissionComponentClass : ScriptComponentClass
{
}

enum ETransmissionState
{
	OFF,
	TRANSMITTING,
	INTERRUPTED,
	DISABLED,
	DONE
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

		if (!Replication.IsServer())
			return;
		
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));

		// Wait until the prefab has a valid RplComponent attached
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (Replication.FindItemId(owner) == Replication.INVALID_ID)
		{
			// Try again next frame – prevents race conditions
			GetGame().GetCallqueue().CallLater(EOnInit, 0, false, owner);
			PrintFormat("TPC EOnInit trying again next frame");
			return;
		}
	
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm)
			bcm.RegisterTransmissionComponent(this);
	
		// State machine tick (server only)
		GetGame().GetCallqueue().CallLater(MainLoop, 1000, true, owner);
	}
	
	override void OnDelete(IEntity owner)
	{
	    GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
	    if (bcm) bcm.UnregisterTransmissionComponent(this);
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
		Replication.BumpMe();
	}

	
	//------------------------------------------------------------------------------------------------
	private void SetTransmissionState(ETransmissionState transmissionState)
	{
		if (m_eTransmissionState != transmissionState) {
			m_eTransmissionState = transmissionState;
			
			Replication.BumpMe();
		}
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
				Replication.BumpMe();
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
