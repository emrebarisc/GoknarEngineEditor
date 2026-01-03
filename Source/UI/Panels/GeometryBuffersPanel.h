#pragma once

#include "EditorPanel.h"

class GeometryBuffersPanel : public IEditorPanel
{
public:
	GeometryBuffersPanel(EditorHUD* hud) : 
		IEditorPanel("GeometryBuffers", hud)
	{

	}
	
	virtual void Draw() override;
};