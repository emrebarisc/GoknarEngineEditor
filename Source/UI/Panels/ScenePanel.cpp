#include "ScenePanel.h"
#include "UI/EditorContext.h"
#include "Goknar/Engine.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Scene.h"
#include "Goknar/Application.h"
#include "Goknar/Lights/DirectionalLight.h"
#include "Goknar/Lights/PointLight.h"
#include "Goknar/Lights/SpotLight.h"
#include "UI/EditorHUD.h"
#include "imgui.h"

void ScenePanel::Draw()
{
    ImGui::Begin(title_.c_str(), &isOpen_);

    DrawSceneLights();
    DrawSceneReferences();
    DrawSceneObjects();

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PAYLOAD"))
        {
            const std::string sourcePath = static_cast<const char*>(payload->Data);
            hud_->InsertSceneReference(sourcePath, Vector3::ZeroVector);
        }

        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}

void ScenePanel::DrawSceneLights()
{
    if (ImGui::TreeNode("Lights"))
    {
        Scene* currentScene = engine->GetApplication()->GetMainScene();
        auto context = EditorContext::Get();

        const std::vector<DirectionalLight*>& directionalLights = currentScene->GetDirectionalLights();
        for (int i = 0; i < directionalLights.size(); ++i)
        {
            DirectionalLight* directionalLight = directionalLights[i];

            if (directionalLight->GetName().find("__Editor__") != std::string::npos ||
                currentScene->GetIsDirectionalLightFromReferencedScene(directionalLight))
            {
                continue;
            }

            std::string label = "Directional Light " + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), context->selectedObject == directionalLight))
            {
                context->SetSelection(directionalLight, EditorSelectionType::DirectionalLight);
            }
        }

        const std::vector<PointLight*>& pointLights = currentScene->GetPointLights();
        for (int i = 0; i < pointLights.size(); ++i)
        {
            PointLight* pointLight = pointLights[i];

            if (pointLight->GetName().find("__Editor__") != std::string::npos ||
                currentScene->GetIsPointLightFromReferencedScene(pointLight))
            {
                continue;
            }

            std::string label = "Point Light " + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), context->selectedObject == pointLight))
            {
                context->SetSelection(pointLight, EditorSelectionType::PointLight);
            }
        }

        const std::vector<SpotLight*>& spotLights = currentScene->GetSpotLights();
        for (int i = 0; i < spotLights.size(); ++i)
        {
            SpotLight* spotLight = spotLights[i];

            if (spotLight->GetName().find("__Editor__") != std::string::npos ||
                currentScene->GetIsSpotLightFromReferencedScene(spotLight))
            {
                continue;
            }

            std::string label = "Spot Light " + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), context->selectedObject == spotLight))
            {
                context->SetSelection(spotLight, EditorSelectionType::SpotLight);
            }
        }

        ImGui::TreePop();
    }
}

void ScenePanel::DrawSceneReferences()
{
    Scene* currentScene = engine->GetApplication()->GetMainScene();
    if (!currentScene || currentScene->GetSceneReferences().empty())
    {
        return;
    }

    if (ImGui::TreeNode("Scenes"))
    {
        for (const SceneReference& sceneReference : currentScene->GetSceneReferences())
        {
            if (sceneReference.sceneRootObject)
            {
                DrawSceneObject(sceneReference.sceneRootObject);
            }
        }

        ImGui::TreePop();
    }
}

void ScenePanel::DrawSceneObjects()
{
    if (ImGui::TreeNode("Objects"))
    {
        Scene* currentScene = engine->GetApplication()->GetMainScene();
        const std::vector<ObjectBase*>& objects = currentScene->GetObjects();
        for (ObjectBase* object : objects)
        {
            if (object->GetName().find("__Editor__") != std::string::npos ||
                currentScene->GetIsObjectFromReferencedScene(object))
            {
                continue;
            }

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
    if (!hasChildren)
    {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

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
