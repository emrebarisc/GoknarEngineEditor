#pragma once

#include "Goknar/Core.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "Goknar/Math/GoknarMath.h"

struct ImGuiContext;

class FreeCameraObject;
class ObjectBase;
class RenderTarget;

enum class EditorSelectionType
{
	None = 0,
	Object,
	DirectionalLight,
	PointLight,
	SpotLight
};

enum class EditorAssetType
{
	None = 0,
	StaticMeshComponent,
	SkeletalMeshComponent,
	Image,
	Audio
};

struct Folder
{
	std::string name;
	std::string path;
	std::vector<Folder*> subFolders;
	std::vector<std::string> files;
	bool isOpen = false;
};

class GOKNAR_API EditorContext
{
public:
	static EditorContext* Get()
	{
		if (!instance_)
		{
			instance_ = new EditorContext();
			instance_->Init();
		}

		return instance_;
	}

	~EditorContext();

	void Init();
	void BuildFileTree();

	void SetSelection(void* obj, EditorSelectionType type)
	{
		selectedObject = obj;
		selectedObjectType = type;
	}

	void ClearSelection()
	{
		selectedObject = nullptr;
		selectedObjectType = EditorSelectionType::None;
	}

	void ClearCreateData()
	{
		objectToCreateType = EditorSelectionType::None;
		objectToCreateName = "";
		isPlacingObject = false;
	}

	template <typename T>
	T* GetSelectionAs()
	{
		return static_cast<T*>(selectedObject);
	}

	void SetCameraMovement(bool value);

	std::string objectToCreateName;
	std::string sceneSavePath;

	Vector2i windowSize;
	Vector2i buttonSize;

	void* selectedObject = nullptr;

	FreeCameraObject* viewportCameraObject{ nullptr };
	RenderTarget* viewportRenderTarget{ nullptr };

	ImGuiContext* imguiContext_{ nullptr };

	EditorSelectionType selectedObjectType = EditorSelectionType::None;
	EditorSelectionType objectToCreateType = EditorSelectionType::None;

	Folder* rootFolder{ nullptr };
	std::unordered_map<std::string, Folder*> folderMap{};

	bool isPlacingObject = false;
private:
	EditorContext();

	inline static EditorContext* instance_{ nullptr };
};