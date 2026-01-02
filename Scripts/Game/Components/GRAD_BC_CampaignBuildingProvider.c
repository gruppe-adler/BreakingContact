modded class SCR_CampaignBuildingProviderComponent
{
	protected GRAD_BC_VehicleSupplyComponent m_SupplyComp;
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		Print("[GRAD BC Supply] Provider component initialized!", LogLevel.NORMAL);
		
		// Cache the supply component reference
		m_SupplyComp = GRAD_BC_VehicleSupplyComponent.Cast(owner.FindComponent(GRAD_BC_VehicleSupplyComponent));
		
		if (m_SupplyComp)
		{
			Print(string.Format("[GRAD BC Supply] Found supply component with %1/%2 supplies", 
				m_SupplyComp.GetCurrentSupplies(), m_SupplyComp.GetMaxSupplies()), LogLevel.NORMAL);
		}
		else
		{
			Print("[GRAD BC Supply] WARNING: No supply component found!", LogLevel.WARNING);
		}
	}
}
