[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "")]
class GRAD_BC_RadioTruckComponentClass : ScriptComponentClass
{
}

class GRAD_BC_RadioTruckComponent : ScriptComponent
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "Update Interval", params: "", category: "Breaking Contact - Radio Truck")];
	protected int m_iRadioTransmissionUpdateInterval;
	

	private bool m_bIsTransmitting;
	
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
		
		if(m_RplComponent.IsMaster())
			GetGame().GetCallqueue().CallLater(mainLoop, 1000, true);
	}
	
	
	//------------------------------------------------------------------------------------------------
	void mainLoop()
	{
		if (m_bIsTransmitting)
        {
			Print(string.Format("Breaking Contact RTC - Transmitting..."), LogLevel.NORMAL);
			
			// todo
			IEntity TPCAntenna = getNearestTransmissionPoint(m_radioTruck.GetOrigin());
			GRAD_BC_TransmissionPointComponent TPC = GRAD_BC_TransmissionPointComponent.Cast(TPCAntenna.FindComponent(GRAD_BC_TransmissionPointComponent));
			TPC.SetTransmissionState(true);
		}

        Print(string.Format("Breaking Contact RTC -  Main Loop Tick"), LogLevel.NORMAL);
	}
	
	
	
	
	//------------------------------------------------------------------------------------------------
	protected IEntity getNearestTransmissionPoint(vector center)
	{
		// searches for nearest transmission object, identified by GRAD_BC_TransmissionPointComponent inside
	  	array<IEntity> nearestTransmissions = new array<IEntity>;
	  	GetGame().GetWorld().QueryEntitiesBySphere(center, 3000, findFirstTransmissionObject, filterTransmissionObjects);
		if (nearestTransmissions.Count() > 0) {
			
			Print(string.Format("Breaking Contact RTC -  get nearest Transmission Point"), LogLevel.NORMAL);
			
				return nearestTransmissions[0];
		} else {
			GRAD_BreakingContactManager BCM = GRAD_BreakingContactManager.Cast(GetGame().FindEntity("GRAD_BreakingContactManager"));
			IEntity transmissionPoint = BCM.spawnTransmissionPoint(center, 10);
			
			Print(string.Format("Breaking Contact RTC -  Create TransmissionPoint"), LogLevel.NORMAL);
			
			return transmissionPoint;
			
		}
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
	
	bool GetTransmissionState() {
		return m_bIsTransmitting;
	}
	
	void SetTransmissionActive(bool setTo) {
		m_bIsTransmitting = setTo;
	}
	
	int GetTransmissionDuration() {
		int duration;
				
		// todo get nearest transmission, get duration of that
		
		return duration;
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
