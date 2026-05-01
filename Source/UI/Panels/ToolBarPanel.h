#pragma once

#include "EditorPanel.h"

#include <future>

class ToolBarPanel : public IEditorPanel
{
public:
	static constexpr float GetToolbarHeight()
	{
		return 64.0f;
	}

	ToolBarPanel(EditorHUD* hud) : 
		IEditorPanel("Tool Bar", hud)
	{

	}
	
	virtual void Draw() override;

	std::future<void> asyncCompileResult;
	std::future<void> asyncRunResult;
};
