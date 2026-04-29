#pragma once

#include "EditorPanel.h"

class MenuBarPanel : public IEditorPanel
{
public:
	MenuBarPanel(EditorHUD* hud) : 
		IEditorPanel("Menu Bar", hud)
	{

	}
	
	virtual void Draw() override;

private:
	void OnProjectSelected(const std::string& directoryPath);
	void OnNewProjectSelected(const std::string& directoryPath, const std::string& projectName);
	void ContinueOpeningProject();
	void ReopenProjectSelector();
	bool SaveSceneToCurrentPath() const;
	void SaveProject();

	std::string editorEngineLocation_;
	std::string pendingProjectDirectoryPath_;
	std::string pendingProjectName_;
	bool shouldOpenEngineLocationPopup_{ false };
};
