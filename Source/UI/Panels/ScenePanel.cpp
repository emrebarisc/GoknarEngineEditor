#include "ScenePanel.h"
#include "UI/EditorContext.h"
#include "Goknar/Engine.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Scene.h"
#include "Goknar/Application.h"
#include "Goknar/Lights/DirectionalLight.h"
#include "Goknar/Lights/PointLight.h"
#include "Goknar/Lights/SpotLight.h"
#include "imgui.h"

void ScenePanel::Draw()
{
    ImGui::Begin(title_.c_str(), &isOpen_);

    DrawSceneLights();
    DrawSceneObjects();

    ImGui::End();
}

void ScenePanel::DrawSceneLights()
{
    if (ImGui::TreeNode("Lights"))
    {
        Scene* currentScene = engine->GetApplication()->GetMainScene();
        auto context = EditorContext::Get();

        // Directional Lights
        const std::vector<DirectionalLight*>& directionalLights = currentScene->GetDirectionalLights();
        for (int i = 0; i < directionalLights.size(); ++i)
        {
            std::string label = "Directional Light " + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), context->selectedObject == directionalLights[i]))
            {
                context->SetSelection(directionalLights[i], EditorSelectionType::DirectionalLight);
            }
        }

        // Point Lights
        const std::vector<PointLight*>& pointLights = currentScene->GetPointLights();
        for (int i = 0; i < pointLights.size(); ++i)
        {
            std::string label = "Point Light " + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), context->selectedObject == pointLights[i]))
            {
                context->SetSelection(pointLights[i], EditorSelectionType::PointLight);
            }
        }

        // Spot Lights
        const std::vector<SpotLight*>& spotLights = currentScene->GetSpotLights();
        for (int i = 0; i < spotLights.size(); ++i)
        {
            std::string label = "Spot Light " + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), context->selectedObject == spotLights[i]))
            {
                context->SetSelection(spotLights[i], EditorSelectionType::SpotLight);
            }
        }

        ImGui::TreePop();
    }
}

void ScenePanel::DrawSceneObjects()
{
    if (ImGui::TreeNode("Objects"))
    {
        const std::vector<ObjectBase*>& objects = engine->GetObjectsOfType<ObjectBase>();
        for (ObjectBase* object : objects)
        {
            if (object->GetParent() == nullptr)
            {
                DrawSceneObject(object);
            }
        }
        ImGui::TreePop();
    }
}

void ScenePanel::DrawSceneObject(ObjectBase* object)
{
    EditorContext* context = EditorContext::Get();
    ImGuiTreeNodeFlags flags = (context->selectedObject == object ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
    
    bool hasChildren = !object->GetChildren().empty();
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;

    bool opened = ImGui::TreeNodeEx((void*)object, flags, "%s", object->GetName().c_str());
    
    if (ImGui::IsItemClicked())
    {
        context->SetSelection(object, EditorSelectionType::Object);
    }

    if (opened)
    {
        for (ObjectBase* child : object->GetChildren())
        {
            DrawSceneObject(child);
        }
        ImGui::TreePop();
    }
}