class GRAD_TransmissionProgress : SCR_ScriptedWidgetComponent
{
	TextWidget m_wTransmissionProgressText;
	float m_fFullDuration;
	float m_fCurrentProgress;
	
	ImageWidget m_wFill;
	ImageWidget m_wEmpty;
	
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		m_wTransmissionProgressText = TextWidget.Cast(w.FindAnyWidget("TransmissionProgressText2"));
		
		m_wEmpty = ImageWidget.Cast(w.FindAnyWidget("Fill"));
		m_wFill = ImageWidget.Cast(w.FindAnyWidget("Empty"));
	}
	
	void Update()
	{
		if (m_fFullDuration == 0)
			return;
		
		m_fCurrentProgress -= GetGame().GetWorld().GetTimeSlice() * 1000;
		float percent = ((float)m_fCurrentProgress) / m_fFullDuration;
		if (percent < 0)
			percent = 0;
		HorizontalLayoutSlot.SetFillWeight(m_wEmpty, percent);
		HorizontalLayoutSlot.SetFillWeight(m_wFill, 1.0 - percent);
		
		if (m_fCurrentProgress < -1)
		{
			m_wRoot.RemoveFromHierarchy();
		}
	}
	
	void SetTime(int time)
	{
		if (m_fFullDuration == 0)
		{
			m_fFullDuration = time;
			GetGame().GetCallqueue().CallLater(Update, 0, true);
		}
		
		m_fCurrentProgress = time;
		
		float timeSeconds = ((float) time) / 1000;
		int seconds = Math.Mod(timeSeconds, 60);
		int minutes = (timeSeconds / 60);
		m_wTransmissionProgressText.SetTextFormat("%1:%2", minutes.ToString(2), seconds.ToString(2));
	}
}