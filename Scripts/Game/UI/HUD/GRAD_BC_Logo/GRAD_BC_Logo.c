
class GRAD_BC_Logo: SCR_InfoDisplayExtended
{
	private ImageWidget m_logo;
	
	override event void DisplayInit(IEntity owner) {
		super.DisplayInit(owner);
		
		m_wRoot = GetGame().GetWorkspace().CreateWidgets("{9A969E139E27E0F5}UI/Layouts/HUD/GRAD_BC_Logo/GRAD_BC_Logo.layout", null);

		if (!m_wRoot) {
			PrintFormat("GRAD_BC_Logo: no m_wRoot found", LogLevel.ERROR);
			return;
		}
		
		Widget w = m_wRoot.FindAnyWidget("GRAD_BC_Logo_Widget");
   		m_logo = ImageWidget.Cast(w);
		
		if (!m_logo) {
			PrintFormat("GRAD_BC_Logo: no GRAD_BC_Logo_Widget found", LogLevel.ERROR);
			return;
		}
		
		HideLogo();
	}
	
	void ShowLogo()
    {
       if (!m_logo)
			return;
		PrintFormat("GRAD_BC_Logo: showLogo called!", LogLevel.VERBOSE);
		m_logo.SetOpacity(1.0);
    	GetGame().GetCallqueue().CallLater(HideLogo, 15000);
    }
	
	private void HideLogo()
    {
        if (m_logo) { m_logo.SetOpacity(0.0); };
		PrintFormat("GRAD_BC_Logo: hiding m_logo", LogLevel.VERBOSE);
    }
}
