//------------------------------------------------------------------------------------------------
//! Map line
class MapIcon
{
	float m_fStartPointX, m_fStartPointY;
	float m_fEndPointX, m_fEndPointY;
	int m_iType;
	string m_sType;
	Widget m_wRootW;
	Widget m_wicon;
	ImageWidget m_wiconImage;
	SCR_MapEntity m_MapEntity;
	GRAD_IconMarkerUI m_OwnerComponent;
	
	//------------------------------------------------------------------------------------------------
	void CreateIcon(notnull Widget rootW)
	{
		m_wRootW = rootW;
		
		Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			return;
		
		m_wicon = GetGame().GetWorkspace().CreateWidgets("{4B995CEAA55BBECC}UI/Layouts/Map/MapDrawicon.layout", mapFrame);		
		m_wiconImage = ImageWidget.Cast(m_wicon.FindAnyWidget("DrawIconImage"));
		
		if (m_wiconImage) {
			if (m_sType != "") {
				m_wiconImage.LoadImageFromSet(m_iType, "{9C5B2BA4695A421C}UI/Textures/Icons/GRAD_BC_mapIcons.imageset.edds", m_sType);
			};
		};
	}
	
	//------------------------------------------------------------------------------------------------
	void Updateicon()
	{	
		if (!m_wicon)	// can happen due to callater used for update
			return;
		
		if (!m_wiconImage)
			return;
				
		int screenX, screenY, endX, endY;

		m_MapEntity.WorldToScreen(m_fStartPointX, m_fStartPointY, screenX, screenY, true);
		m_MapEntity.WorldToScreen(m_fEndPointX, m_fEndPointY, endX, endY, true);
		
		vector iconVector = vector.Zero;
		iconVector[0] = m_fStartPointX - m_fEndPointX;
		iconVector[1] = m_fStartPointY - m_fEndPointY;

		vector angles = iconVector.VectorToAngles();
		if (angles[0] == 90)
			angles[1] =  180 - angles[1]; 	// reverse angles when passing vertical axis
		
		m_wiconImage.SetRotation(angles[1]);
		
		iconVector = m_MapEntity.GetMapWidget().SizeToPixels(iconVector);
		m_wiconImage.SetSize(GetGame().GetWorkspace().DPIUnscale(iconVector.Length()), GetGame().GetWorkspace().DPIUnscale(iconVector.Length()));
		
		FrameSlot.SetPos(m_wicon, GetGame().GetWorkspace().DPIUnscale(screenX), GetGame().GetWorkspace().DPIUnscale(screenY));	// needs unscaled coords
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
			icon.Updateicon();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! SCR_MapEntity event
	protected void OnMapPanEnd(float x, float y)
	{
		foreach (MapIcon icon: m_aicons)
		{
			GetGame().GetCallqueue().CallLater(icon.Updateicon, 0, false); // needs to be delayed by a frame as it cant always update the size after zoom correctly within the same frame
		}
	}
	
	protected void OnMapZoom(float x, float y)
	{
		foreach (MapIcon icon: m_aicons)
		{
			icon.Updateicon();
		}
	}
	
	protected void OnMapZoomEnd(float x, float y)
	{
		foreach (MapIcon icon: m_aicons)
		{
			icon.Updateicon();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnMapOpen(MapConfiguration config)
	{
		m_wDrawingContainer = FrameWidget.Cast(config.RootWidgetRef.FindAnyWidget(SCR_MapConstants.DRAWING_CONTAINER_WIDGET_NAME));
		
		foreach (MapIcon icon: m_aicons)
		{
			int type = icon.m_iType;
			icon.CreateIcon(m_wDrawingContainer);
			GetGame().GetCallqueue().CallLater(icon.Updateicon, 0, false);
		}
						
		m_MapEntity.GetOnMapPan().Insert(OnMapPan);		// pan for scaling
		m_MapEntity.GetOnMapPanEnd().Insert(OnMapPanEnd);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnMapClose(MapConfiguration config)
	{
		m_MapEntity.GetOnMapPan().Remove(OnMapPan);
		m_MapEntity.GetOnMapPanEnd().Remove(OnMapPanEnd);
	}
	
	//------------------------------------------------------------------------------------------------
	void Init()
	{
		SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
	}

	//------------------------------------------------------------------------------------------------
	void AddIcon(float startPointX, float startPointY, float endPointX, float endPointY, int iType, string sType)
	{
		MapIcon icon = new MapIcon(m_MapEntity, this);
		
		icon.m_fStartPointX = startPointX;
		icon.m_fStartPointY = startPointY;
		icon.m_fEndPointX = endPointX;
		icon.m_fEndPointY = endPointY;
		icon.m_iType = iType;
		icon.m_sType = sType;
		
		m_aicons.Insert(icon);
	}
	
	//------------------------------------------------------------------------------------------------
	void GRAD_IconMarkerUI()
	{
		m_MapEntity = SCR_MapEntity.GetMapInstance();
	}
};