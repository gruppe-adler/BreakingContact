
class GRAD_BC_Logo: SCR_InfoDisplay
{
	private ImageWidget m_logo;
	private bool m_isDisplayed;
	
	override event void OnStartDraw(IEntity owner)
	{
		super.OnStartDraw(owner);

		if (!m_logo) m_logo = ImageWidget.Cast(m_wRoot.FindAnyWidget("GRAD_BC_Logo"));
		if (!m_isDisplayed && m_logo) {
			m_logo.SetVisible(false);
		} else if (m_logo) {
			m_logo.SetVisible(true);
		};
	}
	
	void setVisible(bool visible) {
		if (visible) {
			m_isDisplayed = true;
		} else {
			m_isDisplayed = false;
		};
	}
}
