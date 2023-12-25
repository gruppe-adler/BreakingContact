enum ERadioTransmissionState
{
	OFF,
	TRANSMITTING,
	INTERRUPTED,
	DONE
}

[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "")]
class GRAD_BC_TransmissionPointComponentClass : ScriptComponentClass
{
}

class GRAD_BC_TransmissionPointComponent : ScriptComponent
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "MinDistance", params: "", category: "Breaking Contact - Transmission Point")];
	protected int m_TransmissionPointMinDistance;

	[RplProp()]
	protected ERadioTransmissionState m_eRadioTransmissionState;

	[RplProp()]	
	protected int m_iRadioTransmissionDuration;

	//------------------------------------------------------------------------------------------------
	int GetRadioTransmissionDuration()
	{
		//RpcAsk_Authority_SyncVariables();
		
		return m_iRadioTransmissionDuration;
	}

	//------------------------------------------------------------------------------------------------
	ERadioTransmissionState GetRadioTransmissionState()
	{
		return m_eRadioTransmissionState;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetRadioTransmissionState(ERadioTransmissionState radioTransmissionState)
	{
		m_eRadioTransmissionState = radioTransmissionState;
		
		if (m_eRadioTransmissionState == ERadioTransmissionState.TRANSMITTING)
			SetRadioTruckMarkerVisibility(true);
		else
			SetRadioTruckMarkerVisibility(false);
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
		//Print("BC Debug - RpcAsk_Authority_SyncRadioTransmissionDuration()", LogLevel.NORMAL);
		
		Replication.BumpMe();
	}
	
	//------------------------------------------------------------------------------------------------
	protected void UpdateRadioTransmissionDuration()
	{
		// this function runs on server-side only
		
		//Print("BC Debug - UpdateRadioTransmissionDuration()", LogLevel.NORMAL);
		
		if (GetRadioTransmissionState() == ERadioTransmissionState.TRANSMITTING)
		{
			m_iRadioTransmissionDuration += m_iRadioTransmissionUpdateInterval;
			PrintFormat("m_iRadioTransmissionDuration: %1", m_iRadioTransmissionDuration);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void SetRadioTruckMarkerVisibility(bool enableMarkerVisibility)
	{
		// this function runs on server-side only
		
		//Print("BC Debug - CheckMarker()", LogLevel.NORMAL);
		
		if (!m_radioTruck)
		{
			Print("BC Debug - m_radioTruck is null", LogLevel.ERROR);
			return;
		}
			
		if (!m_VehicleWheeledSimulationComponent)
		{
			Print("BC Debug - m_VehicleWheeledSimulationComponent is null", LogLevel.ERROR);
			return;
		}
		
		if (!m_mapDescriptorComponent)
		{
			Print("BC Debug - m_mapDescriptorComponent is null", LogLevel.ERROR);
			return;
		}
	
		// m_VehicleWheeledSimulationComponent.EngineIsOn()
		
		if (enableMarkerVisibility)
		{
			m_mapDescriptorComponent.Item().SetVisible(true);
			//Rpc(RpcAsk_Authority_SetMarkerVisibility, true);
			//SCR_HintManagerComponent.GetInstance().ShowCustomHint("MARKER ON");
			Print("BC Debug - marker on", LogLevel.NORMAL);
		}
		else
		{
			m_mapDescriptorComponent.Item().SetVisible(false);
			//Rpc(RpcAsk_Authority_SetMarkerVisibility, false);
			//SCR_HintManagerComponent.GetInstance().ShowCustomHint("MARKER OFF");
			Print("BC Debug - marker off", LogLevel.NORMAL);
		}
	}
}
