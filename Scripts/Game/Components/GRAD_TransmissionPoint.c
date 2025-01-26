enum ETransmissionState
{
	OFF,
	TRANSMITTING,
	INTERRUPTED,
	DISABLED,
	DONE
}

[BaseContainerProps(configRoot: true)]
class GRAD_TransmissionPoint
{
	//! Position of the transmission point
	vector m_vPosition;
	
	[RplProp()]
	protected float m_iTransmissionProgress;
	
	static float m_iTransmissionDuration = 30.0;
	static float m_iTransmissionUpdateTickSize = 1.0 /m_iTransmissionDuration;
	
	protected ETransmissionState m_eTransmissionState;
	
	bool m_vIsActive;
	
	void Init(vector position) {
		PrintFormat("GRAD_TransmissionPoint created");
		
		m_vPosition = position;
		
		GetGame().GetCallqueue().CallLater(mainLoop, 1000, true);
	}
	
	void mainLoop() {
		PrintFormat("GRAD_TransmissionPoint loop");
		
		if (m_vIsActive) {
			m_iTransmissionProgress = m_iTransmissionProgress + m_iTransmissionUpdateTickSize;
		}
		
		float currentProgress = Math.Floor(m_iTransmissionProgress * 100);
		if (currentProgress >= 100) {
			SetTransmissionState(ETransmissionState.DONE);
		};
	}
	
	vector GetPosition()
	{
		return m_vPosition;
	}
	
	void StartTransmission() {
		m_vIsActive = true;
		PrintFormat("GRAD_TransmissionPoint StartTransmission");
	}
	
	void InterruptTransmission() {
		m_vIsActive = false;
		PrintFormat("GRAD_TransmissionPoint InterruptTransmission");
	}
	
	bool GetTransmissionActive() {
		bool isActive = m_eTransmissionState == ETransmissionState.TRANSMITTING;
		return isActive;
	}
	
	float GetTransmissionDuration() {
		float currentProgress = Math.Floor(m_iTransmissionProgress * 100);
		PrintFormat("GRAD_TransmissionPoint GetTransmissionDuration");
		return currentProgress;
	}
	
	ETransmissionState GetTransmissionState() 
	{
		return m_eTransmissionState;
	}
	
	void SetTransmissionState(ETransmissionState state) {
		m_eTransmissionState = state;		
	}
}
