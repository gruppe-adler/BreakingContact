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
		
		// this works - calls every time map opens
		// Print("GRAD_MapMarkerManager: Draw called", LogLevel.NORMAL);
		
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
		// Print("GRAD_MapMarkerManager: OnMapOpen called", LogLevel.NORMAL);
		
		super.OnMapOpen(config);
		
		m_Ranges = new array<int>;
		m_transmissionPointsActive = new array<vector>;
		m_transmissionPointsDone = new array<vector>;
		m_transmissionPointsInactive = new array<vector>;
		m_transmissionPointsDisabled = new array<vector>;
		
	 	GRAD_BC_BreakingContactManager GRAD_BCM = GRAD_BC_BreakingContactManager.GetInstance();
		if (!GRAD_BCM) {
			Print("GRAD_MapMarkerManager: GRAD_BCM missing", LogLevel.ERROR);
			return;
		}
		
		IEntity radioTruck = GRAD_BCM.GetRadioTruck();
		if (!radioTruck) {
			Print("GRAD_MapMarkerManager: radioTruck missing", LogLevel.ERROR);
			return;
		}
		
		array<GRAD_TransmissionPoint> GRAD_TPCs = GRAD_BCM.GetTransmissionPoints();
		PrintFormat("GRAD_TPCs %1", GRAD_TPCs);
		
		foreach(GRAD_TransmissionPoint GRAD_TPC : GRAD_TPCs)
		{	
			
			ETransmissionState currentState = GRAD_TPC.GetTransmissionState();
			PrintFormat("ETransmissionState %1",currentState);
			
			switch (currentState)
			{
		   		 case ETransmissionState.TRANSMITTING: {
			        m_transmissionPointsActive.Insert(GRAD_TPC.GetPosition());
		        	break;
				}
						
				case ETransmissionState.DISABLED: {
					m_transmissionPointsDisabled.Insert(GRAD_TPC.GetPosition());
					break;
				}
		
			    case ETransmissionState.INTERRUPTED: {
					m_transmissionPointsInactive.Insert(GRAD_TPC.GetPosition());
					break;
				}
		        
		
		    	case ETransmissionState.DONE: {
					m_transmissionPointsDone.Insert(GRAD_TPC.GetPosition());
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
		m_transmissionPointsActive = null;
		m_transmissionPointsDone = null;
		m_transmissionPointsInactive = null;
		m_transmissionPointsDisabled = null;
		m_RangeDefault = null;
		m_IconDestroyed = null;
	}
}
