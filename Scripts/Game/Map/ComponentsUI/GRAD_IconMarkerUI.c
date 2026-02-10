//------------------------------------------------------------------------------------------------
//! Map icon marker for displaying custom icons on the map
class MapIcon
{
	float m_fStartPointX, m_fStartPointY;
	float m_fEndPointX, m_fEndPointY;
	int m_iType;
	int m_iUniqueId;
	string m_sType;
	string m_sLabel;
	string m_textureCache;
	Widget m_wRootW;
	Widget m_wicon;
	ImageWidget m_wiconImage;
	TextWidget m_wLabelText;
	SCR_MapEntity m_MapEntity;
	GRAD_IconMarkerUI m_OwnerComponent;
	RplId rplId;
	bool m_bIsVehicle;

	IEntity m_eEntity;

	//------------------------------------------------------------------------------------------------
	void CreateIcon(notnull Widget rootW)
	{
		m_wRootW = rootW;

		if (!m_MapEntity)
			m_MapEntity = SCR_MapEntity.GetMapInstance();

		// Attach to map frame (like circle markers) so positioning uses map-internal coordinates
		Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			mapFrame = rootW;

		m_wicon = GetGame().GetWorkspace().CreateWidgets("{546311C6714BB3BA}UI/Layouts/Map/MapDrawIcon.layout", mapFrame);

		if (!m_wicon)
		{
			Print("GRAD IconMarkerUI: Failed to create widget from layout", LogLevel.ERROR);
			return;
		}

		// Apply BLEND to all ImageWidgets to prevent black shapes
		ApplyBlendFlagRecursively(m_wicon);

		m_wiconImage = ImageWidget.Cast(m_wicon.FindAnyWidget("DrawIconImage"));

		if (m_wiconImage)
		{
			if (m_sType != "")
			{
				m_wiconImage.LoadImageTexture(0, m_sType, false, false);
				m_wiconImage.SetFlags(m_wiconImage.GetFlags() | WidgetFlags.BLEND | WidgetFlags.VISIBLE);
				m_wiconImage.SetColor(Color.White);
			}
		}
		else
		{
			Print("GRAD IconMarkerUI: DrawIconImage widget not found in layout", LogLevel.ERROR);
		}

		// Set up text label if provided
		m_wLabelText = TextWidget.Cast(m_wicon.FindAnyWidget("DrawIconLabel"));
		if (m_wLabelText && m_sLabel != "")
		{
			m_wLabelText.SetText(m_sLabel);
			m_wLabelText.SetColor(Color.Black);
			m_wLabelText.SetVisible(true);
		}
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
		if (!m_wicon || !m_MapEntity)
			return;

		if (!m_wiconImage)
			return;

		// Update texture if changed
		if (m_sType != "" && m_textureCache != m_sType)
		{
			bool textureLoaded = m_wiconImage.LoadImageTexture(0, m_sType, false, false);
			if (textureLoaded)
			{
				m_wiconImage.SetFlags(m_wiconImage.GetFlags() | WidgetFlags.BLEND | WidgetFlags.VISIBLE);
				m_wiconImage.SetColor(Color.White);
				m_textureCache = m_sType;
			}
		}

		// Check if this is a static marker (start == end) or directional (entity tracking)
		bool isStatic = (m_fStartPointX == m_fEndPointX && m_fStartPointY == m_fEndPointY);

		int screenX, screenY;
		m_MapEntity.WorldToScreen(m_fStartPointX, m_fStartPointY, screenX, screenY, true);

		if (isStatic)
		{
			// Static marker (traffic events): fixed size, no rotation
			float iconSize;
			if (m_bIsVehicle)
				iconSize = 70.0;
			else
				iconSize = 48.0;

			m_wiconImage.SetSize(GetGame().GetWorkspace().DPIUnscale(iconSize), GetGame().GetWorkspace().DPIUnscale(iconSize));
			m_wiconImage.SetRotation(0);
		}
		else
		{
			// Directional marker (entity tracking): size based on world distance, rotated
			int endX, endY;
			m_MapEntity.WorldToScreen(m_fEndPointX, m_fEndPointY, endX, endY, true);

			vector iconVector = vector.Zero;
			iconVector[0] = m_fStartPointX - m_fEndPointX;
			iconVector[1] = m_fStartPointY - m_fEndPointY;

			vector angles = iconVector.VectorToAngles();
			if (angles[0] == 90)
				angles[1] = 180 - angles[1];

			m_wiconImage.SetRotation(angles[1]);

			iconVector = m_MapEntity.GetMapWidget().SizeToPixels(iconVector);
			m_wiconImage.SetSize(GetGame().GetWorkspace().DPIUnscale(iconVector.Length()), GetGame().GetWorkspace().DPIUnscale(iconVector.Length()));
		}

		FrameSlot.SetPos(m_wicon, GetGame().GetWorkspace().DPIUnscale(screenX), GetGame().GetWorkspace().DPIUnscale(screenY));

		m_wicon.SetVisible(true);
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
	protected static int s_iNextUniqueId = 1;

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
	
	protected void OnMapZoom(float zoomVal)
	{
		foreach (MapIcon icon: m_aicons)
		{
			icon.UpdateIcon();
		}
	}

	protected void OnMapZoomEnd(float zoomVal)
	{
		foreach (MapIcon icon: m_aicons)
		{
			GetGame().GetCallqueue().CallLater(icon.UpdateIcon, 0, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnMapOpen(MapConfiguration config)
	{
		m_MapEntity = SCR_MapEntity.GetMapInstance();
		m_wDrawingContainer = FrameWidget.Cast(config.RootWidgetRef.FindAnyWidget(SCR_MapConstants.DRAWING_CONTAINER_WIDGET_NAME));

		if (!m_wDrawingContainer)
		{
			Print("GRAD_IconMarkerUI OnMapOpen: Cant find m_wDrawingContainer", LogLevel.ERROR);
			return;
		}

		foreach (MapIcon icon: m_aicons)
		{
			icon.m_MapEntity = m_MapEntity;
			icon.CreateIcon(m_wDrawingContainer);
			GetGame().GetCallqueue().CallLater(icon.UpdateIcon, 0, false);
		}

		m_MapEntity.GetOnMapPan().Insert(OnMapPan);
		m_MapEntity.GetOnMapPanEnd().Insert(OnMapPanEnd);
		m_MapEntity.GetOnMapZoom().Insert(OnMapZoom);
		m_MapEntity.GetOnMapZoomEnd().Insert(OnMapZoomEnd);
	}

	//------------------------------------------------------------------------------------------------
	void OnMapClose(MapConfiguration config)
	{
		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapPan().Remove(OnMapPan);
			m_MapEntity.GetOnMapPanEnd().Remove(OnMapPanEnd);
			m_MapEntity.GetOnMapZoom().Remove(OnMapZoom);
			m_MapEntity.GetOnMapZoomEnd().Remove(OnMapZoomEnd);
		}
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
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD IconMarkerUI: All icons cleared", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	int AddIcon(float startPointX, float startPointY, float endPointX, float endPointY, string sType, RplId rplId, bool isVehicle = false, string label = "")
	{
		MapIcon icon = new MapIcon(m_MapEntity, this);

		icon.m_fStartPointX = startPointX;
		icon.m_fStartPointY = startPointY;
		icon.m_fEndPointX = endPointX;
		icon.m_fEndPointY = endPointY;
		icon.m_sType = sType;
		icon.m_sLabel = label;
		icon.rplId = rplId;
		icon.m_bIsVehicle = isVehicle;
		icon.m_iUniqueId = s_iNextUniqueId;
		s_iNextUniqueId++;

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD IconMarkerUI: AddIcon id=%1, rplId=%2, label='%3'", icon.m_iUniqueId, rplId, label), LogLevel.NORMAL);

		m_aicons.Insert(icon);

		if (m_wDrawingContainer)
		{
			icon.CreateIcon(m_wDrawingContainer);
			GetGame().GetCallqueue().CallLater(icon.UpdateIcon, 0, false);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD IconMarkerUI: Icon id=%1 created immediately (map open)", icon.m_iUniqueId), LogLevel.NORMAL);
		}
		else
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD IconMarkerUI: Icon id=%1 deferred (map closed, will create on open)", icon.m_iUniqueId), LogLevel.NORMAL);
		}

		return icon.m_iUniqueId;
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
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("GRAD IconMarkerUI SetIcon: m_sType is set to %1, isVehicle=%2", icon.m_sType, isVehicle), LogLevel.NORMAL);
				icon.UpdateIcon();
			};
		}	
	}
	
	//------------------------------------------------------------------------------------------------
	void RemoveIcon(int uniqueId)
	{
		for (int i = m_aicons.Count() - 1; i >= 0; i--)
		{
			MapIcon icon = m_aicons[i];
			if (icon.m_iUniqueId == uniqueId)
			{
				if (icon.m_wicon)
					icon.m_wicon.RemoveFromHierarchy();
				m_aicons.Remove(i);
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("GRAD IconMarkerUI: Removed icon id=%1", uniqueId), LogLevel.NORMAL);
				return;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	void Cleanup()
	{
		// Remove static event subscriptions
		SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);

		// Remove repeating CheckVehicleStates callback
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(CheckVehicleStates);

		// Remove instance-level event subscriptions
		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapPan().Remove(OnMapPan);
			m_MapEntity.GetOnMapPanEnd().Remove(OnMapPanEnd);
			m_MapEntity.GetOnMapZoom().Remove(OnMapZoom);
			m_MapEntity.GetOnMapZoomEnd().Remove(OnMapZoomEnd);
		}

		// Clean up icon widgets
		ClearAllIcons();
	}

	//------------------------------------------------------------------------------------------------
	void GRAD_IconMarkerUI()
	{
		m_MapEntity = SCR_MapEntity.GetMapInstance();
	}
};