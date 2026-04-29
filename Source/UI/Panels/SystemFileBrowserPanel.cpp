#include "SystemFileBrowserPanel.h"

#include "imgui.h"
#include <filesystem>
#include <cstring>

#include "UI/EditorHUD.h"

namespace
{
	std::string NormalizePath(const std::string& path)
	{
		if (path.empty())
		{
			return "";
		}

		return std::filesystem::path(path).lexically_normal().generic_string();
	}

	bool StartsWithPath(const std::string& value, const std::string& prefix)
	{
		return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
	}

	std::string EnsureTrailingSlash(const std::string& path)
	{
		if (path.empty() || path.back() == '/')
		{
			return path;
		}

		return path + "/";
	}
}

SystemFileBrowserPanel::SystemFileBrowserPanel(EditorHUD* hud) :
	IEditorPanel("System File Browser", hud)
{
	isOpen_ = false;

	currentPath_ = std::filesystem::current_path().string();
}

void SystemFileBrowserPanel::SetCurrentPath(const std::string& path)
{
	if (path.empty())
	{
		return;
	}

	std::error_code errorCode;
	std::filesystem::path filesystemPath(path);
	const std::filesystem::path normalizedPath = filesystemPath.lexically_normal();
	if (std::filesystem::exists(normalizedPath, errorCode) && std::filesystem::is_directory(normalizedPath, errorCode))
	{
		const std::string normalizedPathString = NormalizePath(normalizedPath.string());
		if (!browsingRootPath_.empty() && !StartsWithPath(EnsureTrailingSlash(normalizedPathString), browsingRootPath_))
		{
			currentPath_ = browsingRootPath_;
			return;
		}

		currentPath_ = normalizedPath.string();
	}
}

void SystemFileBrowserPanel::SetBrowsingRootPath(const std::string& path)
{
	browsingRootPath_.clear();
	if (path.empty())
	{
		return;
	}

	std::error_code errorCode;
	const std::filesystem::path normalizedPath = std::filesystem::path(path).lexically_normal();
	if (std::filesystem::exists(normalizedPath, errorCode) && std::filesystem::is_directory(normalizedPath, errorCode))
	{
		browsingRootPath_ = EnsureTrailingSlash(NormalizePath(normalizedPath.string()));
		if (currentPath_.empty() || !StartsWithPath(EnsureTrailingSlash(NormalizePath(currentPath_)), browsingRootPath_))
		{
			currentPath_ = normalizedPath.string();
		}
	}
}

void SystemFileBrowserPanel::ClearBrowsingRootPath()
{
	browsingRootPath_.clear();
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
			const std::string parentPath = NormalizePath(path.parent_path().string());
			if (browsingRootPath_.empty())
			{
				currentPath_ = path.parent_path().string();
			}
			else if (StartsWithPath(EnsureTrailingSlash(parentPath), browsingRootPath_))
			{
				currentPath_ = path.parent_path().string();
			}
			else
			{
				currentPath_ = browsingRootPath_;
			}
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
					bool isSelected = (selectedItem_ == path.string());
					ImGui::Selectable(("[Dir] " + filenameString).c_str(), isSelected);

					if (hud_->WasLastItemDoubleClicked(ImGuiMouseButton_Left))
					{
						const std::string normalizedDirectoryPath = EnsureTrailingSlash(NormalizePath(path.string()));
						if (browsingRootPath_.empty() || StartsWithPath(normalizedDirectoryPath, browsingRootPath_))
						{
							currentPath_ = path.string();
							selectedItem_ = "";
							break;
						}
					}
					else
					{
						if (mode_ == FileBrowserMode::OpenDirectory || mode_ == FileBrowserMode::CreateProject)
						{
							selectedItem_ = currentPath_;
						}
					}
				}
				else
				{
					if (mode_ == FileBrowserMode::OpenFile || mode_ == FileBrowserMode::SaveFile)
					{
						bool isSelected = (selectedItem_ == path.string());
						if (ImGui::Selectable(filenameString.c_str(), isSelected))
						{
							selectedItem_ = path.string();
							if (mode_ == FileBrowserMode::SaveFile)
							{
								saveFileName_ = filenameString;
							}
						}
					}
					else
					{
						ImGui::TextDisabled("%s", filenameString.c_str());
					}
				}
			}
			catch (...)
			{
			}
		}
	}
	catch (const std::exception& e)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error reading directory: %s", e.what());
	}

	ImGui::EndChild();

	if (mode_ == FileBrowserMode::SaveFile || mode_ == FileBrowserMode::CreateProject)
	{
		char buf[256];
		strncpy(buf, saveFileName_.c_str(), sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
		
		const char* inputLabel = (mode_ == FileBrowserMode::CreateProject) ? "Project Name" : "File Name";
		if (ImGui::InputText(inputLabel, buf, sizeof(buf)))
		{
			saveFileName_ = buf;
		}
	}
	else
	{
		ImGui::Text("Selected: %s", selectedItem_.c_str());
	}

	ImGui::SameLine(ImGui::GetWindowWidth() - 100);

	const char* buttonText = "Open";
	if (mode_ == FileBrowserMode::SaveFile) buttonText = "Save";
	else if (mode_ == FileBrowserMode::CreateProject) buttonText = "Create";

	bool canConfirm = (mode_ == FileBrowserMode::SaveFile || mode_ == FileBrowserMode::CreateProject) ? !saveFileName_.empty() : !selectedItem_.empty();

	if (ImGui::Button(buttonText) && canConfirm)
	{
		std::string finalPath;

		if (mode_ == FileBrowserMode::CreateProject)
		{
			std::string dirPath = currentPath_;
			for (char& c : dirPath) { if (c == '\\') c = '/'; }
			if (!onProjectSelectionCallback_.isNull())
			{
				onProjectSelectionCallback_(dirPath, saveFileName_);
			}
			SetIsOpen(false);
			ImGui::End();
			return;
		}
		else if (mode_ == FileBrowserMode::SaveFile)
		{
			finalPath = currentPath_;
			if (!finalPath.empty() && finalPath.back() != '/' && finalPath.back() != '\\')
			{
				finalPath += '/';
			}
			finalPath += saveFileName_;
		}
		else if (mode_ == FileBrowserMode::OpenDirectory)
		{
			finalPath = selectedItem_ + '/';
		}
		else
		{
			finalPath = selectedItem_;
		}

		for (char& c : finalPath)
		{
			if (c == '\\')
			{
				c = '/';
			}
		}

		if (!onSelectionCallback_.isNull())
		{
			onSelectionCallback_(finalPath);
		}

		SetIsOpen(false);
	}

	ImGui::End();
}
