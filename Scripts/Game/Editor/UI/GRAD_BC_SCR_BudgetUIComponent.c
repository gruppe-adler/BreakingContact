//------------------------------------------------------------------------------------------------
//! Guards the division by zero in vanilla SCR_BudgetUIComponent.OnBudgetUpdate.
//!
//! BC repurposes budget type 5 to carry a placeable's SUPPLY COST (see BC_SUPPLY_COST_BUDGET in
//! SCR_CampaignBuildingManagerComponent). SCR_CampaignBuildingBudgetEditorComponent's
//! OnEntityCoreBudgetUpdatedOwner fires Event_OnBudgetUpdated for PROPS on *every* budget update,
//! whatever budget actually changed, and GetMaxBudgetValue(PROPS) returns 0 when there is no
//! building provider - which is always the case for a BC vehicle purchase.
//!
//! Vanilla already guards this exact expression in InitializeBudgets:
//!     if (m_iMaxBudget != 0)
//!         m_BudgetProgress.ShowBudget(m_iCurrentBudget / m_iMaxBudget);
//! but OnBudgetUpdate divides unguarded, throwing "Division by zero" every update.
modded class SCR_BudgetUIComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnBudgetUpdate(EEditableEntityBudget budgetType, int originalBudgetValue, int updatedBudgetValue, int maxBudgetValue)
	{
		m_iCurrentBudget = m_BudgetManager.GetCurrentBudgetValue(m_BudgetType);
		m_BudgetManager.GetMaxBudgetValue(m_BudgetType, m_iMaxBudget);

		m_BudgetProgress.HideBudgetChange();

		// The only change from vanilla: skip the update when there is no budget to show a ratio of.
		if (m_iMaxBudget != 0)
			m_BudgetProgress.ShowBudget(m_iCurrentBudget / m_iMaxBudget);
	}
}
