// todo rewrite

[BaseContainerProps()]
class GRAD_MapMarkerManager : GRAD_MapMarkerLayer
{		
	protected ref array<vector> m_transmissionPointsActive;
	protected ref array<vector> m_transmissionPointsInactive;
	protected ref array<vector> m_transmissionPointsDisabled;
	protected ref array<vector> m_transmissionPointsDone;
	protected ref array<int> m_Ranges;
	protected int m_RangeDefault = 1000;
	protected ref SharedItemRef m_IconDestroyed;
	
	override void Draw()
	{			
		m_Commands.Clear();
		
		Print("GRAD_MapMarkerManager: Draw called", LogLevel.NORMAL);
		
		foreach(int i, vector center : m_transmissionPointsActive)
		{			
			DrawCircle(center, m_Ranges[i], ARGB(50, 255, 50, 50));			
		}
		
		foreach(int i, vector center : m_transmissionPointsInactive)
		{			
			DrawCircle(center, m_Ranges[i], ARGB(50, 50, 50, 50));
		}
		
		foreach(int i, vector center : m_transmissionPointsDisabled)
		{			
			DrawImage(center, 25, 25, m_IconDestroyed);
		}
		
		foreach(int i, vector center : m_transmissionPointsDone)
		{			
			DrawCircle(center, m_Ranges[i], ARGB(50, 50, 255, 50));
		}
		
	}
	
	override void OnMapOpen(MapConfiguration config)
	{
		Print("GRAD_MapMarkerManager: OnMapOpen called", LogLevel.NORMAL);
		
		super.OnMapOpen(config);
		
		m_Ranges = new array<int>;
		m_transmissionPointsActive = new array<vector>;
		m_transmissionPointsDone = new array<vector>;
		m_transmissionPointsInactive = new array<vector>;
		m_transmissionPointsDisabled = new array<vector>;
		
		IEntity GRAD_BCM_Entity = GetGame().GetWorld().FindEntityByName("GRAD_BCM");
		if (!GRAD_BCM_Entity) {
			Print("GRAD GRAD_MapMarkerManager: GRAD_BCM Entity missing", LogLevel.ERROR);
			return	;
		}
		
	 	GRAD_BC_BreakingContactManager GRAD_BCM = GRAD_BC_BreakingContactManager.Cast(GRAD_BCM_Entity);
		if (!GRAD_BCM) {
			Print("GRAD GRAD_MapMarkerManager: manager missing", LogLevel.ERROR);
			return;
		}
		
		array<GRAD_BC_TransmissionPointComponent> GRAD_TPCs = GRAD_BCM.GetTransmissionPoints();
		PrintFormat("GRAD_TPCs %1",GRAD_TPCs);
		
		foreach(GRAD_BC_TransmissionPointComponent GRAD_TPC : GRAD_TPCs)
		{	
			
			ETransmissionState currentState = GRAD_TPC.GetTransmissionState();
			PrintFormat("ETransmissionState %1",currentState);
			
			switch (currentState)
			{
		   		 case ETransmissionState.TRANSMITTING: {
			        m_transmissionPointsActive.Insert(GRAD_TPC.GetOrigin());
		        	break;
				}
						
				case ETransmissionState.DISABLED: {
					m_transmissionPointsDisabled.Insert(GRAD_TPC.GetOrigin());
					break;
				}
		
			    case ETransmissionState.INTERRUPTED: {
					m_transmissionPointsInactive.Insert(GRAD_TPC.GetOrigin());
					break;
				}
		        
		
		    	case ETransmissionState.DONE: {
					m_transmissionPointsDone.Insert(GRAD_TPC.GetOrigin());
			        break;
				}
		
		   	 	// Add more cases as needed
		
		   		 default: {
		        	// Handle any other state if necessary
		        	break;
				}
			}
			
			m_Ranges.Insert(m_RangeDefault);
		}
			
		m_IconDestroyed = m_Canvas.LoadTexture("{09A7BA5E10D5E250}UI/Textures/Map/transmission_destroyed.edds");
		
	}
	
	override void OnMapClose(MapConfiguration config)
	{	
		super.OnMapClose(config);
			
		m_Ranges.Clear();
		m_Ranges = null;
		m_transmissionPointsActive.Clear();
		m_transmissionPointsDone.Clear();
		m_transmissionPointsInactive.Clear();
		m_transmissionPointsDisabled.Clear();
	}
}