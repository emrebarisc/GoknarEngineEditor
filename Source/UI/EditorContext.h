#pragma once

#include "Goknar/Core.h"

#include <string>

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

class GOKNAR_API EditorContext
{
public:
	static EditorContext* Get()
	{
		if (!instance_)
		{
			instance_ = new EditorContext();
		}

		return instance_;
	}

	~EditorContext();

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

	bool isPlacingObject = false;
private:
	EditorContext();

	inline static EditorContext* instance_{ nullptr };
};