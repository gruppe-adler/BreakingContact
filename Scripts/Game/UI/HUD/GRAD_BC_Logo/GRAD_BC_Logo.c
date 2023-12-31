
class GRAD_BC_Logo: SCR_InfoDisplay
{
	private ImageWidget m_logo;
	private bool m_isDisplayed;
	
	override event void OnInit(IEntity owner) {
		if (!m_logo) { 
			if (m_wRoot) {
				m_logo = ImageWidget.Cast(m_wRoot.FindAnyWidget("GRAD_BC_Logo_Widget")) 
			};
		};
		
		if (!m_isDisplayed && m_logo) {
			m_logo.SetOpacity(0);
		} else if (m_isDisplayed) {
			m_logo.SetOpacity(1);
		} else if (m_logo) {
			m_logo.SetOpacity(0);
		};	
	}
	
	override event void OnStartDraw(IEntity owner)
	{
		super.OnStartDraw(owner);	
		
		if (!m_logo) { 
			if (m_wRoot) {
				m_logo = ImageWidget.Cast(m_wRoot.FindAnyWidget("GRAD_BC_Logo_Widget")) 
			};
		};
	}
	
	void SetVisible(bool visible) {
		if (m_logo) {
			float opacity;
			if (visible) { opacity = 1.0 };
			m_logo.SetOpacity(opacity);
		} else {
			PrintFormat("GRAD_BC_Logo: m_logo not found");
		}
	}
}
