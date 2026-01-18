#include "ContentBrowserPanel.h"

#include "imgui.h"

void ContentBrowserPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_);

	DrawFolder(EditorContext::Get()->rootFolder);

	ImGui::End();
}

void ContentBrowserPanel::DrawFolder(Folder* folder)
{
	bool nodeOpen = ImGui::TreeNode(folder->name.c_str());
	if (nodeOpen)
	{
		for (auto& sub : folder->subFolders)
		{
			DrawFolder(sub);
		}

		for (auto& file : folder->files)
		{
			ImGui::Text("%s", file.c_str());
		}
		
		ImGui::TreePop();
	}
}