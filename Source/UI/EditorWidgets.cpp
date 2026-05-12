#include "EditorWidgets.h"

#include "imgui.h"

#include "Goknar/Math/GoknarMath.h"
#include "Goknar/Math/Quaternion.h"

#include "EditorUtils.h"
#include "EditorContext.h"

namespace
{
	bool FolderContainsFilteredAssets(const Folder* folder, EditorAssetType filter)
	{
		if (!folder || filter == EditorAssetType::None)
		{
			return true;
		}

		EditorContext* context = EditorContext::Get();
		for (const std::string& file : folder->files)
		{
			if (context->GetAssetType(folder->path + file) == filter)
			{
				return true;
			}
		}

		for (const Folder* subFolder : folder->subFolders)
		{
			if (FolderContainsFilteredAssets(subFolder, filter))
			{
				return true;
			}
		}

		return false;
	}
}

bool EditorWidgets::DrawInputText(const std::string& name, std::string& value)
{
	char* valueChar = const_cast<char*>(value.c_str());
	bool changed = ImGui::InputText(name.c_str(), valueChar, 64);
	value = std::string(valueChar);
	return changed;
}

bool EditorWidgets::DrawInputInt(const std::string& name, int& value)
{
	int newValue = value;
	bool changed = ImGui::InputInt((name).c_str(), &newValue);
	value = newValue;
	return changed;
}

bool EditorWidgets::DrawInputFloat(const std::string& name, float& value)
{
	float newValue = value;
	bool changed = ImGui::InputFloat((name).c_str(), &newValue);
	if (SMALLER_EPSILON < GoknarMath::Abs(value - newValue))
	{
		value = newValue;
		return true;
	}

	return changed;
}

bool EditorWidgets::DrawInputVector3(const std::string& name, Vector3& vector)
{
	bool changed = false;

	float newValueX = vector.x;
	changed |= ImGui::InputFloat((name + "X").c_str(), &newValueX);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.x - newValueX))
	{
		vector.x = newValueX;
		changed = true;
	}

	ImGui::SameLine();

	float newValueY = vector.y;
	changed |= ImGui::InputFloat((name + "Y").c_str(), &newValueY);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.y - newValueY))
	{
		vector.y = newValueY;
		changed = true;
	}

	ImGui::SameLine();

	float newValueZ = vector.z;
	changed |= ImGui::InputFloat((name + "Z").c_str(), &newValueZ);
	if (SMALLER_EPSILON < GoknarMath::Abs(vector.z - newValueZ))
	{
		vector.z = newValueZ;
		changed = true;
	}

	return changed;
}

bool EditorWidgets::DrawInputQuaternion(const  std::string& name, Quaternion& quaternion)
{
	bool changed = false;

	float newQuaternionX = quaternion.x;
	changed |= ImGui::InputFloat((name + "X").c_str(), &newQuaternionX);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.x - newQuaternionX))
	{
		quaternion.x = newQuaternionX;
		changed = true;
	}

	ImGui::SameLine();

	float newQuaternionY = quaternion.y;
	changed |= ImGui::InputFloat((name + "Y").c_str(), &newQuaternionY);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.y - newQuaternionY))
	{
		quaternion.y = newQuaternionY;
		changed = true;
	}

	ImGui::SameLine();

	float newQuaternionZ = quaternion.z;
	changed |= ImGui::InputFloat((name + "Z").c_str(), &newQuaternionZ);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.z - newQuaternionZ))
	{
		quaternion.z = newQuaternionZ;
		changed = true;
	}

	ImGui::SameLine();

	float newQuaternionW = quaternion.w;
	changed |= ImGui::InputFloat((name + "W").c_str(), &newQuaternionW, 64);
	if (SMALLER_EPSILON < GoknarMath::Abs(quaternion.w - newQuaternionW))
	{
		quaternion.w = newQuaternionW;
		changed = true;
	}

	return changed;
}

bool EditorWidgets::DrawCheckbox(const std::string& name, bool& value)
{
	return ImGui::Checkbox(name.c_str(), &value);
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

void EditorWidgets::DrawFileGrid(const Folder* folder, std::string& selectedFileName, bool& isAFileSelected, EditorAssetType filter)
{
	for (const Folder* subFolder : folder->subFolders)
	{
		if (!FolderContainsFilteredAssets(subFolder, filter))
		{
			continue;
		}

		if (ImGui::TreeNode(subFolder->name.c_str()))
		{
			DrawFileGrid(subFolder, selectedFileName, isAFileSelected, filter);
			ImGui::TreePop();
		}
	}

	ImGui::Columns(4, nullptr, false);
	EditorContext* context = EditorContext::Get();
	int fileCount = (int)folder->files.size();
	for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
	{
		std::string fileName = folder->files[fileIndex];
		if (filter != EditorAssetType::None && context->GetAssetType(folder->path + fileName) != filter)
		{
			continue;
		}

		if (ImGui::Button(fileName.c_str(), { 150.f, 30.f }))
		{
			selectedFileName = folder->path + fileName;
			isAFileSelected = true;
		}

		ImGui::NextColumn();
	}
	ImGui::Columns(1, nullptr, false);
}

