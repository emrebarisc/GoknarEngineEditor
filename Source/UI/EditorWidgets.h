#pragma once

#include <string>

#include "imgui.h"

#include "UI/EditorContext.h"
#include "Goknar/Math/GoknarMath.h"

struct Vector3;
class Quaternion;

struct Folder;

class EditorWidgets
{
public:
	static bool DrawInputText(const std::string& name, std::string& value);
	static bool DrawInputInt(const std::string& name, int& value);
	static bool DrawInputFloat(const std::string& name, float& value);
	static bool DrawInputVector3(const std::string& name, Vector3& vector);
	static bool DrawInputQuaternion(const  std::string& name, Quaternion& quaternion);
	static bool DrawCheckbox(const std::string& name, bool& value);

	static bool DrawWindowWithOneTextBoxOneButton(
		const std::string& windowTitle,
		const std::string& text,
		const std::string& currentValue,
		const std::string& buttonText,
		const Vector2i& size,
		std::string& resultText,
		bool& isOpen,
		ImGuiWindowFlags flags);


	static void DrawFileGrid(const Folder* folder, std::string& selectedFileName, bool& isAFileSelected, EditorAssetType filter = EditorAssetType::None);
};
