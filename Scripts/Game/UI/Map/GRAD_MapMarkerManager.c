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

    // Constant for 2*PI
    static const float TWO_PI = Math.PI * 2.0;

    // Persistent draw command for the pulsing circle
    protected ref PolygonDrawCommand m_PulsingCircleCmd;
    protected ref array<ref CanvasWidgetCommand> m_MarkerDrawCommands;

    // Add for progress text widgets
    protected ref array<Widget> m_ProgressTextWidgets;

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
            m_ProgressTextWidgets = new array<Widget>();
            m_IsInitialized = true;
        }

        // Always start by emptying out any old entries (so we repopulate fresh)
        m_AllMarkers.Clear();

        // Subscribe to transmission point events
        GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
        if (bcm)
        {
            if (!m_TransmissionPointInvoker) m_TransmissionPointInvoker = new ScriptInvoker();
            m_TransmissionPointInvoker.Insert(this.PopulateMarkers);
            bcm.AddTransmissionPointListener(m_TransmissionPointInvoker);
        }
        PopulateMarkers();

        // ─── Canvas debug: check existence and size ───────────────
        if (!m_Canvas) {
            Print("GRAD_MapMarkerManager: m_Canvas is NULL!", LogLevel.ERROR);
        } else {
            float w, h;
            m_Canvas.GetScreenSize(w, h);
            // PrintFormat("GRAD_MapMarkerManager: m_Canvas size: %1 x %2", w, h);
            m_Canvas.SetVisible(true);
            // Print widget hierarchy
            Widget parent = m_Canvas;
            while (parent) {
                // PrintFormat("Widget: %1", parent.GetName());
                parent = parent.GetParent();
            }
        }

        // NOTE: CanvasWidget sizing must be set in the layout file, not in script.
        // If you see zero or tiny canvas size in debug, fix the anchors and size in MapCanvasLayer.layout.
        // Remove any SetWidth/SetHeight/SetSize/BringToFront calls from script.
        // Use debug prints to check canvas size after creation.

        m_RegisterPostFrame();
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
    }
    
    void EOnPostFrame(IEntity owner, float timeSlice)
    {
        // Reduce log spam - only log occasionally
        static int logCounter = 0;
        logCounter++;
        if (logCounter % 50 == 0) { // Log every 5 seconds instead of every 100ms
            PrintFormat("GRAD_MapMarkerManager: EOnPostFrame called - canvas=%1", m_Canvas != null);
        }
        
        if (!m_Canvas) {
            if (logCounter % 50 == 0) {
                PrintFormat("GRAD_MapMarkerManager: Canvas is null, returning");
            }
            return; // Canvas not ready
        }

        // Always repopulate markers to get current states
        PopulateMarkers();
        int markerCount = 0;
        if (m_AllMarkers) {
            markerCount = m_AllMarkers.Count();
        }
        if (logCounter % 50 == 0) {
            PrintFormat("GRAD_MapMarkerManager: After PopulateMarkers, marker count: %1", markerCount);
        }

        if (!m_AllMarkers || m_AllMarkers.Count() == 0) {
            if (logCounter % 50 == 0) {
                PrintFormat("GRAD_MapMarkerManager: No markers, clearing draw commands");
            }
            m_Canvas.SetDrawCommands({});
            return; // No markers to draw
        }

        // Initialize draw commands array if needed
        if (!m_MarkerDrawCommands) {
            m_MarkerDrawCommands = new array<ref CanvasWidgetCommand>();
        }
        m_MarkerDrawCommands.Clear();

        float twoPi = 6.28318530718;

        // --- Draw marker circles and outlines ---
        for (int markerIdx = 0; markerIdx < m_AllMarkers.Count(); markerIdx++)
        {
            TransmissionEntry entry = m_AllMarkers[markerIdx];
            if (logCounter % 50 == 0) {
                PrintFormat("GRAD_MapMarkerManager: Processing marker %1 at %2 with state %3", markerIdx, entry.m_Position, entry.m_State);
            }

            float screenX, screenY;
            m_MapEntity.WorldToScreen(entry.m_Position[0], entry.m_Position[2], screenX, screenY, true);
            
            // Convert world radius to screen radius
            float radiusWorldX = entry.m_Position[0] + entry.m_Radius;
            float radiusWorldY = entry.m_Position[2];
            float radiusScreenX, radiusScreenY;
            m_MapEntity.WorldToScreen(radiusWorldX, radiusWorldY, radiusScreenX, radiusScreenY, true);
            float radius = Math.AbsFloat(radiusScreenX - screenX);
            if (logCounter % 50 == 0) {
                PrintFormat("GRAD_MapMarkerManager: Screen position: %1,%2, radius: %3", screenX, screenY, radius);
            }

            // --- Pulsing circle for TRANSMITTING ---
            if (entry.m_State == ETransmissionState.TRANSMITTING)
            {
                if (!m_PulsingCircleCmd) {
                    m_PulsingCircleCmd = new PolygonDrawCommand();
                }
                m_PulsingCircleCmd.m_Vertices = new array<float>();
                
                float alpha = Math.Lerp(0.6, 0.0, m_fPulseTime);
                int iAlpha = Math.Round(alpha * 255);
                m_PulsingCircleCmd.m_iColor = ARGB(iAlpha,255,0,0); // red, fading out

                float pulseRadius = radius * Math.Lerp(0.5, 1.5, m_fPulseTime);
                for (int i = 0; i < 32; i++) {
                    float angle = twoPi * i / 32;
                    float x = screenX + Math.Cos(angle) * pulseRadius;
                    float y = screenY + Math.Sin(angle) * pulseRadius;
                    m_PulsingCircleCmd.m_Vertices.Insert(x);
                    m_PulsingCircleCmd.m_Vertices.Insert(y);
                }

                m_MarkerDrawCommands.Insert(m_PulsingCircleCmd);
                if (logCounter % 50 == 0) {
                    PrintFormat("GRAD_MapMarkerManager: Added pulsing circle for TRANSMITTING at %1,%2 with alpha %3", screenX, screenY, iAlpha);
                }
            }

            // --- Static outline for INTERRUPTED (gray), DONE (green), OFF (blue), or DISABLED (red) ---
            if (entry.m_State == ETransmissionState.INTERRUPTED || entry.m_State == ETransmissionState.DONE || entry.m_State == ETransmissionState.OFF || entry.m_State == ETransmissionState.DISABLED) {
                ref LineDrawCommand outlineCmd = new LineDrawCommand();
                outlineCmd.m_Vertices = new array<float>();
                
                for (int i = 0; i <= 32; i++) { // Note: <= to close the circle
                    float angle = twoPi * i / 32;
                    float x = screenX + Math.Cos(angle) * radius;
                    float y = screenY + Math.Sin(angle) * radius;
                    outlineCmd.m_Vertices.Insert(x);
                    outlineCmd.m_Vertices.Insert(y);
                }
                
                if (entry.m_State == ETransmissionState.INTERRUPTED) {
                    outlineCmd.m_iColor = ARGB(255,128,128,128); // gray
                } else if (entry.m_State == ETransmissionState.DONE) {
                    outlineCmd.m_iColor = ARGB(255,0,255,0); // green
                } else if (entry.m_State == ETransmissionState.DISABLED) {
                    outlineCmd.m_iColor = ARGB(255,255,0,0); // red for destroyed/disabled antennas
                } else {
                    outlineCmd.m_iColor = ARGB(255,0,100,255); // blue for OFF state
                }
                outlineCmd.m_fOutlineWidth = 3.0;
                m_MarkerDrawCommands.Insert(outlineCmd);
                if (logCounter % 50 == 0) {
                    PrintFormat("GRAD_MapMarkerManager: Added outline for state %1 at %2,%3 with color %4", entry.m_State, screenX, screenY, outlineCmd.m_iColor);
                }
            }
        }

        if (logCounter % 50 == 0) {
            PrintFormat("GRAD_MapMarkerManager: Setting %1 draw commands to canvas", m_MarkerDrawCommands.Count());
        }
        m_Canvas.SetDrawCommands(m_MarkerDrawCommands);
    }

    // --- Smarter marker population ---
    // NOTE: The current retry loop is a workaround for late-initialized transmission points.
    // If you can trigger marker population from an event/callback when a transmission point is registered,
    // you can remove the retry loop and call m_AttemptPopulateMarkers() only when needed.
    // For now, the retry loop is a practical solution if no such event is available.

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

        // Remove text widgets
        foreach (Widget w : m_ProgressTextWidgets) {
            if (w) w.RemoveFromHierarchy();
        }
        m_ProgressTextWidgets.Clear();

        m_UnregisterPostFrame();
        // Unsubscribe from event
        GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
        if (bcm && m_TransmissionPointInvoker)
            bcm.RemoveTransmissionPointListener(m_TransmissionPointInvoker);
    }

    // Remove retry loop, use only event/callback
    void PopulateMarkers()
    {
        m_AllMarkers.Clear();
        GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
        if (!bcm)
        {
            Print("GRAD_MapMarkerManager: BreakingContactManager not found!", LogLevel.ERROR);
            return;
        }
        array<GRAD_BC_TransmissionComponent> tpcs = bcm.GetTransmissionPoints();
        if (!tpcs)
        {
            Print("GRAD_MapMarkerManager: No TransmissionPoints returned!", LogLevel.WARNING);
            return;
        }
        int markerCount = 0;
        foreach (GRAD_BC_TransmissionComponent tpc : tpcs)
        {
            if (!tpc) {
                Print("GRAD_MapMarkerManager: Skipping null TPC!", LogLevel.WARNING);
                continue;
            }
            TransmissionEntry entry = new TransmissionEntry();
            entry.m_Position = tpc.GetPosition();
            entry.m_State    = tpc.GetTransmissionState();
            entry.m_Radius   = 1000;
            m_AllMarkers.Insert(entry);
            markerCount++;
        }
        PrintFormat("GRAD_MapMarkerManager: Total markers after PopulateMarkers: %1", markerCount);
    }

    // Add member for invoker
    protected ref ScriptInvoker m_TransmissionPointInvoker;

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
