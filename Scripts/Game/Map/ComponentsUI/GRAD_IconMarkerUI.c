//------------------------------------------------------------------------------------------------
//! Map line
class MapIcon
{
	float m_fStartPointX, m_fStartPointY;
	float m_fEndPointX, m_fEndPointY;
	int m_iType;
	string m_sType;
	string m_textureCache;
	Widget m_wRootW;
	Widget m_wicon;
	ImageWidget m_wiconImage;
	SCR_MapEntity m_MapEntity;
	GRAD_IconMarkerUI m_OwnerComponent;
	RplId rplId;
	bool m_bIsVehicle;
	
	IEntity m_eEntity;
	
	//------------------------------------------------------------------------------------------------
	void CreateIcon(notnull Widget rootW)
	{
		m_wRootW = rootW;
		
		// Use the provided root widget (DrawingContainer) which is the correct layer for map markers
		m_wicon = GetGame().GetWorkspace().CreateWidgets("{546311C6714BB3BA}UI/Layouts/Map/MapDrawIcon.layout", m_wRootW);
		
		if (!m_wicon)
		{
			Print("[ICON DEBUG] CreateIcon: Failed to create widget from layout!", LogLevel.ERROR);
			return;
		}
		
		// Log initial widget size
		float widgetW, widgetH;
		m_wicon.GetScreenSize(widgetW, widgetH);
		Print(string.Format("[ICON DEBUG] CreateIcon: Initial widget size: %.1fx%.1f", widgetW, widgetH), LogLevel.NORMAL);
		
		// Fix for black shapes/shadows: Apply BLEND to all ImageWidgets in the layout
		// This handles shadows or background images that might be part of the layout
		ApplyBlendFlagRecursively(m_wicon);
		
		m_wiconImage = ImageWidget.Cast(m_wicon.FindAnyWidget("DrawIconImage"));
		
		if (m_wiconImage) {
			// Log image widget properties
			float imgW, imgH;
			m_wiconImage.GetScreenSize(imgW, imgH);
			Print(string.Format("[ICON DEBUG] CreateIcon: Image widget size: %.1fx%.1f", imgW, imgH), LogLevel.NORMAL);
			
			if (m_sType != "") {
				// m_wiconImage.LoadImageFromSet(m_iType, "{9C5B2BA4695A421C}UI/Textures/Icons/GRAD_BC_mapIcons.imageset.edds", m_sType);
				Print(string.Format("GRAD IconmarkerUI: LoadImageTexture success 1"), LogLevel.NORMAL);
				m_wiconImage.LoadImageTexture (0, m_sType, false, false);
				// Fix for black icons: Ensure BLEND is set and Color is White
				m_wiconImage.SetFlags(m_wiconImage.GetFlags() | WidgetFlags.BLEND | WidgetFlags.VISIBLE);
				m_wiconImage.SetColor(Color.White);
			} else {
				Print(string.Format("PANIC - m_wiconImage is empty string"), LogLevel.NORMAL);
			};
		} else {
			Print("[ICON DEBUG] CreateIcon: DrawIconImage widget not found in layout!", LogLevel.ERROR);
		};
	}
	
	//------------------------------------------------------------------------------------------------
	void ApplyBlendFlagRecursively(Widget w)
	{
		if (!w) return;
		
		ImageWidget img = ImageWidget.Cast(w);
		if (img)
		{
			img.SetFlags(img.GetFlags() | WidgetFlags.BLEND);
		}
		
		Widget child = w.GetChildren();
		while (child)
		{
			ApplyBlendFlagRecursively(child);
			child = child.GetSibling();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void SetType(string type)
	{
		m_sType = type;
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdateIcon()
	{	
		if (!m_wicon)	// can happen due to callater used for update
			return;
		
		if (!m_wiconImage) {
			Print(string.Format("PANIC - No m_wiconImage found"), LogLevel.NORMAL);
			return;
		};
				
		int screenX, screenY, endX, endY;
		
		if (m_sType != "" && m_textureCache != m_sType) {
			Print(string.Format("GRAD IconmarkerUI: m_textureCache is %1", m_textureCache), LogLevel.NORMAL);
			bool textureLoaded = m_wiconImage.LoadImageTexture (0, m_sType, false, false);
			
			if (textureLoaded) {
				// Fix for black icons: Ensure BLEND is set and Color is White
				m_wiconImage.SetFlags(m_wiconImage.GetFlags() | WidgetFlags.BLEND | WidgetFlags.VISIBLE);
				m_wiconImage.SetColor(Color.White);
				// might be false if map is not open (?)
				m_textureCache = m_sType; // as we have no getter for existing texture WHYEVER :[[
				
				Print(string.Format("GRAD IconmarkerUI: LoadImageTexture success update"), LogLevel.NORMAL);
			}
		};

		m_MapEntity.WorldToScreen(m_fStartPointX, m_fStartPointY, screenX, screenY, true);
		m_MapEntity.WorldToScreen(m_fEndPointX, m_fEndPointY, endX, endY, true);
		
		vector iconVector = vector.Zero;
		iconVector[0] = m_fStartPointX - m_fEndPointX;
		iconVector[1] = m_fStartPointY - m_fEndPointY;

		vector angles = iconVector.VectorToAngles();
		if (angles[0] == 90)
			angles[1] =  180 - angles[1]; 	// reverse angles when passing vertical axis
		
		m_wiconImage.SetRotation(angles[1]);
		
		// Use 48x48 for infantry icons, 70x70 for vehicle icons
		float iconSize;
		if (m_bIsVehicle)
			iconSize = 70.0;
		else
			iconSize = 48.0;
		
		// DEBUG: Log all sizes before setting
		int imgW, imgH;
		float rootW, rootH;
		m_wiconImage.GetImageSize(0, imgW, imgH);
		m_wiconImage.GetScreenSize(rootW, rootH);
		Print(string.Format("[ICON DEBUG] BEFORE - Image texture size: %1x%2, Screen size: %.1fx%.1f", imgW, imgH, rootW, rootH), LogLevel.NORMAL);
		
		// Get parent frame size
		Widget mapFrame = m_wicon.GetParent();
		if (mapFrame)
		{
			float parentW, parentH;
			mapFrame.GetScreenSize(parentW, parentH);
			Print(string.Format("[ICON DEBUG] Parent frame size: %.1fx%.1f", parentW, parentH), LogLevel.NORMAL);
		}
		
		// Set the image size
		m_wiconImage.SetSize(iconSize, iconSize);
		// Set the FrameSlot size to match the icon size
		FrameSlot.SetSize(m_wicon, iconSize, iconSize);
		
		// DEBUG: Log sizes after setting
		m_wiconImage.GetScreenSize(rootW, rootH);
		Print(string.Format("[ICON DEBUG] AFTER - Target: %.1f, Image screen: %.1fx%.1f", iconSize, rootW, rootH), LogLevel.NORMAL);
		
		// Get actual widget size
		float actualW, actualH;
		m_wicon.GetScreenSize(actualW, actualH);
		Print(string.Format("[ICON DEBUG] Root widget actual screen size: %.1fx%.1f", actualW, actualH), LogLevel.NORMAL);
		
		// Simply position at screen coordinates - let layout anchoring handle the rest
		float posX = GetGame().GetWorkspace().DPIUnscale(screenX);
		float posY = GetGame().GetWorkspace().DPIUnscale(screenY);
		FrameSlot.SetAlignment(m_wicon, 0.5, 0.5);
		FrameSlot.SetPos(m_wicon, posX, posY);
		
		m_wicon.SetVisible(true);
		
		Print(string.Format("[ICON DEBUG] Position set to: %.1f, %.1f (screen: %1, %2)", posX, posY, screenX, screenY), LogLevel.NORMAL);
		Print(string.Format("[ICON DEBUG] IsVehicle: %1, Rotation: %.1f degrees", m_bIsVehicle, angles[1]), LogLevel.NORMAL);
		Print("[ICON DEBUG] =====================================", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void MapIcon(SCR_MapEntity mapEnt, GRAD_IconMarkerUI ownerComp)
	{
		m_MapEntity = mapEnt;
		m_OwnerComponent = ownerComp;
	}
};

//------------------------------------------------------------------------------------------------
class GRAD_IconMarkerUI
{
	protected Widget m_wDrawingContainer;
	
	protected ref array<ref MapIcon> m_aicons = new array <ref MapIcon>();
	protected int m_iType;
	
	protected SCR_MapEntity m_MapEntity;
	
	//------------------------------------------------------------------------------------------------
	//! SCR_MapEntity event
	protected void OnMapPan(float x, float y, bool adjustedPan)
	{
		foreach (MapIcon icon: m_aicons)
		{
			icon.UpdateIcon();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! SCR_MapEntity event
	protected void OnMapPanEnd(float x, float y)
	{
		foreach (MapIcon icon: m_aicons)
		{
			GetGame().GetCallqueue().CallLater(icon.UpdateIcon, 0, false); // needs to be delayed by a frame as it cant always update the size after zoom correctly within the same frame
		}
	}
	
	protected void OnMapZoom(float x, float y)
	{
		foreach (MapIcon icon: m_aicons)
		{
			icon.UpdateIcon();
		}
	}
	
	protected void OnMapZoomEnd(float x, float y)
	{
		foreach (MapIcon icon: m_aicons)
		{
			icon.UpdateIcon();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnMapOpen(MapConfiguration config)
	{
		m_wDrawingContainer = FrameWidget.Cast(config.RootWidgetRef.FindAnyWidget(SCR_MapConstants.DRAWING_CONTAINER_WIDGET_NAME));
		
		// fix null pointer
		if (!m_wDrawingContainer) {
			Print(string.Format("GRAD_IconMarkerUI OnMapOpen: Cant find m_wDrawingContainer"), LogLevel.ERROR);
			return;
		}
				
		foreach (MapIcon icon: m_aicons)
		{
			int type = icon.m_iType;
			icon.CreateIcon(m_wDrawingContainer);
			GetGame().GetCallqueue().CallLater(icon.UpdateIcon, 0, false);
		}
						
		m_MapEntity.GetOnMapPan().Insert(OnMapPan);		// pan for scaling
		m_MapEntity.GetOnMapPanEnd().Insert(OnMapPanEnd);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnMapClose(MapConfiguration config)
	{
		m_MapEntity.GetOnMapPan().Remove(OnMapPan);
		m_MapEntity.GetOnMapPanEnd().Remove(OnMapPanEnd);
		m_wDrawingContainer = null;
	}
	
	//------------------------------------------------------------------------------------------------
	void Init()
	{
		SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
		
		// Start periodic vehicle state checking (every 2 seconds)
		GetGame().GetCallqueue().CallLater(CheckVehicleStates, 2000, true);
	}
	
	//------------------------------------------------------------------------------------------------
	// Periodically check if players have entered/exited vehicles and update icons accordingly
	protected void CheckVehicleStates()
	{
		foreach (MapIcon icon: m_aicons)
		{
			if (!icon.rplId.IsValid())
				continue;
			
			// Get the entity from RplId
			Managed obj = Replication.FindItem(icon.rplId);
			if (!obj)
				continue;
			
			RplComponent rplComp = RplComponent.Cast(obj);
			if (!rplComp)
				continue;
			
			IEntity entity = rplComp.GetEntity();
			if (!entity)
				continue;
			
			// Check if it's a character
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
			if (!character)
				continue;
			
			// Check if in vehicle using CompartmentAccessComponent
			CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
			if (!compartmentAccess)
				continue;
			
			bool isInVehicle = compartmentAccess.IsInCompartment();
			
			// Update icon if vehicle state changed
			if (isInVehicle != icon.m_bIsVehicle)
			{
				icon.m_bIsVehicle = isInVehicle;
				icon.UpdateIcon();
				Print(string.Format("GRAD IconMarkerUI: Vehicle state changed for RplId %1, isVehicle=%2", icon.rplId, isInVehicle), LogLevel.NORMAL);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Clear all icons (used when starting replay to remove live markers)
	void ClearAllIcons()
	{
		foreach (MapIcon icon: m_aicons)
		{
			if (icon.m_wicon)
				icon.m_wicon.RemoveFromHierarchy();
		}
		m_aicons.Clear();
		Print("GRAD IconMarkerUI: All icons cleared", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	void AddIcon(float startPointX, float startPointY, float endPointX, float endPointY, string sType, RplId rplId, bool isVehicle = false)
	{
		MapIcon icon = new MapIcon(m_MapEntity, this);
		
		icon.m_fStartPointX = startPointX;
		icon.m_fStartPointY = startPointY;
		icon.m_fEndPointX = endPointX;
		icon.m_fEndPointY = endPointY;
		icon.m_sType = sType;
		icon.rplId = rplId;
		icon.m_bIsVehicle = isVehicle;
		
		Print(string.Format("GRAD IconMarkerUI: SetIcon icon.rplId to %1 , should equal %2", rplId, icon.rplId), LogLevel.WARNING);
		
		m_aicons.Insert(icon);
		
		if (m_wDrawingContainer)
		{
			icon.CreateIcon(m_wDrawingContainer);
			GetGame().GetCallqueue().CallLater(icon.UpdateIcon, 0, false);
		}
	}
	
	
	//------------------------------------------------------------------------------------------------
	void SetIcon(string type, RplId rplId, bool isVehicle = false) 
	{
		Print(string.Format("GRAD IconMarkerUI: SetIcon rplId %1", rplId), LogLevel.WARNING);
		
		foreach (MapIcon icon: m_aicons)
		{
			Print(string.Format("GRAD IconMarkerUI: SetIcon own rplId %1", icon.rplId), LogLevel.WARNING);
			
			// only set icon for the right entity
			if (icon.rplId == rplId) {
				icon.SetType(type);
				icon.m_bIsVehicle = isVehicle;
				Print(string.Format("GRAD IconMarkerUI SetIcon: m_sType is set to %1, isVehicle=%2", icon.m_sType, isVehicle), LogLevel.NORMAL);
				icon.UpdateIcon();
			};
		}	
	}
	
	//------------------------------------------------------------------------------------------------
	void GRAD_IconMarkerUI()
	{
		m_MapEntity = SCR_MapEntity.GetMapInstance();
	}
};