[BaseContainerProps()]
class GRAD_BC_MissionHeader : SCR_MissionHeader
{
	[Attribute(desc: "Override list of vehicle prefabs for ambient traffic. If empty, vehicles are auto-detected from the civilian faction catalog.", params: "et")]
	protected ref array<ResourceName> m_aTrafficVehicleOverrides;

	[Attribute(defvalue: "CIV", desc: "Faction key used to auto-detect civilian vehicles when no override list is set.")]
	protected string m_sCivilianFactionKey;

	[Attribute(defvalue: "0", desc: "Enable verbose debug logging for all Breaking Contact components. Disable for production to improve performance.", category: "Breaking Contact - Debug")]
	protected bool m_bDebugLogs;

	[Attribute(defvalue: "0", desc: "Skip faction elimination win conditions. Enable this when testing alone on a dedicated server so the game does not end immediately.", category: "Breaking Contact - Debug")]
	protected bool m_bSkipFactionElimination;

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

	bool IsDebugLogsEnabled()
	{
		return m_bDebugLogs;
	}

	bool IsSkipFactionElimination()
	{
		return m_bSkipFactionElimination;
	}
}
