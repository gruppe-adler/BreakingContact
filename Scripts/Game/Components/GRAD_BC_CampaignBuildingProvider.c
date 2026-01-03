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
	
	//------------------------------------------------------------------------------------------------
	// Override to include vehicle supplies in the budget value display
	override int GetBudgetValue(EEditableEntityBudget type, out SCR_CampaignBuildingProviderComponent componentToUse)
	{
		// Handle vehicle supplies using the CAMPAIGN budget type (this is what the UI displays as "Available Supplies")
		if (type == EEditableEntityBudget.CAMPAIGN && m_SupplyComp)
		{
			componentToUse = this;
			// Return SPENT amount (Max - Current) so UI shows "Spent / Max"
			return m_SupplyComp.GetMaxSupplies() - m_SupplyComp.GetCurrentSupplies();
		}
		
		return super.GetBudgetValue(type, componentToUse);
	}
	
	//------------------------------------------------------------------------------------------------
	// Override to set max budget value for vehicle supplies display
	override int GetMaxBudgetValue(EEditableEntityBudget budget)
	{
		if (budget == EEditableEntityBudget.CAMPAIGN && m_SupplyComp)
		{
			return m_SupplyComp.GetMaxSupplies();
		}
		
		return super.GetMaxBudgetValue(budget);
	}
	
	//------------------------------------------------------------------------------------------------
	// Override budget check to include custom vehicle supply budget
	override bool IsThereEnoughBudgetToSpawn(notnull array<ref SCR_EntityBudgetValue> budgetCosts)
	{
		// First check vanilla budgets
		if (!super.IsThereEnoughBudgetToSpawn(budgetCosts))
			return false;
		
		// Then check if this spawn requires vehicle supplies (has any budget cost, indicating it's a vehicle)
		if (!budgetCosts.IsEmpty() && m_SupplyComp)
		{
			int cost = 100;
			bool hasSupplies = m_SupplyComp.HasSupplies(cost);
			
			Print(string.Format("[GRAD BC Supply] Budget check - Has %1 supplies: %2", cost, hasSupplies), LogLevel.NORMAL);
			
			if (!hasSupplies)
			{
				Print("[GRAD BC Supply] BLOCKING SPAWN - Insufficient vehicle supplies!", LogLevel.WARNING);
				return false;
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Override to deduct from vehicle supplies instead of props value
	override void AddPropValue(int value)
	{
		// If deducting (negative value) and we have a supply component, deduct from supplies
		if (value < 0 && m_SupplyComp)
		{
			// Don't call super - we handle it with our supply system
			Replication.BumpMe();
			return;
		}
		
		super.AddPropValue(value);
	}
	
	//------------------------------------------------------------------------------------------------
	// Called after an entity is spawned - deduct supplies for vehicles
	override void EntitySpawnedByProvider(int prefabID, SCR_EditableEntityComponent editableEntity)
	{
		super.EntitySpawnedByProvider(prefabID, editableEntity);
		
		if (!editableEntity)
			return;
		
		IEntity spawnedEntity = editableEntity.GetOwner();
		if (!spawnedEntity)
			return;
		
		// Check if this is a vehicle
		Vehicle vehicle = Vehicle.Cast(spawnedEntity);
		if (!vehicle)
			return;
		
		if (!m_SupplyComp)
		{
			Print("[GRAD BC Supply] No supply component - vehicle spawned without cost!", LogLevel.WARNING);
			return;
		}
		
		int cost = 100;
		Print(string.Format("[GRAD BC Supply] Vehicle spawned, deducting %1 supplies...", cost), LogLevel.NORMAL);
		
		bool deducted = m_SupplyComp.DeductSupplies(cost);
		Print(string.Format("[GRAD BC Supply] Deduction result: %1", deducted), LogLevel.NORMAL);
		Print(string.Format("[GRAD BC Supply] New balance: %1/%2", 
			m_SupplyComp.GetCurrentSupplies(), m_SupplyComp.GetMaxSupplies()), LogLevel.NORMAL);
		
		// Force UI update by manipulating the replicated prop value to trigger change detection
		// The UI watches m_iCurrentPropValue for changes
		int currentProps = m_iCurrentPropValue;
		SetPropValue(currentProps + 1);
		SetPropValue(currentProps);
		
		// Also bump replication
		Replication.BumpMe();
	}
}
