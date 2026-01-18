#include "FileBrowserPanel.h"

#include "imgui.h"

#include "UI/EditorContext.h"

FileBrowserPanel::~FileBrowserPanel()
{
}

void FileBrowserPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_None);

	if (ImGui::TreeNode("Assets"))
	{
		DrawFileTree(EditorContext::Get()->rootFolder);
		ImGui::TreePop();
	}

	ImGui::End();
}

void FileBrowserPanel::Init()
{
}

void FileBrowserPanel::DrawFileTree(const Folder* folder)
{
	for (const Folder* subFolder : folder->subFolders)
	{
		if (ImGui::TreeNode(subFolder->name.c_str()))
		{
			DrawFileTree(subFolder);
			ImGui::TreePop();
		}
	}

	for (auto fileName : folder->files)
	{
		ImGui::Text(fileName.c_str());
	}
}
