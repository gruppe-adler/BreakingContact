//------------------------------------------------------------------------------------------------
//! Custom building provider that uses GRAD_BC_VehicleSupplyComponent for budget tracking
[EntityEditorProps(category: "GameScripted/Building", description: "GRAD BC custom provider that uses vehicle supply budget.")]
class GRAD_BC_CampaignBuildingProviderComponentClass : SCR_CampaignBuildingProviderComponentClass
{
}

class GRAD_BC_CampaignBuildingProviderComponent : SCR_CampaignBuildingProviderComponent
{
	protected GRAD_BC_VehicleSupplyComponent m_VehicleSupplyComponent;
	
	//------------------------------------------------------------------------------------------------
	//! Get the vehicle supply component from the entity
	protected GRAD_BC_VehicleSupplyComponent GetVehicleSupplyComponent()
	{
		if (!m_VehicleSupplyComponent)
			m_VehicleSupplyComponent = GRAD_BC_VehicleSupplyComponent.Cast(GetOwner().FindComponent(GRAD_BC_VehicleSupplyComponent));
		
		return m_VehicleSupplyComponent;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Override to use vehicle supply component budget
	override int GetBudgetValue(EEditableEntityBudget type, out SCR_CampaignBuildingProviderComponent componentToUse)
	{
		componentToUse = this;
		
		// Check if we should use master provider budget
		SCR_CampaignBuildingProviderComponent masterProviderComponent;
		bool useMaster = UseMasterProviderBudget(type, masterProviderComponent);
		
		if (useMaster && masterProviderComponent)
		{
			componentToUse = masterProviderComponent;
			return masterProviderComponent.GetBudgetValue(type, componentToUse);
		}
		
		// For PROPS budget
		if (type == EEditableEntityBudget.PROPS)
			return GetCurrentPropValue();
		
		// For AI budget
		if (type == EEditableEntityBudget.AI)
			return GetCurrentAIValue();
		
		// For supplies (CAMPAIGN budget) - check vehicle supply component
		if (type == EEditableEntityBudget.CAMPAIGN)
		{
			GRAD_BC_VehicleSupplyComponent vehicleSupply = GetVehicleSupplyComponent();
			if (vehicleSupply)
			{
				Print(string.Format("[GRAD BC Provider] Getting supplies from vehicle: %1/%2", vehicleSupply.GetCurrentBuildingBudget(), vehicleSupply.GetMaxBuildingBudget()), LogLevel.NORMAL);
				return vehicleSupply.GetCurrentBuildingBudget();
			}
			
			// Fallback to resource component
			SCR_ResourceComponent resource = GetResourceComponent();
			if (!resource)
				return 0;
			
			SCR_ResourceConsumer consumer = resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
			if (!consumer)
				return 0;
			
			float currentSupplies = consumer.GetAggregatedResourceValue();
			return currentSupplies;
		}
		
		return -1;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Override to use vehicle supply component max budget
	override int GetMaxBudgetValue(EEditableEntityBudget budget)
	{
		// For CAMPAIGN budget (supplies), check vehicle supply component
		if (budget == EEditableEntityBudget.CAMPAIGN)
		{
			GRAD_BC_VehicleSupplyComponent vehicleSupply = GetVehicleSupplyComponent();
			if (vehicleSupply)
			{
				Print(string.Format("[GRAD BC Provider] Getting max supplies: %1", vehicleSupply.GetMaxBuildingBudget()), LogLevel.NORMAL);
				return vehicleSupply.GetMaxBuildingBudget();
			}
		}
		
		// Fallback to default implementation
		return super.GetMaxBudgetValue(budget);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Override to use vehicle supply component for max budget when checking master
	override int GetMaxBudgetValueFromMasterIfNeeded(EEditableEntityBudget budget)
	{
		SCR_CampaignBuildingBudgetToEvaluateData budgetData = GetBudgetData(budget);
		if (!budgetData)
			return -1;
		
		SCR_CampaignBuildingProviderComponent masterProviderComponent;
		
		// If we should use the master's budget, also check master's budget
		if (UseMasterProviderBudget(budgetData.GetBudget(), masterProviderComponent))
		{
			if (masterProviderComponent)
				return masterProviderComponent.GetMaxBudgetValue(budget);
		}
		
		// For CAMPAIGN budget (supplies), check vehicle supply component
		if (budget == EEditableEntityBudget.CAMPAIGN)
		{
			GRAD_BC_VehicleSupplyComponent vehicleSupply = GetVehicleSupplyComponent();
			if (vehicleSupply)
				return vehicleSupply.GetMaxBuildingBudget();
		}
		
		// Otherwise return normal max budget
		SCR_CampaignBuildingMaxValueBudgetToEvaluateData maxValueBudgetData = SCR_CampaignBuildingMaxValueBudgetToEvaluateData.Cast(budgetData);
		if (maxValueBudgetData)
			return maxValueBudgetData.GetMaxValue();
			
		return -1;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Override SetPropValue to update vehicle supply component
	override void SetPropValue(int value)
	{
		super.SetPropValue(value);
		
		GRAD_BC_VehicleSupplyComponent vehicleSupply = GetVehicleSupplyComponent();
		if (vehicleSupply)
			vehicleSupply.SetCurrentBuildingBudget(value);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Override AddPropValue to update vehicle supply component
	override void AddPropValue(int value)
	{
		super.AddPropValue(value);
		
		GRAD_BC_VehicleSupplyComponent vehicleSupply = GetVehicleSupplyComponent();
		if (vehicleSupply)
			vehicleSupply.AddBuildingBudget(value);
	}
}
