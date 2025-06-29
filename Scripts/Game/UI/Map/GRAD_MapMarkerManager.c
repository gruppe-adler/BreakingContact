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

        // Pulse timer update
        float dt = GetGame().GetWorld().GetWorldTime() / 1000.0;
        m_fPulseTime = Math.Mod(dt, m_fPulseDuration) / m_fPulseDuration;

        PrintFormat("GRAD_MapMarkerManager: Draw() called, m_AllMarkers.Count() = %1", m_AllMarkers.Count());

        // Iterate every transmission entry, draw a circle or icon based on its state
        foreach (TransmissionEntry entry : m_AllMarkers)
        {
            PrintFormat("GRAD_MapMarkerManager: Drawing marker at %1 with state %2", entry.m_Position, entry.m_State);
            switch (entry.m_State)
            {
                case ETransmissionState.TRANSMITTING:
                    DrawPulsingCircle(entry.m_Position, 1.0, entry.m_Radius, ARGB(255,255,50,50));
                    break;
                case ETransmissionState.INTERRUPTED:
                    DrawStaticCircle(entry.m_Position, entry.m_Radius, ARGB(128, 50, 50, 50));
                    break;
                case ETransmissionState.DONE:
                    DrawStaticCircle(entry.m_Position, entry.m_Radius, ARGB(128, 50, 255, 50));
                    break;
                case ETransmissionState.DISABLED:
                    DrawImage(entry.m_Position, 25, 25, m_IconDestroyed);
                    break;
                default:
                    DrawStaticCircle(entry.m_Position, entry.m_Radius, ARGB(128, 200, 200, 200));
                    break;
            }
        }

        // Test: draw a fixed circle at (500, 500)
        ref array<float> testVertices = new array<float>();
        float testRadius = 10000;
        float twoPi = 6.28318530718;
        for (int i = 0; i < 32; i++) {
            float angle = twoPi * i / 32;
            float x = 500 + Math.Cos(angle) * testRadius;
            float y = 500 + Math.Sin(angle) * testRadius;
            testVertices.Insert(x);
            testVertices.Insert(y);
        }
        ref PolygonDrawCommand testCmd = new PolygonDrawCommand();
        testCmd.m_iColor = ARGB(255,0,0,255); // blue
        testCmd.m_Vertices = testVertices;
        m_Commands.Insert(testCmd);

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

    // Helper: Draw a pulsing polygon circle
    void DrawPulsingCircle(vector worldPos, float minRadius, float maxRadius, int color, int segments = 32)
    {
        Print("DrawPulsingCircle: called", LogLevel.ERROR);
        float radius = Math.Lerp(minRadius, maxRadius, m_fPulseTime);
        float alpha = 1.0 - m_fPulseTime;
        int a = Math.Clamp(Math.Floor(255 * alpha), 0, 255);
        int fadedColor = ARGB(255,255,0,0); // fully opaque red for debug

        vector screenPos = m_Widget.GetWorkspace().ProjWorldToScreen(worldPos, GetGame().GetWorld());
        PrintFormat("DrawPulsingCircle: worldPos=%1, screenPos=%2, radius=%3", worldPos, screenPos, radius);

        ref array<float> vertices = new array<float>();
        float twoPi = 6.28318530718;
        for (int i = 0; i < segments; i++)
        {
            float angle = twoPi * i / segments;
            float x = screenPos[0] + Math.Cos(angle) * radius;
            float y = screenPos[1] + Math.Sin(angle) * radius;
            vertices.Insert(x);
            vertices.Insert(y);
        }

        ref PolygonDrawCommand cmd = new PolygonDrawCommand();
        cmd.m_iColor = fadedColor;
        cmd.m_Vertices = vertices;
        m_Commands.Insert(cmd);
    }

    // Helper: Draw a static polygon circle
    void DrawStaticCircle(vector worldPos, float radius, int color, int segments = 32)
    {
        vector screenPos = m_Widget.GetWorkspace().ProjWorldToScreen(worldPos, GetGame().GetWorld());
        PrintFormat("DrawStaticCircle: worldPos=%1, screenPos=%2, radius=%3", worldPos, screenPos, radius);
        ref array<float> vertices = new array<float>();
        float twoPi = 6.28318530718;
        for (int i = 0; i < segments; i++)
        {
            float angle = twoPi * i / segments;
            float x = screenPos[0] + Math.Cos(angle) * radius;
            float y = screenPos[1] + Math.Sin(angle) * radius;
            vertices.Insert(x);
            vertices.Insert(y);
        }
        ref PolygonDrawCommand cmd = new PolygonDrawCommand();
        cmd.m_iColor = ARGB(255,0,255,0); // fully opaque green for debug
        cmd.m_Vertices = vertices;
        m_Commands.Insert(cmd);
    }
}
