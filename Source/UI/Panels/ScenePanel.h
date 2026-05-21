#pragma once

#include "EditorPanel.h"
#include <string>
#include <unordered_map>
#include <vector>

class ObjectBase;

class ScenePanel : public IEditorPanel
{
public:
    ScenePanel(EditorHUD* hud) : 
        IEditorPanel("Scene", hud) 
    {

    }
    
    virtual void Draw() override;

private:
    void DrawSearchBox();
    void DrawSceneLights();
    void DrawSceneReferences();
    void DrawSceneObjects();
    void DrawSceneObject(ObjectBase* object, bool allowHierarchyEditing);
    void DrawSceneObjectDropTarget(ObjectBase* targetObject, bool insertBefore);
    bool ShouldDrawSceneObject(const ObjectBase* object) const;
    bool ObjectMatchesSearch(const ObjectBase* object) const;
    bool ValueMatchesSearch(const std::string& value) const;
    bool IsSearchActive() const;
    std::vector<ObjectBase*> GetOrderedObjects(ObjectBase* parentObject, const std::vector<ObjectBase*>& objects);
    void RemoveObjectFromEditorOrder(ObjectBase* object);
    void AppendObjectToEditorOrder(ObjectBase* parentObject, ObjectBase* object);
    void HandleObjectDragDropSource(ObjectBase* object);
    void HandleObjectDragDropTarget(ObjectBase* object);
    void HandleRootObjectDropTarget();
    void DrawRootObjectDropTarget();
    bool CanReparentObject(const ObjectBase* draggedObject, const ObjectBase* newParent) const;
    bool IsObjectDescendantOf(const ObjectBase* object, const ObjectBase* potentialAncestor) const;
    void ReorderObject(ObjectBase* draggedObject, ObjectBase* targetObject, bool insertBefore);
    void ReparentObject(ObjectBase* draggedObject, ObjectBase* newParent);

    std::unordered_map<ObjectBase*, std::vector<ObjectBase*>> editorObjectOrderByParent_;
    char searchBuffer_[128]{};
};
