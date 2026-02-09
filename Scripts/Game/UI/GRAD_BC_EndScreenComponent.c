class GRAD_BC_EndScreenComponent : ScriptedWidgetComponent
{
	protected TextWidget m_TitleText;
	protected TextWidget m_SubtitleText;
	protected Widget m_wRoot;
	
	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		m_wRoot = w;
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_EndScreenComponent: HandlerAttached called", LogLevel.NORMAL);
		
		m_TitleText = TextWidget.Cast(m_wRoot.FindAnyWidget("TitleText"));
		m_SubtitleText = TextWidget.Cast(m_wRoot.FindAnyWidget("SubtitleText"));
		
		if (!m_TitleText)
			Print("GRAD_BC_EndScreenComponent: TitleText widget not found!", LogLevel.ERROR);
		if (!m_SubtitleText)
			Print("GRAD_BC_EndScreenComponent: SubtitleText widget not found!", LogLevel.ERROR);
		
		// Try updating immediately and also with a delay
		UpdateEndscreenText();
		GetGame().GetCallqueue().CallLater(UpdateEndscreenText, 100, false);
	}
	
	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdateEndscreenText()
	{
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm)
		{
			Print("GRAD_BC_EndScreenComponent: Could not find BCM instance", LogLevel.WARNING);
			return;
		}
		
		string title, subtitle;
		bcm.GetEndscreenText(title, subtitle);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_EndScreenComponent: Got from BCM: title='%1', subtitle='%2'", title, subtitle), LogLevel.NORMAL);
		
		if (m_TitleText)
		{
			m_TitleText.SetText(title);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_EndScreenComponent: Set TitleText to '%1'", title), LogLevel.NORMAL);
		}
		
		if (m_SubtitleText)
		{
			m_SubtitleText.SetText(subtitle);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_EndScreenComponent: Set SubtitleText to '%1'", subtitle), LogLevel.NORMAL);
		}
	}
}
