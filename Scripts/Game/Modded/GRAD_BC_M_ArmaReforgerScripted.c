modded class ArmaReforgerScripted
{
	override event void OnWorldPostProcess(World world)
	{
		super.OnWorldPostProcess(world);
		SCR_FuelConsumptionComponent.SetGlobalFuelConsumptionScale(1);
	}
}
