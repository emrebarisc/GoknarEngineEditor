#pragma once

#include "EditorPanel.h"

class ObjectNameToCreatePanel : public IEditorPanel
{
public:
	ObjectNameToCreatePanel(EditorHUD* hud) : 
		IEditorPanel("Object Name To Create", hud)
	{
		isOpen_ = false;
	}
	
	virtual void Draw() override;
};