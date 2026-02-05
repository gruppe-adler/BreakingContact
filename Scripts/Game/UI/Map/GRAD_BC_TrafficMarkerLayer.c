class TrafficMarkerEntry
{
    vector m_Position;
    string m_Label;
    string m_Timestamp;
    float m_CreationTime;
}

[BaseContainerProps()]
class GRAD_BC_TrafficMarkerLayer : GRAD_MapMarkerLayer
{
    protected ref array<ref TrafficMarkerEntry> m_TrafficMarkers;
    protected bool m_IsInitialized = false;
    protected ref array<Widget> m_MarkerTextWidgets;
    
    static const int MARKER_RADIUS = 10;
    static const int MARKER_COLOR = 0xFFFFFF00; // Yellow
    static const int TEXT_COLOR = 0xFFFFFFFF; // White
    static const float MARKER_LIFETIME = 300.0; // 5 minutes
    
    void GRAD_BC_TrafficMarkerLayer()
    {
        Print("GRAD_BC_TrafficMarkerLayer: Constructor called", LogLevel.NORMAL);
    }
    
    override void OnMapOpen(MapConfiguration config)
    {
        super.OnMapOpen(config);
        
        if (!m_IsInitialized)
        {
            m_TrafficMarkers = new array<ref TrafficMarkerEntry>();
            m_MarkerTextWidgets = new array<Widget>();
            
            m_IsInitialized = true;
            Print("GRAD_BC_TrafficMarkerLayer: Initialized", LogLevel.NORMAL);
        }
    }
    
    override void OnMapClose(MapConfiguration config)
    {
        // Clean up text widgets
        foreach (Widget w : m_MarkerTextWidgets)
        {
            if (w)
                w.RemoveFromHierarchy();
        }
        m_MarkerTextWidgets.Clear();
        
        super.OnMapClose(config);
    }
    
    void AddTrafficMarker(vector position, string label)
    {
        if (!m_TrafficMarkers)
            return;
            
        TrafficMarkerEntry entry = new TrafficMarkerEntry();
        entry.m_Position = position;
        entry.m_Label = label;
        entry.m_CreationTime = System.GetTickCount() / 1000.0;
        
        // Format timestamp
        int year, month, day, hour, minute, second;
        System.GetYearMonthDayUTC(year, month, day);
        System.GetHourMinuteSecondUTC(hour, minute, second);
        entry.m_Timestamp = string.Format("%1:%2:%3", hour.ToString(2), minute.ToString(2), second.ToString(2));
        
        m_TrafficMarkers.Insert(entry);
        
        Print(string.Format("GRAD_BC_TrafficMarkerLayer: Added traffic marker at %1 with label '%2'", position, label), LogLevel.NORMAL);
    }
    
    void ClearAllMarkers()
    {
        if (m_TrafficMarkers)
            m_TrafficMarkers.Clear();
            
        foreach (Widget w : m_MarkerTextWidgets)
        {
            if (w)
                w.RemoveFromHierarchy();
        }
        m_MarkerTextWidgets.Clear();
    }
    
    override void Draw()
    {
        if (!m_TrafficMarkers || !m_Canvas)
            return;
        
        // CRITICAL: Clear commands array before drawing (this was missing!)
        if (m_Commands)
            m_Commands.Clear();
            
        // Clean up old markers
        float currentTime = System.GetTickCount() / 1000.0;
        for (int i = m_TrafficMarkers.Count() - 1; i >= 0; i--)
        {
            TrafficMarkerEntry entry = m_TrafficMarkers[i];
            if (currentTime - entry.m_CreationTime > MARKER_LIFETIME)
            {
                m_TrafficMarkers.Remove(i);
            }
        }
        
        // Draw markers
        foreach (TrafficMarkerEntry entry : m_TrafficMarkers)
        {
            // Draw yellow circle
            DrawCircle(entry.m_Position, MARKER_RADIUS, MARKER_COLOR, 24);
            
            // Draw text label and timestamp
            DrawMarkerText(entry.m_Position, entry.m_Label, entry.m_Timestamp);
        }
    }
    
    protected void DrawMarkerText(vector worldPos, string label, string timestamp)
    {
        if (!m_MapEntity)
            return;
            
        float screenX, screenY;
        m_MapEntity.WorldToScreen(worldPos[0], worldPos[2], screenX, screenY, true);
        
        // Find map frame widget
        Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget("MapFrame");
        if (!mapFrame)
            return;
            
        // Create text widget for the question mark
        Widget questionWidget = GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(TEXT_COLOR), 1, mapFrame);
        if (questionWidget)
        {
            TextWidget questionTxt = TextWidget.Cast(questionWidget);
            if (questionTxt)
            {
                questionTxt.SetText("?");
                questionTxt.SetExactFontSize(20);
                questionTxt.SetFont("RobotoCondensedBold");
                
                float qWidth, qHeight;
                questionTxt.GetTextSize(qWidth, qHeight);
                FrameSlot.SetPos(questionWidget, screenX - qWidth / 2, screenY - qHeight / 2);
                FrameSlot.SetSizeToContent(questionWidget, true);
                
                m_MarkerTextWidgets.Insert(questionWidget);
            }
            else
            {
                questionWidget.RemoveFromHierarchy();
            }
        }
        
        // Create text widget for label and timestamp
        Widget labelWidget = GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(MARKER_COLOR), 1, mapFrame);
        if (labelWidget)
        {
            TextWidget labelTxt = TextWidget.Cast(labelWidget);
            if (labelTxt)
            {
                string displayText = string.Format("%1\n%2", label, timestamp);
                labelTxt.SetText(displayText);
                labelTxt.SetExactFontSize(11);
                labelTxt.SetFont("RobotoCondensed");
                
                FrameSlot.SetPos(labelWidget, screenX + MARKER_RADIUS + 5, screenY - 10);
                FrameSlot.SetSizeToContent(labelWidget, true);
                
                m_MarkerTextWidgets.Insert(labelWidget);
            }
            else
            {
                labelWidget.RemoveFromHierarchy();
            }
        }
    }
    
    override void Update(float timeSlice)
    {
        // Clear old text widgets before redraw
        foreach (Widget w : m_MarkerTextWidgets)
        {
            if (w)
                w.RemoveFromHierarchy();
        }
        m_MarkerTextWidgets.Clear();
        
        // Draw (which clears and fills m_Commands)
        Draw();
        
        // Apply draw commands to canvas
        if (m_Commands && m_Commands.Count() > 0)
        {
            m_Canvas.SetDrawCommands(m_Commands);
        }
    }
}
