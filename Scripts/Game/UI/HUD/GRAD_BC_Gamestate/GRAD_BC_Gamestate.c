
class GRAD_BC_Gamestate: SCR_InfoDisplayExtended
{
	private RichTextWidget m_text;
	
	override event void DisplayInit(IEntity owner) {
		super.DisplayInit(owner);
		
		m_wRoot = GetGame().GetWorkspace().CreateWidgets("{A36BC6B57CB2DF15}UI/Layouts/HUD/GRAD_BC_Gamestate/GRAD_BC_Gamestate.layout", null);

		if (!m_wRoot) {
			PrintFormat("GRAD_BC_Gamestate: no m_wRoot found", LogLevel.ERROR);
			return;
		}
		
		Widget w = m_wRoot.FindAnyWidget("GRAD_BC_Gamestate_text");
   		m_text = RichTextWidget.Cast(w);
		
		if (!m_text) {
			PrintFormat("GRAD_BC_Gamestate: no m_text found", LogLevel.ERROR);
			return;
		}
	}
	
	void ShowText(string message)
    {
       // if (!m_logo) return;
		m_text.SetText(message);
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		PrintFormat("GRAD_BC_Gamestate: showText called!", LogLevel.VERBOSE);
		//	m_logo.SetOpacity(1.0);
    		GetGame().GetCallqueue().CallLater(HideLogo, 15000);
    }
	
	private void HideLogo()
    {
        // if (m_logo) { m_logo.SetOpacity(0.0); };
		super.Show(false, 3.0, EAnimationCurve.EASE_OUT_QUART);
		PrintFormat("GRAD_BC_Gamestate: hiding m_text", LogLevel.VERBOSE);
    }
}
