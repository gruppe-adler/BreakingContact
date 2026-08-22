//------------------------------------------------------------------------------------------------
// Fixes a crash in COALITION Lobby's safestart teardown.
//
// COA_SafestartManager.DeactivateSafeStartEHs() walks every entity that had safestart handlers
// applied and removes them. Partway through it does:
//
//     COA_PolyZoneEffectHandler polyZoneEffectHandler =
//         COA_PolyZoneEffectHandler.Cast(controlledEntity.FindComponent(COA_PolyZoneEffectHandler));
//     polyZoneEffectHandler.ClearAllEffects();
//
// with no null check. Any entity lacking that component throws "NULL pointer to instance", which
// ABORTS THE WHOLE LOOP - every entity after the failing one keeps its safestart handlers:
//
//     OnWeaponFired(...)   { delete entity; }
//     OnGrenadeThrown(...) { delete entity; }
//
// The result is a session that looks live (players can fire) while projectiles and thrown
// grenades are silently deleted at the muzzle for the affected characters. Observed in-game:
// an RPG that launched but never detonated, and HE grenades that never went off.
//
// This override reimplements the teardown defensively - same work, every entity processed, each
// component null-checked, so one bad entity cannot strand the rest.
//------------------------------------------------------------------------------------------------
modded class COA_SafestartManager
{
	//------------------------------------------------------------------------------------------------
	//! Removes safestart restrictions from every tracked entity.
	//!
	//! Overridden rather than post-called because the base implementation aborts mid-loop on a
	//! null COA_PolyZoneEffectHandler, so calling super() first would still strand entities.
	override protected void DeactivateSafeStartEHs()
	{
		int restored = 0;
		int skipped = 0;

		// Snapshot the keys first. The base implementation removes entries from the map while
		// iterating it, which is what makes the crash mid-loop so damaging; iterating a copy keeps
		// the traversal well-defined and lets the map itself stay intact for the scheduled retries.
		array<IEntity> trackedEntities = {};
		foreach (IEntity tracked, bool hasHandlersFlag : m_mEntitiesWithEHsMap)
		{
			trackedEntities.Insert(tracked);
		}

		foreach (IEntity controlledEntity : trackedEntities)
		{
			if (!controlledEntity)
			{
				skipped++;
				continue;
			}

			// Re-enable damage handling
			SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(
				controlledEntity.FindComponent(SCR_CharacterDamageManagerComponent));
			if (damageManager)
				damageManager.EnableDamageHandling(true);

			// Turn off weapon safety
			CharacterControllerComponent charComp = CharacterControllerComponent.Cast(
				controlledEntity.FindComponent(CharacterControllerComponent));
			if (charComp)
				charComp.SetSafety(false, false);

			// Remove the projectile/grenade deletion handlers. This is the critical part: leaving
			// these attached is what silently destroys everything the player fires or throws.
			EventHandlerManagerComponent eventHandler = EventHandlerManagerComponent.Cast(
				controlledEntity.FindComponent(EventHandlerManagerComponent));
			if (eventHandler)
			{
				eventHandler.RemoveScriptHandler("OnProjectileShot", this, OnWeaponFired);
				eventHandler.RemoveScriptHandler("OnGrenadeThrown", this, OnGrenadeThrown);
				restored++;
			}
			else
			{
				skipped++;
			}

			// The unguarded call in the base implementation. Not every entity carries this
			// component, and a missing one must not prevent the handlers above from being removed.
			COA_PolyZoneEffectHandler polyZoneEffectHandler = COA_PolyZoneEffectHandler.Cast(
				controlledEntity.FindComponent(COA_PolyZoneEffectHandler));
			if (polyZoneEffectHandler)
				polyZoneEffectHandler.ClearAllEffects();
		}

		// Deliberately NOT clearing m_mEntitiesWithEHsMap. ToggleSafeStartServer schedules this
		// method several times (immediately, +1500ms and +12500ms) as its own safety net against
		// entities that were mid-spawn on the first pass. Clearing the map would turn every repeat
		// call into a no-op and defeat that. The base implementation removes entries individually
		// as it goes; leaving the map populated keeps the retries meaningful, and removing an
		// already-removed handler is harmless.

		Print(string.Format("BC: safestart teardown - handlers removed from %1 entities, %2 skipped",
			restored, skipped), LogLevel.NORMAL);
	}
}
