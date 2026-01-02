modded class SCR_CampaignBuildingStartUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		Print("========================================", LogLevel.NORMAL);
		Print("[GRAD BC Supply] PerformAction called!", LogLevel.NORMAL);
		Print(string.Format("[GRAD BC Supply] Owner entity: %1", GetOwner()), LogLevel.NORMAL);
		
		// Simple flat cost deduction - deduct 100 supplies per vehicle spawn
		GRAD_BC_VehicleSupplyComponent supplyComp = GRAD_BC_VehicleSupplyComponent.Cast(GetOwner().FindComponent(GRAD_BC_VehicleSupplyComponent));
		
		Print(string.Format("[GRAD BC Supply] Supply component found: %1", supplyComp != null), LogLevel.NORMAL);
		
		if (supplyComp)
		{
			int currentSupplies = supplyComp.GetCurrentSupplies();
			int maxSupplies = supplyComp.GetMaxSupplies();
			int cost = 100;
			
			Print(string.Format("[GRAD BC Supply] Current supplies: %1/%2", currentSupplies, maxSupplies), LogLevel.NORMAL);
			Print(string.Format("[GRAD BC Supply] Vehicle cost: %1", cost), LogLevel.NORMAL);
			Print(string.Format("[GRAD BC Supply] Has enough supplies: %1", supplyComp.HasSupplies(cost)), LogLevel.NORMAL);
			
			if (!supplyComp.HasSupplies(cost))
			{
				Print("[GRAD BC Supply] NOT ENOUGH SUPPLIES - BLOCKING SPAWN!", LogLevel.WARNING);
				Print("========================================", LogLevel.NORMAL);
				return; // Don't spawn
			}
			
			Print("[GRAD BC Supply] Deducting supplies...", LogLevel.NORMAL);
			bool deducted = supplyComp.DeductSupplies(cost);
			Print(string.Format("[GRAD BC Supply] Deduction successful: %1", deducted), LogLevel.NORMAL);
			
			if (!deducted)
			{
				Print("[GRAD BC Supply] DEDUCTION FAILED - BLOCKING SPAWN!", LogLevel.ERROR);
				Print("========================================", LogLevel.NORMAL);
				return;
			}
			
			Print(string.Format("[GRAD BC Supply] New balance: %1/%2", supplyComp.GetCurrentSupplies(), supplyComp.GetMaxSupplies()), LogLevel.NORMAL);
		}
		else
		{
			Print("[GRAD BC Supply] WARNING: No supply component found - spawning without cost!", LogLevel.WARNING);
		}
		
		Print("[GRAD BC Supply] Calling super.PerformAction to spawn vehicle...", LogLevel.NORMAL);
		Print("========================================", LogLevel.NORMAL);
		
		// Perform the actual spawn
		super.PerformAction(pOwnerEntity, pUserEntity);
	}
}
