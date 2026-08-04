//------------------------------------------------------------------------------------------------
//! Condition for showing the confirm spawn button
//! Only shows for USSR faction and while choosing spawn
/*
[BaseContainerProps()]
class GRAD_BC_ConfirmSpawnCondition : SCR_AvailableActionCondition
{
    protected static int s_iLogThrottle = 0;

    //------------------------------------------------------------------------------------------------
    override bool IsAvailable(notnull SCR_AvailableActionsConditionData data)
    {
        if (!super.IsAvailable(data))
            return false;

        // Get character from the provided condition data
        SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(data.GetCharacter());
        if (!ch)
            return false;
        
        // Get faction affiliation component
        FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(ch.FindComponent(FactionAffiliationComponent));
        if (!factionComp)
            return false;
            
        // Get the actual faction
        Faction faction = factionComp.GetAffiliatedFaction();
        if (!faction)
            return false;
        
        // Only show for USSR faction (not BLUFOR)
        if (faction.GetFactionKey() != "USSR")
            return false;
        
        // Use the static GetInstance() method to get the player component for the local player
        GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
        if (!playerComponent)
            return false;
        
        // Only show if player is choosing spawn
        if (!playerComponent.IsChoosingSpawn())
            return false;
        
        // Only enable button if spawn position calculation is complete
        if (!playerComponent.IsSpawnPositionReady())
        {
            // Still return true to show the button, but it will be visually disabled
            // and the ConfirmSpawn() method will block execution
            return true;
        }
        
        return true;
    }
}
*/