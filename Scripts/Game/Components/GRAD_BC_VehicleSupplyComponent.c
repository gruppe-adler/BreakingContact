class GRAD_BC_VehicleSupplyComponentClass: ScriptComponentClass
{
};

class GRAD_BC_VehicleSupplyComponent : ScriptComponent
{
	[Attribute("1000", UIWidgets.EditBox, "Starting supply amount")]
	int m_iCurrentSupplies;
	
	[Attribute("1000", UIWidgets.EditBox, "Maximum supply amount")]
	int m_iMaxSupplies;
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		// Ensure supplies are initialized to 1000 if they're 0
		if (m_iCurrentSupplies == 0)
			m_iCurrentSupplies = 1000;
		if (m_iMaxSupplies == 0)
			m_iMaxSupplies = 1000;
		
		Print(string.Format("[GRAD BC Supply Component] Initialized on entity: %1", owner), LogLevel.NORMAL);
		Print(string.Format("[GRAD BC Supply Component] Supplies: %1/%2", m_iCurrentSupplies, m_iMaxSupplies), LogLevel.NORMAL);
		
		// Sync with vanilla resource system so budget checking works
		SyncToVanillaResourceSystem(owner);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void SyncToVanillaResourceSystem(IEntity owner)
	{
		SCR_ResourceComponent resourceComp = SCR_ResourceComponent.FindResourceComponent(owner);
		if (!resourceComp)
			return;
		
		SCR_ResourceContainer container = resourceComp.GetContainer(EResourceType.SUPPLIES);
		if (container)
		{
			container.SetResourceValue(m_iCurrentSupplies);
			Print(string.Format("[GRAD BC Supply] Synced %1 supplies to vanilla container", m_iCurrentSupplies), LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	int GetCurrentSupplies()
	{
		return m_iCurrentSupplies;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetMaxSupplies()
	{
		return m_iMaxSupplies;
	}
	
	//------------------------------------------------------------------------------------------------
	bool HasSupplies(int cost)
	{
		return m_iCurrentSupplies >= cost;
	}
	
	//------------------------------------------------------------------------------------------------
	bool DeductSupplies(int cost)
	{
		if (!HasSupplies(cost))
		{
			Print(string.Format("[GRAD BC Supply] Cannot deduct %1 - only have %2!", cost, m_iCurrentSupplies), LogLevel.WARNING);
			return false;
		}
			
		m_iCurrentSupplies -= cost;
		Print(string.Format("[GRAD BC Supply] Deducted %1 supplies. Remaining: %2/%3", cost, m_iCurrentSupplies, m_iMaxSupplies), LogLevel.NORMAL);
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	void AddSupplies(int amount)
	{
		m_iCurrentSupplies = Math.Min(m_iCurrentSupplies + amount, m_iMaxSupplies);
		Print(string.Format("[GRAD BC Supply] Added %1 supplies. Current: %2/%3", amount, m_iCurrentSupplies, m_iMaxSupplies), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetSupplies(int amount)
	{
		int oldValue = m_iCurrentSupplies;
		m_iCurrentSupplies = Math.Clamp(amount, 0, m_iMaxSupplies);
		Print(string.Format("[GRAD BC Supply] SetSupplies: %1 -> %2 (requested %3)", oldValue, m_iCurrentSupplies, amount), LogLevel.NORMAL);
		
		// Sync to vanilla resource system for budget checking
		IEntity owner = GetOwner();
		if (owner)
			SyncToVanillaResourceSystem(owner);
	}
	
	//------------------------------------------------------------------------------------------------
	// Building budget methods for GRAD_BC_CampaignBuildingProviderComponent integration
	//------------------------------------------------------------------------------------------------
	int GetCurrentBuildingBudget()
	{
		return m_iCurrentSupplies;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetMaxBuildingBudget()
	{
		return m_iMaxSupplies;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetCurrentBuildingBudget(int value)
	{
		SetSupplies(value);
	}
	
	//------------------------------------------------------------------------------------------------
	void AddBuildingBudget(int amount)
	{
		AddSupplies(amount);
	}
}
