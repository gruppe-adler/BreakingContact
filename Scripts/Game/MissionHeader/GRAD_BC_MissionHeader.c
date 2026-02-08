[BaseContainerProps()]
class GRAD_BC_MissionHeader : SCR_MissionHeader
{
	[Attribute(desc: "Override list of vehicle prefabs for ambient traffic. If empty, vehicles are auto-detected from the civilian faction catalog.", params: "et")]
	protected ref array<ResourceName> m_aTrafficVehicleOverrides;

	[Attribute(defvalue: "CIV", desc: "Faction key used to auto-detect civilian vehicles when no override list is set.")]
	protected string m_sCivilianFactionKey;

	bool HasTrafficVehicleOverrides()
	{
		return (m_aTrafficVehicleOverrides && !m_aTrafficVehicleOverrides.IsEmpty());
	}

	void GetTrafficVehicleOverrides(notnull array<ResourceName> outPrefabs)
	{
		if (!m_aTrafficVehicleOverrides)
			return;

		foreach (ResourceName rn : m_aTrafficVehicleOverrides)
		{
			outPrefabs.Insert(rn);
		}
	}

	string GetCivilianFactionKey()
	{
		return m_sCivilianFactionKey;
	}
}
