[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "")]
class GRAD_BC_RadioTruckComponentClass : ScriptComponentClass
{
}

class GRAD_BC_RadioTruckComponent : ScriptComponent
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "Update Interval", params: "", category: "Breaking Contact - Radio Truck")];
	protected int m_iRadioTransmissionUpdateInterval;
	

	
	private Vehicle m_radioTruck;
	
	private SCR_MapDescriptorComponent m_mapDescriptorComponent;
	private VehicleWheeledSimulation_SA m_VehicleWheeledSimulationComponent;
	
	private RplComponent m_RplComponent;
	
	private GRAD_BC_TransmissionPointComponent m_nearestTransmissionPoint;
	
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
	protected IEntity getNearestTransmissionPoint(vector center)
	{
		// searches for nearest transmission object, identified by GRAD_BC_TransmissionPointComponent inside
	  	array<IEntity> nearestTransmissions = new array<IEntity>;
	  	GetGame().GetWorld().QueryEntitiesBySphere(center, 3000, findFirstTransmissionObject, filterTransmissionObjects);
		if (nearestTransmissions.Count() > 0) {
				return nearestTransmissions[0];
		} else {
			IEntity RadioTruckComponent = GetGame().FindEntity("GRAD_BC_RadioTruckComponent");
			RadioTruckComponent.spawnTransmissionPoint(center);
		}
		
		return center;
	}
	
	//------------------------------------------------------------------------------------------------
	// filters for GRAD_BC_TransmissionPointComponent
	protected bool filterTransmissionObjects(IEntity ent) 
	{
	  return (ent.FindComponent(GRAD_BC_TransmissionPointComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	// stops filter at first successful hit
	protected bool findFirstTransmissionObject(IEntity ent)
    {	
		m_nearestTransmissionPoint = GRAD_BC_TransmissionPointComponent.Cast(ent.FindComponent(GRAD_BC_TransmissionPointComponent));
        if (!m_nearestTransmissionPoint)
            return true; //Continue search

        return false; //Stop search
    }
	
	
	
	/*
	
	 //Check if garage is nearby
        GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), m_fGarageSearchRadius, FindFirstGarage, FilterGarage);
        return (m_GarageManager);
     }

    //------------------------------------------------------------------------------------------------
    bool FilterGarage(IEntity ent)
    {
        return (ent.FindComponent(EL_GarageManagerComponent));
    }

    //------------------------------------------------------------------------------------------------
    bool FindFirstGarage(IEntity ent)
    {
        m_GarageManager = EL_GarageManagerComponent.Cast(ent.FindComponent(EL_GarageManagerComponent));
        if (!m_GarageManager)
            return true; //Continue search

        return false; //Stop search
    }
	
	*/
	


	

	
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
