#include "ObjectCreationPanel.h"
#include "imgui.h"

#include "Goknar/Factories/DynamicObjectFactory.h"

#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/Panels/ObjectNameToCreatePanel.h"

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
