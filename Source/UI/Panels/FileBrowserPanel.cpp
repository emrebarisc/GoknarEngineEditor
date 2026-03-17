#include "FileBrowserPanel.h"

#include "imgui.h"

#include "Goknar/Engine.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Renderer/Texture.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Model/SkeletalMesh.h"

#include "ImageViewerPanel.h"
#include "MeshViewerPanel.h"
#include "SkeletalMeshViewerPanel.h"
#include "UI/EditorHUD.h"

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

	// --- Header and Navigation ---
	if (currentFolder_ != EditorContext::Get()->rootFolder)
	{
		if (ImGui::Button(".. Back"))
		{
			// Get the current path (e.g., "Content/Textures/Characters/")
			std::string currentPath = currentFolder_->path;

			// Remove the trailing slash if it exists
			if (!currentPath.empty() && currentPath.back() == '/') currentPath.pop_back();

			// Find the last slash to determine the parent path
			size_t lastSlash = currentPath.find_last_of('/');

			if (lastSlash != std::string::npos)
			{
				// Extract parent path (e.g., "Content/Textures/")
				std::string parentPath = currentPath.substr(0, lastSlash + 1);

				EditorContext* context = EditorContext::Get();
				// Check if the parent exists in the folderMap
				if (context->folderMap.find(parentPath) != context->folderMap.end())
				{
					currentFolder_ = context->folderMap[parentPath];
				}
				else
				{
					// Fallback to root if parent path is not found in the map
					currentFolder_ = context->rootFolder;
				}
			}
			else
			{
				// If no slashes remain, we must be at the top level
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
	ImGui::EndChild();

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

	ImGui::End();
}

void FileBrowserPanel::DrawGrid()
{
	if (!currentFolder_) return;

	// Use the dynamic thumbnailSize_ variable here
	float padding = 16.0f;
	float cellSize = thumbnailSize_ + padding;
	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columns = (int)(panelWidth / cellSize);
	if (columns < 1) columns = 1;

	// Texture constants
	const float atlasSize = 1024.0f;
	const float spriteSize = 128.0f;

	// Helper to calculate UVs correctly
	// uv0: Top-Left coordinate
	// uv1: Bottom-Right coordinate (Top-Left + SpriteSize)
	auto GetUV0 = [&](float x, float y) { return ImVec2(x / atlasSize, y / atlasSize); };
	auto GetUV1 = [&](float x, float y) { return ImVec2((x + spriteSize) / atlasSize, (y + spriteSize) / atlasSize); };

	// Using the specific path requested
	Image* uiImage = engine->GetResourceManager()->GetContent<Image>("Textures/UI/T_UI.png");
	if (!uiImage || !uiImage->GetGeneratedTexture()) return;

	ImTextureID atlasID = (ImTextureID)(intptr_t)uiImage->GetGeneratedTexture()->GetRendererTextureId();

	if (ImGui::BeginTable("FileGrid", columns))
	{
		// --- 1. Draw Folders ---
		for (Folder* sub : currentFolder_->subFolders)
		{
			ImGui::TableNextColumn();
			ImGui::PushID(sub->name.c_str());

			ImVec2 fUv0 = GetUV0(0.0f, 128.0f);
			ImVec2 fUv1 = GetUV1(0.0f, 128.0f);

			if (ImGui::ImageButton("##folder", atlasID, { thumbnailSize_, thumbnailSize_ }, fUv0, fUv1))
			{
				// Selection logic
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				currentFolder_ = sub;
			}

			ImGui::TextWrapped("%s", sub->name.c_str());
			ImGui::PopID();
		}

		// --- 2. Draw Files ---
		for (const std::string& file : currentFolder_->files)
		{
			ImGui::TableNextColumn();
			ImGui::PushID(file.c_str());

			ResourceType resourceType = ResourceManagerUtils::GetResourceType(file);

			ImVec2 uv0, uv1;
			if (resourceType == ResourceType::Image)
			{
				// Image icon at (128, 0)
				uv0 = GetUV0(128.0f, 0.0f);
				uv1 = GetUV1(128.0f, 0.0f);
			}
			else if (resourceType == ResourceType::Model)
			{
				// Mesh icon at (256, 0)
				uv0 = GetUV0(256.0f, 0.0f);
				uv1 = GetUV1(256.0f, 0.0f);
			}
			else
			{
				// Unknown icon at (0, 0)
				uv0 = GetUV0(0.0f, 0.0f);
				uv1 = GetUV1(0.0f, 0.0f);
			}

			ImGui::ImageButton("##file", atlasID, { thumbnailSize_, thumbnailSize_ }, uv0, uv1);

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
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
					// If it's a source code file or other unknown text file
					if (file.find(".cpp") != std::string::npos || file.find(".h") != std::string::npos || file.find(".hpp") != std::string::npos)
					{
						std::string fullPath = currentFolder_->path + "/" + file;
#ifdef _WIN32
						std::string command = "start \"\" \"" + fullPath + "\"";
						system(command.c_str());
#else
						// Mac or Linux alternative - though mostly just care about VS on Windows
						std::string command = "open \"" + fullPath + "\"";
						system(command.c_str());
#endif
					}
				}
			}

			ImGui::TextWrapped("%s", file.c_str());
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}