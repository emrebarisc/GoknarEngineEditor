#include "FileBrowserPanel.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <cstring>

#include "imgui.h"
#include "tinyxml2.h"

#include "Goknar/Engine.h"
#include "Goknar/Application.h"
#include "Goknar/Helpers/ContentPathUtils.h"
#include "Goknar/Helpers/SceneParser.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Renderer/Texture.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Model/SkeletalMesh.h"
#include "Goknar/Materials/MaterialFunction.h"
#include "Goknar/Materials/MaterialFunctionSerializer.h"

#include "UI/EditorAssetPathUtils.h"
#include "UI/EditorSourceCodeUtils.h"
#include "UI/EditorUtils.h"

#include "AnimationGraphPanel.h"
#include "ClassCreationPanel.h"
#include "ImageViewerPanel.h"
#include "MeshViewerPanel.h"
#include "ShaderEditor/ShaderEditorPanel.h"
#include "SkeletalMeshViewerPanel.h"
#include "UI/EditorHUD.h"

namespace
{
	bool StartsWith(const std::string& value, const std::string& prefix)
	{
		return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
	}

	std::string NormalizePath(const std::string& path)
	{
		return std::filesystem::path(path).lexically_normal().generic_string();
	}

	std::string EnsureTrailingSlash(const std::string& path)
	{
		if (path.empty() || path.back() == '/')
		{
			return path;
		}

		return path + "/";
	}

	std::string QuoteForShell(const std::string& path)
	{
		return "\"" + path + "\"";
	}

	std::string ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		return value;
	}

	std::string GetAbsoluteProjectPath(const std::string& relativePath)
	{
		return NormalizePath((std::filesystem::path(EditorAssetPathUtils::GetProjectRootPath()) / relativePath).generic_string());
	}

	std::filesystem::path GetDuplicatePath(const std::filesystem::path& sourcePath)
	{
		const std::filesystem::path parentPath = sourcePath.parent_path();
		const bool isDirectory = std::filesystem::is_directory(sourcePath);
		const std::string extension = isDirectory ? "" : sourcePath.extension().generic_string();
		const std::string filename = sourcePath.filename().generic_string();
		const std::string stem = isDirectory ? filename : sourcePath.stem().generic_string();
		const std::string duplicateBaseName = stem.empty() ? filename : stem;

		std::filesystem::path duplicatePath = parentPath / (duplicateBaseName + "_Copy" + extension);
		int duplicateIndex = 2;
		while (std::filesystem::exists(duplicatePath))
		{
			duplicatePath = parentPath / (duplicateBaseName + "_Copy" + std::to_string(duplicateIndex) + extension);
			++duplicateIndex;
		}

		return duplicatePath;
	}

	bool CopyFileSystemItem(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath)
	{
		std::error_code errorCode;
		if (std::filesystem::is_directory(sourcePath, errorCode))
		{
			std::filesystem::create_directories(destinationPath.parent_path(), errorCode);
			if (errorCode)
			{
				return false;
			}

			std::filesystem::copy(sourcePath, destinationPath, std::filesystem::copy_options::recursive, errorCode);
			return !errorCode;
		}

		std::filesystem::create_directories(destinationPath.parent_path(), errorCode);
		if (errorCode)
		{
			return false;
		}

		std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::none, errorCode);
		return !errorCode;
	}

	ImVec2 RemapTextureUV(const Texture* texture, const ImVec2& uv)
	{
		return texture ?
			ImVec2(
				texture->GetAtlasUOffset() + uv.x * texture->GetAtlasUScale(),
				texture->GetAtlasVOffset() + uv.y * texture->GetAtlasVScale()) :
			uv;
	}

	bool IsContentDirectory(const std::string& path)
	{
		return StartsWith(NormalizePath(path), NormalizePath(EditorAssetPathUtils::GetContentRootPath()));
	}

	bool IsSourceDirectory(const std::string& path)
	{
		const std::string sourceRoot = EnsureTrailingSlash(NormalizePath((std::filesystem::path(EditorAssetPathUtils::GetProjectRootPath()) / "Source").generic_string()));
		return StartsWith(EnsureTrailingSlash(NormalizePath(path)), sourceRoot);
	}

	std::string TryGetGameAssetFileType(const std::string& filePath)
	{
		tinyxml2::XMLDocument document;
		if (document.LoadFile(filePath.c_str()) != tinyxml2::XML_SUCCESS)
		{
			return "";
		}

		tinyxml2::XMLElement* root = document.FirstChildElement("GameAsset");
		const char* fileType = root ? root->Attribute("FileType") : nullptr;
		if (fileType)
		{
			return fileType;
		}

		if (document.FirstChildElement("Scene"))
		{
			return "Scene";
		}

		return "";
	}

	std::string GetLowerExtension(const std::string& filePath)
	{
		std::string extension = std::filesystem::path(filePath).extension().generic_string();
		if (!extension.empty() && extension.front() == '.')
		{
			extension.erase(extension.begin());
		}

		return ToLower(extension);
	}

	bool IsHeaderFile(const std::string& filePath)
	{
		const std::string extension = GetLowerExtension(filePath);
		return extension == "h" || extension == "hpp" || extension == "inl";
	}

	bool IsSourceFile(const std::string& filePath)
	{
		const std::string extension = GetLowerExtension(filePath);
		return extension == "c" || extension == "cc" || extension == "cpp" || extension == "cxx";
	}

	bool IsSourceCodeFile(const std::string& filePath)
	{
		return IsHeaderFile(filePath) || IsSourceFile(filePath);
	}

	bool WriteMaterialAssetFile(const std::string& filePath)
	{
		tinyxml2::XMLDocument document;
		tinyxml2::XMLElement* root = document.NewElement("GameAsset");
		root->SetAttribute("FileType", "Material");
		document.InsertFirstChild(root);
		return document.SaveFile(filePath.c_str()) == tinyxml2::XML_SUCCESS;
	}

	bool WriteAnimationGraphAssetFile(const std::string& filePath)
	{
		tinyxml2::XMLDocument document;
		tinyxml2::XMLElement* root = document.NewElement("GameAsset");
		root->SetAttribute("FileType", "AnimationGraph");
		root->SetAttribute("SchemaVersion", "2");
		document.InsertFirstChild(root);

		root->InsertEndChild(document.NewElement("Variables"));

		tinyxml2::XMLElement* statesElement = document.NewElement("States");
		tinyxml2::XMLElement* stateElement = document.NewElement("State");
		stateElement->SetAttribute("id", 1);
		stateElement->SetAttribute("name", "RootState");
		stateElement->InsertEndChild(document.NewElement("OutboundConnections"));

		tinyxml2::XMLElement* entryNodeElement = document.NewElement("EntryNode");
		entryNodeElement->SetAttribute("id", 1);
		stateElement->InsertEndChild(entryNodeElement);

		tinyxml2::XMLElement* nodesElement = document.NewElement("Nodes");
		tinyxml2::XMLElement* nodeElement = document.NewElement("Node");
		nodeElement->SetAttribute("id", 1);
		nodeElement->SetAttribute("type", "Clip");
		nodeElement->SetAttribute("animationName", "Idle");
		nodeElement->SetAttribute("loop", true);
		nodeElement->SetAttribute("playRate", 1.0f);
		nodeElement->InsertEndChild(document.NewElement("OutboundConnections"));
		nodesElement->InsertEndChild(nodeElement);

		stateElement->InsertEndChild(nodesElement);
		statesElement->InsertEndChild(stateElement);
		root->InsertEndChild(statesElement);

		return document.SaveFile(filePath.c_str()) == tinyxml2::XML_SUCCESS;
	}

	bool WriteSceneAssetFile(const std::string& filePath)
	{
		tinyxml2::XMLDocument document;
		tinyxml2::XMLElement* root = document.NewElement("Scene");
		document.InsertFirstChild(root);

		root->InsertEndChild(document.NewElement("Lights"));
		root->InsertEndChild(document.NewElement("Objects"));

		return document.SaveFile(filePath.c_str()) == tinyxml2::XML_SUCCESS;
	}

	bool WriteMaterialFunctionAssetFile(const std::string& filePath)
	{
		MaterialFunction materialFunction;
		materialFunction.SetName(std::filesystem::path(filePath).filename().generic_string());
		materialFunction.SetGeneratedFunctionName("");
		materialFunction.SetGeneratedFunctionDefinitions("");
		materialFunction.SetOutputs({ { "Result", MaterialFunctionPinType::Float } });

		const std::filesystem::path contentRoot = std::filesystem::path(EditorAssetPathUtils::GetContentRootPath());
		const std::string relativePath = std::filesystem::path(filePath).lexically_relative(contentRoot).generic_string();
		return MaterialFunctionSerializer::Serialize(relativePath, materialFunction);
	}
}

FileBrowserPanel::FileBrowserPanel(EditorHUD* hud) :
	IEditorPanel("File Browser", hud)
{
	SetCurrentFolder(EditorContext::Get()->rootFolder);
	observedFileTreeVersion_ = EditorContext::Get()->GetFileTreeVersion();
}

void FileBrowserPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_);

	SynchronizeCurrentFolder();

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
						SetCurrentFolder(context->folderMap[parentPath]);
					}
					else
					{
						SetCurrentFolder(context->rootFolder);
					}
				}
				else
				{
					SetCurrentFolder(EditorContext::Get()->rootFolder);
				}
			}
			ImGui::SameLine();
		}
		ImGui::Text("Path: %s", currentFolder_->path.c_str());
		ImGui::Separator();

		float footerHeight = 25.0f;
		ImGui::BeginChild("GridRegion", ImVec2(0, -footerHeight));
		DrawGrid();

		if (ImGui::BeginPopupContextWindow("EmptySpaceMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			const std::string targetDirectory = currentFolder_ ? GetAbsoluteProjectPath(currentFolder_->path) : "";
			if (!targetDirectory.empty() && ImGui::MenuItem("Show in Explorer"))
			{
				EditorSourceCodeUtils::ShowInFileExplorer(targetDirectory, false);
			}
			if (!targetDirectory.empty())
			{
				ImGui::Separator();
			}
			DrawCreateContentMenu(targetDirectory);
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
					// 1. Rename primary content
					std::filesystem::rename(srcPath, destPath);

					// 2. Rename editor reflection file/folder
					std::string srcReflection = EditorAssetPathUtils::ToEditorReflectionPath(srcPath.generic_string());
					std::string destReflection = EditorAssetPathUtils::ToEditorReflectionPath(destPath.generic_string());

					if (std::filesystem::exists(srcReflection))
					{
						EditorAssetPathUtils::EnsureDirectoryForFile(destReflection);
						std::filesystem::rename(srcReflection, destReflection);
					}

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

	if (isCreatingFolder_)
	{
		ImGui::OpenPopup("Create Folder");
	}

	if (ImGui::BeginPopupModal("Create Folder", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (shouldFocusCreationNameInput_)
		{
			ImGui::SetKeyboardFocusHere();
			shouldFocusCreationNameInput_ = false;
		}

		const bool isNameSubmitted = ImGui::InputText("Folder Name", creationNameBuffer_, sizeof(creationNameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue);
		if (isNameSubmitted || ImGui::Button("OK", ImVec2(120, 0)))
		{
			FinalizeFolderCreation();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			isCreatingFolder_ = false;
			pendingCreationDirectory_.clear();
			shouldFocusCreationNameInput_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (isCreatingAsset_)
	{
		ImGui::OpenPopup("Create Asset");
	}

	if (ImGui::BeginPopupModal("Create Asset", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const char* assetTypeLabel = pendingAssetCreationType_ == PendingAssetCreationType::Material ? "Material" :
			pendingAssetCreationType_ == PendingAssetCreationType::MaterialFunction ? "Material Function" :
			pendingAssetCreationType_ == PendingAssetCreationType::AnimationGraph ? "Animation Graph" :
			pendingAssetCreationType_ == PendingAssetCreationType::Scene ? "Scene" :
			"Asset";
		ImGui::Text("Asset Type: %s", assetTypeLabel);

		if (shouldFocusCreationNameInput_)
		{
			ImGui::SetKeyboardFocusHere();
			shouldFocusCreationNameInput_ = false;
		}

		const bool isNameSubmitted = ImGui::InputText("Asset Name", creationNameBuffer_, sizeof(creationNameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue);
		if (isNameSubmitted || ImGui::Button("OK", ImVec2(120, 0)))
		{
			FinalizeAssetCreation();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			isCreatingAsset_ = false;
			pendingCreationDirectory_.clear();
			shouldFocusCreationNameInput_ = false;
			pendingAssetCreationType_ = PendingAssetCreationType::None;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	DrawSaveChangesPrompt();
	UpdateCMakeRebuildResult();

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

	ImGui::SetNextItemOpen(folder->isOpen, ImGuiCond_Always);

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (currentFolder_ == folder) flags |= ImGuiTreeNodeFlags_Selected;
	if (folder->subFolders.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

	bool isOpen = ImGui::TreeNodeEx(folder->name.c_str(), flags);
	if (ImGui::IsItemToggledOpen())
	{
		folder->isOpen = isOpen;
	}

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		SetCurrentFolder(folder);
	}

	if (hud_->WasLastItemDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
	{
		folder->isOpen = !folder->isOpen;
		SetCurrentFolder(folder);
		isOpen = folder->isOpen;
	}

	// Interactions
	std::string folderFullPath = GetAbsoluteProjectPath(folder->path);
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

	Image* uiImage = EditorUtils::GetEditorContent<Image>("Textures/UI/T_UI.png");
	Texture* uiTexture = uiImage ? uiImage->GetGeneratedTexture() : nullptr;
	if (!uiTexture)
	{
		return;
	}

	ImTextureID atlasID = (ImTextureID)(intptr_t)uiTexture->GetRendererTextureId();

	auto GetUV0 = [&](float x, float y) { return RemapTextureUV(uiTexture, ImVec2(x / atlasSize, y / atlasSize)); };
	auto GetUV1 = [&](float x, float y) { return RemapTextureUV(uiTexture, ImVec2((x + spriteSize) / atlasSize, (y + spriteSize) / atlasSize)); };

	if (ImGui::BeginTable("FileGrid", columns))
	{
		for (Folder* sub : currentFolder_->subFolders)
		{
			ImGui::TableNextColumn();
			ImGui::PushID(sub->name.c_str());

			ImVec2 fUv0 = GetUV0(0.0f, 128.0f);
			ImVec2 fUv1 = GetUV1(0.0f, 128.0f);

			ImGui::ImageButton("##folder", atlasID, { thumbnailSize_, thumbnailSize_ }, fUv0, fUv1);
			if (hud_->WasLastItemDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
			{
				SetCurrentFolder(sub);
			}

			// Interactions
			std::string subFullPath = GetAbsoluteProjectPath(sub->path);
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
				if (IsHeaderFile(file))
				{
					uv0 = GetUV0(766.f, 0.0f);
					uv1 = GetUV1(766.f, 0.0f);
				}
				else if (IsSourceFile(file))
				{
					uv0 = GetUV0(896.f, 0.0f);
					uv1 = GetUV1(896.f, 0.0f);
				}
			}

			ImGui::ImageButton("##file", atlasID, { thumbnailSize_, thumbnailSize_ }, uv0, uv1);
			if (hud_->WasLastItemDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
			{
				std::string fileFullPath = GetAbsoluteProjectPath(currentFolder_->path + file);
				const std::string assetFileType = TryGetGameAssetFileType(fileFullPath);
				const EditorAssetType assetType = EditorContext::Get()->GetAssetType(currentFolder_->path + file);
				if (assetFileType == "Scene" || assetType == EditorAssetType::Scene)
				{
					RequestOpenScene(fileFullPath);
				}
				else if (!assetFileType.empty())
				{
					OpenAssetFile(fileFullPath);
				}
				else if (resourceType == ResourceType::Image)
				{
					int idx = 0;
					while (Image* img = engine->GetResourceManager()->GetResourceContainer()->GetImage(idx++))
					{
						if (img->GetPath().find(file) != std::string::npos)
						{
							ImageViewerPanel* viewer = (ImageViewerPanel*)hud_->GetPanel<ImageViewerPanel>();
							if (viewer)
							{
								viewer->SetTargetImage(img);
								viewer->SetIsOpen(true);
							}
							break;
						}
					}
				}
				else if (resourceType == ResourceType::Model)
				{
					const std::string contentRelativePath = EditorAssetPathUtils::ToContentRelativePath(fileFullPath);
					if (SkeletalMesh* skeletalMesh = engine->GetResourceManager()->GetContent<SkeletalMesh>(contentRelativePath))
					{
						SkeletalMeshViewerPanel* viewer = (SkeletalMeshViewerPanel*)hud_->GetPanel<SkeletalMeshViewerPanel>();
						if (viewer)
						{
							viewer->SetTargetSkeletalMesh(skeletalMesh);
							viewer->SetIsOpen(true);
						}
					}
					else if (StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>(contentRelativePath))
					{
						MeshViewerPanel* viewer = (MeshViewerPanel*)hud_->GetPanel<MeshViewerPanel>();
						if (viewer)
						{
							viewer->SetTargetStaticMesh(staticMesh);
							viewer->SetIsOpen(true);
						}
					}
				}
				else
				{
					if (IsSourceCodeFile(file))
					{
						EditorSourceCodeUtils::OpenSourceFile(fileFullPath);
					}
				}
			}

			// Interactions
			std::string fileFullPath = GetAbsoluteProjectPath(currentFolder_->path + file);
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

		ImGui::Text("Dragging: %s", std::filesystem::path(cleanPath).filename().string().c_str());
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
	if (ImGui::BeginPopupContextItem(itemPath.c_str()))
	{
		if (isFolder)
		{
			DrawCreateContentMenu(itemPath);
			ImGui::Separator();
		}

		if (ImGui::MenuItem("Show in Explorer"))
		{
			EditorSourceCodeUtils::ShowInFileExplorer(itemPath, true);
		}
		ImGui::Separator();

		if (ImGui::MenuItem("Rename"))
		{
			isRenaming_ = true;
			selectedItemForMenu_ = itemPath;
			strncpy(renameBuffer_, itemName.c_str(), sizeof(renameBuffer_) - 1);
			renameBuffer_[sizeof(renameBuffer_) - 1] = '\0';
		}
		if (ImGui::MenuItem("Duplicate"))
		{
			DuplicateFileSystemItem(itemPath);
		}
		if (ImGui::MenuItem("Delete"))
		{
			try {
				std::string cleanPath = itemPath;
				if (!cleanPath.empty() && cleanPath.back() == '/') cleanPath.pop_back();
				const bool shouldRebuildCMakeFiles = IsSourceDirectory(cleanPath);

				// 1. Delete primary content
				std::filesystem::remove_all(cleanPath);

				// 2. Delete editor reflection file/folder
				std::string reflectionPath = EditorAssetPathUtils::ToEditorReflectionPath(cleanPath);
				if (std::filesystem::exists(reflectionPath))
				{
					std::filesystem::remove_all(reflectionPath);
				}

				needsRefresh_ = true;
				if (shouldRebuildCMakeFiles)
				{
					RebuildCMakeFiles();
				}
			}
			catch (...) {}
		}
		ImGui::EndPopup();
	}
}

void FileBrowserPanel::DrawCreateContentMenu(const std::string& targetDirectory)
{
	const bool isContentDirectory = IsContentDirectory(targetDirectory);

	if (isContentDirectory && ImGui::MenuItem("Create Folder"))
	{
		isCreatingFolder_ = true;
		isCreatingAsset_ = false;
		pendingCreationDirectory_ = EnsureTrailingSlash(targetDirectory);
		shouldFocusCreationNameInput_ = true;
		creationNameBuffer_[0] = '\0';
	}

	if (isContentDirectory && ImGui::BeginMenu("Create Asset"))
	{
		if (ImGui::MenuItem("Material"))
		{
			isCreatingFolder_ = false;
			isCreatingAsset_ = true;
			pendingCreationDirectory_ = EnsureTrailingSlash(targetDirectory);
			pendingAssetCreationType_ = PendingAssetCreationType::Material;
			shouldFocusCreationNameInput_ = true;
			creationNameBuffer_[0] = '\0';
		}

		if (ImGui::MenuItem("Material Function"))
		{
			isCreatingFolder_ = false;
			isCreatingAsset_ = true;
			pendingCreationDirectory_ = EnsureTrailingSlash(targetDirectory);
			pendingAssetCreationType_ = PendingAssetCreationType::MaterialFunction;
			shouldFocusCreationNameInput_ = true;
			creationNameBuffer_[0] = '\0';
		}

		if (ImGui::MenuItem("Animation Graph"))
		{
			isCreatingFolder_ = false;
			isCreatingAsset_ = true;
			pendingCreationDirectory_ = EnsureTrailingSlash(targetDirectory);
			pendingAssetCreationType_ = PendingAssetCreationType::AnimationGraph;
			shouldFocusCreationNameInput_ = true;
			creationNameBuffer_[0] = '\0';
		}

		if (ImGui::MenuItem("Scene"))
		{
			isCreatingFolder_ = false;
			isCreatingAsset_ = true;
			pendingCreationDirectory_ = EnsureTrailingSlash(targetDirectory);
			pendingAssetCreationType_ = PendingAssetCreationType::Scene;
			shouldFocusCreationNameInput_ = true;
			creationNameBuffer_[0] = '\0';
		}

		ImGui::EndMenu();
	}

	if (!targetDirectory.empty())
	{
		if (isContentDirectory)
		{
			ImGui::Separator();
		}

		if (ImGui::MenuItem("Create a class"))
		{
			OpenClassCreationPanel(IsSourceDirectory(targetDirectory) ? targetDirectory : "");
		}
	}
}

void FileBrowserPanel::OpenClassCreationPanel(const std::string& initialDirectory)
{
	if (ClassCreationPanel* classCreationPanel = hud_->GetPanel<ClassCreationPanel>())
	{
		classCreationPanel->Open(initialDirectory);
		hud_->ShowPanel<ClassCreationPanel>();
	}
}

void FileBrowserPanel::OpenAssetFile(const std::string& filePath)
{
	const std::string fileType = TryGetGameAssetFileType(filePath);
	if (fileType == "Material")
	{
		if (ShaderEditorPanel* shaderEditorPanel = hud_->GetPanel<ShaderEditorPanel>())
		{
			shaderEditorPanel->OnMaterialOpened(filePath);
			shaderEditorPanel->SetIsOpen(true);
		}
	}
	else if (fileType == "MaterialFunction")
	{
		if (ShaderEditorPanel* shaderEditorPanel = hud_->GetPanel<ShaderEditorPanel>())
		{
			shaderEditorPanel->OnMaterialFunctionOpened(filePath);
			shaderEditorPanel->SetIsOpen(true);
		}
	}
	else if (fileType == "AnimationGraph")
	{
		if (AnimationGraphPanel* animationGraphPanel = hud_->GetPanel<AnimationGraphPanel>())
		{
			animationGraphPanel->OpenAnimationGraph(filePath);
			animationGraphPanel->SetIsOpen(true);
		}
	}
	else if (fileType == "Scene")
	{
		RequestOpenScene(filePath);
	}
}

void FileBrowserPanel::RequestOpenScene(const std::string& filePath)
{
	pendingSceneToOpen_ = ContentPathUtils::NormalizePath(EditorAssetPathUtils::ToContentRelativePath(filePath));
	if (pendingSceneToOpen_.empty())
	{
		return;
	}

	EditorContext* context = EditorContext::Get();
	if (pendingSceneToOpen_ == ContentPathUtils::NormalizePath(context->sceneSavePath))
	{
		pendingSceneToOpen_.clear();
		return;
	}

	if (context->GetIsSceneDirty() && pendingSceneToOpen_ != context->sceneSavePath)
	{
		shouldOpenSaveChangesPopup_ = true;
		return;
	}

	OpenPendingScene();
}

void FileBrowserPanel::DrawSaveChangesPrompt()
{
	if (shouldOpenSaveChangesPopup_)
	{
		ImGui::OpenPopup("Save changes?");
		shouldOpenSaveChangesPopup_ = false;
	}

	if (ImGui::BeginPopupModal("Save changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("Save changes to the current scene before opening another scene?");

		if (ImGui::Button("Yes", ImVec2(120, 0)))
		{
			if (SaveCurrentScene())
			{
				OpenPendingScene();
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("No", ImVec2(120, 0)))
		{
			OpenPendingScene();
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			pendingSceneToOpen_.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void FileBrowserPanel::OpenPendingScene()
{
	if (pendingSceneToOpen_.empty())
	{
		return;
	}

	if (engine->GetApplication()->OpenScene(pendingSceneToOpen_))
	{
		EditorContext* context = EditorContext::Get();
		context->sceneSavePath = pendingSceneToOpen_;
		context->ClearSceneDirty();
		context->ClearSelection();
	}

	pendingSceneToOpen_.clear();
}

bool FileBrowserPanel::SaveCurrentScene()
{
	EditorContext* context = EditorContext::Get();
	if (context->sceneSavePath.empty())
	{
		return false;
	}

	hud_->PrepareSceneForSave();
	SceneParser::SaveScene(engine->GetApplication()->GetMainScene(), ContentDir + context->sceneSavePath);
	context->ClearSceneDirty();
	return true;
}

void FileBrowserPanel::SetCurrentFolder(Folder* folder)
{
	currentFolder_ = folder;
	currentFolderPath_ = currentFolder_ ? currentFolder_->path : "";
}

void FileBrowserPanel::RestoreCurrentFolderFromPath()
{
	EditorContext* context = EditorContext::Get();
	Folder* restoredFolder = context->rootFolder;

	if (!currentFolderPath_.empty())
	{
		auto folderIterator = context->folderMap.find(currentFolderPath_);
		if (folderIterator != context->folderMap.end())
		{
			restoredFolder = folderIterator->second;
		}
	}

	SetCurrentFolder(restoredFolder);
	observedFileTreeVersion_ = context->GetFileTreeVersion();
}

bool FileBrowserPanel::IsKnownFolder(Folder* folder) const
{
	const EditorContext* context = EditorContext::Get();
	if (!folder)
	{
		return false;
	}

	if (folder == context->rootFolder)
	{
		return true;
	}

	for (const auto& folderPair : context->folderMap)
	{
		if (folderPair.second == folder)
		{
			return true;
		}
	}

	return false;
}

void FileBrowserPanel::SynchronizeCurrentFolder()
{
	const EditorContext* context = EditorContext::Get();
	if (observedFileTreeVersion_ != context->GetFileTreeVersion() || !IsKnownFolder(currentFolder_))
	{
		RestoreCurrentFolderFromPath();
	}

	if (!currentFolder_)
	{
		SetCurrentFolder(EditorContext::Get()->rootFolder);
	}
}

void FileBrowserPanel::UpdateCMakeRebuildResult()
{
	if (asyncCMakeRebuildResult_.valid() &&
		asyncCMakeRebuildResult_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		asyncCMakeRebuildResult_.get();
	}
}

void FileBrowserPanel::RebuildCMakeFiles()
{
	UpdateCMakeRebuildResult();
	if (asyncCMakeRebuildResult_.valid())
	{
		return;
	}

	const std::string projectRoot = EditorAssetPathUtils::GetProjectRootPath();
	std::string command;
#ifdef GOKNAR_PLATFORM_WINDOWS
	command = "pushd " + QuoteForShell(projectRoot) + " && Build.sh nobuild";
#else
	command = "cd " + QuoteForShell(projectRoot) + " && ./Build.sh nobuild";
#endif

	asyncCMakeRebuildResult_ = std::async(std::launch::async,
		[command]()
		{
			std::system(command.c_str());
		});
}

void FileBrowserPanel::DuplicateFileSystemItem(const std::string& source)
{
	try
	{
		std::string cleanSource = source;
		if (!cleanSource.empty() && cleanSource.back() == '/')
		{
			cleanSource.pop_back();
		}

		const std::filesystem::path sourcePath(cleanSource);
		if (!std::filesystem::exists(sourcePath))
		{
			return;
		}

		const std::filesystem::path destinationPath = GetDuplicatePath(sourcePath);
		if (!CopyFileSystemItem(sourcePath, destinationPath))
		{
			return;
		}

		const std::string sourceReflection = EditorAssetPathUtils::ToEditorReflectionPath(sourcePath.generic_string());
		if (std::filesystem::exists(sourceReflection))
		{
			const std::string destinationReflection = EditorAssetPathUtils::ToEditorReflectionPath(destinationPath.generic_string());
			const std::filesystem::path destinationReflectionPath(destinationReflection);
			if (!std::filesystem::exists(destinationReflectionPath))
			{
				CopyFileSystemItem(sourceReflection, destinationReflectionPath);
			}
		}

		needsRefresh_ = true;
		if (IsSourceDirectory(sourcePath.generic_string()))
		{
			RebuildCMakeFiles();
		}
	}
	catch (...)
	{
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
			// 1. Move primary content
			std::filesystem::rename(srcPath, destPath);

			// 2. Move editor reflection file/folder
			std::string srcReflection = EditorAssetPathUtils::ToEditorReflectionPath(srcPath.generic_string());
			std::string destReflection = EditorAssetPathUtils::ToEditorReflectionPath(destPath.generic_string());

			if (std::filesystem::exists(srcReflection))
			{
				EditorAssetPathUtils::EnsureDirectoryForFile(destReflection);
				std::filesystem::rename(srcReflection, destReflection);
			}

			needsRefresh_ = true;
		}
	}
	catch (...) {}
}

void FileBrowserPanel::RefreshAndRestore()
{
	const std::string currentPathToRestore = currentFolderPath_;

	EditorContext::Get()->BuildFileTree();

	if (!currentPathToRestore.empty())
	{
		auto it = EditorContext::Get()->folderMap.find(currentPathToRestore);
		if (it != EditorContext::Get()->folderMap.end())
		{
			SetCurrentFolder(it->second);
			observedFileTreeVersion_ = EditorContext::Get()->GetFileTreeVersion();
			return;
		}
	}
	SetCurrentFolder(EditorContext::Get()->rootFolder);
	observedFileTreeVersion_ = EditorContext::Get()->GetFileTreeVersion();
}

void FileBrowserPanel::FinalizeFolderCreation()
{
	try
	{
		if (!pendingCreationDirectory_.empty() && creationNameBuffer_[0] != '\0')
		{
			const std::filesystem::path newFolderPath = std::filesystem::path(pendingCreationDirectory_) / creationNameBuffer_;
			if (!std::filesystem::exists(newFolderPath))
			{
				// 1. Create content directory
				std::filesystem::create_directories(newFolderPath);

				// 2. Create editor reflection directory
				std::string reflectionDir = EditorAssetPathUtils::ToEditorReflectionPath(newFolderPath.generic_string());
				std::filesystem::create_directories(reflectionDir);

				needsRefresh_ = true;
			}
		}
	}
	catch (...)
	{
	}

	isCreatingFolder_ = false;
	pendingCreationDirectory_.clear();
	shouldFocusCreationNameInput_ = false;
	creationNameBuffer_[0] = '\0';
}

void FileBrowserPanel::FinalizeAssetCreation()
{
	try
	{
		if (!pendingCreationDirectory_.empty() && creationNameBuffer_[0] != '\0')
		{
			const std::filesystem::path newAssetPath = std::filesystem::path(pendingCreationDirectory_) / creationNameBuffer_;
			if (!std::filesystem::exists(newAssetPath))
			{
				std::filesystem::create_directories(newAssetPath.parent_path());

				// Ensure editor reflection directory exists for this new asset
				std::string reflectionPath = EditorAssetPathUtils::ToEditorReflectionPath(newAssetPath.generic_string());
				EditorAssetPathUtils::EnsureDirectoryForFile(reflectionPath);

				bool isSaved = false;
				if (pendingAssetCreationType_ == PendingAssetCreationType::Material)
				{
					isSaved = WriteMaterialAssetFile(newAssetPath.generic_string());
				}
				else if (pendingAssetCreationType_ == PendingAssetCreationType::MaterialFunction)
				{
					isSaved = WriteMaterialFunctionAssetFile(newAssetPath.generic_string());
				}
				else if (pendingAssetCreationType_ == PendingAssetCreationType::AnimationGraph)
				{
					isSaved = WriteAnimationGraphAssetFile(newAssetPath.generic_string());
				}
				else if (pendingAssetCreationType_ == PendingAssetCreationType::Scene)
				{
					isSaved = WriteSceneAssetFile(newAssetPath.generic_string());
				}

				if (isSaved)
				{
					needsRefresh_ = true;
				}
			}
		}
	}
	catch (...)
	{
	}

	isCreatingAsset_ = false;
	pendingCreationDirectory_.clear();
	shouldFocusCreationNameInput_ = false;
	pendingAssetCreationType_ = PendingAssetCreationType::None;
	creationNameBuffer_[0] = '\0';
}
