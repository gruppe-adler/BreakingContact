[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "")]
class GRAD_BC_RadioTruckComponentClass : ScriptComponentClass
{
}

class GRAD_BC_RadioTruckComponent : ScriptComponent
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "Update Interval", params: "", category: "Breaking Contact - Radio Truck")];
	protected int m_iRadioTransmissionUpdateInterval;
	
	[RplProp()]
	protected ERadioTransmissionState m_eRadioTransmissionState;

	[RplProp()]	
	protected int m_iRadioTransmissionDuration;
	
	private Vehicle m_radioTruck;
	
	private SCR_MapDescriptorComponent m_mapDescriptorComponent;
	private VehicleWheeledSimulation_SA m_VehicleWheeledSimulationComponent;
	
	private RplComponent m_RplComponent;
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		//Print("BC Debug - OnPostInit()", LogLevel.NORMAL);
		
		m_radioTruck = Vehicle.Cast(GetOwner());
		
		m_mapDescriptorComponent = SCR_MapDescriptorComponent.Cast(m_radioTruck.FindComponent(SCR_MapDescriptorComponent));
		m_VehicleWheeledSimulationComponent = VehicleWheeledSimulation_SA.Cast(m_radioTruck.FindComponent(VehicleWheeledSimulation_SA));
		
		m_RplComponent = RplComponent.Cast(m_radioTruck.FindComponent(RplComponent));
		
		//PrintFormat("BC Debug - IsMaster(): %1", m_RplComponent.IsMaster()); // IsMaster() does not mean Authority
		//PrintFormat("BC Debug - IsProxy(): %1", m_RplComponent.IsProxy());
		//PrintFormat("BC Debug - IsOwner(): %1", m_RplComponent.IsOwner());
		
		// Initially set radio transmission state to off and disable the map marker
		//SetRadioTransmissionState(m_eRadioTransmissionState);
		GetGame().GetCallqueue().CallLater(SetRadioTransmissionState, 5000, false, m_eRadioTransmissionState);
		
		if(m_RplComponent.IsMaster())
			GetGame().GetCallqueue().CallLater(UpdateRadioTransmissionDuration, m_iRadioTransmissionUpdateInterval, true);
	}

	//------------------------------------------------------------------------------------------------
	int GetRadioTransmissionDuration()
	{
		//RpcAsk_Authority_SyncVariables();
		
		return m_iRadioTransmissionDuration;
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
	
	/*
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Authority_SetMarkerVisibility(bool isVisible)
	{
		Print("BC Debug - RpcAsk_Authority_SetMarkerVisibility()", LogLevel.NORMAL);
		
		m_mapDescriptorComponent.Item().SetVisible(isVisible);
		
		m_bIsVisible = isVisible;

		Replication.BumpMe();
		
		Rpc(RpcDo_Broadcast_SetMarkerVisibility, isVisible);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_Broadcast_SetMarkerVisibility(bool isVisible)
	{
		Print("BC Debug - RpcDo_Broadcast_SetMarkerVisibility()", LogLevel.NORMAL);
		
		m_mapDescriptorComponent.Item().SetVisible(isVisible);
	}
	*/
}

enum ERadioTransmissionState
{
	OFF,
	TRANSMITTING,
	INTERRUPTED,
	DONE
}