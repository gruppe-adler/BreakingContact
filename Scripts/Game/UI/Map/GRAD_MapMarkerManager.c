
class TransmissionEntry
{
    vector              m_Position;
    ETransmissionState  m_State;
    int                 m_Radius;
    GRAD_BC_TransmissionComponent m_Component; // Store reference to get live progress data
    
    // Store map state when static marker was created to detect view changes
    float               m_StoredMapZoom = -1;
    vector              m_StoredMapCenter = vector.Zero;
    float               m_StoredScreenX = -1;
    float               m_StoredScreenY = -1;
    float               m_StoredScreenRadius = -1;
    bool                m_StaticCoordsValid = false;
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
    protected ref array<Widget> m_TransmissionTextWidgets; // Text markers for transmission points
    
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
            m_TransmissionTextWidgets = new array<Widget>();
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

        // Auto-dismiss mission description panel on map open
        DismissMissionDescriptionPanel();
        
        // Retry dismissal after a short delay in case panel loads later
        GetGame().GetCallqueue().CallLater(DismissMissionDescriptionPanel, 100, false);
        GetGame().GetCallqueue().CallLater(DismissMissionDescriptionPanel, 500, false);

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
                    
                    // Update state immediately
                    m_LastStates[i] = newState;
                    
                    // Force immediate redraw when state changes
                    GetGame().GetCallqueue().CallLater(DrawMarkers, 1, false);
                }
            }
        }
    }

    // Lightweight frame update - only for animation and rendering when needed
    void EOnPostFrame(IEntity owner, float timeSlice)
    {
        if (!m_Canvas) {
            return; // Canvas not ready
        }

        // Always redraw every frame to handle map movement and zoom for ALL marker types
        // Static markers need constant redrawing just like pulsing markers to respond to map changes
        bool hasAnyMarkers = false;
        if (m_AllMarkers) {
            hasAnyMarkers = m_AllMarkers.Count() > 0;
        }
        
        // Always redraw if we have any markers at all - this ensures static markers respond to map changes
        if (m_NeedsRedraw || hasAnyMarkers) {
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
            
            // Debug zoom-responsive radius calculation
            if (entry.m_State == ETransmissionState.DONE || entry.m_State == ETransmissionState.INTERRUPTED) {
                static int debugCounter = 0;
                debugCounter++;
                if (debugCounter % 60 == 0) { // Log every 60 frames (1 second)
                    /* PrintFormat("GRAD_MapMarkerManager: ZOOM DEBUG - State=%1, WorldPos=%2,%3, ScreenPos=%4,%5, WorldRadius=%6, ScreenRadius=%7", 
                        entry.m_State, entry.m_Position[0], entry.m_Position[2], screenX, screenY, entry.m_Radius, transmissionRadius);
					*/
                }
            }
            
            // Use full transmission radius for static markers to show 1000m range
            float staticRadius = transmissionRadius;

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
                // Use both PolygonDrawCommand for fill and LineDrawCommand for outline for maximum visibility
                ref PolygonDrawCommand fillCmd = new PolygonDrawCommand();
                fillCmd.m_Vertices = new array<float>();
                
                ref LineDrawCommand outlineCmd = new LineDrawCommand();
                outlineCmd.m_Vertices = new array<float>();
                
                // Use EXACTLY the same coordinates and radius as calculated above for pulsing markers
                // Don't recalculate - use the exact same screenX, screenY, and transmissionRadius
                float useScreenX = screenX;
                float useScreenY = screenY;
                float useRadius = transmissionRadius;
                
                // Safety bounds checking to prevent GPU crashes from invalid coordinates
                if (useScreenX < -10000 || useScreenX > 10000 || useScreenY < -10000 || useScreenY > 10000 || useRadius < 0 || useRadius > 5000) {
                    PrintFormat("GRAD_MapMarkerManager: Invalid coordinates detected, skipping marker - Screen: %1,%2, Radius: %3", useScreenX, useScreenY, useRadius);
                    continue; // Skip this marker to prevent GPU issues
                }
                
                for (int i = 0; i <= 32; i++) {
                    float angle = twoPi * i / 32;
                    float x = useScreenX + Math.Cos(angle) * useRadius;
                    float y = useScreenY + Math.Sin(angle) * useRadius;
                    fillCmd.m_Vertices.Insert(x);
                    fillCmd.m_Vertices.Insert(y);
                    outlineCmd.m_Vertices.Insert(x);
                    outlineCmd.m_Vertices.Insert(y);
                }
                
                if (entry.m_State == ETransmissionState.INTERRUPTED) {
                    fillCmd.m_iColor = ARGB(15,255,255,0); // Semi-transparent YELLOW for high visibility
                    outlineCmd.m_iColor = ARGB(150,255,128,0); // Bright orange outline
                    outlineCmd.m_fWidth = 6.0;
                    outlineCmd.m_bShouldEnclose = true;
                    /* PrintFormat("GRAD_MapMarkerManager: Drawing INTERRUPTED marker at %1,%2 with radius=%3 (world radius=%4)",  
                        useScreenX, useScreenY, useRadius, entry.m_Radius); */
                } else if (entry.m_State == ETransmissionState.DONE) {
                    fillCmd.m_iColor = ARGB(15,0,255,0); // Semi-transparent green
                    outlineCmd.m_iColor = ARGB(150,0,255,0); // Bright green outline
                    outlineCmd.m_fWidth = 4.0;
                    outlineCmd.m_bShouldEnclose = true;
                    /* PrintFormat("GRAD_MapMarkerManager: Drawing DONE marker at %1,%2 with radius=%3 (world radius=%4)", 
                        useScreenX, useScreenY, useRadius, entry.m_Radius); */
                } else if (entry.m_State == ETransmissionState.DISABLED) {
                    fillCmd.m_iColor = ARGB(15,255,0,0); // Semi-transparent red
                    outlineCmd.m_iColor = ARGB(150,255,0,0); // Bright red outline
                    outlineCmd.m_fWidth = 4.0;
                    outlineCmd.m_bShouldEnclose = true;
                } else {
                    fillCmd.m_iColor = ARGB(15,0,100,255); // Semi-transparent blue
                    outlineCmd.m_iColor = ARGB(150,0,150,255); // Bright blue outline
                    outlineCmd.m_fWidth = 4.0;
                    outlineCmd.m_bShouldEnclose = true;
                }
                
                m_MarkerDrawCommands.Insert(fillCmd);
                m_MarkerDrawCommands.Insert(outlineCmd);
            }
        }

        m_Canvas.SetDrawCommands(m_MarkerDrawCommands);
        
        // Update text markers every frame to keep them in sync with map movement
        UpdateTransmissionTextMarkers();
    }
    
    // Update transmission text markers that display percentage on hover
    void UpdateTransmissionTextMarkers()
    {
        if (!m_AllMarkers || !m_Canvas) {
            return;
        }
        
        // Only log occasionally to reduce spam
        static int debugCounter = 0;
        debugCounter++;
        bool shouldLog = (debugCounter % 60 == 0); // Log every 60 calls (~1 second)
        
        if (shouldLog) {
            PrintFormat("GRAD_MapMarkerManager: DEBUG V2 - UpdateTransmissionTextMarkers called with %1 markers", m_AllMarkers.Count());
        }
        
        // Remove old text widgets
        foreach (Widget w : m_TransmissionTextWidgets) {
            if (w) w.RemoveFromHierarchy();
        }
        m_TransmissionTextWidgets.Clear();
        
        // Get the map frame to attach text widgets to
        Widget mapRoot = m_MapEntity.GetMapMenuRoot();
        if (!mapRoot) return;
        
        // Find the actual MAP canvas using the EXACT same method as SCR_MapMarkerEntity
        // Use SCR_MapConstants.MAP_FRAME_NAME to get the correct parent widget
        Widget mapFrame = mapRoot.FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
        if (!mapFrame) {
            // Fallback to other names if MAP_FRAME_NAME doesn't exist
            mapFrame = mapRoot.FindAnyWidget("MapFrame");
            if (!mapFrame) {
                mapFrame = mapRoot.FindAnyWidget("Canvas");
            }
            if (!mapFrame) {
                mapFrame = mapRoot.FindAnyWidget("Frame");
            }
            if (!mapFrame) {
                mapFrame = mapRoot;
            }
        }
        
        if (shouldLog) {
            PrintFormat("GRAD_MapMarkerManager: Using official map frame widget '%1' (same as SCR_MapMarkerEntity)", mapFrame.GetName());
        }
        
        if (shouldLog) {
            PrintFormat("GRAD_MapMarkerManager: Using widget '%1' as parent for text widgets", mapFrame.GetName());
        }
        
        // Create text markers for each transmission point
        for (int i = 0; i < m_AllMarkers.Count(); i++)
        {
            TransmissionEntry entry = m_AllMarkers[i];
            if (!entry.m_Component) {
                Print(string.Format("GRAD_MapMarkerManager: Marker %1 has no component, skipping", i), LogLevel.NORMAL);
                continue;
            }
            
            // Calculate screen position
            float screenX, screenY;
            m_MapEntity.WorldToScreen(entry.m_Position[0], entry.m_Position[2], screenX, screenY, true);
            
            if (shouldLog) {
                Print(string.Format("GRAD_MapMarkerManager: Creating text widget for marker %1 at screen pos %2,%3", i, screenX, screenY), LogLevel.NORMAL);
            }
            
            // Create text widget from layout
            Widget textWidget = GetGame().GetWorkspace().CreateWidgets("{BF487CF20D30CF50}UI/Layouts/Map/MapDrawText.layout", mapFrame);
            if (!textWidget) {
                if (shouldLog) {
                    Print(string.Format("GRAD_MapMarkerManager: Failed to create text widget for marker %1", i), LogLevel.NORMAL);
                }
                continue;
            }
            
            if (shouldLog) {
                Print(string.Format("GRAD_MapMarkerManager: Text widget created successfully for marker %1", i), LogLevel.NORMAL);
                
                // Debug frame properties
                Print(string.Format("GRAD_MapMarkerManager: Frame widget type: %1, visible: %2", textWidget.Type().ToString(), textWidget.IsVisible()), LogLevel.NORMAL);
            }
            
            // Ensure frame widget is properly configured for background
            FrameWidget frameComp = FrameWidget.Cast(textWidget);
            if (frameComp) {
                frameComp.SetColor(Color.FromRGBA(255, 0, 0, 255)); // BRIGHT RED background for testing visibility
                frameComp.SetVisible(true);
                frameComp.SetOpacity(1.0);
                if (shouldLog) {
                    Print(string.Format("GRAD_MapMarkerManager: Frame background configured (RED) for marker %1", i), LogLevel.NORMAL);
                }
            } else {
                if (shouldLog) {
                    Print(string.Format("GRAD_MapMarkerManager: Warning - Root widget is not a FrameWidget for marker %1", i), LogLevel.NORMAL);
                }
            }
            
            // Find the actual TextWidget inside the layout (might be wrapped in a frame)
            TextWidget textComp = TextWidget.Cast(textWidget);
            if (!textComp) {
                // If root isn't a TextWidget, search for one inside (in case it's wrapped in a frame)
                textComp = TextWidget.Cast(textWidget.FindAnyWidget("DrawTextWidget"));
                if (!textComp) {
                    // Try other common text widget names as fallback
                    textComp = TextWidget.Cast(textWidget.FindAnyWidget("Text"));
                    if (!textComp) {
                        textComp = TextWidget.Cast(textWidget.FindAnyWidget("TextWidget"));
                        if (!textComp) {
                            textComp = TextWidget.Cast(textWidget.FindAnyWidget("Label"));
                            if (!textComp) {
                                PrintFormat("GRAD_MapMarkerManager: Could not find TextWidget in layout for marker %1", i);
                                textWidget.RemoveFromHierarchy();
                                continue;
                            }
                        }
                    }
                }
            }
            
            if (shouldLog) {
                Print(string.Format("GRAD_MapMarkerManager: Found TextWidget for marker %1", i), LogLevel.NORMAL);
                
                // Debug widget properties - only log occasionally
                Print(string.Format("GRAD_MapMarkerManager: TextWidget visible: %1, enabled: %2", textComp.IsVisible(), textComp.IsEnabled()), LogLevel.NORMAL);
            }
            
            // Get transmission progress percentage
            float progress = entry.m_Component.GetTransmissionDuration() * 100.0;
            string progressText = "";
			
            // Set base text styling - ensure all properties are applied
            textComp.SetColor(Color.FromRGBA(255, 255, 0, 255)); // BRIGHT YELLOW text for testing visibility
            textComp.SetVisible(true);
            textComp.SetOpacity(1.0);
            
            // Font properties should be applied from the layout file
            // No need to check GetFont() as it doesn't exist in the API
			
            // Format text based on state
            switch (entry.m_State) {
                case ETransmissionState.TRANSMITTING:
                    progressText = string.Format("%1%%", Math.Floor(progress));
                    // Keep default color for transmitting
                    break;
                case ETransmissionState.INTERRUPTED:
                    progressText = string.Format("INT %1%%", Math.Floor(progress));
                    break;
                case ETransmissionState.DONE:
                    progressText = "DONE";
                    break;
                case ETransmissionState.DISABLED:
                    progressText = string.Format("DIS %1%%", Math.Floor(progress));
                    break;
                case ETransmissionState.OFF:
                    progressText = "OFF";
                    break;
                default:
                    progressText = "UNKNOWN STATE";
                    break;
            }
            
            textComp.SetText(progressText);
            if (shouldLog) {
                PrintFormat("GRAD_MapMarkerManager: Set text '%1' for marker %2", progressText, i);
            }
            
            // Position text widget above the marker center using EXACT same method as SCR_MapMarkerEntity
            // Key insight: WorldToScreen() returns DPI-scaled coordinates, but widgets need unscaled coordinates!
            float textX = GetGame().GetWorkspace().DPIUnscale(screenX);
            float textY = GetGame().GetWorkspace().DPIUnscale(screenY - 40); // 40 pixels above marker
            
            if (shouldLog) {
                PrintFormat("GRAD_MapMarkerManager: DPI scaling - Screen: %1,%2 -> Unscaled: %3,%4", screenX, screenY, textX, textY);
            }
            
            // Set widget position using EXACT same method as SCR_MapMarkerEntity
            // Don't override layout anchoring - just set position
            FrameSlot.SetPos(textWidget, textX, textY);
            
            if (shouldLog) {
                PrintFormat("GRAD_MapMarkerManager: Positioned text widget at unscaled %1,%2 for marker %3", textX, textY, i);
            }
            
            // Add to our tracking array
            m_TransmissionTextWidgets.Insert(textWidget);
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
        
        // Remove transmission text widgets
        foreach (Widget w : m_TransmissionTextWidgets) {
            if (w) w.RemoveFromHierarchy();
        }
        m_TransmissionTextWidgets.Clear();

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
            entry.m_Component = tpc; // Store reference for live progress data
            m_AllMarkers.Insert(entry);
            markerCount++;
            
            // Debug state information
            PrintFormat("GRAD_MapMarkerManager: Added marker %1 at %2 with state %3", markerCount-1, entry.m_Position, entry.m_State);
        }
        PrintFormat("GRAD_MapMarkerManager: Total markers after PopulateMarkers: %1", markerCount);
    }
    
    // ───────────────────────────────────────────────────────────────────────────────
    // DismissMissionDescriptionPanel
    //
    // Automatically hide the mission description panel when map opens
    // ───────────────────────────────────────────────────────────────────────────────
    void DismissMissionDescriptionPanel()
    {
        if (!m_MapEntity) {
            Print("GRAD_MapMarkerManager: No map entity for mission description dismissal", LogLevel.WARNING);
            return;
        }
        
        Widget mapRoot = m_MapEntity.GetMapMenuRoot();
        if (!mapRoot) {
            Print("GRAD_MapMarkerManager: No map root for mission description dismissal", LogLevel.WARNING);
            return;
        }
        
        Print("GRAD_MapMarkerManager: Attempting to dismiss mission description panel", LogLevel.NORMAL);
        
        // Try to find the mission description panel widget
        array<string> possibleNames = {
            "MissionDescriptionListScroll"
        };
        
        bool panelFound = false;
        foreach (string widgetName : possibleNames) {
            Widget descriptionPanel = mapRoot.FindAnyWidget(widgetName);
            if (descriptionPanel) {
                // Check if it's actually visible before hiding
                if (descriptionPanel.IsVisible()) {
                    descriptionPanel.SetVisible(false);
                    PrintFormat("GRAD_MapMarkerManager: Mission description panel '%1' automatically dismissed", widgetName);
                    panelFound = true;
                } else {
                    PrintFormat("GRAD_MapMarkerManager: Mission description panel '%1' found but already hidden", widgetName);
                }
                break; // Only process the first one found
            }
        }
        
        if (!panelFound) {
            // Try a more aggressive search by looking for any widget containing relevant keywords
            Print("GRAD_MapMarkerManager: Standard mission description panel not found, trying recursive search", LogLevel.NORMAL);
            SearchAndHideMissionPanel(mapRoot);
        }
        
        // Also try to find and dismiss any modal dialogs or overlays that might be mission-related
        DismissModalPanels(mapRoot);
    }
    
    // Recursive method to search for mission description panels
    void SearchAndHideMissionPanel(Widget parent)
    {
        if (!parent) return;
        
        string widgetName = parent.GetName();
        if (widgetName.Length() == 0) {
            // Skip widgets without names
            Widget child = parent.GetChildren();
            while (child) {
                SearchAndHideMissionPanel(child);
                child = child.GetSibling();
            }
            return;
        }
        
        string lowerName = widgetName;
        lowerName.ToLower();
        
        // Check if this widget looks like a mission description panel
        bool isMissionPanel = (lowerName.Contains("mission") && (lowerName.Contains("description") || lowerName.Contains("brief"))) ||
                             (lowerName.Contains("task") && lowerName.Contains("list")) ||
                             (lowerName.Contains("objective") && lowerName.Contains("list")) ||
                             lowerName.Contains("briefing");
        
        if (isMissionPanel && parent.IsVisible()) {
            parent.SetVisible(false);
            PrintFormat("GRAD_MapMarkerManager: Found and hid mission panel: '%1'", widgetName);
            return; // Found and hidden, no need to search children
        }
        
        // Check children recursively
        Widget child = parent.GetChildren();
        while (child) {
            SearchAndHideMissionPanel(child);
            child = child.GetSibling();
        }
    }
    
    // Method to dismiss modal panels that might be blocking the map
    void DismissModalPanels(Widget mapRoot)
    {
        array<string> modalNames = {
            "ModalPanel",
            "Dialog",
            "OverlayPanel", 
            "InfoPanel",
            "NotificationPanel"
        };
        
        foreach (string modalName : modalNames) {
            Widget modal = mapRoot.FindAnyWidget(modalName);
            if (modal && modal.IsVisible()) {
                modal.SetVisible(false);
                PrintFormat("GRAD_MapMarkerManager: Dismissed modal panel: '%1'", modalName);
            }
        }
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
