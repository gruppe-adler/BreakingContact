//------------------------------------------------------------------------------------------------
// Reports HE grenade detonations to the replay system the moment they happen.
//
// TimerTriggerComponent is the fuse on every timed explosive - it is what the RGD5 and M67 carry,
// holding their ExplosionEffect in PROJECTILE_EFFECTS with TIMER 4.
//
// Why this replaces polling: GRAD_BC_GrenadeTracker sampled WasTriggered() every 250ms, which
// never once caught a detonation across many test rounds. The flag is set and the grenade entity
// is destroyed within the same frame, so a poll almost always misses the window entirely. Hooking
// the component means the event cannot be missed regardless of timing.
//
// Smoke does not use this path - it never detonates, and GRAD_BC_GrenadeTracker still handles it
// by watching where the canister comes to rest.
//------------------------------------------------------------------------------------------------
modded class TimerTriggerComponent
{
	// Set once this fuse has reported, so a trigger that fires repeatedly cannot record twice.
	protected bool m_bBCReported = false;

	//------------------------------------------------------------------------------------------------
	//! Physics contact on the fuse's owner. Grenades bounce before detonating, so this fires
	//! several times per throw - the guard in ReportDetonationToReplay keeps only the first.
	//!
	//! NOTE: only `event` methods can be overridden here. OnUserTrigger/WasTriggered are
	//! `proto external`, i.e. engine-implemented, and are callable but not overridable.
	override protected void EOnContact(IEntity owner, IEntity other, Contact contact)
	{
		super.EOnContact(owner, other, contact);

		// Contact alone is not detonation - a grenade lands and rolls before its fuse expires.
		// WasTriggered() is queried here rather than polled, so the check happens on an engine
		// event rather than on a 250ms timer that kept missing the single frame it is true.
		if (!WasTriggered())
			return;

		ReportDetonationToReplay(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Fired when the trigger goes off close enough to the thrower to matter. Also routed to the
	//! replay, since the detonation position is exactly what is being reported.
	override void TriggeredInSafetyDistance(IEntity pHitEntity, inout vector outMat[3], IEntity damageSource, notnull Instigator instigator, string colliderName, float speed)
	{
		super.TriggeredInSafetyDistance(pHitEntity, outMat, damageSource, instigator, colliderName, speed);

		// outMat is declared [3]: valid indices are 0..2 and outMat[0] is the world position.
		ReportDetonationAt(damageSource, outMat[0]);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReportDetonationToReplay(IEntity owner)
	{
		if (!owner)
			return;

		ReportDetonationAt(owner, owner.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	protected void ReportDetonationAt(IEntity owner, vector detonationPos)
	{
		if (m_bBCReported)
			return;

		if (!owner)
			return;

		// Recording is server-authoritative, matching the rest of the replay system.
		if (!Replication.IsServer())
			return;

		string prefabPath = "";
		EntityPrefabData prefabData = owner.GetPrefabData();
		if (prefabData)
			prefabPath = prefabData.GetPrefabName();

		// Only explosives the replay knows how to draw. Smoke is excluded deliberately: it is
		// handled as a lingering cloud rather than a point detonation.
		EGradBCExplosiveKind kind;
		if (!GRAD_BC_ExplosiveCatalog.Classify(prefabPath, kind))
			return;

		if (kind == EGradBCExplosiveKind.SMOKE)
			return;

		GRAD_BC_ReplayManager replayManager = GRAD_BC_ReplayManager.GetInstance();
		if (!replayManager)
			return;

		m_bBCReported = true;

		BaseWorld world = GetGame().GetWorld();
		float now = 0;
		if (world)
			now = world.GetWorldTime() / 1000.0;

		replayManager.RecordExplosiveEvent(
			kind,
			detonationPos,
			detonationPos,
			now,
			now + GRAD_BC_ExplosiveCatalog.HE_FLASH_DURATION,
			0,
			"");

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC: HE detonation '%1' at %2", prefabPath, detonationPos.ToString()), LogLevel.NORMAL);
	}
}
