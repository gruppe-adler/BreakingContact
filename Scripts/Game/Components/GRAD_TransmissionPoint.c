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

	//! progress of transmission
	float m_vProgress;
	
	[RplProp()]
	protected ETransmissionState m_eTransmissionState;
	
	bool m_vIsActive;
	
	void SetPosition(vector position) {
		m_vPosition = position;
	}
	
	vector GetPosition()
	{
		return m_vPosition;
	}
	
	void SetTransmissionActive(bool setActive) {
		m_vIsActive = setActive;
	}
	
	bool GetTransmissionActive() {
		return m_vIsActive;
	}
	
	float GetTransmissionDuration() {
		return m_vProgress;
	}
	
	ETransmissionState GetTransmissionState() 
	{
		return m_eTransmissionState;
	}

	//------------------------------------------------------------------------------------------------
	// destructor
	/*
	void ~GRAD_TransmissionPoint()
	{
		m_eTransmissionState = null;
		m_vIsActive = null;
	}
	*/
}
