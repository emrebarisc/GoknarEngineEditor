#include "ObjectCreationPanel.h"
#include "UI/EditorContext.h"
#include "Goknar/Factories/DynamicObjectFactory.h"
#include "imgui.h"

void ObjectCreationPanel::Draw()
{
    ImGui::Begin(title_.c_str(), &isOpen_);

    if (ImGui::TreeNode("Basic Lights"))
    {
        if (ImGui::Button("Directional Light")) SetObjectToCreate(EditorSelectionType::DirectionalLight);
        if (ImGui::Button("Point Light"))       SetObjectToCreate(EditorSelectionType::PointLight);
        if (ImGui::Button("Spot Light"))        SetObjectToCreate(EditorSelectionType::SpotLight);
        ImGui::TreePop();
    }

    ImGui::Separator();

    if (ImGui::TreeNode("Engine Objects"))
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

    // Status text for what is currently "armed" to be placed in the viewport
    auto context = EditorContext::Get();
    if (context->isPlacingObject)
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Placement Mode Active");
        ImGui::Text("Click in Viewport to place: %s", 
            context->objectToCreateName.empty() ? "Light" : context->objectToCreateName.c_str());
        
        if (ImGui::Button("Cancel (ESC)"))
        {
            context->isPlacingObject = false;
        }
    }

    ImGui::End();
}

void ObjectCreationPanel::SetObjectToCreate(EditorSelectionType type, const std::string& name)
{
    auto context = EditorContext::Get();
    context->isPlacingObject = true;
    context->selectedObjectType = type; // The type we WANT to create
    context->objectToCreateName = name;
}