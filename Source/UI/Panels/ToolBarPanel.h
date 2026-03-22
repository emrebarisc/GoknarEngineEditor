#pragma once

#include "EditorPanel.h"

#include <future>

class ToolBarPanel : public IEditorPanel
{
public:
	ToolBarPanel(EditorHUD* hud) : 
		IEditorPanel("Tool Bar", hud)
	{

	}
	
	virtual void Draw() override;

	std::future<void> asyncCompileResult;
	std::future<void> asyncRunResult;
};