#pragma once

#include "EditorPanel.h"

class FileBrowserPanel : public IEditorPanel
{
public:
	FileBrowserPanel(EditorHUD* hud) : 
		IEditorPanel("FileBrowser", hud)
	{

	}

	~FileBrowserPanel();
	
	virtual void Draw() override;
	virtual void Init() override;

private:
	struct Folder
	{
		std::string folderName;
		std::string fullPath;
		std::vector<Folder*> subFolders;
		std::vector<std::string> fileNames;
	};

	void BuildFileTree();
	void DrawFileTree(Folder* folder);
	void DrawFileGrid(Folder* folder, std::string& selectedFileName, bool& isAFileSelected);

	Folder* rootFolder{ nullptr };
	std::unordered_map<std::string, Folder*> folderMap{};

};