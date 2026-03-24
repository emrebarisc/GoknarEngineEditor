#include "FileBrowserPanel.h"

#include <filesystem>
#include <fstream>
#include <cstring>

#include "imgui.h"

#include "Goknar/Engine.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Renderer/Texture.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Model/SkeletalMesh.h"

#include "UI/EditorUtils.h"

#include "ImageViewerPanel.h"
#include "MeshViewerPanel.h"
#include "SkeletalMeshViewerPanel.h"
#include "UI/EditorHUD.h"

extern std::string ProjectDir;

FileBrowserPanel::FileBrowserPanel(EditorHUD* hud) :
	IEditorPanel("File Browser", hud)
{
	currentFolder_ = EditorContext::Get()->rootFolder;
}

void FileBrowserPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_);

	if (!currentFolder_)
	{
		currentFolder_ = EditorContext::Get()->rootFolder;
	}

	if (ImGui::BeginTable("BrowserSplitLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("TreePane", ImGuiTableColumnFlags_WidthFixed, 200.0f);
		ImGui::TableSetupColumn("GridPane", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();

		// --- Left Pane: Folder Tree ---
		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("FolderTreeRegion");
		DrawFolderTree(EditorContext::Get()->rootFolder);
		ImGui::EndChild();

		// --- Right Pane: Content Grid ---
		ImGui::TableSetColumnIndex(1);

		if (currentFolder_ != EditorContext::Get()->rootFolder)
		{
			if (ImGui::Button(".. Back"))
			{
				std::string currentPath = currentFolder_->path;
				if (!currentPath.empty() && currentPath.back() == '/') currentPath.pop_back();
				size_t lastSlash = currentPath.find_last_of('/');

				if (lastSlash != std::string::npos)
				{
					std::string parentPath = currentPath.substr(0, lastSlash + 1);
					EditorContext* context = EditorContext::Get();
					if (context->folderMap.find(parentPath) != context->folderMap.end())
					{
						currentFolder_ = context->folderMap[parentPath];
					}
					else
					{
						currentFolder_ = context->rootFolder;
					}
				}
				else
				{
					currentFolder_ = EditorContext::Get()->rootFolder;
				}
			}
			ImGui::SameLine();
		}
		ImGui::Text("Path: %s", currentFolder_->path.c_str());
		ImGui::Separator();

		float footerHeight = 25.0f;
		ImGui::BeginChild("GridRegion", ImVec2(0, -footerHeight));
		DrawGrid();

		// NEW: Empty space right-click menu (won't open if hovering over a file/folder item)
		if (ImGui::BeginPopupContextWindow("EmptySpaceMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Add new file"))
			{
				isCreatingFile_ = true;
				fileNameBuffer_[0] = '\0'; // Clear buffer
			}
			ImGui::EndPopup();
		}

		ImGui::EndChild();

		ImGui::EndTable();
	}

	// Footer Slider
	float sliderWidth = 50.0f;
	float sliderHeight = 20.0f;
	float padding = 10.0f;

	ImVec2 windowSize = ImGui::GetWindowSize();
	ImVec2 sliderPos = ImVec2(
		windowSize.x - sliderWidth - padding + ImGui::GetScrollX(),
		windowSize.y - sliderHeight - padding + ImGui::GetScrollY()
	);
	ImGui::SetCursorPos(sliderPos);

	ImGui::PushItemWidth(sliderWidth);
	ImGui::SliderFloat("##sizeSlider", &thumbnailSize_, 32.0f, 64.0f, "");
	ImGui::PopItemWidth();

	// Global Rename Modal Handler
	if (isRenaming_ && !selectedItemForMenu_.empty())
	{
		ImGui::OpenPopup("Rename Item");
	}

	if (ImGui::BeginPopupModal("Rename Item", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("New Name", renameBuffer_, sizeof(renameBuffer_));
		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			try {
				std::string cleanPath = selectedItemForMenu_;
				if (!cleanPath.empty() && cleanPath.back() == '/') cleanPath.pop_back();

				std::filesystem::path srcPath(cleanPath);
				std::filesystem::path destPath = srcPath.parent_path() / renameBuffer_;

				if (srcPath != destPath && !std::filesystem::exists(destPath))
				{
					std::filesystem::rename(srcPath, destPath);
					needsRefresh_ = true; // Defer refresh
				}
			}
			catch (...) {}

			isRenaming_ = false;
			selectedItemForMenu_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			isRenaming_ = false;
			selectedItemForMenu_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// NEW: Global Create File Modal Handler
	if (isCreatingFile_)
	{
		ImGui::OpenPopup("Create New File");
	}

	if (ImGui::BeginPopupModal("Create New File", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("File Name", fileNameBuffer_, sizeof(fileNameBuffer_));
		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			try {
				std::string targetDir = ProjectDir + currentFolder_->path;
				if (!targetDir.empty() && targetDir.back() != '/') targetDir += '/';

				std::filesystem::path newFilePath = std::filesystem::path(targetDir) / fileNameBuffer_;

				if (!std::filesystem::exists(newFilePath))
				{
					// Create an empty file
					std::ofstream ofs(newFilePath);
					ofs.close();
					needsRefresh_ = true; // Defer refresh
				}
			}
			catch (...) {}

			isCreatingFile_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			isCreatingFile_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::End();

	// Perform the delayed folder refresh outside of the ImGui loops
	if (needsRefresh_)
	{
		RefreshAndRestore();
		needsRefresh_ = false;
	}
}

void FileBrowserPanel::DrawFolderTree(Folder* folder)
{
	if (!folder) return;

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (currentFolder_ == folder) flags |= ImGuiTreeNodeFlags_Selected;
	if (folder->subFolders.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

	bool isOpen = ImGui::TreeNodeEx(folder->name.c_str(), flags);

	// Navigation
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		currentFolder_ = folder;
	}

	// Interactions
	std::string folderFullPath = ProjectDir + folder->path;
	HandleDragDropSource(folderFullPath, "FOLDER_PAYLOAD");
	HandleDragDropTarget(folderFullPath);
	HandleContextMenu(folderFullPath, folder->name, true);

	if (isOpen)
	{
		for (Folder* sub : folder->subFolders)
		{
			DrawFolderTree(sub);
		}
		ImGui::TreePop();
	}
}

void FileBrowserPanel::DrawGrid()
{
	if (!currentFolder_)
	{
		return;
	}

	float padding = 16.0f;
	float cellSize = thumbnailSize_ + padding;
	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columns = (int)(panelWidth / cellSize);
	if (columns < 1) columns = 1;

	const float atlasSize = 1024.0f;
	const float spriteSize = 128.0f;

	auto GetUV0 = [&](float x, float y) { return ImVec2(x / atlasSize, y / atlasSize); };
	auto GetUV1 = [&](float x, float y) { return ImVec2((x + spriteSize) / atlasSize, (y + spriteSize) / atlasSize); };

	Image* uiImage = EditorUtils::GetEditorContent<Image>("Textures/UI/T_UI.png");
	if (!uiImage || !uiImage->GetGeneratedTexture())
	{
		return;
	}

	ImTextureID atlasID = (ImTextureID)(intptr_t)uiImage->GetGeneratedTexture()->GetRendererTextureId();

	if (ImGui::BeginTable("FileGrid", columns))
	{
		for (Folder* sub : currentFolder_->subFolders)
		{
			ImGui::TableNextColumn();
			ImGui::PushID(sub->name.c_str());

			ImVec2 fUv0 = GetUV0(0.0f, 128.0f);
			ImVec2 fUv1 = GetUV1(0.0f, 128.0f);

			if (ImGui::ImageButton("##folder", atlasID, { thumbnailSize_, thumbnailSize_ }, fUv0, fUv1))
			{
				currentFolder_ = sub;
			}

			// Interactions
			std::string subFullPath = ProjectDir + sub->path;
			HandleDragDropSource(subFullPath, "FOLDER_PAYLOAD");
			HandleDragDropTarget(subFullPath);
			HandleContextMenu(subFullPath, sub->name, true);

			ImGui::TextWrapped("%s", sub->name.c_str());
			ImGui::PopID();
		}

		for (const std::string& file : currentFolder_->files)
		{
			ImGui::TableNextColumn();
			ImGui::PushID(file.c_str());

			ResourceType resourceType = ResourceManagerUtils::GetResourceType(file);

			ImVec2 uv0 = GetUV0(0.0f, 0.0f);
			ImVec2 uv1 = GetUV0(0.0f, 0.0f);

			if (resourceType == ResourceType::Image)
			{
				uv0 = GetUV0(128.0f, 0.0f);
				uv1 = GetUV1(128.0f, 0.0f);
			}
			else if (resourceType == ResourceType::Model)
			{
				uv0 = GetUV0(256.0f, 0.0f);
				uv1 = GetUV1(256.0f, 0.0f);
			}
			else
			{
				std::string extension = ResourceManagerUtils::GetExtension(file);

				if (extension == "h")
				{
					uv0 = GetUV0(766.f, 0.0f);
					uv1 = GetUV1(766.f, 0.0f);
				}
				else if (extension == "cpp")
				{
					uv0 = GetUV0(896.f, 0.0f);
					uv1 = GetUV1(896.f, 0.0f);
				}
			}

			if (ImGui::ImageButton("##file", atlasID, { thumbnailSize_, thumbnailSize_ }, uv0, uv1))
			{
				if (resourceType == ResourceType::Image)
				{
					int idx = 0;
					while (Image* img = engine->GetResourceManager()->GetResourceContainer()->GetImage(idx++))
					{
						if (img->GetPath().find(file) != std::string::npos)
						{
							ImageViewerPanel* viewer = (ImageViewerPanel*)hud_->GetPanel<ImageViewerPanel>();
							if (viewer)
							{
								viewer->SetTargetTexture(img->GetGeneratedTexture());
								viewer->SetIsOpen(true);
							}
							break;
						}
					}
				}
				else if (resourceType == ResourceType::Model)
				{
					int idx = 0;
					while (MeshUnit* mesh = engine->GetResourceManager()->GetResourceContainer()->GetMesh(idx++))
					{
						if (mesh->GetPath().find(file) != std::string::npos)
						{
							if (SkeletalMesh* skeletalMesh = dynamic_cast<SkeletalMesh*>(mesh))
							{
								SkeletalMeshViewerPanel* viewer = (SkeletalMeshViewerPanel*)hud_->GetPanel<SkeletalMeshViewerPanel>();
								if (viewer)
								{
									viewer->SetTargetSkeletalMesh(skeletalMesh);
									viewer->SetIsOpen(true);
								}
							}
							else
							{
								StaticMesh* staticMesh = dynamic_cast<StaticMesh*>(mesh);

								MeshViewerPanel* viewer = (MeshViewerPanel*)hud_->GetPanel<MeshViewerPanel>();
								if (viewer)
								{
									viewer->SetTargetStaticMesh(staticMesh);
									viewer->SetIsOpen(true);
								}
							}
							break;
						}
					}
				}
				else
				{
					std::string extension = ResourceManagerUtils::GetExtension(file);

					if (extension == "h" || extension == "cpp")
					{
						std::string fullPath = ProjectDir + currentFolder_->path + file;
						std::string command;
#ifdef GOKNAR_PLATFORM_WINDOWS
						command = "start \"\" \"" + fullPath + "\"";
#else
						command = "open \"" + fullPath + "\"";
#endif
						system(command.c_str());
					}
				}
			}

			// Interactions
			std::string fileFullPath = ProjectDir + currentFolder_->path + file;
			HandleDragDropSource(fileFullPath, "FILE_PAYLOAD");
			HandleContextMenu(fileFullPath, file, false);

			ImGui::TextWrapped("%s", file.c_str());
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}

void FileBrowserPanel::HandleDragDropSource(const std::string& sourcePath, const std::string& payloadName)
{
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		ImGui::SetDragDropPayload(payloadName.c_str(), sourcePath.c_str(), sourcePath.size() + 1);

		std::string cleanPath = sourcePath;
		if (!cleanPath.empty() && cleanPath.back() == '/') cleanPath.pop_back();

		ImGui::Text("Moving: %s", std::filesystem::path(cleanPath).filename().string().c_str());
		ImGui::EndDragDropSource();
	}
}

void FileBrowserPanel::HandleDragDropTarget(const std::string& targetPath)
{
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FOLDER_PAYLOAD"))
		{
			std::string sourcePath = (const char*)payload->Data;
			MoveFileSystemItem(sourcePath, targetPath);
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PAYLOAD"))
		{
			std::string sourcePath = (const char*)payload->Data;
			MoveFileSystemItem(sourcePath, targetPath);
		}
		ImGui::EndDragDropTarget();
	}
}

void FileBrowserPanel::HandleContextMenu(const std::string& itemPath, const std::string& itemName, bool isFolder)
{
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Rename"))
		{
			isRenaming_ = true;
			selectedItemForMenu_ = itemPath;
			strncpy(renameBuffer_, itemName.c_str(), sizeof(renameBuffer_) - 1);
			renameBuffer_[sizeof(renameBuffer_) - 1] = '\0';
		}
		if (ImGui::MenuItem("Delete"))
		{
			try {
				std::string cleanPath = itemPath;
				if (!cleanPath.empty() && cleanPath.back() == '/') cleanPath.pop_back();
				std::filesystem::remove_all(cleanPath);
				needsRefresh_ = true;
			}
			catch (...) {}
		}
		ImGui::EndPopup();
	}
}

void FileBrowserPanel::MoveFileSystemItem(const std::string& source, const std::string& targetFolder)
{
	try {
		std::string cleanSource = source;
		if (!cleanSource.empty() && cleanSource.back() == '/') cleanSource.pop_back();

		std::string cleanTarget = targetFolder;
		if (!cleanTarget.empty() && cleanTarget.back() == '/') cleanTarget.pop_back();

		std::filesystem::path srcPath(cleanSource);
		std::filesystem::path destPath(cleanTarget);
		destPath /= srcPath.filename();

		if (srcPath != destPath && !std::filesystem::exists(destPath))
		{
			std::filesystem::rename(srcPath, destPath);
			needsRefresh_ = true;
		}
	}
	catch (...) {}
}

void FileBrowserPanel::RefreshAndRestore()
{
	std::string currentPathToRestore = currentFolder_ ? currentFolder_->path : "";

	EditorContext::Get()->BuildFileTree();

	if (!currentPathToRestore.empty())
	{
		auto it = EditorContext::Get()->folderMap.find(currentPathToRestore);
		if (it != EditorContext::Get()->folderMap.end())
		{
			currentFolder_ = it->second;
			return;
		}
	}
	currentFolder_ = EditorContext::Get()->rootFolder;
}