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

    void GRAD_MapMarkerManager()
    {
        Print("GRAD_MapMarkerManager: Constructor called", LogLevel.ERROR);
    }
    
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

    // Pulse animation variables
    protected float m_fPulseDuration = 2.0; // seconds for a full cycle
    protected float m_fPulseTime = 0.0;

    // Persistent draw command for the pulsing circle
    protected ref PolygonDrawCommand m_PulsingCircleCmd;
    protected ref array<ref CanvasWidgetCommand> m_MarkerDrawCommands;

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
            m_PulsingCircleCmd = new PolygonDrawCommand();
            m_MarkerDrawCommands = { m_PulsingCircleCmd };
            m_Canvas.SetDrawCommands(m_MarkerDrawCommands);
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
        int markerCount = 0;
        foreach (GRAD_BC_TransmissionComponent tpc : tpcs)
        {
            if (!tpc) {
                Print("GRAD_MapMarkerManager: Skipping null TPC!", LogLevel.WARNING);
                continue; // guard against null references
            }

            TransmissionEntry entry = new TransmissionEntry();
            entry.m_Position = tpc.GetPosition();
            entry.m_State    = tpc.GetTransmissionState();
            entry.m_Radius   = 1000; // Force radius to 1000m for visibility

            PrintFormat("GRAD_MapMarkerManager: Added marker at %1 with state %2", entry.m_Position, entry.m_State);
            m_AllMarkers.Insert(entry);
            markerCount++;
        }
        PrintFormat("GRAD_MapMarkerManager: Total markers after OnMapOpen: %1", markerCount);
        // Now m_AllMarkers holds one entry per transmission point,
        // each tagged with its position, state, and drawing radius.

        // ─── Canvas debug: check existence and size ───────────────
        if (!m_Canvas) {
            Print("GRAD_MapMarkerManager: m_Canvas is NULL!", LogLevel.ERROR);
        } else {
            float w, h;
            m_Canvas.GetScreenSize(w, h);
            PrintFormat("GRAD_MapMarkerManager: m_Canvas size: %1 x %2", w, h);
            m_Canvas.SetVisible(true);
            // Print widget hierarchy
            Widget parent = m_Canvas;
            while (parent) {
                PrintFormat("Widget: %1", parent.GetName());
                parent = parent.GetParent();
            }
        }

        // NOTE: CanvasWidget sizing must be set in the layout file, not in script.
        // If you see zero or tiny canvas size in debug, fix the anchors and size in MapCanvasLayer.layout.
        // Remove any SetWidth/SetHeight/SetSize/BringToFront calls from script.
        // Use debug prints to check canvas size after creation.

        m_RegisterPostFrame();
    }

    // Try to populate markers, retrying if none found (max 10 tries)
    void m_AttemptPopulateMarkers(int attempt)
    {
        GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
        if (!bcm) {
            Print("GRAD_MapMarkerManager: BreakingContactManager not found!", LogLevel.ERROR);
            return;
        }
        array<GRAD_BC_TransmissionComponent> tpcs = bcm.GetTransmissionPoints();
        if (!tpcs || tpcs.Count() == 0) {
            if (attempt < 10) {
                PrintFormat("GRAD_MapMarkerManager: No TransmissionPoints yet, retrying (%1)...", attempt+1);
                GetGame().GetCallqueue().CallLater(this.m_AttemptPopulateMarkers, 100, false, attempt+1);
            } else {
                Print("GRAD_MapMarkerManager: No TransmissionPoints after retries!", LogLevel.WARNING);
            }
            return;
        }
        int markerCount = 0;
        foreach (GRAD_BC_TransmissionComponent tpc : tpcs)
        {
            if (!tpc) continue;
            TransmissionEntry entry = new TransmissionEntry();
            entry.m_Position = tpc.GetPosition();
            entry.m_State    = tpc.GetTransmissionState();
            entry.m_Radius   = 1000;
            m_AllMarkers.Insert(entry);
            markerCount++;
        }
        PrintFormat("GRAD_MapMarkerManager: Total markers after populate: %1", markerCount);
    }

    // ───────────────────────────────────────────────────────────────────────────────
    // Draw
    //
    // Called every frame while the map is open. We clear the previous draw commands
    // (inherited from GRAD_MapMarkerLayer) and then insert a new command for each entry.
    // ───────────────────────────────────────────────────────────────────────────────
    override void Draw()
    {
        // Only update pulse timer for use in EOnPostFrame
        float dt = GetGame().GetWorld().GetWorldTime() / 1000.0;
        m_fPulseTime = Math.Mod(dt, m_fPulseDuration) / m_fPulseDuration;
        PrintFormat("GRAD_MapMarkerManager: Draw() called, m_AllMarkers.Count() = %1", m_AllMarkers.Count());
    }

    void EOnPostFrame(IEntity owner, float timeSlice)
    {
        Print("EOnPostFrame: updating marker vertices", LogLevel.ERROR);
        m_PulsingCircleCmd.m_Vertices = new array<float>();
        // Fade out: start at 30% opacity, go to 0% at full size
        float alpha = Math.Lerp(0.3, 0.0, m_fPulseTime); // 0.3 = 30% opacity
        int iAlpha = Math.Round(alpha * 255);
        m_PulsingCircleCmd.m_iColor = ARGB(iAlpha,255,0,0); // red, fading out

        // Remove any static outline commands from previous frame
        while (m_MarkerDrawCommands.Count() > 2) m_MarkerDrawCommands.Remove(m_MarkerDrawCommands.Count() - 1);

        if (m_AllMarkers.Count() > 0) {
            TransmissionEntry entry = m_AllMarkers[0];
            float screenX, screenY;
            m_MapEntity.WorldToScreen(entry.m_Position[0], entry.m_Position[2], screenX, screenY, true);
            PrintFormat("Marker world pos: %1, screenX: %2, screenY: %3", entry.m_Position, screenX, screenY);
            // Convert 1000m world radius to map screen pixels
            float worldRadius = 1000.0; // meters
            float edgeWorldX = entry.m_Position[0] + worldRadius;
            float edgeWorldY = entry.m_Position[2];
            float edgeScreenX, edgeScreenY;
            m_MapEntity.WorldToScreen(edgeWorldX, edgeWorldY, edgeScreenX, edgeScreenY, true);
            float radius = Math.Sqrt(Math.Pow(edgeScreenX - screenX, 2) + Math.Pow(edgeScreenY - screenY, 2));
            PrintFormat("Marker screen radius (for 1000m): %1", radius);
            float twoPi = 6.28318530718;
            for (int i = 0; i < 32; i++) {
                float angle = twoPi * i / 32;
                float x = screenX + Math.Cos(angle) * radius;
                float y = screenY + Math.Sin(angle) * radius;
                m_PulsingCircleCmd.m_Vertices.Insert(x);
                m_PulsingCircleCmd.m_Vertices.Insert(y);
            }

            // Draw static outline for INTERRUPTED (gray) or DONE (green)
            if (entry.m_State == ETransmissionState.INTERRUPTED || entry.m_State == ETransmissionState.DONE) {
                ref LineDrawCommand outlineCmd = new LineDrawCommand();
                outlineCmd.m_Vertices = new array<float>();
                float outlineRadius = radius; // use calculated screen radius for outline
                for (int i = 0; i < 32; i++) {
                    float angle = twoPi * i / 32;
                    float x = screenX + Math.Cos(angle) * outlineRadius;
                    float y = screenY + Math.Sin(angle) * outlineRadius;
                    outlineCmd.m_Vertices.Insert(x);
                    outlineCmd.m_Vertices.Insert(y);
                }
                // Close loop
                outlineCmd.m_Vertices.Insert(screenX + Math.Cos(0) * outlineRadius);
                outlineCmd.m_Vertices.Insert(screenY + Math.Sin(0) * outlineRadius);
                if (entry.m_State == ETransmissionState.INTERRUPTED) {
                    outlineCmd.m_iColor = ARGB(255,128,128,128); // gray
                } else {
                    outlineCmd.m_iColor = ARGB(255,0,255,0); // green
                }
                outlineCmd.m_fOutlineWidth = 3.0; // 3px outline
                m_MarkerDrawCommands.Insert(outlineCmd);
            }
        }
        // Remove debug rectangle code
        m_Canvas.SetDrawCommands(m_MarkerDrawCommands);
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

        m_UnregisterPostFrame();
    }

    // Use null for the owner argument in CallLater, since EOnPostFrame expects IEntity and we don't use it
    void m_RegisterPostFrame()
    {
        GetGame().GetCallqueue().CallLater(this.EOnPostFrame, 0, true, null, 0.0);
    }
    void m_UnregisterPostFrame()
    {
        GetGame().GetCallqueue().Remove(this.EOnPostFrame);
    }
}
