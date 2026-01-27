[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "manages the radio truck itself")]
class GRAD_BC_RadioTruckComponentClass : ScriptComponentClass
{
}

class GRAD_BC_RadioTruckComponent : ScriptComponent
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "Update Interval", params: "", category: "Breaking Contact - Radio Truck")];
	protected int m_iRadioTransmissionUpdateInterval;

	static float m_iMaxTransmissionDistance = 500.0;

	[RplProp()]
	protected bool m_bIsTransmitting;
	
	[RplProp()]
	protected bool m_bIsDisabled = false;

	private Vehicle m_radioTruck;
	private IEntity m_commandBox; // The command box entity that has the antenna bones

	protected float m_fSavedFuelRatio = -1.0;

	private SCR_MapDescriptorComponent m_mapDescriptorComponent;
	private VehicleWheeledSimulation_SA_B m_VehicleWheeledSimulationComponent;

	private RplComponent m_RplComponent;

	private GRAD_BC_TransmissionComponent m_nearestTransmissionPoint;

	// Event handler for vehicle destruction
	private SCR_VehicleDamageManagerComponent m_DamageManager;

	// Track if we've already processed destruction to avoid multiple triggers
	private bool m_bDestructionProcessed = false;

	// Antenna animation variables
	private static const int ANTENNA_SEGMENT_COUNT = 8;
	private ref array<TNodeId> m_aAntennaBoneIds = new array<TNodeId>();

	[Attribute("90", UIWidgets.Slider, "Rotation angle for each antenna segment when extended (degrees)", "0 180 1")]
	protected float m_fAntennaExtendAngle;

	[Attribute("10000", UIWidgets.EditBox, "Time to fully extend/retract antenna (milliseconds)")]
	protected int m_iAntennaAnimationTime;

	// Debug attributes for testing antenna translation in-game
	[Attribute("0.4", UIWidgets.EditBox, "Debug: Translation per segment in meters (Z-axis up)")]
	protected float m_fDebugTranslationPerSegment;

	[Attribute("0", UIWidgets.CheckBox, "Debug: Enable live preview mode (updates continuously)")]
	protected bool m_bDebugLivePreview;

	private float m_fLastDebugTranslation = 0;

	private bool m_bAntennaExtended = false;
	private bool m_bAntennaAnimating = false;
	private int m_iCurrentAnimationSegment = 0;
	private bool m_bAntennaRaising = false;
	protected float m_fAnimationProgress = 0; // 0.0 = retracted, 1.0 = fully extended
	protected float m_fAnimationSpeed;      // Calculated as 1.0 / (time_in_seconds)

	// Antenna prop spawning
	[Attribute("{F23A470F0A7A46A0}Assets/Props/Military/Antennas/Antenna_R142_01/Dst/Antenna_R142_01_dst_03.xob", UIWidgets.ResourcePickerThumbnail, "Antenna prop to spawn when extended", "xob")]
	protected ResourceName m_sAntennaPropResource;

	// Red Light on Top spawning
	[Attribute("{5A9683E2DC0239EC}Prefabs/Vehicles/Helicopters/UH1H/Lights/VehicleLight_UH1H_Navigating_Base.et", UIWidgets.ResourcePickerThumbnail, "Red light prop to spawn on top when extended", "et")]
	protected ResourceName m_sRedLightPropResource;
	

	private IEntity m_AntennaPropEntity = null;
	private IEntity m_RedLightPropEntity = null;
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		Print("BC Debug - ANTENNA: OnPostInit() START", LogLevel.NORMAL);

		m_radioTruck = Vehicle.Cast(GetOwner());

		if (!m_radioTruck)
		{
			Print("BC Debug - ANTENNA: ERROR - Could not cast owner to Vehicle!", LogLevel.ERROR);
			return;
		}

		Print(string.Format("BC Debug - ANTENNA: Radio truck entity found: %1", m_radioTruck.GetName()), LogLevel.NORMAL);

		m_mapDescriptorComponent = SCR_MapDescriptorComponent.Cast(m_radioTruck.FindComponent(SCR_MapDescriptorComponent));
		Print(string.Format("BC Debug - ANTENNA: Map descriptor component: %1", m_mapDescriptorComponent != null), LogLevel.NORMAL);

		m_VehicleWheeledSimulationComponent = VehicleWheeledSimulation_SA_B.Cast(m_radioTruck.FindComponent(VehicleWheeledSimulation_SA_B));
		Print(string.Format("BC Debug - ANTENNA: Vehicle simulation component: %1", m_VehicleWheeledSimulationComponent != null), LogLevel.NORMAL);

		m_RplComponent = RplComponent.Cast(m_radioTruck.FindComponent(RplComponent));
		Print(string.Format("BC Debug - ANTENNA: RplComponent: %1", m_RplComponent != null), LogLevel.NORMAL);

		// Set up damage manager for fire detection
		m_DamageManager = SCR_VehicleDamageManagerComponent.Cast(m_radioTruck.FindComponent(SCR_VehicleDamageManagerComponent));
		if (m_DamageManager)
		{
			Print(string.Format("BC Debug - Found damage manager for fire detection"), LogLevel.NORMAL);
		}
		else
		{
			Print("BC Debug - Warning: Could not find damage manager component for radio truck", LogLevel.WARNING);
		}

		Print("BC Debug - ANTENNA: Scheduling delayed antenna bones initialization...", LogLevel.NORMAL);

		// Delay antenna bone initialization to ensure entity hierarchy is fully constructed
		GetGame().GetCallqueue().CallLater(InitializeAntennaBones, 500, false);

		Print("BC Debug - ANTENNA: Antenna bones initialization scheduled", LogLevel.NORMAL);

		//PrintFormat("BC Debug - IsMaster(): %1", m_RplComponent.IsMaster()); // IsMaster() does not mean Authority
		//PrintFormat("BC Debug - IsProxy(): %1", m_RplComponent.IsProxy());
		//PrintFormat("BC Debug - IsOwner(): %1", m_RplComponent.IsOwner());

		if (m_RplComponent.IsMaster())
			GetGame().GetCallqueue().CallLater(mainLoop, 1000, true);

		SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	// Ease in/out function (cubic easing)
	//------------------------------------------------------------------------------------------------
	float EaseInOutCubic(float t)
	{
		if (t < 0.5)
			return 4 * t * t * t;
		else
		{
			float f = ((2 * t) - 2);
			return 0.5 * f * f * f + 1;
		}
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_bAntennaAnimating)
			return;

		// Update progress - convert milliseconds to seconds for timeSlice
		if (m_bAntennaRaising)
			m_fAnimationProgress += timeSlice / (m_iAntennaAnimationTime / 1000.0);
		else
			m_fAnimationProgress -= timeSlice / (m_iAntennaAnimationTime / 1000.0);

		// Clamp and check for completion
		m_fAnimationProgress = Math.Clamp(m_fAnimationProgress, 0, 1);

		// Apply easing for smooth animation
		float easedProgress = EaseInOutCubic(m_fAnimationProgress);
		UpdateAntennaBones(easedProgress);

		// Spawn antenna prop when fully extended
		if (m_bAntennaRaising && m_fAnimationProgress >= 1.0 && !m_AntennaPropEntity)
		{
			SpawnAntennaProp();
		}

		if (m_fAnimationProgress >= 1.0 || m_fAnimationProgress <= 0.0)
		{
			m_bAntennaAnimating = false;
			m_bAntennaExtended = (m_fAnimationProgress >= 1.0);
		}
	}

void UpdateAntennaBones(float progress)
{
    for (int i = 0; i < ANTENNA_SEGMENT_COUNT; i++)
    {
        TNodeId boneId = m_aAntennaBoneIds[i];
        if (boneId == -1) continue;

        // Determine how much THIS specific segment should be extended
        // This creates a "telescopic" effect where segments extend one after another
        float segmentTarget = 1.0 / ANTENNA_SEGMENT_COUNT;
        float segmentStart = i * segmentTarget;
        float segmentLocalProgress = Math.Clamp((progress - segmentStart) / segmentTarget, 0, 1);

        vector mat[4];
        Math3D.MatrixIdentity4(mat);

        // Segment lengths with graduated gaps for top 4 segments
        
        float segmentLength;
        if (i <= 3)
            segmentLength = 0.5;
        else if (i == 4)
            segmentLength = 0.45;
        else if (i == 5)
            segmentLength = 0.35;
        else if (i == 6)
            segmentLength = 0.3;
        else // i == 7
            segmentLength = 0.25;

        float zTrans = segmentLocalProgress * segmentLength;
        mat[3] = Vector(0, 0, zTrans);

        m_commandBox.GetAnimation().SetBoneMatrix(m_commandBox, boneId, mat);
    }
}

	
	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		// No event handlers to clean up
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// Find command box via SlotManagerComponent (alternative method)
	//------------------------------------------------------------------------------------------------
	IEntity FindCommandBoxViaSlots(IEntity parent)
	{
		Print("BC Debug - ANTENNA: Searching via SlotManagerComponent...", LogLevel.NORMAL);

		SlotManagerComponent slotManager = SlotManagerComponent.Cast(parent.FindComponent(SlotManagerComponent));
		if (!slotManager)
		{
			Print("BC Debug - ANTENNA: No SlotManagerComponent found", LogLevel.WARNING);
			return null;
		}

		array<EntitySlotInfo> slots = new array<EntitySlotInfo>();
		slotManager.GetSlotInfos(slots);

		Print(string.Format("BC Debug - ANTENNA: Found %1 slots", slots.Count()), LogLevel.NORMAL);

		foreach (EntitySlotInfo slotInfo : slots)
		{
			IEntity attachedEntity = slotInfo.GetAttachedEntity();
			if (!attachedEntity)
				continue;

			// Get entity name and class name
			string entityName = attachedEntity.GetName();
			string className = attachedEntity.ClassName();

			Print(string.Format("BC Debug - ANTENNA: Slot - Name:'%1' Class:'%2'", entityName, className), LogLevel.NORMAL);

			// Check if this entity has the antenna bones
			Animation anim = attachedEntity.GetAnimation();
			if (anim)
			{
				TNodeId testBoneId = anim.GetBoneIndex("v_antenna_01");
				if (testBoneId != -1)
				{
					Print(string.Format("BC Debug - ANTENNA: >>> FOUND COMMAND BOX - has v_antenna_01 bone! Name:'%1' Class:'%2'", entityName, className), LogLevel.NORMAL);
					return attachedEntity;
				}
			}
		}

		Print("BC Debug - ANTENNA: No slot entity found with antenna bones", LogLevel.WARNING);
		return null;
	}

	//------------------------------------------------------------------------------------------------
	// Find the command box child entity
	//------------------------------------------------------------------------------------------------
	IEntity FindCommandBox(IEntity parent, int depth = 0)
	{
		if (depth == 0)
		{
			Print("BC Debug - ANTENNA: Starting command box search...", LogLevel.NORMAL);
		}

		string indent = "";
		for (int i = 0; i < depth; i++)
		{
			indent += "  ";
		}

		IEntity child = parent.GetChildren();

		if (!child)
		{
			Print(string.Format("BC Debug - ANTENNA: %1No children found for entity %2", indent, parent.GetName()), LogLevel.NORMAL);
			return null;
		}

		while (child)
		{
			string childName = child.GetName();
			string className = child.ClassName();
			Print(string.Format("BC Debug - ANTENNA: %1Child [depth %2]: Name:'%3' Class:'%4'", indent, depth, childName, className), LogLevel.NORMAL);

			// Check if this is the command box by name or class
			string childNameLower = childName;
			childNameLower.ToLower();
			string classNameLower = className;
			classNameLower.ToLower();

			if (childNameLower.Contains("combox") || childNameLower.Contains("command") || classNameLower.Contains("combox") || classNameLower.Contains("command"))
			{
				Print(string.Format("BC Debug - ANTENNA: %1>>> FOUND COMMAND BOX: %2 (%3)", indent, childName, className), LogLevel.NORMAL);
				return child;
			}

			// Recursively search children (limit depth to prevent infinite loops)
			if (depth < 10)
			{
				IEntity foundInChild = FindCommandBox(child, depth + 1);
				if (foundInChild)
					return foundInChild;
			}

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	// Initialize antenna bone references
	//------------------------------------------------------------------------------------------------
	void InitializeAntennaBones()
	{
		Print("BC Debug - ANTENNA: InitializeAntennaBones() called (delayed)", LogLevel.NORMAL);

		// Try method 1: Search child entities
		m_commandBox = FindCommandBox(m_radioTruck);

		// Try method 2: Search via SlotManagerComponent if method 1 failed
		if (!m_commandBox)
		{
			Print("BC Debug - ANTENNA: Trying SlotManagerComponent method...", LogLevel.NORMAL);
			m_commandBox = FindCommandBoxViaSlots(m_radioTruck);
		}

		if (!m_commandBox)
		{
			Print("BC Debug - ANTENNA: Command box not found via any method! Using main truck entity...", LogLevel.WARNING);
			m_commandBox = m_radioTruck;
		}
		else
		{
			Print(string.Format("BC Debug - ANTENNA: Using command box entity: %1", m_commandBox.GetName()), LogLevel.NORMAL);
		}

		Animation anim = m_commandBox.GetAnimation();
		if (!anim)
		{
			Print("BC Debug - ANTENNA: No animation found on command box!", LogLevel.ERROR);
			return;
		}

		Print("BC Debug - ANTENNA: Animation object found, searching for bones...", LogLevel.NORMAL);

		// Find all 8 antenna segment bones (v_antenna_01 through v_antenna_08)
		for (int i = 1; i <= ANTENNA_SEGMENT_COUNT; i++)
		{
			string boneName = string.Format("v_antenna_%1", i.ToString(2)); // Formats as 01, 02, etc.
			TNodeId boneId = anim.GetBoneIndex(boneName);

			if (boneId == -1)
			{
				Print(string.Format("BC Debug - ANTENNA: Bone '%1' NOT FOUND (ID: %2)", boneName, boneId), LogLevel.WARNING);
			}
			else
			{
				Print(string.Format("BC Debug - ANTENNA: Found bone '%1' with ID %2", boneName, boneId), LogLevel.NORMAL);
			}

			m_aAntennaBoneIds.Insert(boneId);
		}

		Print(string.Format("BC Debug - ANTENNA: Initialization complete. Total bones in array: %1", m_aAntennaBoneIds.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Initialize bones immediately for Workbench (no delay needed in editor)
	//------------------------------------------------------------------------------------------------
	void InitializeAntennaBonesImmediate()
	{
		if (m_aAntennaBoneIds.Count() > 0)
			return; // Already initialized

		IEntity entityToUse = GetOwner();
		if (!entityToUse)
			return;

		// In Workbench, try to find command box directly
		m_commandBox = FindCommandBox(entityToUse);

		if (!m_commandBox)
			m_commandBox = FindCommandBoxViaSlots(entityToUse);

		if (!m_commandBox)
			m_commandBox = entityToUse;

		Animation anim = m_commandBox.GetAnimation();
		if (!anim)
			return;

		// Find all 8 antenna segment bones
		for (int i = 1; i <= ANTENNA_SEGMENT_COUNT; i++)
		{
			string boneName = string.Format("v_antenna_%1", i.ToString(2));
			TNodeId boneId = anim.GetBoneIndex(boneName);
			m_aAntennaBoneIds.Insert(boneId);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Start antenna raising animation
	//------------------------------------------------------------------------------------------------
	void RaiseAntenna()
	{
		Print(string.Format("BC Debug - ANTENNA: RaiseAntenna() called. Extended: %1, Animating: %2", m_bAntennaExtended, m_bAntennaAnimating), LogLevel.NORMAL);

		if (m_bAntennaExtended || m_bAntennaAnimating)
		{
			Print("BC Debug - ANTENNA: Antenna already extended or animating, aborting", LogLevel.NORMAL);
			return;
		}

		Print(string.Format("BC Debug - ANTENNA: Starting smooth antenna raise animation. Animation time: %1ms", m_iAntennaAnimationTime), LogLevel.NORMAL);
		m_bAntennaAnimating = true;
		m_bAntennaRaising = true;
		// EOnFixedFrame will handle the smooth animation
	}

	//------------------------------------------------------------------------------------------------
	// Start antenna lowering animation
	//------------------------------------------------------------------------------------------------
	void LowerAntenna()
	{
		Print(string.Format("BC Debug - ANTENNA: LowerAntenna() called. Extended: %1, Animating: %2", m_bAntennaExtended, m_bAntennaAnimating), LogLevel.NORMAL);

		if (!m_bAntennaExtended || m_bAntennaAnimating)
		{
			Print("BC Debug - ANTENNA: Antenna already retracted or animating, aborting", LogLevel.NORMAL);
			return;
		}

		// Remove antenna prop as soon as retraction starts
		RemoveAntennaProp();

		Print(string.Format("BC Debug - ANTENNA: Starting smooth antenna lower animation. Animation time: %1ms", m_iAntennaAnimationTime), LogLevel.NORMAL);
		m_bAntennaAnimating = true;
		m_bAntennaRaising = false;
		// EOnFixedFrame will handle the smooth animation
	}

	//------------------------------------------------------------------------------------------------
	// Spawn antenna prop at the top when fully extended
	//------------------------------------------------------------------------------------------------
	void SpawnAntennaProp()
	{
		if (m_AntennaPropEntity)
		{
			Print("BC Debug - ANTENNA: Antenna prop already spawned", LogLevel.WARNING);
			return;
		}

		if (!m_commandBox)
		{
			Print("BC Debug - ANTENNA: Cannot spawn antenna prop - command box not found", LogLevel.ERROR);
			return;
		}

		// Get the top antenna bone (v_antenna_08)
		Animation anim = m_commandBox.GetAnimation();
		if (!anim)
		{
			Print("BC Debug - ANTENNA: Cannot spawn antenna prop - no animation component", LogLevel.ERROR);
			return;
		}

		TNodeId topBoneId = anim.GetBoneIndex("v_antenna_08");
		if (topBoneId == -1)
		{
			Print("BC Debug - ANTENNA: Cannot spawn antenna prop - v_antenna_08 bone not found", LogLevel.ERROR);
			return;
		}

		// Note: .xob files cannot be spawned directly as entities
		// You need to create a prefab (.et file) that wraps the .xob mesh
		// For now, we'll skip spawning the antenna prop or you can:
		// 1. Create a prefab in Workbench that uses the .xob as its mesh
		// 2. Update m_sAntennaPropResource to point to that .et prefab instead

		Print("BC Debug - ANTENNA: Skipping antenna prop spawn - .xob files need to be wrapped in a prefab (.et)", LogLevel.WARNING);

		// Example of what you would do with a proper prefab:
		// Resource antennaResource = Resource.Load(m_sAntennaPropResource);
		// if (antennaResource && antennaResource.IsValid())
		// {
		//     m_AntennaPropEntity = GetGame().SpawnEntityPrefab(antennaResource, GetGame().GetWorld());
		//     if (m_AntennaPropEntity)
		//     {
		//         m_AntennaPropEntity.SetOrigin(vector.Zero);
		//         m_commandBox.AddChild(m_AntennaPropEntity, topBoneId, EAddChildFlags.AUTO_TRANSFORM);
		//     }
		// }

		// Spawn red light prop
		Resource lightResource = Resource.Load(m_sRedLightPropResource);
		if (!lightResource || !lightResource.IsValid())
		{
			Print(string.Format("BC Debug - ANTENNA: Failed to load red light prop resource: %1", m_sRedLightPropResource), LogLevel.ERROR);
			return;
		}

		m_RedLightPropEntity = GetGame().SpawnEntityPrefab(lightResource, GetGame().GetWorld());

		if (m_RedLightPropEntity)
		{
			// Attach to the command box at the top bone with pivot ID
			m_RedLightPropEntity.SetOrigin(vector.Zero);
			m_commandBox.AddChild(m_RedLightPropEntity, topBoneId, EAddChildFlags.AUTO_TRANSFORM);
			Print("BC Debug - ANTENNA: Red light prop spawned and attached successfully", LogLevel.NORMAL);
		}
		else
		{
			Print("BC Debug - ANTENNA: Failed to spawn red light prop entity", LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Remove antenna prop when retraction starts
	//------------------------------------------------------------------------------------------------
	void RemoveAntennaProp()
	{
		if (m_AntennaPropEntity)
		{
			Print("BC Debug - ANTENNA: Removing antenna prop", LogLevel.NORMAL);
			SCR_EntityHelper.DeleteEntityAndChildren(m_AntennaPropEntity);
			m_AntennaPropEntity = null;
		}

		if (m_RedLightPropEntity)
		{
			Print("BC Debug - ANTENNA: Removing red light prop", LogLevel.NORMAL);
			SCR_EntityHelper.DeleteEntityAndChildren(m_RedLightPropEntity);
			m_RedLightPropEntity = null;
		}
	}



	//------------------------------------------------------------------------------------------------
	// Check if truck is allowed to move (antenna must be fully retracted and not animating)
	//------------------------------------------------------------------------------------------------
	bool CanTruckMove()
	{
		return !m_bAntennaExtended && !m_bAntennaAnimating;
	}

	//------------------------------------------------------------------------------------------------
	void mainLoop()
	{
		// Apply brakes if transmitting OR if antenna is extended/animating
		if (GetTransmissionActive() || !CanTruckMove())
		{
			applyBrakes();
		}
		
		// Check if radio truck can still move (most important for gamemode)
		if (!m_bDestructionProcessed && m_DamageManager)
		{
			bool canMove = SCR_AIVehicleUsability.VehicleCanMove(m_radioTruck, m_DamageManager);
			
			// Debug logging every few iterations
			static int debugCounter = 0;
			debugCounter++;
			if (debugCounter % 10 == 0) // Every 10 iterations
			{
				Print(string.Format("BC Debug - Movement check: canMove=%1, State=%2", 
					canMove, m_DamageManager.GetState()), LogLevel.NORMAL);
			}
			
			// Consider destroyed if vehicle cannot move (players can't flee)
			if (!canMove)
			{
				Print("BC Debug - MAINLOOP: Radio truck cannot move - considering it destroyed!", LogLevel.NORMAL);
				m_bDestructionProcessed = true;

				// Force retract antenna on destruction
				if (m_bAntennaExtended || m_bAntennaAnimating)
				{
					// Stop animation and force retract
					m_bAntennaAnimating = false;
					m_bAntennaExtended = false;
				}

				// Try to get the damage instigator
				IEntity lastInstigator = null;
				Instigator damageInstigator = m_DamageManager.GetInstigator();
				if (damageInstigator)
				{
					lastInstigator = damageInstigator.GetInstigatorEntity();
				}

				string destroyerFaction = GetInstigatorFactionFromEntity(lastInstigator);

				// If no instigator (truck disabled/immobilized without direct damage), BLUFOR wins
				if (destroyerFaction == "")
				{
					destroyerFaction = "DISABLED";
					Print("BC Debug - MAINLOOP: Radio truck disabled (no damage instigator) - BLUFOR wins", LogLevel.NORMAL);
				}
				else
				{
					Print(string.Format("BC Debug - MAINLOOP: Radio truck destroyed by faction: %1", destroyerFaction), LogLevel.NORMAL);
				}

				// Notify the Breaking Contact Manager
				GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
				if (bcm)
				{
					bcm.SetRadioTruckDestroyed(destroyerFaction);
				}
			}
		}
	}
	
	void applyBrakes() {
		RplComponent rplComp = RplComponent.Cast(m_radioTruck.FindComponent(RplComponent));
		// currently log is on server always, even when players steer the truck :/
		if (rplComp.IsMaster()) {
				return;
		};

		CarControllerComponent carController = CarControllerComponent.Cast(m_radioTruck.FindComponent(CarControllerComponent));
		// apparently this does not work?
		if (carController && !carController.GetPersistentHandBrake()) {
			carController.SetPersistentHandBrake(true);
			Print(string.Format("Breaking Contact RTC - setting handbrake (Antenna extended: %1, Animating: %2)", m_bAntennaExtended, m_bAntennaAnimating), LogLevel.NORMAL);
		}

		VehicleWheeledSimulation simulation = carController.GetSimulation();
		if (simulation && !simulation.GetBrake()) {
			simulation.SetBreak(1.0, true);
			Print(string.Format("Breaking Contact RTC - setting brake (Antenna extended: %1, Animating: %2)", m_bAntennaExtended, m_bAntennaAnimating), LogLevel.NORMAL);
		}
	}
	
	bool GetTransmissionActive() 
	{
		return m_bIsTransmitting;
	}
	
	bool GetIsDisabled()
	{
		return m_bIsDisabled;
	}
	
	void SetIsDisabled(bool disabled)
	{
		m_bIsDisabled = disabled;
		if (disabled)
		{
			// Stop any active transmission when disabled
			SetTransmissionActive(false);

			// Force retract antenna if extended
			if (m_bAntennaExtended || m_bAntennaAnimating)
			{
				LowerAntenna();
			}
		}
		Replication.BumpMe();
		Print(string.Format("Breaking Contact RTC - Radio truck disabled state set to %1", m_bIsDisabled), LogLevel.NORMAL);
	}

	void SetTransmissionActive(bool setTo) {
		// Don't allow transmission if the radio truck is disabled
		if (m_bIsDisabled && setTo)
		{
			Print("Breaking Contact RTC - Cannot start transmission: Radio truck is disabled", LogLevel.WARNING);
			return;
		}

		// Don't allow starting transmission if antenna is still animating
		if (setTo && m_bAntennaAnimating)
		{
			Print("Breaking Contact RTC - Cannot start transmission: Antenna is still animating", LogLevel.WARNING);
			return;
		}

		m_bIsTransmitting = setTo;
		Replication.BumpMe();

		Print(string.Format("Breaking Contact RTC -  Setting m_bIsTransmitting to %1", m_bIsTransmitting), LogLevel.NORMAL);

		// Trigger antenna animation
		if (setTo)
		{
			RaiseAntenna();
		}
		else
		{
			LowerAntenna();
		}
		
		// Immediately notify the BreakingContactManager to handle transmission points
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm) {
			bcm.ManageMarkers(); // Force immediate update instead of waiting for mainLoop
		}
		
		SCR_VehicleDamageManagerComponent VDMC = SCR_VehicleDamageManagerComponent.Cast(m_radioTruck.FindComponent(SCR_VehicleDamageManagerComponent));

		// disable transmissions for every transmission point
		if (!m_bIsTransmitting) {
			if (VDMC) {
				EnableVehicleAndRestoreFuel(m_radioTruck);
				
				Print(string.Format("Breaking Contact RTC -  Enabling Engine due to transmission ended"), LogLevel.NORMAL);
			}
		} else {
			if (VDMC) {
				
				SCR_CarControllerComponent carController = SCR_CarControllerComponent.Cast(m_radioTruck.FindComponent(SCR_CarControllerComponent));
				if (carController)
				{
					
					
				    DisableVehicleAndSaveFuel(m_radioTruck);
				    
				    // 2. Den Watchdog starten (ruft die Funktion KeepFuelEmpty alle 1000ms auf)
				    GetGame().GetCallqueue().CallLater(KeepFuelEmpty, 1000, true, m_radioTruck);
				    
				    Print("Fahrzeug stillgelegt (Kein Treibstoff).");
				} else {
					Print(string.Format("Breaking Contact RTC - No Car Controller found"), LogLevel.NORMAL);
				}
				Print(string.Format("Breaking Contact RTC -  Disabling Engine due to transmission started"), LogLevel.NORMAL);
			}
		}
	}

	
	protected GRAD_BC_TransmissionComponent GetNearestTPC()
	{
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm) {
			Print(string.Format("Breaking Contact RTC - No BCM found!"), LogLevel.ERROR);
			return null;
		}
	
		// ‘false’: I only need the nearest – do **not** spawn a new one.
		return bcm.GetNearestTransmissionPoint(m_radioTruck.GetOrigin(), false);
	}
	
	
	void DisableVehicleAndSaveFuel(IEntity vehicleEntity)
	{
	    SCR_FuelManagerComponent fuelManager = SCR_FuelManagerComponent.Cast(vehicleEntity.FindComponent(SCR_FuelManagerComponent));
	    
	    if (fuelManager)
	    {
	        array<BaseFuelNode> fuelNodes = {};
	        fuelManager.GetFuelNodesList(fuelNodes);
	        
	        // Wir speichern den Stand des ersten Tanks. 
	        // (In Arma leeren sich Tanks meist gleichmäßig, das reicht also für 99% der Fälle)
	        if (!fuelNodes.IsEmpty() && fuelNodes[0].GetMaxFuel() > 0)
	        {
	            m_fSavedFuelRatio = fuelNodes[0].GetFuel() / fuelNodes[0].GetMaxFuel();
	            Print(string.Format("Fuel gespeichert: %1 Prozent", m_fSavedFuelRatio * 100));
	        }
	        else
	        {
	            // Fallback, falls irgendwas komisch ist: Gehe von 50% oder voll aus
	            m_fSavedFuelRatio = 0.5; 
	        }
	    }
	
	    // Jetzt Motor aus und Loop starten (wie vorher besprochen)
	    SCR_CarControllerComponent carController = SCR_CarControllerComponent.Cast(vehicleEntity.FindComponent(SCR_CarControllerComponent));
	    if (carController)
	    {
	        carController.StopEngine(true);
	        GetGame().GetCallqueue().CallLater(KeepFuelEmpty, 1000, true, vehicleEntity);
	    }
	}
	
	void EnableVehicleAndRestoreFuel(IEntity vehicleEntity)
	{
	    // 1. Loop stoppen
	    GetGame().GetCallqueue().Remove(KeepFuelEmpty);
		
		if (!vehicleEntity) return;
		
		// Check: Ist das Fahrzeug mittlerweile zerstört worden?
		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(vehicleEntity.FindComponent(SCR_DamageManagerComponent));
	    if (damageManager && damageManager.IsDestroyed())
	    {
	        Print("Warnung: Versuch, zerstörtes Fahrzeug wiederherzustellen abgebrochen.");
	        m_fSavedFuelRatio = -1.0; // Reset
	        return;
	    }
	
	    // 2. Fuel wiederherstellen
	    SCR_FuelManagerComponent fuelManager = SCR_FuelManagerComponent.Cast(vehicleEntity.FindComponent(SCR_FuelManagerComponent));
	
	    if (fuelManager && m_fSavedFuelRatio >= 0)
	    {
	        array<BaseFuelNode> fuelNodes = {};
	        fuelManager.GetFuelNodesList(fuelNodes);
	        
	        foreach (BaseFuelNode node : fuelNodes)
	        {
	            if (node)
	            {
	                // Setze Fuel basierend auf dem gespeicherten Verhältnis
	                float fuelToSet = node.GetMaxFuel() * m_fSavedFuelRatio;
	                node.SetFuel(fuelToSet);
	            }
	        }
	        Print(string.Format("Fuel wiederhergestellt auf %1 Prozent", m_fSavedFuelRatio * 100));
	        
	        // Reset der Variable (optional, zur Sicherheit)
	        m_fSavedFuelRatio = -1.0;
	    }
	}

		
	void KeepFuelEmpty(IEntity vehicleEntity)
	{
	    // Sicherheitscheck: Existiert das Fahrzeug noch?
	    if (!vehicleEntity) 
	    {
	        GetGame().GetCallqueue().Remove(KeepFuelEmpty);
	        return;
	    }
		
		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(vehicleEntity.FindComponent(SCR_DamageManagerComponent));
	    if (damageManager && damageManager.IsDestroyed())
	    {
	        // Fahrzeug ist Schrott -> Wir brauchen keinen Sprit mehr entziehen.
	        // Loop stoppen, um Server-Ressourcen zu sparen.
	        GetGame().GetCallqueue().Remove(KeepFuelEmpty);
	        Print("Fahrzeug wurde zerstört - Fuel-Lock Script gestoppt.");
	        return;
	    }
	
	    SCR_FuelManagerComponent fuelManager = SCR_FuelManagerComponent.Cast(vehicleEntity.FindComponent(SCR_FuelManagerComponent));
	    
	    if (fuelManager)
	    {
	        // Wir holen uns alle Tanks des Fahrzeugs (manche Trucks haben mehrere)
	        array<BaseFuelNode> fuelNodes = {};
	        fuelManager.GetFuelNodesList(fuelNodes);
			
			Print(string.Format("Breaking Contact RTC - Keeping fuel at zero"), LogLevel.NORMAL);
	        
	        foreach (BaseFuelNode node : fuelNodes)
	        {
	            // Setze Sprit auf 0. Das wird automatisch repliziert.
	            if (node.GetFuel() > 0)
	            {
	                node.SetFuel(0);
	            }
	        }
	    }
	}
		

	//------------------------------------------------------------------------------------------------
	void SyncVariables()
	{
		Rpc(RpcAsk_Authority_SyncVariables);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Authority_SyncVariables()
	{
		//Print("BC Debug - RpcAsk_Authority_SyncTransmissionDuration()", LogLevel.NORMAL);

		Replication.BumpMe();
	}
	
	//------------------------------------------------------------------------------------------------
	void OnRadioTruckDamageStateChanged(EDamageState previousState, EDamageState newState, IEntity instigator, notnull Instigator inst)
	{
		Print(string.Format("BC Debug - Radio truck damage state changed from %1 to %2", previousState, newState), LogLevel.NORMAL);
		Print(string.Format("BC Debug - Available damage states: UNDAMAGED=%1, INTERMEDIARY=%2, DESTROYED=%3, STATE1=%4, STATE2=%5, STATE3=%6", 
			EDamageState.UNDAMAGED, EDamageState.INTERMEDIARY, EDamageState.DESTROYED, EDamageState.STATE1, EDamageState.STATE2, EDamageState.STATE3), LogLevel.NORMAL);
		
		// Get health information for better debugging
		HitZone defaultHitZone = m_DamageManager.GetDefaultHitZone();
		float currentHealth = 0;
		float maxHealth = 0;
		if (defaultHitZone)
		{
			currentHealth = defaultHitZone.GetHealth();
			maxHealth = defaultHitZone.GetMaxHealth();
		}
		Print(string.Format("BC Debug - Health after state change: %1/%2 (%3%%)", 
			currentHealth, maxHealth, (currentHealth/maxHealth)*100), LogLevel.NORMAL);
		
		// Check if the vehicle is destroyed - check multiple states that might indicate destruction
		if (newState == EDamageState.DESTROYED ||
			newState == EDamageState.INTERMEDIARY ||
			newState == EDamageState.STATE1 ||
			newState == EDamageState.STATE2 ||
			newState == EDamageState.STATE3 ||
			(defaultHitZone && currentHealth <= 0))
		{
			Print(string.Format("BC Debug - Radio truck has been destroyed! State: %1, Health: %2", newState, currentHealth), LogLevel.NORMAL);

			// Prevent multiple processing
			if (m_bDestructionProcessed)
			{
				Print("BC Debug - Destruction already processed, skipping", LogLevel.NORMAL);
				return;
			}
			m_bDestructionProcessed = true;

			// Force retract antenna on destruction
			if (m_bAntennaExtended || m_bAntennaAnimating)
			{
				// Stop animation and force retract
				m_bAntennaAnimating = false;
				m_bAntennaExtended = false;
			}

			// Get the instigator of the damage to determine which faction destroyed it
			string destroyerFaction = GetInstigatorFactionFromEntity(instigator);

			Print(string.Format("BC Debug - Radio truck destroyed by faction: %1", destroyerFaction), LogLevel.NORMAL);

			// Notify the Breaking Contact Manager
			GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
			if (bcm)
			{
				bcm.SetRadioTruckDestroyed(destroyerFaction);
			}
		}
		else
		{
			Print(string.Format("BC Debug - Radio truck damage state is %1, not triggering destruction", newState), LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	string GetInstigatorFactionFromEntity(IEntity instigator)
	{
		if (!instigator)
		{
			Print("BC Debug - No instigator provided", LogLevel.WARNING);
			return "";
		}
		
		// Check if it's a character
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(instigator);
		if (character)
		{
			string factionKey = character.GetFactionKey();
			Print(string.Format("BC Debug - Damage instigator faction: %1", factionKey), LogLevel.NORMAL);
			return factionKey;
		}
		
		// Check if it's a vehicle with a faction
		Vehicle vehicle = Vehicle.Cast(instigator);
		if (vehicle)
		{
			FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
			if (factionComp && factionComp.GetAffiliatedFaction())
			{
				string factionKey = factionComp.GetAffiliatedFaction().GetFactionKey();
				Print(string.Format("BC Debug - Vehicle instigator faction: %1", factionKey), LogLevel.NORMAL);
				return factionKey;
			} else {
				string factionKey = "Empty";
				return factionKey;
			}
		}
		
		// Check if it's a projectile or other damage source
		// For projectiles, we might need to get the shooter/owner
		// Try to find if there's a weapon or shooter component
		WeaponComponent weaponComp = WeaponComponent.Cast(instigator.FindComponent(WeaponComponent));
		if (weaponComp)
		{
			// Try to get the weapon owner (the entity holding the weapon)
			IEntity weaponOwner = weaponComp.GetOwner();
			if (weaponOwner)
			{
				// Check if the weapon owner is a character
				SCR_ChimeraCharacter ownerChar = SCR_ChimeraCharacter.Cast(weaponOwner);
				if (ownerChar)
				{
					string factionKey = ownerChar.GetFactionKey();
					Print(string.Format("BC Debug - Weapon owner faction: %1", factionKey), LogLevel.NORMAL);
					return factionKey;
				}
			}
		}
		
		// Try alternative approach: check if instigator has faction affiliation directly
		FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(instigator.FindComponent(FactionAffiliationComponent));
		if (factionComp && factionComp.GetAffiliatedFaction())
		{
			string factionKey = factionComp.GetAffiliatedFaction().GetFactionKey();
			Print(string.Format("BC Debug - Direct faction affiliation: %1", factionKey), LogLevel.NORMAL);
			return factionKey;
		}
		
		Print("BC Debug - Could not determine instigator faction", LogLevel.WARNING);
		return ""; // Unknown faction
	}
	
	//------------------------------------------------------------------------------------------------
	void OnAnyVehicleDestroyed(int playerId)
	{
		// This is called for ANY vehicle destruction, so we need to check if it's our radio truck
		// Unfortunately, the static event doesn't provide the vehicle entity, so we check our own state
		if (!m_bDestructionProcessed && m_DamageManager)
		{
			EDamageState currentState = m_DamageManager.GetState();
			
			// Get health information
			HitZone defaultHitZone = m_DamageManager.GetDefaultHitZone();
			float currentHealth = 0;
			if (defaultHitZone)
			{
				currentHealth = defaultHitZone.GetHealth();
			}
			
			if (currentState == EDamageState.DESTROYED ||
				currentState == EDamageState.INTERMEDIARY ||
				currentState == EDamageState.STATE1 ||
				currentState == EDamageState.STATE2 ||
				currentState == EDamageState.STATE3 ||
				(defaultHitZone && currentHealth <= 0))
			{
				Print(string.Format("BC Debug - STATIC EVENT: Radio truck destruction detected via static event! State: %1, Health: %2", currentState, currentHealth), LogLevel.NORMAL);
				m_bDestructionProcessed = true;

				// Force retract antenna on destruction
				if (m_bAntennaExtended || m_bAntennaAnimating)
				{
					// Stop animation and force retract
					m_bAntennaAnimating = false;
					m_bAntennaExtended = false;
				}

				// Try to get the damage instigator
				IEntity lastInstigator = null;
				Instigator damageInstigator = m_DamageManager.GetInstigator();
				if (damageInstigator)
				{
					lastInstigator = damageInstigator.GetInstigatorEntity();
				}

				string destroyerFaction = GetInstigatorFactionFromEntity(lastInstigator);

				Print(string.Format("BC Debug - STATIC EVENT: Radio truck destroyed by faction: %1", destroyerFaction), LogLevel.NORMAL);

				// Notify the Breaking Contact Manager
				GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
				if (bcm)
				{
					bcm.SetRadioTruckDestroyed(destroyerFaction);
				}
			}
		}
	}


	/*

	//Check if garage is nearby
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), m_fGarageSearchRadius, FindFirstGarage, FilterGarage);
		return (m_GarageManager);
	}

	//------------------------------------------------------------------------------------------------
	bool FilterGarage(IEntity ent)
	{
		return (ent.FindComponent(EL_GarageManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	bool FindFirstGarage(IEntity ent)
	{
		m_GarageManager = EL_GarageManagerComponent.Cast(ent.FindComponent(EL_GarageManagerComponent));
		if (!m_GarageManager)
			return true; //Continue search

		return false; //Stop search
	}

	*/






	/*
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Authority_SetMarkerVisibility(bool isVisible)
	{
		Print("BC Debug - RpcAsk_Authority_SetMarkerVisibility()", LogLevel.NORMAL);

		m_mapDescriptorComponent.Item().SetVisible(isVisible);

		m_bIsVisible = isVisible;

		Replication.BumpMe();

		Rpc(RpcDo_Broadcast_SetMarkerVisibility, isVisible);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_Broadcast_SetMarkerVisibility(bool isVisible)
	{
		Print("BC Debug - RpcDo_Broadcast_SetMarkerVisibility()", LogLevel.NORMAL);

		m_mapDescriptorComponent.Item().SetVisible(isVisible);
	}
	*/
}
