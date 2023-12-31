enum e_currentDisplay {
	STARTED,
	INTERRUPTED,
	DONE
}

class GRAD_BC_Transmission: SCR_InfoDisplay
{
	private ImageWidget m_infoImage;
	private bool m_isDisplayed;
	protected e_currentDisplay	m_currentDisplayCached;
	protected e_currentDisplay	m_currentDisplay;
	
	override event void OnInit(IEntity owner) {
		super.OnInit(owner);
		
		if (m_wRoot) {
			if (!m_infoImage) { 
				m_infoImage = ImageWidget.Cast(m_wRoot.FindAnyWidget("GRAD_BC_Transmission_Widget")) 
			};
		};
		
		
		if (!m_isDisplayed && m_infoImage) {
			m_infoImage.SetVisible(false);
		} else if (m_infoImage && m_isDisplayed) {
			m_infoImage.SetVisible(true);
		} else if (m_infoImage){
			m_infoImage.SetVisible(false);
		};
	}
	
	override event void OnStartDraw(IEntity owner)
	{
		super.OnStartDraw(owner);
		
		if (m_wRoot) {
			if (!m_infoImage) { 
				m_infoImage = ImageWidget.Cast(m_wRoot.FindAnyWidget("GRAD_BC_Transmission_Widget")) 
			};
		};
	}
	
	void TransmissionStarted() {
		if (!m_infoImage) {
			PrintFormat("GRAD_BC_Transmission: transmission started m_infoImage not found");
			return;
		}
		m_infoImage.LoadImageTexture(0,"{B8C70A7749D318C2}UI/Transmission/us_established.edds");
		SetVisible(true);
		PrintFormat("GRAD_BC_Transmission: m_infoImage set to started");
		m_currentDisplayCached = e_currentDisplay.STARTED;
		m_currentDisplay = e_currentDisplay.STARTED;
		// GetGame().GetCallqueue().CallLater(SetInvisible,15000,false,m_currentDisplay);
	}
	
	void TransmissionInterrupted() {
		if (!m_infoImage) {
			PrintFormat("GRAD_BC_Transmission: transmission started m_infoImage not found");
			return;
		}
		m_infoImage.LoadImageTexture(0,"{2AE03288555F4237}UI/Transmission/us_cutoff.edds");
		SetVisible(true);
		PrintFormat("GRAD_BC_Transmission: m_infoImage set to interrupted");
		m_currentDisplayCached = e_currentDisplay.INTERRUPTED;
		m_currentDisplay = e_currentDisplay.INTERRUPTED;
		// GetGame().GetCallqueue().CallLater(SetInvisible,15000,false,m_currentDisplay);
	}
	
	void TransmissionDone() {
		if (!m_infoImage) {
			PrintFormat("GRAD_BC_Transmission: transmission started m_infoImage not found");
			return;
		}
		m_infoImage.LoadImageTexture(0,"{92B573238A373130}UI/Transmission/us_done.edds");
		SetVisible(true);
		PrintFormat("GRAD_BC_Transmission: m_infoImage set to done");
		m_currentDisplayCached = e_currentDisplay.DONE;
		m_currentDisplay = e_currentDisplay.DONE;
		// GetGame().GetCallqueue().CallLater(SetInvisible,15000,false,m_currentDisplay);
	}
	
	void SetVisible(bool visible) {
		if (m_infoImage) {
			float opacity;
			if (visible) { 
				opacity = 1.0 
			} else {
				opacity = 0.0;
			};
			m_infoImage.SetOpacity(opacity);
		} else {
			PrintFormat("GRAD_BC_Transmission: m_infoImage not found");
		}
	}
	
	void SetInvisible(e_currentDisplay currentDisplay) {
		// only hide if nothing stacked / hides afterwards
		if (m_currentDisplayCached == currentDisplay) {
			if (m_infoImage) {
				m_infoImage.SetOpacity(0.0);
			} else {
				PrintFormat("GRAD_BC_Transmission: m_infoImage not found");
			}
		} else {
			PrintFormat("GRAD_BC_Transmission: not setting invisible as other image already displayed");
		}
	}
}

