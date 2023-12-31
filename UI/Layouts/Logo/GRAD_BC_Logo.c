
class GRAD_BC_Logo: SCR_InfoDisplay
{
	private ImageWidget m_logo;
	
	override event void OnStartDraw(IEntity owner)
	{
		super.OnStartDraw(owner);

		if (!m_logo) m_logo = ImageWidget.Cast(m_wRoot.FindAnyWidget("GRAD_BC_Logo"));
	}
}
