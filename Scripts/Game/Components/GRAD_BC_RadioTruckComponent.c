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
	
	protected float m_fSavedFuelRatio = -1.0;

	private SCR_MapDescriptorComponent m_mapDescriptorComponent;
	private VehicleWheeledSimulation_SA_B m_VehicleWheeledSimulationComponent;

	private RplComponent m_RplComponent;

	private GRAD_BC_TransmissionComponent m_nearestTransmissionPoint;
	
	// Event handler for vehicle destruction
	private SCR_VehicleDamageManagerComponent m_DamageManager;
	
	// Track if we've already processed destruction to avoid multiple triggers
	private bool m_bDestructionProcessed = false;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		//Print("BC Debug - OnPostInit()", LogLevel.NORMAL);

		m_radioTruck = Vehicle.Cast(GetOwner());

		m_mapDescriptorComponent = SCR_MapDescriptorComponent.Cast(m_radioTruck.FindComponent(SCR_MapDescriptorComponent));
		m_VehicleWheeledSimulationComponent = VehicleWheeledSimulation_SA_B.Cast(m_radioTruck.FindComponent(VehicleWheeledSimulation_SA_B));

		m_RplComponent = RplComponent.Cast(m_radioTruck.FindComponent(RplComponent));

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

		//PrintFormat("BC Debug - IsMaster(): %1", m_RplComponent.IsMaster()); // IsMaster() does not mean Authority
		//PrintFormat("BC Debug - IsProxy(): %1", m_RplComponent.IsProxy());
		//PrintFormat("BC Debug - IsOwner(): %1", m_RplComponent.IsOwner());

		if (m_RplComponent.IsMaster())
			GetGame().GetCallqueue().CallLater(mainLoop, 1000, true);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		// No event handlers to clean up anymore
		super.OnDelete(owner);
	}


	//------------------------------------------------------------------------------------------------
	void mainLoop()
	{
		if (GetTransmissionActive()) { applyBrakes(); }
		
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
				
				// Try to get the damage instigator
				IEntity lastInstigator = null;
				Instigator damageInstigator = m_DamageManager.GetInstigator();
				if (damageInstigator)
				{
					lastInstigator = damageInstigator.GetInstigatorEntity();
				}
				
				string destroyerFaction = GetInstigatorFactionFromEntity(lastInstigator);
				
				Print(string.Format("BC Debug - MAINLOOP: Radio truck destroyed by faction: %1", destroyerFaction), LogLevel.NORMAL);
				
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
			Print(string.Format("Breaking Contact RTC - setting handbrake"), LogLevel.NORMAL);
		}
		
		VehicleWheeledSimulation simulation = carController.GetSimulation();
		if (simulation && !simulation.GetBrake()) {
			simulation.SetBreak(1.0, true);	
			Print(string.Format("Breaking Contact RTC - setting brake"), LogLevel.NORMAL);
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
		
		m_bIsTransmitting = setTo;
		Replication.BumpMe();
		
		Print(string.Format("Breaking Contact RTC -  Setting m_bIsTransmitting to %1", m_bIsTransmitting), LogLevel.NORMAL);
		
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
