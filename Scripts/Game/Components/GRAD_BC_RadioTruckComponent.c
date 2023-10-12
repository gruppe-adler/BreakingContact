[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "")]
class GRAD_BC_RadioTruckComponentClass : ScriptComponentClass
{
}

class GRAD_BC_RadioTruckComponent : ScriptComponent
{
	[Attribute(defvalue: "2000", uiwidget: UIWidgets.EditBox, desc: "Update Interval for checking EngineOn() and show/hide marker", params: "", category: "Breaking Contact - Radio Truck")];
	protected int m_iMarkerUpdateInterval;
	
	protected Vehicle m_radioTruck;
	
	protected SCR_MapDescriptorComponent m_mapDescriptorComponent;
	protected VehicleWheeledSimulation_SA m_VehicleWheeledSimulationComponent;
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		//Print("BC Debug - OnPostInit()", LogLevel.NORMAL);
		
		m_radioTruck = Vehicle.Cast(GetOwner());
		
		m_mapDescriptorComponent = SCR_MapDescriptorComponent.Cast(m_radioTruck.FindComponent(SCR_MapDescriptorComponent));
		m_VehicleWheeledSimulationComponent = VehicleWheeledSimulation_SA.Cast(m_radioTruck.FindComponent(VehicleWheeledSimulation_SA));
		
		GetGame().GetCallqueue().CallLater(SetRadioTruckMarkerVisibility, m_iMarkerUpdateInterval, true); 
	}
	
	//------------------------------------------------------------------------------------------------
	void SetRadioTruckMarkerVisibility()
	{
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
	
		if (m_VehicleWheeledSimulationComponent.EngineIsOn())
		{			
			m_mapDescriptorComponent.Item().SetVisible(false);
			//SCR_HintManagerComponent.GetInstance().ShowCustomHint("MARKER OFF");
			//Print("BC Debug - marker off", LogLevel.NORMAL);
		}
		else
		{
			m_mapDescriptorComponent.Item().SetVisible(true);
			//SCR_HintManagerComponent.GetInstance().ShowCustomHint("MARKER ON");
			//Print("BC Debug - marker on", LogLevel.NORMAL);
		}
	}
}