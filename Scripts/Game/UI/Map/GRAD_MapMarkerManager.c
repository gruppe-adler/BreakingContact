//------------------------------------------------------------------------------------------------
// Simple helper that holds a transmission point’s position, state, and radius.
//------------------------------------------------------------------------------------------------
class TransmissionEntry
{
    vector              m_Position;
    ETransmissionState  m_State;
    int                 m_Radius;
}

//------------------------------------------------------------------------------------------------
// GRAD_MapMarkerManager
//
// Inherits from GRAD_MapMarkerLayer (which in turn extends SCR_MapModuleBase).
// - OnMapOpen: Populate m_AllMarkers from the current GRAD_BC_TransmissionComponent list.
// - Draw(): Iterate m_AllMarkers, draw circles or icons based on each entry’s state.
// - OnMapClose: Clear the list so next open starts fresh.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class GRAD_MapMarkerManager : GRAD_MapMarkerLayer
{
    // ───────────────────────────────────────────────────────────────────────────────
    // Member variables
    // ───────────────────────────────────────────────────────────────────────────────

    // holds (position, state, radius).
    protected ref array<ref TransmissionEntry> m_AllMarkers;

    // Default radius to use when drawing a circle around a transmission point
    protected int m_RangeDefault = 1000;

    // We only need to load this once; do not reload on every map open.
    protected ref SharedItemRef m_IconDestroyed;

    // A simple flag so we only create our array & load the texture once.
    protected bool m_IsInitialized = false;

    // ───────────────────────────────────────────────────────────────────────────────
    // OnMapOpen
    // ───────────────────────────────────────────────────────────────────────────────
    override void OnMapOpen(MapConfiguration config)
    {
        super.OnMapOpen(config);

        // Create our draw‐command array (cleared by default on each open)
        m_Commands = new array<ref CanvasWidgetCommand>();

        // Create the widget & canvas from the layout (inherited from GRAD_MapMarkerLayer)
        m_Widget = GetGame().GetWorkspace().CreateWidgets(m_Layout);
        m_Canvas = CanvasWidget.Cast(m_Widget.FindAnyWidget("Canvas"));

        // ─── Lazy Initialization ───────────────────────────────────────────────
        // The first time OnMapOpen runs, allocate m_AllMarkers and load the “destroyed” icon.
        // Subsequent calls will simply Clear() the same array.
        if (!m_IsInitialized)
        {
            m_AllMarkers = new array<ref TransmissionEntry>();
            m_IconDestroyed = m_Canvas.LoadTexture("{09A7BA5E10D5E250}UI/Textures/Map/transmission_destroyed.edds");
            m_IsInitialized = true;
        }

        // Always start by emptying out any old entries (so we repopulate fresh)
        m_AllMarkers.Clear();

        // ─── Populate m_AllMarkers ─────────────────────────────────────────────
        // 1) Grab the BreakingContactManager
        GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
        if (!bcm)
        {
            Print("GRAD_MapMarkerManager: BreakingContactManager not found!", LogLevel.ERROR);
            return;
        }

        // 2) Get the list of all existing transmission‐point objects
        array<GRAD_BC_TransmissionComponent> tpcs = bcm.GetTransmissionPoints();
        if (!tpcs)
        {
            Print("GRAD_MapMarkerManager: No TransmissionPoints returned!", LogLevel.WARNING);
            return;
        }

        // 3) For each TPC, build a TransmissionEntry and insert it into m_AllMarkers
        foreach (GRAD_BC_TransmissionComponent tpc : tpcs)
        {
            if (!tpc)
                continue; // guard against null references

            TransmissionEntry entry = new TransmissionEntry();
            entry.m_Position = tpc.GetPosition();
            entry.m_State    = tpc.GetTransmissionState();
            entry.m_Radius   = m_RangeDefault; // Or compute a custom radius if needed

            m_AllMarkers.Insert(entry);
        }

        // Now m_AllMarkers holds one entry per transmission point,
        // each tagged with its position, state, and drawing radius.
    }

    // ───────────────────────────────────────────────────────────────────────────────
    // Draw
    //
    // Called every frame while the map is open. We clear the previous draw commands
    // (inherited from GRAD_MapMarkerLayer) and then insert a new command for each entry.
    // ───────────────────────────────────────────────────────────────────────────────
    override void Draw()
    {
        // Ensure the draw‐command buffer is empty at the start of each Draw()
        m_Commands.Clear();

        // Iterate every transmission entry, draw a circle or icon based on its state
        foreach (TransmissionEntry entry : m_AllMarkers)
        {
            switch (entry.m_State)
            {
                case ETransmissionState.TRANSMITTING:
                    // Semi‐transparent red circle
                    DrawCircle(entry.m_Position, entry.m_Radius, ARGB(50, 255, 50, 50));
                    break;

                case ETransmissionState.INTERRUPTED:
                    // Semi‐transparent gray circle
                    DrawCircle(entry.m_Position, entry.m_Radius, ARGB(50, 50, 50, 50));
                    break;

                case ETransmissionState.DONE:
                    // Semi‐transparent green circle
                    DrawCircle(entry.m_Position, entry.m_Radius, ARGB(50, 50, 255, 50));
                    break;

                case ETransmissionState.DISABLED:
                    // Draw the “destroyed” icon at that position
                    DrawImage(entry.m_Position, 25, 25, m_IconDestroyed);
                    break;

                default:
                    // If you add more states in the future, handle them here
                    break;
            }
        }

        // Finally, push the collected draw commands onto the Canvas
        if (m_Commands.Count() > 0)
        {
            m_Canvas.SetDrawCommands(m_Commands);
        }
    }

    // ───────────────────────────────────────────────────────────────────────────────
    // OnMapClose
    //
    // Called once when the map closes: simply clear our entries.
    // We do not need to null‐out everything—just clear the array.
    // ───────────────────────────────────────────────────────────────────────────────
    override void OnMapClose(MapConfiguration config)
    {
        super.OnMapClose(config);

        // Remove the widget from the workspace
        if (m_Widget)
            m_Widget.RemoveFromHierarchy();

        // Clear out our list so next map‐open starts fresh
        if (m_AllMarkers)
            m_AllMarkers.Clear();
    }
}
