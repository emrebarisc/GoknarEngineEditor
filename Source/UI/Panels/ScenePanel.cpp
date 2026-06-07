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

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <iterator>

namespace
{
    constexpr const char* SceneObjectDragDropPayload = "SCENE_OBJECT_PAYLOAD";

    std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    }
}

void ScenePanel::Draw()
{
    ImGui::Begin(title_.c_str(), &isOpen_);

    DrawSearchBox();
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

void ScenePanel::DrawSearchBox()
{
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##ScenePanelSearch", "Search", searchBuffer_, sizeof(searchBuffer_));
}

void ScenePanel::DrawSceneLights()
{
    if (IsSearchActive())
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

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
            if (!ValueMatchesSearch(label) && !ValueMatchesSearch(directionalLight->GetName()))
            {
                continue;
            }

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
            if (!ValueMatchesSearch(label) && !ValueMatchesSearch(pointLight->GetName()))
            {
                continue;
            }

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
            if (!ValueMatchesSearch(label) && !ValueMatchesSearch(spotLight->GetName()))
            {
                continue;
            }

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

    if (IsSearchActive())
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    if (ImGui::TreeNode("Scenes"))
    {
        for (const SceneReference& sceneReference : currentScene->GetSceneReferences())
        {
            if (sceneReference.sceneRootObject)
            {
                DrawSceneObject(sceneReference.sceneRootObject, false);
            }
        }

        ImGui::TreePop();
    }
}

void ScenePanel::DrawSceneObjects()
{
    if (IsSearchActive())
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    const bool opened = ImGui::TreeNode("Objects");
    HandleRootObjectDropTarget();

    if (opened)
    {
        Scene* currentScene = engine->GetApplication()->GetMainScene();
        const std::vector<ObjectBase*>& objects = currentScene->GetObjects();
        std::vector<ObjectBase*> rootObjects;
        for (ObjectBase* object : objects)
        {
            if (object->GetName().find("__Editor__") != std::string::npos ||
                currentScene->GetIsObjectFromReferencedScene(object))
            {
                continue;
            }

            if (object->GetParent() == nullptr)
            {
                rootObjects.push_back(object);
            }
        }

        for (ObjectBase* object : GetOrderedObjects(nullptr, rootObjects))
        {
            DrawSceneObject(object, true);
        }

        DrawRootObjectDropTarget();
        ImGui::TreePop();
    }
}

void ScenePanel::DrawSceneObject(ObjectBase* object, bool allowHierarchyEditing)
{
    if (!object || !ShouldDrawSceneObject(object))
    {
        return;
    }

    EditorContext* context = EditorContext::Get();
    ImGuiTreeNodeFlags flags = (context->IsObjectSelected(object) ? ImGuiTreeNodeFlags_Selected : 0) |
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;
    
    bool hasChildren = !object->GetChildren().empty();
    if (!hasChildren)
    {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (IsSearchActive())
    {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    if (allowHierarchyEditing)
    {
        DrawSceneObjectDropTarget(object, true);
    }

    bool opened = ImGui::TreeNodeEx((void*)object, flags, "%s", object->GetName().c_str());
    
    if (ImGui::IsItemClicked())
    {
        if (ImGui::GetIO().KeyCtrl)
        {
            context->RemoveObjectSelection(object);
        }
        else
        {
            context->SetObjectSelection(object, ImGui::GetIO().KeyShift);
        }
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && ImGui::GetIO().KeyCtrl)
    {
        context->RemoveObjectSelection(object);
    }

    if (allowHierarchyEditing)
    {
        HandleObjectDragDropSource(object);
        HandleObjectDragDropTarget(object);
    }

    if (opened)
    {
        const std::vector<ObjectBase*> children = GetOrderedObjects(object, object->GetChildren());
        for (ObjectBase* child : children)
        {
            DrawSceneObject(child, allowHierarchyEditing);
        }
        ImGui::TreePop();
    }

    if (allowHierarchyEditing)
    {
        DrawSceneObjectDropTarget(object, false);
    }
}

void ScenePanel::DrawSceneObjectDropTarget(ObjectBase* targetObject, bool insertBefore)
{
    if (!targetObject)
    {
        return;
    }

    ImGui::PushID(targetObject);
    ImGui::PushID(insertBefore ? "InsertBefore" : "InsertAfter");

    const float targetWidth = std::max(ImGui::GetContentRegionAvail().x, 1.f);
    const float targetHeight = 1.f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.f));
    ImGui::InvisibleButton("##SceneObjectOrderDropTarget", ImVec2(targetWidth, targetHeight));
    ImGui::PopStyleVar();

    const bool isHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (isHovered)
    {
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(min.x, (min.y + max.y) * 0.5f),
            ImVec2(max.x, (min.y + max.y) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_DragDropTarget),
            2.f);
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(SceneObjectDragDropPayload))
        {
            ObjectBase* draggedObject = *static_cast<ObjectBase* const*>(payload->Data);
            ReorderObject(draggedObject, targetObject, insertBefore);
        }

        ImGui::EndDragDropTarget();
    }

    ImGui::PopID();
    ImGui::PopID();
}

bool ScenePanel::ShouldDrawSceneObject(const ObjectBase* object) const
{
    if (!object)
    {
        return false;
    }

    if (ObjectMatchesSearch(object))
    {
        return true;
    }

    for (ObjectBase* child : object->GetChildren())
    {
        if (ShouldDrawSceneObject(child))
        {
            return true;
        }
    }

    return false;
}

bool ScenePanel::ObjectMatchesSearch(const ObjectBase* object) const
{
    if (!object)
    {
        return false;
    }

    return ValueMatchesSearch(object->GetName()) || ValueMatchesSearch(object->GetNameWithoutId());
}

bool ScenePanel::ValueMatchesSearch(const std::string& value) const
{
    if (!IsSearchActive())
    {
        return true;
    }

    const std::string query = ToLower(searchBuffer_);
    return ToLower(value).find(query) != std::string::npos;
}

bool ScenePanel::IsSearchActive() const
{
    return searchBuffer_[0] != '\0';
}

std::vector<ObjectBase*> ScenePanel::GetOrderedObjects(ObjectBase* parentObject, const std::vector<ObjectBase*>& objects)
{
    std::vector<ObjectBase*>& orderedObjects = editorObjectOrderByParent_[parentObject];

    orderedObjects.erase(
        std::remove_if(
            orderedObjects.begin(),
            orderedObjects.end(),
            [&objects](ObjectBase* orderedObject)
            {
                return std::find(objects.begin(), objects.end(), orderedObject) == objects.end();
            }),
        orderedObjects.end());

    for (ObjectBase* object : objects)
    {
        if (std::find(orderedObjects.begin(), orderedObjects.end(), object) == orderedObjects.end())
        {
            orderedObjects.push_back(object);
        }
    }

    return orderedObjects;
}

void ScenePanel::RemoveObjectFromEditorOrder(ObjectBase* object)
{
    if (!object)
    {
        return;
    }

    for (auto& objectOrderPair : editorObjectOrderByParent_)
    {
        std::vector<ObjectBase*>& orderedObjects = objectOrderPair.second;
        orderedObjects.erase(std::remove(orderedObjects.begin(), orderedObjects.end(), object), orderedObjects.end());
    }
}

void ScenePanel::AppendObjectToEditorOrder(ObjectBase* parentObject, ObjectBase* object)
{
    if (!object)
    {
        return;
    }

    std::vector<ObjectBase*>& orderedObjects = editorObjectOrderByParent_[parentObject];
    if (std::find(orderedObjects.begin(), orderedObjects.end(), object) == orderedObjects.end())
    {
        orderedObjects.push_back(object);
    }
}

void ScenePanel::HandleObjectDragDropSource(ObjectBase* object)
{
    if (!object)
    {
        return;
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        ImGui::SetDragDropPayload(SceneObjectDragDropPayload, &object, sizeof(ObjectBase*));
        ImGui::Text("%s", object->GetName().c_str());
        ImGui::EndDragDropSource();
    }
}

void ScenePanel::HandleObjectDragDropTarget(ObjectBase* object)
{
    if (!object || !ImGui::BeginDragDropTarget())
    {
        return;
    }

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(SceneObjectDragDropPayload))
    {
        ObjectBase* draggedObject = *static_cast<ObjectBase* const*>(payload->Data);
        ReparentObject(draggedObject, object);
    }

    ImGui::EndDragDropTarget();
}

void ScenePanel::HandleRootObjectDropTarget()
{
    if (!ImGui::BeginDragDropTarget())
    {
        return;
    }

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(SceneObjectDragDropPayload))
    {
        ObjectBase* draggedObject = *static_cast<ObjectBase* const*>(payload->Data);
        ReparentObject(draggedObject, nullptr);
    }

    ImGui::EndDragDropTarget();
}

void ScenePanel::DrawRootObjectDropTarget()
{
    ImGui::PushID("RootObjectDropTarget");

    const float targetWidth = std::max(ImGui::GetContentRegionAvail().x, 1.f);
    const float targetHeight = std::max(ImGui::GetTextLineHeightWithSpacing() * 2.f, 32.f);
    ImGui::InvisibleButton("##RootObjectDropTarget", ImVec2(targetWidth, targetHeight));

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(SceneObjectDragDropPayload))
        {
            ObjectBase* draggedObject = *static_cast<ObjectBase* const*>(payload->Data);
            ReparentObject(draggedObject, nullptr);
        }

        ImGui::EndDragDropTarget();
    }

    ImGui::PopID();
}

bool ScenePanel::CanReparentObject(const ObjectBase* draggedObject, const ObjectBase* newParent) const
{
    if (!draggedObject || draggedObject == newParent)
    {
        return false;
    }

    if (draggedObject->GetParent() == newParent)
    {
        return false;
    }

    return !newParent || !IsObjectDescendantOf(newParent, draggedObject);
}

bool ScenePanel::IsObjectDescendantOf(const ObjectBase* object, const ObjectBase* potentialAncestor) const
{
    if (!object || !potentialAncestor)
    {
        return false;
    }

    const ObjectBase* parent = object->GetParent();
    while (parent)
    {
        if (parent == potentialAncestor)
        {
            return true;
        }

        parent = parent->GetParent();
    }

    return false;
}

void ScenePanel::ReorderObject(ObjectBase* draggedObject, ObjectBase* targetObject, bool insertBefore)
{
    if (!draggedObject || !targetObject || draggedObject == targetObject)
    {
        return;
    }

    ObjectBase* targetParent = targetObject->GetParent();
    if (draggedObject->GetParent() != targetParent)
    {
        if (!CanReparentObject(draggedObject, targetParent))
        {
            return;
        }

        ReparentObject(draggedObject, targetParent);
    }

    std::vector<ObjectBase*>& orderedObjects = editorObjectOrderByParent_[targetParent];
    orderedObjects.erase(std::remove(orderedObjects.begin(), orderedObjects.end(), draggedObject), orderedObjects.end());

    auto targetIterator = std::find(orderedObjects.begin(), orderedObjects.end(), targetObject);
    if (targetIterator == orderedObjects.end())
    {
        orderedObjects.push_back(targetObject);
        targetIterator = std::prev(orderedObjects.end());
    }

    if (!insertBefore)
    {
        ++targetIterator;
    }

    orderedObjects.insert(targetIterator, draggedObject);
}

void ScenePanel::ReparentObject(ObjectBase* draggedObject, ObjectBase* newParent)
{
    if (!CanReparentObject(draggedObject, newParent))
    {
        return;
    }

    Vector3 worldPosition;
    Vector3 worldScaling;
    Quaternion worldRotation;
    draggedObject->GetWorldTransformationMatrix().Decompose(worldPosition, worldScaling, worldRotation);

    RemoveObjectFromEditorOrder(draggedObject);

    draggedObject->SetParent(nullptr);
    draggedObject->SetWorldPosition(worldPosition, false);
    draggedObject->SetWorldRotation(worldRotation, false);
    draggedObject->SetWorldScaling(worldScaling, false);

    if (newParent)
    {
        draggedObject->SetParent(newParent, SnappingRule::KeepWorldAll);
    }
    else
    {
        draggedObject->SetWorldScaling(worldScaling);
    }

    AppendObjectToEditorOrder(newParent, draggedObject);
    EditorContext::Get()->MarkSceneDirty("Scene object hierarchy changed");
}
