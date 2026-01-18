#include "EditorWidgets.h"

#include "imgui.h"

#include "Goknar/Math/GoknarMath.h"
#include "Goknar/Math/Quaternion.h"

#include "EditorUtils.h"

void EditorWidgets::DrawInputText(const std::string& name, std::string& value)
{
	char* valueChar = const_cast<char*>(value.c_str());
	ImGui::InputText(name.c_str(), valueChar, 64);
	value = std::string(valueChar);
}

void EditorWidgets::DrawInputInt(const std::string& name, int& value)
{
	int newValue = value;
	ImGui::InputInt((name).c_str(), &newValue);
	value = newValue;
}

void EditorWidgets::DrawInputFloat(const std::string& name, float& value)
{
	float newValue = value;
	ImGui::InputFloat((name).c_str(), &newValue);
	if (SMALLER_EPSILON < GoknarMath::Abs(value - newValue))
	{
		value = newValue;
	}
}

void EditorWidgets::DrawInputVector3(const std::string& name, Vector3& vector)
{
	float newValueX = vector.x;
	ImGui::InputFloat((name + "X").c_str(), &newValueX);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.x - newValueX))
	{
		vector.x = newValueX;
	}

	ImGui::SameLine();

	float newValueY = vector.y;
	ImGui::InputFloat((name + "Y").c_str(), &newValueY);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.y - newValueY))
	{
		vector.y = newValueY;
	}

	ImGui::SameLine();

	float newValueZ = vector.z;
	ImGui::InputFloat((name + "Z").c_str(), &newValueZ);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.z - newValueZ))
	{
		vector.z = newValueZ;
	}
}

void EditorWidgets::DrawInputQuaternion(const  std::string& name, Quaternion& quaternion)
{
	float newQuaternionX = quaternion.x;
	ImGui::InputFloat((name + "X").c_str(), &newQuaternionX);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.x - newQuaternionX))
	{
		quaternion.x = newQuaternionX;
	}

	ImGui::SameLine();

	float newQuaternionY = quaternion.y;
	ImGui::InputFloat((name + "Y").c_str(), &newQuaternionY);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.y - newQuaternionY))
	{
		quaternion.y = newQuaternionY;
	}

	ImGui::SameLine();

	float newQuaternionZ = quaternion.z;
	ImGui::InputFloat((name + "Z").c_str(), &newQuaternionZ);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.z - newQuaternionZ))
	{
		quaternion.z = newQuaternionZ;
	}

	ImGui::SameLine();

	float newQuaternionW = quaternion.w;
	ImGui::InputFloat((name + "W").c_str(), &newQuaternionW, 64);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.w - newQuaternionW))
	{
		quaternion.w = newQuaternionW;
	}
}

void EditorWidgets::DrawCheckbox(const std::string& name, bool& value)
{
	ImGui::Checkbox(name.c_str(), &value);
}

bool EditorWidgets::DrawWindowWithOneTextBoxOneButton(const std::string& windowTitle, const std::string& text, const std::string& currentValue, const std::string& buttonText, const Vector2i& size, std::string& resultText, bool& isOpen, ImGuiWindowFlags flags)
{
	ImGui::SetNextWindowPos(EditorUtils::ToImVec2((EditorContext::Get()->windowSize - size) * 0.5f));
	ImGui::SetNextWindowSize(EditorUtils::ToImVec2(size));

	flags |= ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize;

	ImGui::Begin(windowTitle.c_str(), &isOpen, flags);

	ImGui::Text(text.c_str());
	ImGui::SameLine();

	char* input = const_cast<char*>(currentValue.c_str());
	ImGui::InputText((std::string("##") + windowTitle + "_TextBox").c_str(), input, 1024);

	std::string inputString = input;
	bool buttonClicked = ImGui::Button(buttonText.c_str());
	buttonClicked = buttonClicked && !inputString.empty();

	if (buttonClicked)
	{
		resultText = inputString;
	}

	ImGui::End();

	return buttonClicked;
}

void EditorWidgets::DrawFileGrid(const Folder* folder, std::string& selectedFileName, bool& isAFileSelected)
{
	for (const Folder* subFolder : folder->subFolders)
	{
		if (ImGui::TreeNode(subFolder->name.c_str()))
		{
			DrawFileGrid(subFolder, selectedFileName, isAFileSelected);
			ImGui::TreePop();
		}
	}

	ImGui::Columns(4, nullptr, false);
	int fileCount = folder->files.size();
	for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
	{
		std::string fileName = folder->files[fileIndex];

		if (ImGui::Button(fileName.c_str(), { 150.f, 30.f }))
		{
			selectedFileName = folder->path + fileName;
			isAFileSelected = true;
		}

		ImGui::NextColumn();
	}
	ImGui::Columns(1, nullptr, false);
}

