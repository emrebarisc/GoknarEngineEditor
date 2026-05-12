#include "ObjectCreationPanel.h"
#include "imgui.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#include "Goknar/Factories/DynamicObjectFactory.h"

#include "UI/EditorAssetPathUtils.h"
#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/Panels/ObjectNameToCreatePanel.h"

namespace
{
    std::vector<std::string> GetProjectScenePaths()
    {
        std::vector<std::string> scenePaths;
        EditorContext* context = EditorContext::Get();
        for (const auto& [assetPath, assetType] : context->assetTypeMap)
        {
            if (assetType == EditorAssetType::Scene)
            {
                scenePaths.push_back(EditorAssetPathUtils::ToContentRelativePath(assetPath));
            }
        }

        std::sort(scenePaths.begin(), scenePaths.end());
        return scenePaths;
    }
}

void ObjectCreationPanel::Draw()
{
    ImGui::Begin(title_.c_str(), &isOpen_);

    if (ImGui::TreeNode("Basic Lights"))
    {
        if (ImGui::Button("Directional Light"))
        {
            SetObjectToCreate(EditorSelectionType::DirectionalLight);
        }

        if (ImGui::Button("Point Light"))
        {
            SetObjectToCreate(EditorSelectionType::PointLight);
        }

        if (ImGui::Button("Spot Light"))
        {
            SetObjectToCreate(EditorSelectionType::SpotLight);
        }

        ImGui::TreePop();
    }

    ImGui::Separator();

    if (ImGui::TreeNode("Objects"))
    {
        const auto& objectMap = DynamicObjectFactory::GetInstance()->GetObjectMap();
        for (auto const& [name, factory] : objectMap)
        {
            if (ImGui::Button(name.c_str()))
            {
                SetObjectToCreate(EditorSelectionType::Object, name);
            }
        }
        ImGui::TreePop();
    }

    ImGui::Separator();

    if (ImGui::TreeNode("Scene"))
    {
        const std::vector<std::string> scenePaths = GetProjectScenePaths();
        if (scenePaths.empty())
        {
            ImGui::TextDisabled("No scenes found");
        }

        for (const std::string& scenePath : scenePaths)
        {
            ImGui::PushID(scenePath.c_str());
            const std::string label = std::filesystem::path(scenePath).filename().generic_string();
            if (ImGui::Button(label.c_str()))
            {
                SetObjectToCreate(EditorSelectionType::Scene, scenePath);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", scenePath.c_str());
            }
            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    auto context = EditorContext::Get();
    if (context->isPlacingObject)
    {
        hud_->ShowPanel<ObjectNameToCreatePanel>();
    }

    ImGui::End();
}

void ObjectCreationPanel::SetObjectToCreate(EditorSelectionType type, const std::string& name)
{
    auto context = EditorContext::Get();
    context->isPlacingObject = true;
    context->objectToCreateType = type;
    context->objectToCreateName = name;
}
