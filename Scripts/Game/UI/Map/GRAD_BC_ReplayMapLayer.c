//
[BaseContainerProps()]
class GRAD_BC_ReplayMapLayer : GRAD_MapMarkerLayer // Inherit from proven working class
{
	// Map vehicle prefab and faction to icon key
	protected string GetVehicleIconKey(string prefab, string factionKey, bool isEmpty)
	{
		if (factionKey.IsEmpty())
			factionKey = "Empty";
		
		if (prefab.IsEmpty()) return "";
		string key = "";
		string pf = prefab;
		pf.ToLower();
		// Special case: ambient/empty vehicles (no faction)
		if (factionKey == "Empty")
		{
			isEmpty = true;
		}
		// Normal logic for faction vehicles
		if (pf.Contains("lav"))
		{
			if (isEmpty) key = "LAV_empty";
			else if (factionKey == "US") key = "LAV_blufor";
			else if (factionKey == "USSR") key = "LAV_opfor";
		}
		else if (pf.Contains("m151a2"))
		{
			if (pf.Contains("transport"))
			{
				if (isEmpty) key = "M151A2_closed_empty";
				else if (factionKey == "US") key = "M151A2_closed_blufor";
				else if (factionKey == "USSR") key = "M151A2_closed_opfor";
			}
			else if (pf.Contains("open"))
			{
				if (isEmpty) key = "M151A2_open_empty";
				else if (factionKey == "US") key = "M151A2_open_blufor";
				else if (factionKey == "USSR") key = "M151A2_open_opfor";
			}
			else
			{
				if (isEmpty) key = "M151A2_empty";
				else if (factionKey == "US") key = "M151A2_blufor";
				else if (factionKey == "USSR") key = "M151A2_opfor";
			}
		}
		else if (pf.Contains("m923"))
		{
			if (isEmpty) key = "M923A1_closed_empty";
			else if (factionKey == "US") key = "M923A1_closed_blufor";
			else if (factionKey == "USSR") key = "M923A1_closed_opfor";
		}
		else if (pf.Contains("m998"))
		{
			if (pf.Contains("m2hb"))
			{
				if (isEmpty) key = "M998_M2HB_empty";
				else if (factionKey == "US") key = "M998_M2HB_blufor";
				else if (factionKey == "USSR") key = "M998_M2HB_opfor";
			}
			else if (pf.Contains("transport"))
			{
				if (isEmpty) key = "M998_closed_empty";
				else if (factionKey == "US") key = "M998_closed_blufor";
				else if (factionKey == "USSR") key = "M998_closed_opfor";
			}
			else
			{
				if (isEmpty) key = "M998_open_empty";
				else if (factionKey == "US") key = "M998_open_blufor";
				else if (factionKey == "USSR") key = "M998_open_opfor";
			}
		}
		else if (pf.Contains("uaz452") || pf.Contains("uaz_452"))
		{
			if (isEmpty) key = "UAZ_452_empty";
			else if (factionKey == "US") key = "UAZ_452_blufor";
			else if (factionKey == "USSR") key = "UAZ_452_opfor";
		}
		else if (pf.Contains("btr"))
		{
			if (isEmpty) key = "BTR_empty";
			else if (factionKey == "US") key = "BTR_blufor";
			else if (factionKey == "USSR") key = "BTR_opfor";
		}
		else if (pf.Contains("S1203"))
		{
			if (isEmpty) key = "S1203_empty";
			else if (factionKey == "US") key = "S1203_blufor";
			else if (factionKey == "USSR") key = "S1203_opfor";
		}
		else if (pf.Contains("S105"))
		{
			if (isEmpty) key = "S105_empty";
			else if (factionKey == "US") key = "S105_blufor";
			else if (factionKey == "USSR") key = "S105_opfor";
		}
		else if (pf.Contains("uaz469") || pf.Contains("uaz_469"))
		{
			if (pf.Contains("pkm"))
			{
				if (isEmpty) key = "UAZ_469_PKM_empty";
				else if (factionKey == "US") key = "UAZ_469_PKM_blufor";
				else if (factionKey == "USSR") key = "UAZ_469_PKM_opfor";
			}
			else if (pf.Contains("open"))
			{
				if (isEmpty) key = "UAZ_469_Open_empty";
				else if (factionKey == "US") key = "UAZ_469_Open_blufor";
				else if (factionKey == "USSR") key = "UAZ_469_Open_opfor";
			}
			else
			{
				if (isEmpty) key = "UAZ_469_closed_empty";
				else if (factionKey == "US") key = "UAZ_469_closed_blufor";
				else if (factionKey == "USSR") key = "UAZ_469_closed_opfor";
			}
		}
		else if (pf.Contains("uh1h1"))
		{
			if (isEmpty) key = "UH1H1_empty";
			else if (factionKey == "US") key = "UH1H1_blufor";
			else if (factionKey == "USSR") key = "UH1H1_opfor";
		}
		else if (pf.Contains("ural4320"))
		{
			if (pf.Contains("command") || pf.Contains("radio"))
			{
				if (isEmpty) key = "Radiotruck_empty";
				else if (factionKey == "US") key = "Radiotruck_blufor";
				else if (factionKey == "USSR") key = "Radiotruck_opfor";
			}
			else if (pf.Contains("transport") || pf.Contains("covered"))
			{
				if (isEmpty) key = "Ural_empty";
				else if (factionKey == "US") key = "Ural_blufor";
				else if (factionKey == "USSR") key = "Ural_opfor";
			}
			else
			{
				if (isEmpty) key = "Ural_Open_empty";
				else if (factionKey == "US") key = "Ural_Open_blufor";
				else if (factionKey == "USSR") key = "UralOpen_opfor";
			}
		}
		return key;
	}

	// Vehicle icon textures
	protected ref map<string, string> m_vehicleIconTextures = new map<string, string>();
	// Replay display state
	protected ref array<ref GRAD_BC_ReplayPlayerMarker> m_playerMarkers = {};
	protected ref array<ref GRAD_BC_ReplayProjectileMarker> m_projectileMarkers = {};
	protected ref array<ref GRAD_BC_ReplayTransmissionMarker> m_transmissionMarkers = {};
	protected ref array<ref GRAD_BC_ReplayRadioTruckMarker> m_radioTruckMarkers = {};
	protected ref array<ref GRAD_BC_ReplayVehicleMarker> m_vehicleMarkers = {};
	
	// Replay mode flag - true when actively viewing replay, false for normal map use
	protected bool m_bIsInReplayMode = false;
	
	// Keep last frame data for persistent display
	protected ref array<ref GRAD_BC_ReplayPlayerMarker> m_lastFramePlayerMarkers = {};
	protected ref array<ref GRAD_BC_ReplayProjectileMarker> m_lastFrameProjectileMarkers = {};
	protected ref array<ref GRAD_BC_ReplayTransmissionMarker> m_lastFrameTransmissionMarkers = {};
	protected ref array<ref GRAD_BC_ReplayRadioTruckMarker> m_lastFrameRadioTruckMarkers = {};
	protected ref array<ref GRAD_BC_ReplayVehicleMarker> m_lastFrameVehicleMarkers = {};
	protected bool m_hasLastFrame = false;
	
	// Unit type icon textures
	protected ref map<string, string> m_unitTypeTextures = new map<string, string>();
	
	// Cached loaded textures
	protected ref map<string, ref SharedItemRef> m_loadedTextures = new map<string, ref SharedItemRef>();
	
	// Transmission and radio truck icon paths
	protected ref map<string, string> m_transmissionIconPaths = new map<string, string>();
	
	//------------------------------------------------------------------------------------------------
	override void Init()
	{
		// Vehicle icon mapping
		m_vehicleIconTextures.Set("LAV_blufor", "{1D2C64090C772F84}UI/Textures/Icons/LAV_blufor.edds");
		m_vehicleIconTextures.Set("LAV_empty", "{AD78B1EE505E01FC}UI/Textures/Icons/LAV_empty.edds");
		m_vehicleIconTextures.Set("LAV_opfor", "{2F20DFA7DAF579E3}UI/Textures/Icons/LAV_opfor.edds");

		m_vehicleIconTextures.Set("BTR_blufor", "{06FB93A5E254D29E}UI/Textures/Icons/BTR_blufor.edds");
		m_vehicleIconTextures.Set("BTR_empty", "{4F0602CC01556B32}UI/Textures/Icons/BTR_empty.edds");
		m_vehicleIconTextures.Set("BTR_opfor", "{D26C47510DE3338D}UI/Textures/Icons/BRDM_opfor.edds");

		m_vehicleIconTextures.Set("M151A2_blufor", "{F134996BEE493C80}UI/Textures/Icons/M151A2_blufor.edds");
		m_vehicleIconTextures.Set("M151A2_empty", "{3F818F4DDA023873}UI/Textures/Icons/M151A2_empty.edds");
		m_vehicleIconTextures.Set("M151A2_open_blufor", "{77AC2D8B6883E41F}UI/Textures/Icons/M151A2_open_blufor.edds");
		m_vehicleIconTextures.Set("M151A2_open_empty", "{CA8A6C10410526AA}UI/Textures/Icons/M151A2_open_empty.edds");
		m_vehicleIconTextures.Set("M151A2_open_opfor", "{48D20259CBAE5EB5}UI/Textures/Icons/M151A2_open_opfor.edds");
		m_vehicleIconTextures.Set("M151A2_opfor", "{BDD9E10450A9406C}UI/Textures/Icons/M151A2_opfor.edds");
		
		m_vehicleIconTextures.Set("M923A1_closed_blufor", "{92B598738E1468A1}UI/Textures/Icons/M923A1_closed_blufor.edds");
		m_vehicleIconTextures.Set("M923A1_closed_empty", "{C2FD136933FF34FF}UI/Textures/Icons/M923A1_closed_empty.edds");
		m_vehicleIconTextures.Set("M923A1_closed_opfor", "{40A57D20B9544CE0}UI/Textures/Icons/M923A1_closed_opfor.edds");

		m_vehicleIconTextures.Set("M998_closed_blufor", "{6BC2F3B2C876C86F}UI/Textures/Icons/M998_closed_blufor.edds");
		m_vehicleIconTextures.Set("M998_closed_empty", "{2F7D445FFCB4E602}UI/Textures/Icons/M998_closed_empty.edds");
		m_vehicleIconTextures.Set("M998_closed_opfor", "{AD252A16761F9E1D}UI/Textures/Icons/M998_closed_opfor.edds");
		m_vehicleIconTextures.Set("M998_M2HB_blufor", "{BDE1B2689B13B6D4}UI/Textures/Icons/M998_M2HB_blufor.edds");
		m_vehicleIconTextures.Set("M998_M2HB_empty", "{91A96E1BFE6A80DE}UI/Textures/Icons/M998_M2HB_empty.edds");
		m_vehicleIconTextures.Set("M998_M2HB_opfor", "{13F1005274C1F8C1}UI/Textures/Icons/M998_M2HB_opfor.edds");
		m_vehicleIconTextures.Set("M998_open_blufor", "{13D8AB44ACDAB597}UI/Textures/Icons/M998_open_blufor.edds");
		m_vehicleIconTextures.Set("M998_open_empty", "{0D4CC53522A25D19}UI/Textures/Icons/M998_open_empty.edds");
		m_vehicleIconTextures.Set("M998_open_opfor", "{8F14AB7CA8092506}UI/Textures/Icons/M998_open_opfor.edds");

		m_vehicleIconTextures.Set("UAZ_452_opfor", "{D5FBFD6881DC4B58}UI/Textures/Icons/UAZ_452_opfor.edds");
		m_vehicleIconTextures.Set("UAZ_452_blufor", "{7AD5D1D4CA9B9DFF}UI/Textures/Icons/UAZ_452_blufor.edds");
		m_vehicleIconTextures.Set("UAZ_452_empty", "{D74712B1A79AFEFB}UI/Textures/Icons/UAZ_452_empty.edds");

		m_vehicleIconTextures.Set("UAZ_469_closed_blufor", "{613ACF2847FB8583}UI/Textures/Icons/UAZ_469 closed_blufor.edds");
		m_vehicleIconTextures.Set("UAZ_469_closed_empty", "{529E5BA357DB08E3}UI/Textures/Icons/UAZ_469 closed_empty.edds");
		m_vehicleIconTextures.Set("UAZ_469_Open_blufor", "{BE066D044B960605}UI/Textures/Icons/UAZ_469 Open_blufor.edds");
		m_vehicleIconTextures.Set("UAZ_469_Open_empty", "{2826A28533C37A7B}UI/Textures/Icons/UAZ_469 Open_empty.edds");
		m_vehicleIconTextures.Set("UAZ_469_Open_opfor", "{AA7ECCCCB9680264}UI/Textures/Icons/UAZ_469 Open_opfor.edds");
		m_vehicleIconTextures.Set("UAZ_469_PKM_blufor", "{8BF3272F9BF556DD}UI/Textures/Icons/UAZ_469 PKM_blufor.edds");
		m_vehicleIconTextures.Set("UAZ_469_PKM_empty", "{D3C0984F7BD3DE72}UI/Textures/Icons/UAZ_469 PKM_empty.edds");
		m_vehicleIconTextures.Set("UAZ_469_closed_opfor", "{90645BA8C54E7ABC}UI/Textures/Icons/UAZ_469_closed_opfor.edds");
		m_vehicleIconTextures.Set("UAZ_469_PKM_opfor", "{2B897E967D0AC339}UI/Textures/Icons/UAZ_469_PKM_opfor.edds");

		m_vehicleIconTextures.Set("UH1H1_blufor", "{A4FDF3DA7E1BF136}UI/Textures/Icons/UH1H1_blufor.edds");
		m_vehicleIconTextures.Set("UH1H1_empty", "{519C8DBD9918D6CC}UI/Textures/Icons/UH1H1_empty.edds");
		m_vehicleIconTextures.Set("UH1H1_opfor", "{D3C4E3F413B3AED3}UI/Textures/Icons/UH1H1_opfor.edds");

		m_vehicleIconTextures.Set("Ural_blufor", "{87808F1F7D6839E4}UI/Textures/Icons/Ural_blufor.edds");
		m_vehicleIconTextures.Set("Ural_empty", "{8020A00CF179274D}UI/Textures/Icons/Ural_empty.edds");
		m_vehicleIconTextures.Set("Ural_Open_blufor", "{16ED7010CF5FA66B}UI/Textures/Icons/Ural_Open_blufor.edds");
		m_vehicleIconTextures.Set("Ural_Open_empty", "{BD154D82B60DC8F0}UI/Textures/Icons/Ural_Open_empty.edds");
		m_vehicleIconTextures.Set("Ural_opfor", "{07E46F71B9B92027}UI/Textures/Icons/Ural_opfor.edds");
		m_vehicleIconTextures.Set("UralOpen_opfor", "{AD63B0D696D2577D}UI/Textures/Icons/UralOpen_opfor.edds");

		m_vehicleIconTextures.Set("Radiotruck_blufor", "{ABB9C00A86D1437D}UI/Textures/Icons/Radiotruck_blufor.edds");
		m_vehicleIconTextures.Set("Radiotruck_empty", "{AB02F6EFC1893111}UI/Textures/Icons/Radiotruck_Empty.edds");
		m_vehicleIconTextures.Set("Radiotruck_opfor", "{295A98A64B22490E}UI/Textures/Icons/Radiotruck_opfor.edds");

		m_vehicleIconTextures.Set("Commandvehicle_blufor", "{2CCAAB5BEDD7BAA1}UI/Textures/Icons/Commandvehicle_blufor.edds");
		m_vehicleIconTextures.Set("Commandvehicle_empty", "{C2436C5A1B9CF72D}UI/Textures/Icons/Commandvehicle_Empty.edds");
		m_vehicleIconTextures.Set("Commandvehicle_opfor", "{401B021391378F32}UI/Textures/Icons/Commandvehicle_opfor.edds");

		m_vehicleIconTextures.Set("S105_blufor", "{8705E23234AE5219}UI/Textures/Icons/S105_blufor.edds");
		m_vehicleIconTextures.Set("S105_empty", "{116570AE64B80F4E}UI/Textures/Icons/S105_Empty.edds");
		m_vehicleIconTextures.Set("S105_opfor", "{933D1EE7EE137751}UI/Textures/Icons/S105_opfor.edds");

		m_vehicleIconTextures.Set("S1203_blufor", "{E04DE98D3C890A8A}UI/Textures/Icons/S1203_blufor.edds");
		m_vehicleIconTextures.Set("S1203_empty", "{1040C8443019C220}UI/Textures/Icons/S1203_empty.edds");
		m_vehicleIconTextures.Set("S1203_opfor", "{9218A60DBAB2BA3F}UI/Textures/Icons/S1203_opfor.edds");

		
	
		super.Init();
		
		Print("GRAD_BC_ReplayMapLayer: Initializing replay map layer", LogLevel.NORMAL);
		
		// Initialize transmission and radio truck icon paths
		m_transmissionIconPaths.Set("transmission_active", "{3E2F061E35D2DA76}UI/Textures/Icons/GRAD_BC_mapIcons.imageset:transmission_active");
		m_transmissionIconPaths.Set("transmission_default", "{3E2F061E35D2DA76}UI/Textures/Icons/GRAD_BC_mapIcons.imageset:transmission_default");
		m_transmissionIconPaths.Set("transmission_interrupted", "{3E2F061E35D2DA76}UI/Textures/Icons/GRAD_BC_mapIcons.imageset:transmission_interrupted");
		m_transmissionIconPaths.Set("radiotruck_active", "{3E2F061E35D2DA76}UI/Textures/Icons/GRAD_BC_mapIcons.imageset:radiotruck_active");
		
		// Initialize unit type textures with new faction-sensitive icons
		// Format: m_unitTypeTextures.Set("Role_Faction", "texturePath");
		// Factions: US (blufor), USSR (opfor), CIV (civ)
		// AntiTank
		m_unitTypeTextures.Set("AntiTank_US", "{ECC9BB91F97CEC7E}UI/Textures/Icons/iconman_at_blufor.edds");
		m_unitTypeTextures.Set("AntiTank_USSR", "{449232651E2D6688}UI/Textures/Icons/iconman_at_opfor.edds");
		m_unitTypeTextures.Set("AntiTank_CIV", "{EAD70F3FE70432A4}UI/Textures/Icons/iconman_at_civ.edds");
		m_unitTypeTextures.Set("AT_US", "{ECC9BB91F97CEC7E}UI/Textures/Icons/iconman_at_blufor.edds");
		m_unitTypeTextures.Set("AT_USSR", "{449232651E2D6688}UI/Textures/Icons/iconman_at_opfor.edds");
		m_unitTypeTextures.Set("AT_CIV", "{EAD70F3FE70432A4}UI/Textures/Icons/iconman_at_civ.edds");
		m_unitTypeTextures.Set("AAT_US", "{ECC9BB91F97CEC7E}UI/Textures/Icons/iconman_at_blufor.edds");
		m_unitTypeTextures.Set("AAT_USSR", "{449232651E2D6688}UI/Textures/Icons/iconman_at_opfor.edds");
		m_unitTypeTextures.Set("AAT_CIV", "{EAD70F3FE70432A4}UI/Textures/Icons/iconman_at_civ.edds");
		// Engineer
		m_unitTypeTextures.Set("Engineer_US", "{67FCB2D6D795807C}UI/Textures/Icons/iconman_engineer_blufor.edds");
		m_unitTypeTextures.Set("Engineer_USSR", "{0D1394432D5C8C2A}UI/Textures/Icons/iconman_engineer_opfor.edds");
		m_unitTypeTextures.Set("Engineer_CIV", "{9F9D2F059235FF68}UI/Textures/Icons/iconman_engineer_civ.edds");
		// Grenadier
		m_unitTypeTextures.Set("Grenadier_US", "{138C2049645E03C1}UI/Textures/Icons/iconman_explosive_blufor.edds");
		m_unitTypeTextures.Set("Grenadier_USSR", "{687A58A5F6834059}UI/Textures/Icons/iconman_explosive_opfor.edds");
		m_unitTypeTextures.Set("Grenadier_CIV", "{93CE7B5AB88B8235}UI/Textures/Icons/iconman_explosive_civ.edds");
		// Medic
		m_unitTypeTextures.Set("Medic_US", "{C6C1954CA9E57C00}UI/Textures/Icons/iconman_medic_blufor.edds");
		m_unitTypeTextures.Set("Medic_USSR", "{1C9664FDA31BF765}UI/Textures/Icons/iconman_medic_opfor.edds");
		m_unitTypeTextures.Set("Medic_CIV", "{5857673AF18884E7}UI/Textures/Icons/iconman_medic_civ.edds");
		// MachineGunner
		m_unitTypeTextures.Set("MachineGunner_US", "{3EA1BC7E0E0B0533}UI/Textures/Icons/iconman_mg_blufor.edds");
		m_unitTypeTextures.Set("MachineGunner_USSR", "{65CED072E552385C}UI/Textures/Icons/iconman_mg_opfor.edds");
		m_unitTypeTextures.Set("MachineGunner_CIV", "{FEF73C39377E824B}UI/Textures/Icons/iconman_mg_civ.edds");
		m_unitTypeTextures.Set("AR_US", "{3EA1BC7E0E0B0533}UI/Textures/Icons/iconman_mg_blufor.edds");
		m_unitTypeTextures.Set("AR_USSR", "{65CED072E552385C}UI/Textures/Icons/iconman_mg_opfor.edds");
		m_unitTypeTextures.Set("AR_CIV", "{FEF73C39377E824B}UI/Textures/Icons/iconman_mg_civ.edds");
		m_unitTypeTextures.Set("AutomaticRifleman_US", "{3EA1BC7E0E0B0533}UI/Textures/Icons/iconman_mg_blufor.edds");
		m_unitTypeTextures.Set("AutomaticRifleman_USSR", "{65CED072E552385C}UI/Textures/Icons/iconman_mg_opfor.edds");
		m_unitTypeTextures.Set("AutomaticRifleman_CIV", "{FEF73C39377E824B}UI/Textures/Icons/iconman_mg_civ.edds");
		// Rifleman
		m_unitTypeTextures.Set("Rifleman_US", "{9912E888E7CC2E28}UI/Textures/Icons/iconman_rifleman_blufor.edds");
		m_unitTypeTextures.Set("Rifleman_USSR", "{A3894E64547137A3}UI/Textures/Icons/iconman_rifleman_opfor.edds");
		m_unitTypeTextures.Set("Rifleman_CIV", "{97ECC0C7F6184F3F}UI/Textures/Icons/iconman_rifleman_civ.edds");
		// Squad Leader
		m_unitTypeTextures.Set("Leader_US", "{E146B4EA5F726807}UI/Textures/Icons/iconman_squadleader_blufor.edds");
		m_unitTypeTextures.Set("Leader_USSR", "{E32B1F3A2323E5C4}UI/Textures/Icons/iconman_squadleader_opfor.edds");
		m_unitTypeTextures.Set("Leader_CIV", "{94FC55A740EFF5F5}UI/Textures/Icons/iconman_squadleader_civ.edds");
		m_unitTypeTextures.Set("SL_US", "{E146B4EA5F726807}UI/Textures/Icons/iconman_squadleader_blufor.edds");
		m_unitTypeTextures.Set("SL_USSR", "{E32B1F3A2323E5C4}UI/Textures/Icons/iconman_squadleader_opfor.edds");
		m_unitTypeTextures.Set("SL_CIV", "{94FC55A740EFF5F5}UI/Textures/Icons/iconman_squadleader_civ.edds");
		// Team Leader
		m_unitTypeTextures.Set("TeamLeader_US", "{DEBB5CF0EDAEAA90}UI/Textures/Icons/iconman_teamleader_blufor.edds");
		m_unitTypeTextures.Set("TeamLeader_USSR", "{7043346D3A86D4AC}UI/Textures/Icons/iconman_teamleader_opfor.edds");
		m_unitTypeTextures.Set("TeamLeader_CIV", "{03ACA7A746ACE869}UI/Textures/Icons/iconman_teamleader_civ.edds");
		// Virtual
		m_unitTypeTextures.Set("Virtual_US", "{0E582F43978CE7F8}UI/Textures/Icons/iconman_virtual_blufor.edds");
		m_unitTypeTextures.Set("Virtual_USSR", "{3E17667584578998}UI/Textures/Icons/iconman_virtual_opfor.edds");
		m_unitTypeTextures.Set("Virtual_CIV", "{1D125ACAB33784C1}UI/Textures/Icons/iconman_virtual_civ.edds");
		// Recon/Sharpshooter/Spotter
		m_unitTypeTextures.Set("Sharpshooter_US", "{0F4868B3AFD5B26F}UI/Textures/Icons/iconmanrecon_blufor.edds");
		m_unitTypeTextures.Set("Sharpshooter_USSR", "{AD41A08D77783D67}UI/Textures/Icons/iconmanrecon_opfor.edds");
		m_unitTypeTextures.Set("Sharpshooter_CIV", "{CA0BFA20F232081B}UI/Textures/Icons/iconmanrecon_civ.edds");
		m_unitTypeTextures.Set("Recon_US", "{0F4868B3AFD5B26F}UI/Textures/Icons/iconmanrecon_blufor.edds");
		m_unitTypeTextures.Set("Recon_USSR", "{AD41A08D77783D67}UI/Textures/Icons/iconmanrecon_opfor.edds");
		m_unitTypeTextures.Set("Recon_CIV", "{CA0BFA20F232081B}UI/Textures/Icons/iconmanrecon_civ.edds");
		m_unitTypeTextures.Set("Spotter_US", "{0F4868B3AFD5B26F}UI/Textures/Icons/iconmanrecon_blufor.edds");
		m_unitTypeTextures.Set("Spotter_USSR", "{AD41A08D77783D67}UI/Textures/Icons/iconmanrecon_opfor.edds");
		m_unitTypeTextures.Set("Spotter_CIV", "{CA0BFA20F232081B}UI/Textures/Icons/iconmanrecon_civ.edds");
		// Officer fallback (no new icon provided)
		m_unitTypeTextures.Set("Officer_US", "{E146B4EA5F726807}UI/Textures/Icons/iconman_squadleader_blufor.edds");
		m_unitTypeTextures.Set("Officer_USSR", "{E32B1F3A2323E5C4}UI/Textures/Icons/iconman_squadleader_opfor.edds");
		m_unitTypeTextures.Set("Officer_CIV", "{94FC55A740EFF5F5}UI/Textures/Icons/iconman_squadleader_civ.edds");

		// Vehicle icons (unchanged)
		m_unitTypeTextures.Set("ArmedCar", "{EB7C826CD3D2BE53}UI/Textures/Icons/iconarmedcar_ca.edds");
		m_unitTypeTextures.Set("Car", "{AAEC4011C3FAAE79}UI/Textures/Icons/iconcar_ca.edds");
		m_unitTypeTextures.Set("Helicopter", "{7F728098B42A124F}UI/Textures/Icons/iconheli_ca.edds");
		m_unitTypeTextures.Set("HelicopterArmed", "{0729FDD4F156F1DD}UI/Textures/Icons/iconheliarmed_ca.edds");
		m_unitTypeTextures.Set("Truck", "{DDACA1439DB45633}UI/Textures/Icons/icontruck_ca.edds");
		m_unitTypeTextures.Set("Tank", "{CAC18FF5CC2A427D}UI/Textures/Icons/icontank_ca.edds");
		m_unitTypeTextures.Set("Plane", "{D9861CB8FECEC812}UI/Textures/Icons/iconplane_ca.edds");

		// Default fallback for unknown unit types
		m_unitTypeTextures.Set("Default", "{9912E888E7CC2E28}UI/Textures/Icons/iconman_rifleman_blufor.edds");
		Print("GRAD_BC_ReplayMapLayer: Replay map layer ready with faction-sensitive unit type textures", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config); // This handles widget/canvas creation in parent
		
		Print("GRAD_BC_ReplayMapLayer: OnMapOpen called, canvas should be initialized by parent", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);
		Print("GRAD_BC_ReplayMapLayer: OnMapClose called, cleaning up", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	// Use the proven Draw() pattern from GRAD_MapMarkerManager
	override void Draw()
	{
		// Clear previous commands (inherited from GRAD_MapMarkerLayer)
		m_Commands.Clear();
		
		// Check if we should draw anything
		GRAD_BC_ReplayManager replayManager = GRAD_BC_ReplayManager.GetInstance();
		
		// Draw progress bar at top of screen during replay
		if (replayManager && replayManager.IsPlayingBack() && m_MapEntity)
		{
			DrawProgressBar(replayManager);
		}
		
		bool shouldDraw = false;
		array<ref GRAD_BC_ReplayPlayerMarker> markersToRender = {};
		
		if (replayManager && replayManager.IsPlayingBack())
		{
			// During playback - use current markers
			shouldDraw = m_playerMarkers.Count() > 0 || m_vehicleMarkers.Count() > 0;
			markersToRender = m_playerMarkers;
			
			static int drawCallCounter = 0;
			drawCallCounter++;
			if (drawCallCounter % 50 == 0)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Draw() during playback - %1 player markers, %2 vehicle markers", markersToRender.Count(), m_vehicleMarkers.Count()));
			}
		}
		else if (m_hasLastFrame)
		{
			// After replay - use saved last frame
			shouldDraw = m_lastFramePlayerMarkers.Count() > 0 || m_lastFrameVehicleMarkers.Count() > 0;
			markersToRender = m_lastFramePlayerMarkers;
			
			static int persistentDrawCounter = 0;
			persistentDrawCounter++;
			if (persistentDrawCounter % 100 == 0)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Draw() persistent mode - %1 saved player markers, %2 saved vehicle markers", markersToRender.Count(), m_lastFrameVehicleMarkers.Count()));
			}
		}
		
		if (!shouldDraw || !m_MapEntity)
		{
			return; // Nothing to draw or no map entity
		}
		
		// Draw player markers with unit type icons and directional indicators
		foreach (GRAD_BC_ReplayPlayerMarker marker : markersToRender)
		{
			if (!marker.isVisible)
				continue;

			// Debug: Log vehicle state for each marker (first 20 only)
			static int vehicleDebugCount = 0;
			vehicleDebugCount++;
			if (vehicleDebugCount <= 20)
			{
				Print(string.Format(
					"GRAD_BC_ReplayMapLayer: PlayerMarker id=%1 name=%2 isInVehicle=%3 vehicleType='%4' unitType='%5' faction='%6'", 
					marker.playerId, marker.playerName, marker.isInVehicle, marker.vehicleType, marker.unitType, marker.factionKey
				), LogLevel.NORMAL);
			}

			// Draw unit icon with directional chevron (use vehicle icon if in vehicle)
			string vehicleIconKey = "";
			if (marker.isInVehicle)
			{
				vehicleIconKey = GetVehicleIconKey(marker.vehicleType, marker.factionKey, false);
				DrawUnitMarker(marker.position, marker.direction, marker.unitType, marker.factionKey, true, marker.isAlive, vehicleIconKey);
			}
			else
			{
				DrawUnitMarker(marker.position, marker.direction, marker.unitType, marker.factionKey, false, marker.isAlive);
			}
		}

		// Draw empty vehicle markers (always show all vehicles, even if empty)
		array<ref GRAD_BC_ReplayVehicleMarker> vehiclesToRender;
		if (replayManager && replayManager.IsPlayingBack())
			vehiclesToRender = m_vehicleMarkers;
		else if (m_hasLastFrame)
			vehiclesToRender = m_lastFrameVehicleMarkers;
		else
			vehiclesToRender = {};

		foreach (GRAD_BC_ReplayVehicleMarker vehicleMarker : vehiclesToRender)
		{
			if (!vehicleMarker.isVisible)
				continue;

			// Check if a player is in this vehicle
			bool isOccupied = false;
			foreach (GRAD_BC_ReplayPlayerMarker playerMarker : markersToRender)
			{
				if (playerMarker.isInVehicle && playerMarker.vehicleId == vehicleMarker.entityId)
				{
					isOccupied = true;
					break;
				}
			}

			if (isOccupied)
				continue;

			string vehicleIconKey = GetVehicleIconKey(vehicleMarker.vehicleType, vehicleMarker.factionKey, true);
			Print(string.Format("BC Debug - VehicleMarker: vehicleType=%1, factionKey=%2, isEmpty=%3, vehicleIconKey=%4", vehicleMarker.vehicleType, vehicleMarker.factionKey, vehicleMarker.isEmpty, vehicleIconKey), LogLevel.NORMAL);
			DrawUnitMarker(vehicleMarker.position, vehicleMarker.direction, "", vehicleMarker.factionKey, true, true, vehicleIconKey);
		}
		
		// Draw projectiles as lines from firing position to impact position
		array<ref GRAD_BC_ReplayProjectileMarker> projectilesToRender = {};
		bool isLiveReplay = false;
		if (replayManager && replayManager.IsPlayingBack())
		{
			projectilesToRender = m_projectileMarkers;
			isLiveReplay = true;
		}
		else if (m_hasLastFrame)
		{
			projectilesToRender = m_lastFrameProjectileMarkers;
		}
		
		// Debug: Log projectile rendering details
		if (projectilesToRender.Count() > 0)
		{
			string source;
			if (isLiveReplay)
				source = "live replay";
			else
				source = "last frame";
			
			Print(string.Format("GRAD_BC_ReplayMapLayer: Drawing %1 projectiles (source: %2)", 
				projectilesToRender.Count(), 
				source), LogLevel.NORMAL);
		}
		
		int visibleCount = 0;
		foreach (GRAD_BC_ReplayProjectileMarker projMarker : projectilesToRender)
		{
			if (!projMarker.isVisible)
				continue;
			
			visibleCount++;
			
			// Debug: Log first few projectile lines being drawn
			if (visibleCount <= 3)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Drawing projectile line - Type: %1, From: [%2, %3, %4] To: [%5, %6, %7]",
					projMarker.projectileType,
					projMarker.position[0], projMarker.position[1], projMarker.position[2],
					projMarker.impactPosition[0], projMarker.impactPosition[1], projMarker.impactPosition[2]), LogLevel.NORMAL);
			}
				
			// Draw line from firing position to impact position (bright red for visibility)
			DrawLine(projMarker.position, projMarker.impactPosition, 2, Color.RED);
		}
		
		// Debug: Summary of what was drawn
		if (visibleCount > 0)
		{
			Print(string.Format("GRAD_BC_ReplayMapLayer: Drew %1 visible projectile lines (out of %2 total projectiles)",
				visibleCount, projectilesToRender.Count()), LogLevel.NORMAL);
		}
		
		// Draw transmissions with state indicators and progress bars
		// DISABLED DURING REPLAY - transmissions should only show in live mode
		
		array<ref GRAD_BC_ReplayTransmissionMarker> transmissionsToRender = {};
		if (replayManager && replayManager.IsPlayingBack())
		{
			transmissionsToRender = m_transmissionMarkers;
		}
		else if (m_hasLastFrame)
		{
			transmissionsToRender = m_lastFrameTransmissionMarkers;
		}
		
		foreach (GRAD_BC_ReplayTransmissionMarker transMarker : transmissionsToRender)
		{
			if (!transMarker.isVisible)
				continue;
			
			// Choose icon based on transmission state
			string iconName;
			int stateColor; // Color for progress bar
			switch (transMarker.state)
			{
				case ETransmissionState.OFF:
					iconName = "transmission_default";
					stateColor = 0xFF808080; // Gray
					break;
				case ETransmissionState.TRANSMITTING:
					iconName = "transmission_active";
					stateColor = 0xFF00FF00; // Green
					break;
				case ETransmissionState.INTERRUPTED:
					iconName = "transmission_interrupted";
					stateColor = 0xFFFF8800; // Orange
					break;
				case ETransmissionState.DISABLED:
					iconName = "transmission_interrupted"; // Use interrupted icon for disabled
					stateColor = 0xFFFF0000; // Red
					break;
				case ETransmissionState.DONE:
					iconName = "transmission_active"; // Use active icon for done
					stateColor = 0xFF0088FF; // Blue
					break;
				default:
					iconName = "transmission_default";
					stateColor = 0xFFFFFFFF; // White
			}
			
			// Draw transmission point icon
			string iconPath = m_transmissionIconPaths.Get(iconName);
			if (iconPath)
				DrawTransmissionIcon(transMarker.position, 40, 40, iconPath);
			
			// Draw progress bar if transmitting
			if (transMarker.state == ETransmissionState.TRANSMITTING || transMarker.progress > 0)
			{
				// Draw progress bar beneath the transmission marker
				int xcp, ycp;
				m_MapEntity.WorldToScreen(transMarker.position[0], transMarker.position[2], xcp, ycp, true);
				
				int barWidth = 60;
				int barHeight = 8;
				int barX = xcp - barWidth / 2;
				int barY = ycp + 30; // Below the circle
				
				// Draw background bar (dark gray) using PolygonDrawCommand
				PolygonDrawCommand bgBar = new PolygonDrawCommand();
				bgBar.m_iColor = 0xFF303030;
				bgBar.m_Vertices = new array<float>;
				bgBar.m_Vertices.Insert(barX);
				bgBar.m_Vertices.Insert(barY);
				bgBar.m_Vertices.Insert(barX + barWidth);
				bgBar.m_Vertices.Insert(barY);
				bgBar.m_Vertices.Insert(barX + barWidth);
				bgBar.m_Vertices.Insert(barY + barHeight);
				bgBar.m_Vertices.Insert(barX);
				bgBar.m_Vertices.Insert(barY + barHeight);
				m_Commands.Insert(bgBar);
				
				// Draw progress fill
				int fillWidth = barWidth * transMarker.progress;
				if (fillWidth > 0)
				{
					PolygonDrawCommand fillBar = new PolygonDrawCommand();
					fillBar.m_iColor = stateColor;
					fillBar.m_Vertices = new array<float>;
					fillBar.m_Vertices.Insert(barX);
					fillBar.m_Vertices.Insert(barY);
					fillBar.m_Vertices.Insert(barX + fillWidth);
					fillBar.m_Vertices.Insert(barY);
					fillBar.m_Vertices.Insert(barX + fillWidth);
					fillBar.m_Vertices.Insert(barY + barHeight);
					fillBar.m_Vertices.Insert(barX);
					fillBar.m_Vertices.Insert(barY + barHeight);
					m_Commands.Insert(fillBar);
				}
			}
		}
		
		
		// Draw radio truck markers
		array<ref GRAD_BC_ReplayRadioTruckMarker> radioTrucksToRender = {};
		if (replayManager && replayManager.IsPlayingBack())
		{
			radioTrucksToRender = m_radioTruckMarkers;
		}
		else if (m_hasLastFrame)
		{
			radioTrucksToRender = m_lastFrameRadioTruckMarkers;
		}
		
		foreach (GRAD_BC_ReplayRadioTruckMarker truckMarker : radioTrucksToRender)
		{
			if (!truckMarker.isVisible || truckMarker.isDestroyed)
				continue;

			// Fix for double rendering: Skip if occupied (player marker will draw vehicle)
			if (!truckMarker.isEmpty)
				continue;

			// Fix for double rendering: Skip if occupied (player marker will draw vehicle)
			if (!truckMarker.isEmpty)
				continue;

			// Draw the vehicle marker for the radio truck (use Ural as default, or match prefab if available)
			string vehicleIconKey = "Radiotruck_empty";
			if (!truckMarker.isEmpty)
			{
				if (truckMarker.factionKey == "US") vehicleIconKey = "Radiotruck_blufor";
				else if (truckMarker.factionKey == "USSR") vehicleIconKey = "Radiotruck_opfor";
			}
			DrawUnitMarker(truckMarker.position, truckMarker.direction, "", truckMarker.factionKey, true, true, vehicleIconKey);

		}
		
		Print(string.Format("GRAD_BC_ReplayMapLayer: Draw() completed - %1 player markers, %2 projectile markers, %3 total commands", 
			markersToRender.Count(), projectilesToRender.Count(), m_Commands.Count()));
	}
	
	//------------------------------------------------------------------------------------------------
	// Draw an image texture at a world position
	override void DrawImage(vector center, int width, int height, SharedItemRef tex)
	{
		ImageDrawCommand cmd = new ImageDrawCommand();
		
		float xcp, ycp;        
		m_MapEntity.WorldToScreen(center[0], center[2], xcp, ycp, true);
		
		cmd.m_Position = Vector(xcp - (width/2), ycp - (height/2), 0);
		cmd.m_pTexture = tex;
		cmd.m_Size = Vector(width, height, 0);
		cmd.m_iFlags = WidgetFlags.BLEND;
		m_Commands.Insert(cmd);
	}
	
	//------------------------------------------------------------------------------------------------
	// Draw an image with a specific color tint
	void DrawImageColor(vector center, int width, int height, SharedItemRef tex, int color)
	{
		ImageDrawCommand cmd = new ImageDrawCommand();
		
		float xcp, ycp;        
		m_MapEntity.WorldToScreen(center[0], center[2], xcp, ycp, true);
		
		cmd.m_Position = Vector(xcp - (width/2), ycp - (height/2), 0);
		cmd.m_pTexture = tex;
		cmd.m_Size = Vector(width, height, 0);
		cmd.m_iColor = color;
		cmd.m_iFlags = WidgetFlags.BLEND;
		m_Commands.Insert(cmd);
	}
	
	//------------------------------------------------------------------------------------------------
	// Draw an image with rotation
	// Rotation happens around center of the image
	void DrawImageColorRotated(vector center, int width, int height, SharedItemRef tex, float rotationDegrees)
	{
		ImageDrawCommand cmd = new ImageDrawCommand();
		cmd.m_pTexture = tex;
		cmd.m_iColor = 0xFFFFFFFF;
		
		float xcp, ycp;        
		m_MapEntity.WorldToScreen(center[0], center[2], xcp, ycp, true);
		
		// Use m_Pivot to rotate around the center
		cmd.m_Position = Vector(xcp - (width * 0.5), ycp - (height * 0.5), 0);
		cmd.m_Size = Vector(width, height, 0);
		cmd.m_Pivot = Vector(width * 0.5, height * 0.5, 0);
		cmd.m_fRotation = rotationDegrees;
		cmd.m_iFlags = WidgetFlags.BLEND;
		
		m_Commands.Insert(cmd);
	}
	
	//------------------------------------------------------------------------------------------------
	// Draw a transmission/radio truck icon from texture path
	void DrawTransmissionIcon(vector center, int width, int height, string texturePath)
	{
		if (!m_Canvas || !texturePath)
			return;
			
		   float xcp, ycp;        
		m_MapEntity.WorldToScreen(center[0], center[2], xcp, ycp, true);
		
		   // Check if texture is already loaded, otherwise load and cache it
		   SharedItemRef texture = m_loadedTextures.Get(texturePath);
		   if (!texture)
		   {
			   texture = m_Canvas.LoadTexture(texturePath);
			   if (texture)
				   m_loadedTextures.Set(texturePath, texture);
		   }
		   if (!texture)
		   {
			   Print(string.Format("BC Debug - DrawTransmissionIcon: Fallback to circle for texturePath=%1", texturePath), LogLevel.WARNING);
			   DrawCircle(center, width * 0.3, 0xFFFFFFFF, 12);
			   return;
		   }
		   // Create image draw command
		   ImageDrawCommand cmd = new ImageDrawCommand();
		   cmd.m_Position = Vector(xcp - (width/2), ycp - (height/2), 0);
		   cmd.m_Size = Vector(width, height, 0);
		   cmd.m_iColor = 0xFFFFFFFF;
		   cmd.m_pTexture = texture;
		   cmd.m_iFlags = WidgetFlags.BLEND;
		   m_Commands.Insert(cmd);
	}
	
	//------------------------------------------------------------------------------------------------
	// Map vehicle prefab name to icon type
	
	//------------------------------------------------------------------------------------------------
	// Draw a unit marker with icon and directional chevron
	   protected void DrawUnitMarker(vector position, float direction, string unitType, string factionKey, bool isVehicle, bool isAlive, string vehicleIconKey = "")
	   {
		   float iconSize;
		   if (isVehicle)
			   iconSize = 64.0;
		   else
			   iconSize = 32.0;

		   string texturePath;
			if (isVehicle)
		    {
		        if (vehicleIconKey != "")
		        {
		            texturePath = m_vehicleIconTextures.Get(vehicleIconKey);
		        }
		    }
		    else // Is infantry
		    {
		        string roleStr = unitType;
		        if (roleStr == "")
		           roleStr = "Rifleman";
		
		        string key = roleStr + "_" + factionKey;
		        texturePath = m_unitTypeTextures.Get(key);
		    }
		
		    // Centralized fallback
		    if (texturePath == "")
		    {
		        if (isVehicle)
		        {
		             // Fallback for vehicle to a generic empty vehicle icon
		             texturePath = m_vehicleIconTextures.Get("Commandvehicle_empty");
		        } else {
		             texturePath = m_unitTypeTextures.Get("Default");
        		}
		        Print(string.Format("BC Debug - DrawUnitMarker: Using fallback icon for vehicle/unit. isVehicle: %1, unitType: %2, faction: %3, vehicleIconKey: %4", isVehicle, unitType, factionKey, vehicleIconKey), LogLevel.WARNING);
		    }


		   SharedItemRef texture = m_loadedTextures.Get(texturePath);
		   if (!texture && m_Canvas && texturePath != "")
		   {
			   texture = m_Canvas.LoadTexture(texturePath);
			   if (texture)
				   m_loadedTextures.Set(texturePath, texture);
		   }

		   int iconPixelSize;
		   if (isVehicle)
			   iconPixelSize = 64;
		   else
			   iconPixelSize = 32;

		   if (texture)
		   {
			   // Fix rotation: convert world yaw to map icon rotation
			   float iconRotation = -direction + 90.0; // Map north-up, world yaw=0 east? Adjust as needed
			   Print(string.Format("BC Debug - DrawUnitMarker: direction=%.2f, iconRotation=%.2f", direction, iconRotation), LogLevel.NORMAL);
			   DrawImageColorRotated(position, iconPixelSize, iconPixelSize, texture, iconRotation);
		   }
		   else
		   {
			   Print(string.Format("BC Debug - DrawUnitMarker: Fallback to circle for texturePath=%1", texturePath), LogLevel.WARNING);
			   DrawCircle(position, iconSize * 0.3, 0xFFFFFFFF, 12);
		   }
	   }
	
	//------------------------------------------------------------------------------------------------
	// Called by replay manager to update marker positions
	void UpdateReplayFrame(GRAD_BC_ReplayFrame frame)
	{
		Print(string.Format("GRAD_BC_ReplayMapLayer: Received frame with %1 players, %2 projectiles, %3 transmissions, %4 radio trucks, %5 vehicles", 
			frame.players.Count(), frame.projectiles.Count(), frame.transmissions.Count(), frame.radioTrucks.Count(), frame.vehicles.Count()), LogLevel.NORMAL);
		
		// Build new marker arrays first (don't clear existing ones yet to avoid empty frames)
		array<ref GRAD_BC_ReplayPlayerMarker> newPlayerMarkers = {};
		array<ref GRAD_BC_ReplayProjectileMarker> newProjectileMarkers = {};
		array<ref GRAD_BC_ReplayTransmissionMarker> newTransmissionMarkers = {};
		array<ref GRAD_BC_ReplayRadioTruckMarker> newRadioTruckMarkers = {};
		array<ref GRAD_BC_ReplayVehicleMarker> newVehicleMarkers = {};
		
		// Create player markers
		foreach (GRAD_BC_PlayerSnapshot playerSnapshot : frame.players)
		{
			GRAD_BC_ReplayPlayerMarker marker = new GRAD_BC_ReplayPlayerMarker();
			marker.playerId = playerSnapshot.playerId;
			marker.playerName = playerSnapshot.playerName;
			marker.factionKey = playerSnapshot.factionKey;
			marker.position = playerSnapshot.position;
			marker.direction = playerSnapshot.angles[0]; // Yaw
			marker.isAlive = playerSnapshot.isAlive;
			marker.isInVehicle = playerSnapshot.isInVehicle;
			marker.vehicleId = playerSnapshot.vehicleId;
			marker.unitType = playerSnapshot.unitRole; // Infantry role from snapshot
			marker.vehicleType = playerSnapshot.vehicleType; // Vehicle type from snapshot
			marker.isVisible = true;
			
			// Debug: Log position and faction data for first few frames
			static int positionLogCount = 0;
			positionLogCount++;
			if (positionLogCount <= 10)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Player %1 (%2) faction: '%3', position: [%4, %5, %6], direction: %7°, type: %8", 
					marker.playerId, marker.playerName, marker.factionKey, marker.position[0], marker.position[1], marker.position[2], 
					marker.direction, marker.unitType));
			}
			
			newPlayerMarkers.Insert(marker);
		}
		
		// Create projectile markers
		foreach (GRAD_BC_ProjectileSnapshot projSnapshot : frame.projectiles)
		{
			GRAD_BC_ReplayProjectileMarker marker = new GRAD_BC_ReplayProjectileMarker();
			marker.projectileType = projSnapshot.projectileType;
			marker.position = projSnapshot.position; // firing position
			marker.impactPosition = projSnapshot.impactPosition; // impact position
			marker.velocity = projSnapshot.velocity;
			marker.isVisible = true;
			
			// Debug log for first few projectiles
			static int projLogCount = 0;
			projLogCount++;
			if (projLogCount <= 5)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Projectile %1 - Type: %2, From: [%3, %4, %5], To: [%6, %7, %8]",
					projLogCount, marker.projectileType,
					marker.position[0], marker.position[1], marker.position[2],
					marker.impactPosition[0], marker.impactPosition[1], marker.impactPosition[2]));
			}
			
			newProjectileMarkers.Insert(marker);
		}
		
		// Create transmission markers
		foreach (GRAD_BC_TransmissionSnapshot transSnapshot : frame.transmissions)
		{
			GRAD_BC_ReplayTransmissionMarker marker = new GRAD_BC_ReplayTransmissionMarker();
			marker.position = transSnapshot.position;
			marker.state = transSnapshot.state;
			marker.progress = transSnapshot.progress;
			marker.isVisible = true;
			
			// Debug log for first few transmissions
			static int transLogCount = 0;
			transLogCount++;
			if (transLogCount <= 5)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Transmission %1 - State: %2, Progress: %3%%, Position: [%4, %5, %6]",
					transLogCount, marker.state, marker.progress * 100,
					marker.position[0], marker.position[1], marker.position[2]));
			}
			
			newTransmissionMarkers.Insert(marker);
		}
		
		// Create radio truck markers
		foreach (GRAD_BC_RadioTruckSnapshot truckSnapshot : frame.radioTrucks)
		{
			GRAD_BC_ReplayRadioTruckMarker marker = new GRAD_BC_ReplayRadioTruckMarker();
			marker.position = truckSnapshot.position;
			marker.direction = truckSnapshot.angles[0]; // Yaw
			marker.isActive = truckSnapshot.isActive;
			marker.isDestroyed = truckSnapshot.isDestroyed;
			marker.isEmpty = truckSnapshot.isEmpty;
			marker.factionKey = truckSnapshot.factionKey;
			marker.isVisible = true;
			
			newRadioTruckMarkers.Insert(marker);
		}
		
		// Create vehicle markers
		foreach (GRAD_BC_VehicleSnapshot vehicleSnapshot : frame.vehicles)
		{
			GRAD_BC_ReplayVehicleMarker marker = new GRAD_BC_ReplayVehicleMarker();
			marker.entityId = vehicleSnapshot.entityId;
			marker.vehicleType = vehicleSnapshot.vehicleType;
			marker.factionKey = vehicleSnapshot.factionKey;
			marker.position = vehicleSnapshot.position;
			marker.direction = vehicleSnapshot.angles[0]; // Yaw
			marker.isVisible = true;
			marker.isEmpty = vehicleSnapshot.isEmpty;
			newVehicleMarkers.Insert(marker);
		}
		
		// Now atomically swap the marker arrays to avoid empty frames during rendering
		m_playerMarkers = newPlayerMarkers;
		m_projectileMarkers = newProjectileMarkers;
		m_transmissionMarkers = newTransmissionMarkers;
		m_radioTruckMarkers = newRadioTruckMarkers;
		m_vehicleMarkers = newVehicleMarkers;
		
		// Save this frame as the last frame for persistent display
		m_lastFramePlayerMarkers.Clear();
		m_lastFrameProjectileMarkers.Clear();
		m_lastFrameTransmissionMarkers.Clear();
		m_lastFrameRadioTruckMarkers.Clear();
		m_lastFrameVehicleMarkers.Clear();
		
		// Deep copy current markers to last frame
		foreach (GRAD_BC_ReplayPlayerMarker playerMarker : m_playerMarkers)
		{
			GRAD_BC_ReplayPlayerMarker lastMarker = new GRAD_BC_ReplayPlayerMarker();
			lastMarker.playerId = playerMarker.playerId;
			lastMarker.playerName = playerMarker.playerName;
			lastMarker.factionKey = playerMarker.factionKey;
			lastMarker.position = playerMarker.position;
			lastMarker.direction = playerMarker.direction;
			lastMarker.isAlive = playerMarker.isAlive;
			lastMarker.isInVehicle = playerMarker.isInVehicle;
			lastMarker.vehicleId = playerMarker.vehicleId;
			lastMarker.unitType = playerMarker.unitType;
			lastMarker.vehicleType = playerMarker.vehicleType;
			lastMarker.isVisible = playerMarker.isVisible;
			m_lastFramePlayerMarkers.Insert(lastMarker);
		}
		
		foreach (GRAD_BC_ReplayProjectileMarker projMarker : m_projectileMarkers)
		{
			GRAD_BC_ReplayProjectileMarker lastMarker = new GRAD_BC_ReplayProjectileMarker();
			lastMarker.projectileType = projMarker.projectileType;
			lastMarker.position = projMarker.position;
			lastMarker.impactPosition = projMarker.impactPosition;
			lastMarker.velocity = projMarker.velocity;
			lastMarker.isVisible = projMarker.isVisible;
			m_lastFrameProjectileMarkers.Insert(lastMarker);
		}
		
		foreach (GRAD_BC_ReplayTransmissionMarker transMarker : m_transmissionMarkers)
		{
			GRAD_BC_ReplayTransmissionMarker lastMarker = new GRAD_BC_ReplayTransmissionMarker();
			lastMarker.position = transMarker.position;
			lastMarker.state = transMarker.state;
			lastMarker.progress = transMarker.progress;
			lastMarker.isVisible = transMarker.isVisible;
			m_lastFrameTransmissionMarkers.Insert(lastMarker);
		}
		
		foreach (GRAD_BC_ReplayRadioTruckMarker truckMarker : m_radioTruckMarkers)
		{
			GRAD_BC_ReplayRadioTruckMarker lastMarker = new GRAD_BC_ReplayRadioTruckMarker();
			lastMarker.position = truckMarker.position;
			lastMarker.direction = truckMarker.direction;
			lastMarker.isActive = truckMarker.isActive;
			lastMarker.isDestroyed = truckMarker.isDestroyed;
			lastMarker.isVisible = truckMarker.isVisible;
			lastMarker.isEmpty = truckMarker.isEmpty;
			lastMarker.factionKey = truckMarker.factionKey;
			m_lastFrameRadioTruckMarkers.Insert(lastMarker);
		}
		
		foreach (GRAD_BC_ReplayVehicleMarker vehicleMarker : m_vehicleMarkers)
		{
			GRAD_BC_ReplayVehicleMarker lastMarker = new GRAD_BC_ReplayVehicleMarker();
			lastMarker.entityId = vehicleMarker.entityId;
			lastMarker.vehicleType = vehicleMarker.vehicleType;
			lastMarker.factionKey = vehicleMarker.factionKey;
			lastMarker.position = vehicleMarker.position;
			lastMarker.direction = vehicleMarker.direction;
			lastMarker.isVisible = vehicleMarker.isVisible;
			m_lastFrameVehicleMarkers.Insert(lastMarker);
		}
		
		m_hasLastFrame = true;
		m_bIsInReplayMode = true; // Mark that we're in replay mode
		Print(string.Format("GRAD_BC_ReplayMapLayer: Saved last frame with %1 players, %2 projectiles, %3 transmissions, %4 radio trucks, %5 vehicles for persistent display", 
			m_lastFramePlayerMarkers.Count(), m_lastFrameProjectileMarkers.Count(), m_lastFrameTransmissionMarkers.Count(), m_lastFrameRadioTruckMarkers.Count(), m_lastFrameVehicleMarkers.Count()));
		
		// Log frame update for debugging
		Print(string.Format("GRAD_BC_ReplayMapLayer: Updated frame with %1 players, %2 projectiles, %3 transmissions, %4 radio trucks", 
			m_playerMarkers.Count(), m_projectileMarkers.Count(), m_transmissionMarkers.Count(), m_radioTruckMarkers.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	// Draw progress bar showing replay progress at top of map
	void DrawProgressBar(GRAD_BC_ReplayManager replayManager)
	{
		if (!replayManager || !m_MapEntity)
			return;
			
		float progress = replayManager.GetReplayProgress();
		float currentTime = replayManager.GetCurrentReplayTime();
		float totalTime = replayManager.GetTotalReplayDuration();
		float playbackSpeed = replayManager.GetPlaybackSpeed();
		
		// Get screen dimensions
		float screenW, screenH;
		m_MapEntity.GetMapWidget().GetScreenSize(screenW, screenH);
		
		// Progress bar dimensions and position
		float barWidth = screenW - 100; // Leave margins
		float barHeight = 25;
		float barX = 50; // Left margin
		float barY = 20; // Top margin
		float cornerRadius = 12; // Rounded corner size
		
		// Draw background with rounded corners (dark semi-transparent)
		// Main background rectangle
		PolygonDrawCommand bgBar = new PolygonDrawCommand();
		bgBar.m_iColor = 0xCC1A1A1A; // Dark gray semi-transparent
		bgBar.m_Vertices = new array<float>;
		bgBar.m_Vertices.Insert(barX + cornerRadius);
		bgBar.m_Vertices.Insert(barY);
		bgBar.m_Vertices.Insert(barX + barWidth - cornerRadius);
		bgBar.m_Vertices.Insert(barY);
		bgBar.m_Vertices.Insert(barX + barWidth - cornerRadius);
		bgBar.m_Vertices.Insert(barY + barHeight);
		bgBar.m_Vertices.Insert(barX + cornerRadius);
		bgBar.m_Vertices.Insert(barY + barHeight);
		m_Commands.Insert(bgBar);
		
		// Draw progress fill with rounded corners (blue)
		int fillWidth = barWidth * progress;
		if (fillWidth > cornerRadius * 2) // Only draw if there's enough space for corners
		{
			PolygonDrawCommand fillBar = new PolygonDrawCommand();
			fillBar.m_iColor = 0xFFD18D1F; // Blue color for active tint
			fillBar.m_Vertices = new array<float>;
			fillBar.m_Vertices.Insert(barX + cornerRadius);
			fillBar.m_Vertices.Insert(barY);
			fillBar.m_Vertices.Insert(barX + fillWidth - cornerRadius);
			fillBar.m_Vertices.Insert(barY);
			fillBar.m_Vertices.Insert(barX + fillWidth - cornerRadius);
			fillBar.m_Vertices.Insert(barY + barHeight);
			fillBar.m_Vertices.Insert(barX + cornerRadius);
			fillBar.m_Vertices.Insert(barY + barHeight);
			m_Commands.Insert(fillBar);
			
			// Draw rounded left edge of fill
			DrawScreenCircle(barX + cornerRadius, barY + barHeight/2, cornerRadius, 0xFF3A7FD5, 8);
			
			// Draw rounded right edge of fill if progress is not complete
			if (progress < 0.99)
			{
				DrawScreenCircle(barX + fillWidth - cornerRadius, barY + barHeight/2, cornerRadius, 0xFF3A7FD5, 8);
			}
		}
		
		// Draw rounded corners for background using circles
		DrawScreenCircle(barX + cornerRadius, barY + barHeight/2, cornerRadius, 0xCC1A1A1A, 8);
		DrawScreenCircle(barX + barWidth - cornerRadius, barY + barHeight/2, cornerRadius, 0xCC1A1A1A, 8);
		
		// Draw border (white) with rounded effect
		LineDrawCommand border = new LineDrawCommand();
		border.m_iColor = 0xFFFFFFFF;
		border.m_fWidth = 2;
		border.m_Vertices = new array<float>;
		border.m_Vertices.Insert(barX);
		border.m_Vertices.Insert(barY);
		border.m_Vertices.Insert(barX + barWidth);
		border.m_Vertices.Insert(barY);
		border.m_Vertices.Insert(barX + barWidth);
		border.m_Vertices.Insert(barY + barHeight);
		border.m_Vertices.Insert(barX);
		border.m_Vertices.Insert(barY + barHeight);
		border.m_bShouldEnclose = true;
		m_Commands.Insert(border);
		
		// Add text display (time and speed)
		// Format time as MM:SS
		int currentMin = Math.Floor(currentTime / 60);
		int currentSec = Math.Floor(currentTime - (currentMin * 60));
		int totalMin = Math.Floor(totalTime / 60);
		int totalSec = Math.Floor(totalTime - (totalMin * 60));
		
		// Create formatted time strings
		string currentMinStr = currentMin.ToString();
		if (currentMin < 10) currentMinStr = "0" + currentMinStr;
		string currentSecStr = currentSec.ToString();
		if (currentSec < 10) currentSecStr = "0" + currentSecStr;
		string totalMinStr = totalMin.ToString();
		if (totalMin < 10) totalMinStr = "0" + totalMinStr;
		string totalSecStr = totalSec.ToString();
		if (totalSec < 10) totalSecStr = "0" + totalSecStr;
		
		string timeText = string.Format("REPLAY: %1:%2 / %3:%4 (%.1fx speed)", 
			currentMinStr, currentSecStr, totalMinStr, totalSecStr, playbackSpeed);
		
		// Note: Text rendering in map layers is limited, so we'll use the progress bar as the primary indicator
		// The time display will be in the replay controls widget
		
		Print(string.Format("GRAD_BC_ReplayMapLayer: Progress bar drawn - %.1f%% complete (%1:%2/%3:%4)", 
			progress * 100, currentMinStr, currentSecStr, totalMinStr, totalSecStr), LogLevel.VERBOSE);
	}
	
	//------------------------------------------------------------------------------------------------
	// Helper to draw a circle in screen coordinates (for UI elements like progress bar)
	void DrawScreenCircle(float screenX, float screenY, float radius, int color, int n = 16)
	{
		PolygonDrawCommand cmd = new PolygonDrawCommand();
		cmd.m_iColor = color;
		cmd.m_Vertices = new array<float>;
		
		for(int i = 0; i < n; i++)
		{
			float theta = i*(2*Math.PI/n);
			float x = screenX + radius*Math.Cos(theta);
			float y = screenY + radius*Math.Sin(theta);
			cmd.m_Vertices.Insert(x);
			cmd.m_Vertices.Insert(y);
		}
		m_Commands.Insert(cmd);
	}
	
	//------------------------------------------------------------------------------------------------
	// Override DrawLine to apply DPIUnscale for projectiles
	override void DrawLine(vector startPos, vector endPos, float width, int color)
	{
		PolygonDrawCommand cmd = new PolygonDrawCommand();
		cmd.m_iColor = color;
		
		cmd.m_Vertices = new array<float>;
		
		float x1, y1, x2, y2;
		m_MapEntity.WorldToScreen(startPos[0], startPos[2], x1, y1, true);
		m_MapEntity.WorldToScreen(endPos[0], endPos[2], x2, y2, true);
		
		// Create a thick line by drawing a rectangle
		float dx = x2 - x1;
		float dy = y2 - y1;
		float length = Math.Sqrt(dx * dx + dy * dy);
		
		if (length > 0)
		{
			float offsetX = (-dy / length) * (width / 2.0);
			float offsetY = (dx / length) * (width / 2.0);
			
			cmd.m_Vertices.Insert(x1 + offsetX); cmd.m_Vertices.Insert(y1 + offsetY);
			cmd.m_Vertices.Insert(x2 + offsetX); cmd.m_Vertices.Insert(y2 + offsetY);
			cmd.m_Vertices.Insert(x2 - offsetX); cmd.m_Vertices.Insert(y2 - offsetY);
			cmd.m_Vertices.Insert(x1 - offsetX); cmd.m_Vertices.Insert(y1 - offsetY);
		}
		
		m_Commands.Insert(cmd);
	}
}

// Marker classes for replay display
class GRAD_BC_ReplayPlayerMarker : Managed
{
	int playerId;
	string playerName;
	string factionKey;
	vector position;
	float direction;
	bool isAlive;
	bool isInVehicle;
	RplId vehicleId;
	string unitType; // Infantry unit type (Rifleman, Medic, etc.)
	string vehicleType; // Vehicle prefab name when in vehicle
	bool isVisible;
}

class GRAD_BC_ReplayProjectileMarker : Managed
{
	string projectileType;
	vector position; // firing position
	vector impactPosition; // impact/endpoint position
	vector velocity;
	bool isVisible;
}

class GRAD_BC_ReplayTransmissionMarker : Managed
{
	vector position;
	ETransmissionState state;
	float progress; // 0.0 to 1.0
	bool isVisible;
}

class GRAD_BC_ReplayRadioTruckMarker : Managed
{
	vector position;
	float direction;
	bool isActive; // whether radio truck is transmitting
	bool isDestroyed; // whether radio truck is destroyed
	bool isEmpty;
	string factionKey;
	bool isVisible;
}

class GRAD_BC_ReplayVehicleMarker : Managed
{
    int entityId;
    string vehicleType;
    string factionKey;
    vector position;
	float direction;
	bool isVisible;
	bool isEmpty;
}
