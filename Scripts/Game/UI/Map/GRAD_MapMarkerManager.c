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
    
    // Add state change tracking for instant updates
    protected ref array<ETransmissionState> m_LastStates;
    protected ref ScriptInvoker m_TransmissionPointInvoker;
    
    // Separate rendering timer from state checking
    protected bool m_NeedsRedraw = false;
    
    // Debouncing for event spam prevention
    protected float m_LastStateChangeTime = 0.0;

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
            m_LastStates = new array<ETransmissionState>();
            m_IsInitialized = true;
        }

        // Always start by emptying out any old entries (so we repopulate fresh)
        m_AllMarkers.Clear();

        // Subscribe to transmission point events for instant state updates
        GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
        if (bcm)
        {
            if (!m_TransmissionPointInvoker) m_TransmissionPointInvoker = new ScriptInvoker();
            m_TransmissionPointInvoker.Insert(this.OnTransmissionStateChanged);
            bcm.AddTransmissionPointListener(m_TransmissionPointInvoker);
        }
        
        // Initial population
        PopulateMarkers();
        m_NeedsRedraw = true;

        // Register for frame updates (for animation only)
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
    
    // Event-driven state change handler for instant updates with debouncing
    void OnTransmissionStateChanged()
    {
        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
        
        // Debounce rapid events (only process if 100ms has passed since last change)
        if (currentTime - m_LastStateChangeTime < 0.1) {
            return;
        }
        m_LastStateChangeTime = currentTime;
        
        PrintFormat("GRAD_MapMarkerManager: Transmission state changed - triggering instant update");
        PopulateMarkers();
        CheckForStateChanges();
        m_NeedsRedraw = true;
        DrawMarkers(); // Instant redraw on state change
    }
    
    // Separate method to check for state changes and show hints
    void CheckForStateChanges()
    {
        if (!m_AllMarkers) return;
        
        if (!m_LastStates) {
            m_LastStates = new array<ETransmissionState>();
        }
        
        int markerCount = m_AllMarkers.Count();
        bool stateChanged = false;
        
        // Resize tracking array if needed
        if (m_LastStates.Count() != markerCount) {
            stateChanged = true;
            PrintFormat("GRAD_MapMarkerManager: Marker count changed from %1 to %2", m_LastStates.Count(), markerCount);
            m_LastStates.Clear();
            m_LastStates.Resize(markerCount);
            // Initialize new states
            for (int i = 0; i < markerCount; i++) {
                m_LastStates[i] = m_AllMarkers[i].m_State;
            }
        } else {
            // Check for individual state changes
            for (int i = 0; i < markerCount; i++) {
                if (m_AllMarkers[i].m_State != m_LastStates[i]) {
                    stateChanged = true;
                    ETransmissionState oldState = m_LastStates[i];
                    ETransmissionState newState = m_AllMarkers[i].m_State;
                    PrintFormat("GRAD_MapMarkerManager: State changed for marker %1: %2 -> %3", i, oldState, newState);
                    
                    // Show instant hint for state change
                    ShowTransmissionHint(newState);
                    
                    m_LastStates[i] = newState;
                }
            }
        }
    }
    
    // Method to show transmission hints
    void ShowTransmissionHint(ETransmissionState state)
    {
        string message = "";
        string title = "Transmission Update";
        
        switch (state) {
            case ETransmissionState.TRANSMITTING:
                message = "Transmission started";
                break;
            case ETransmissionState.INTERRUPTED:
                message = "Transmission interrupted!";
                break;
            case ETransmissionState.DONE:
                message = "Transmission completed";
                break;
            case ETransmissionState.DISABLED:
                message = "Transmission disabled";
                break;
            case ETransmissionState.OFF:
                message = "Transmission stopped";
                break;
        }
        
        if (message != "") {
            GRAD_PlayerComponent playerComp = GRAD_PlayerComponent.GetInstance();
            if (playerComp) {
                playerComp.ShowHint(message, title, 5, false);
                PrintFormat("GRAD_MapMarkerManager: Showing hint: %1", message);
            }
        }
    }

    // Lightweight frame update - only for animation and rendering when needed
    void EOnPostFrame(IEntity owner, float timeSlice)
    {
        if (!m_Canvas) {
            return; // Canvas not ready
        }

        // Only redraw if state changed or we need animation
        bool hasTransmitting = false;
        if (m_AllMarkers) {
            for (int i = 0; i < m_AllMarkers.Count(); i++) {
                if (m_AllMarkers[i].m_State == ETransmissionState.TRANSMITTING) {
                    hasTransmitting = true;
                    break;
                }
            }
        }
        
        // Always redraw if we need it, or if there's a transmitting animation
        if (m_NeedsRedraw || hasTransmitting) {
            DrawMarkers();
            m_NeedsRedraw = false; // Reset the flag after drawing
        }
    }
    
    // Separate drawing method
    void DrawMarkers()
    {
        if (!m_AllMarkers || m_AllMarkers.Count() == 0) {
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

            float screenX, screenY;
            m_MapEntity.WorldToScreen(entry.m_Position[0], entry.m_Position[2], screenX, screenY, true);
            
            // Calculate transmission range for pulsing (so it represents real 1000m range)
            float radiusWorldX = entry.m_Position[0] + entry.m_Radius;
            float radiusWorldY = entry.m_Position[2];
            float radiusScreenX, radiusScreenY;
            m_MapEntity.WorldToScreen(radiusWorldX, radiusWorldY, radiusScreenX, radiusScreenY, true);
            float transmissionRadius = Math.AbsFloat(radiusScreenX - screenX);
            
            // Use smaller fixed size for outlines (for visibility)
            float outlineRadius = 15.0; // Fixed 15 pixel radius for outline visibility

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

                // Pulse from small size to full transmission range (1000m on map)
                float pulseRadius = transmissionRadius * Math.Lerp(0.1, 1.0, m_fPulseTime);
                for (int i = 0; i < 32; i++) {
                    float angle = twoPi * i / 32;
                    float x = screenX + Math.Cos(angle) * pulseRadius;
                    float y = screenY + Math.Sin(angle) * pulseRadius;
                    m_PulsingCircleCmd.m_Vertices.Insert(x);
                    m_PulsingCircleCmd.m_Vertices.Insert(y);
                }

                m_MarkerDrawCommands.Insert(m_PulsingCircleCmd);
            }

            // --- Static outline for INTERRUPTED (gray), DONE (green), OFF (blue), or DISABLED (red) ---
            if (entry.m_State == ETransmissionState.INTERRUPTED || entry.m_State == ETransmissionState.DONE || entry.m_State == ETransmissionState.OFF || entry.m_State == ETransmissionState.DISABLED) {
                ref LineDrawCommand outlineCmd = new LineDrawCommand();
                outlineCmd.m_Vertices = new array<float>();
                
                for (int i = 0; i <= 32; i++) { // Note: <= to close the circle
                    float angle = twoPi * i / 32;
                    float x = screenX + Math.Cos(angle) * outlineRadius;
                    float y = screenY + Math.Sin(angle) * outlineRadius;
                    outlineCmd.m_Vertices.Insert(x);
                    outlineCmd.m_Vertices.Insert(y);
                }
                
                if (entry.m_State == ETransmissionState.INTERRUPTED) {
                    outlineCmd.m_iColor = ARGB(255,128,128,128); // Make gray visible for interrupted
                    PrintFormat("GRAD_MapMarkerManager: Drawing INTERRUPTED marker at %1,%2 with gray color", screenX, screenY);
                } else if (entry.m_State == ETransmissionState.DONE) {
                    outlineCmd.m_iColor = ARGB(150,0,255,0); // transparent green for completed
                } else if (entry.m_State == ETransmissionState.DISABLED) {
                    outlineCmd.m_iColor = ARGB(150,255,0,0); // transparent red for disabled
                } else {
                    outlineCmd.m_iColor = ARGB(255,0,100,255); // blue for OFF state
                }
                outlineCmd.m_fOutlineWidth = 4.0; // Increase thickness for better visibility
                m_MarkerDrawCommands.Insert(outlineCmd);
            }
        }

        m_Canvas.SetDrawCommands(m_MarkerDrawCommands);
        static int drawCounter = 0;
        drawCounter++;
        if (drawCounter % 10 == 0) { // Only log every 10th draw to reduce spam
            PrintFormat("GRAD_MapMarkerManager: Drew %1 markers (draw #%2)", m_MarkerDrawCommands.Count(), drawCounter);
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
            
            // Debug state information
            PrintFormat("GRAD_MapMarkerManager: Added marker %1 at %2 with state %3", markerCount-1, entry.m_Position, entry.m_State);
        }
        PrintFormat("GRAD_MapMarkerManager: Total markers after PopulateMarkers: %1", markerCount);
    }

    // Use higher FPS timer for smooth animation, but only draw when needed
    void m_RegisterPostFrame()
    {
        GetGame().GetCallqueue().CallLater(this.EOnPostFrame, 16, true, null, 0.0); // ~60 FPS for smooth animation
    }
    void m_UnregisterPostFrame()
    {
        GetGame().GetCallqueue().Remove(this.EOnPostFrame);
    }
}
