#include "SystemFileBrowserPanel.h"

#include "imgui.h"
#include <filesystem>
#include <cstring>

SystemFileBrowserPanel::SystemFileBrowserPanel(EditorHUD* hud) :
	IEditorPanel("System File Browser", hud)
{
	isOpen_ = false;

	currentPath_ = std::filesystem::current_path().string();
}

void SystemFileBrowserPanel::Draw()
{
	if (!ImGui::Begin(title_.c_str(), &isOpen_))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Current Path: %s", currentPath_.c_str());

	if (ImGui::Button("Up"))
	{
		std::filesystem::path path(currentPath_);
		if (path.has_parent_path())
		{
			currentPath_ = path.parent_path().string();
		}
	}

	ImGui::Separator();

	ImGui::BeginChild("FilesRegion", ImVec2(0, -30), true);

	try
	{
		for (const auto& entry : std::filesystem::directory_iterator(currentPath_, std::filesystem::directory_options::skip_permission_denied))
		{
			try
			{
				const auto& path = entry.path();
				auto filenameString = path.filename().string();
				bool isDirectory = entry.is_directory();

				if (isDirectory)
				{
					bool isSelected = (selectedFolder_ == path.string());
					if (ImGui::Selectable(("[Dir] " + filenameString).c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
					{
						if (ImGui::IsMouseDoubleClicked(0))
						{
							currentPath_ = path.string();
							selectedFolder_ = path.string();
							break;
						}
						else
						{
							selectedFolder_ = path.string();
						}
					}
				}
				else
				{
					ImGui::TextDisabled("%s", filenameString.c_str());
				}
			}
			catch (...)
			{
				// Skip files that cause errors such as invalid string conversions
			}
		}
	}
	catch (const std::exception& e)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error reading directory: %s", e.what());
	}

	ImGui::EndChild();

	ImGui::Text("Selected: %s", selectedFolder_.c_str());
	ImGui::SameLine(ImGui::GetWindowWidth() - 100);
	if (ImGui::Button("Open") && !selectedFolder_.empty())
	{
		for (char& c : selectedFolder_)
		{
			if (c == '\\')
			{
				c = '/';
			}
		}

		if (!onDirectorySelectedCallback_.isNull())
		{
			onDirectorySelectedCallback_(selectedFolder_ + '/');
		}

		SetIsOpen(false);
	}

	ImGui::End();
}
