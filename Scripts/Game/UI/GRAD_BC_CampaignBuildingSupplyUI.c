//------------------------------------------------------------------------------------------------
//! Custom UI component to display vehicle supply budget in building mode
class GRAD_BC_CampaignBuildingSupplyUIComponent : SCR_BaseEditorUIComponent
{
	protected TextWidget m_wSupplyValueCurrent;
	protected TextWidget m_wSupplyValueMax;
	protected ImageWidget m_wSupplyIcon;
	
	protected GRAD_BC_VehicleSupplyComponent m_VehicleSupplyComponent;
	
	//------------------------------------------------------------------------------------------------
	override void HandlerAttachedScripted(Widget w)
	{
		super.HandlerAttachedScripted(w);
		
		// Find the text widgets
		m_wSupplyValueCurrent = TextWidget.Cast(w.FindAnyWidget("Supply_Value_Current"));
		m_wSupplyValueMax = TextWidget.Cast(w.FindAnyWidget("Supply_Value_Max"));
		m_wSupplyIcon = ImageWidget.Cast(w.FindAnyWidget("Supply_Icon"));
		
		Print("[GRAD BC Supply UI] HandlerAttached - widgets found: Current=" + (m_wSupplyValueCurrent != null) + ", Max=" + (m_wSupplyValueMax != null), LogLevel.NORMAL);
		
		// Start update loop
		GetGame().GetCallqueue().CallLater(UpdateSupplyDisplay, 100, true);
	}
	
	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
		super.HandlerDeattached(w);
		GetGame().GetCallqueue().Remove(UpdateSupplyDisplay);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void UpdateSupplyDisplay()
	{
		if (!m_wSupplyValueCurrent || !m_wSupplyValueMax)
		{
			Print("[GRAD BC Supply UI] Widgets not found!", LogLevel.WARNING);
			return;
		}
		
		// Find the provider entity that has our vehicle supply component
		// Since there's typically only one combox with GRAD_BC_VehicleSupplyComponent in the game,
		// we can search for it through the building manager or nearby the player
		if (!m_VehicleSupplyComponent)
		{
			// First, try to get it from the game mode's building manager
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (!gameMode)
			{
				Print("[GRAD BC Supply UI] No game mode found", LogLevel.WARNING);
				return;
			}
			
			SCR_CampaignBuildingManagerComponent buildingManager = SCR_CampaignBuildingManagerComponent.Cast(gameMode.FindComponent(SCR_CampaignBuildingManagerComponent));
			if (!buildingManager)
			{
				Print("[GRAD BC Supply UI] No building manager found", LogLevel.WARNING);
				return;
			}
			
			// Since we can't easily get the provider from the building manager,
			// let's search through military base system which tracks all bases
			SCR_MilitaryBaseSystem baseSystem = SCR_MilitaryBaseSystem.GetInstance();
			if (!baseSystem)
			{
				Print("[GRAD BC Supply UI] No military base system found", LogLevel.WARNING);
				return;
			}
			
			// Get all registered bases
			array<SCR_MilitaryBaseComponent> allBases = {};
			baseSystem.GetBases(allBases);
			
			Print(string.Format("[GRAD BC Supply UI] Searching %1 bases for vehicle supply component...", allBases.Count()), LogLevel.NORMAL);
			
			foreach (SCR_MilitaryBaseComponent base : allBases)
			{
				if (!base)
					continue;
				
				// Get all providers at this base
				array<SCR_CampaignBuildingProviderComponent> providers = {};
				base.GetBuildingProviders(providers);
				
				foreach (SCR_CampaignBuildingProviderComponent provider : providers)
				{
					if (!provider)
						continue;
					
					IEntity providerEntity = provider.GetOwner();
					if (!providerEntity)
						continue;
					
					// Check if this provider has our vehicle supply component
					GRAD_BC_VehicleSupplyComponent supplyComp = GRAD_BC_VehicleSupplyComponent.Cast(providerEntity.FindComponent(GRAD_BC_VehicleSupplyComponent));
					if (supplyComp)
					{
						m_VehicleSupplyComponent = supplyComp;
						Print(string.Format("[GRAD BC Supply UI] Found supply component on provider: %1 with %2/%3 supplies", 
							providerEntity, supplyComp.GetCurrentSupplies(), supplyComp.GetMaxSupplies()), LogLevel.NORMAL);
						break;
					}
				}
				
				if (m_VehicleSupplyComponent)
					break;
			}
			
			if (!m_VehicleSupplyComponent)
			{
				Print("[GRAD BC Supply UI] No vehicle supply component found on any provider entity", LogLevel.WARNING);
				return;
			}
		}
		
		// Update the display using vehicle supply component
		if (m_VehicleSupplyComponent)
		{
			int current = m_VehicleSupplyComponent.GetCurrentSupplies();
			int max = m_VehicleSupplyComponent.GetMaxSupplies();
			
			m_wSupplyValueCurrent.SetText(current.ToString());
			m_wSupplyValueMax.SetText(max.ToString());
			
			// Debug logging every 5 seconds
			static int updateCounter = 0;
			updateCounter++;
			if (updateCounter % 50 == 0)
			{
				Print(string.Format("[GRAD BC Supply UI] Displaying vehicle supplies: %1/%2", current, max), LogLevel.NORMAL);
			}
		}
	}
}
