
class GRAD_BC_Logo: SCR_InfoDisplayExtended
{
	private ImageWidget m_logo;
	private bool m_isDisplayed;
	
	override event void DisplayInit(IEntity owner) {
		super.DisplayInit(owner);
		
		if (m_wRoot) {
			if (!m_logo) {
				m_logo = ImageWidget.Cast(m_wRoot.FindAnyWidget("GRAD_BC_Logo_Widget"));
				m_logo.SetOpacity(0.0);
			};
		};
	}
	
	override event void DisplayStartDraw(IEntity owner)
	{
		super.DisplayStartDraw(owner);	
		
		if (!m_logo) { 
			if (m_wRoot) {
				m_logo = ImageWidget.Cast(m_wRoot.FindAnyWidget("GRAD_BC_Logo_Widget"));
				m_logo.SetOpacity(0.0);
			};
		};
	}
	
	void SetVisible(bool visible) {
		if (m_logo) {
			float opacity;
			if (visible) { 
				opacity = 1.0;
				GetGame().GetCallqueue().CallLater(SetVisible,15000,false,false);	// make go away after 15s
			} else { 
				opacity = 0.0; 
			};
			m_logo.SetOpacity(opacity);
		} else {
			PrintFormat("GRAD_BC_Logo: m_logo not found");
		}
	}
}
