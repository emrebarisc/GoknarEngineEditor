#pragma once

#include "EditorPanel.h"

#include <string>

class ObjectBase;
class ParticleSystemComponent;

enum class ParticleSystemAssetSelectionTarget
{
	None = 0,
	StaticMesh,
	BillboardTexture,
	BillboardMaterial
};

class ParticleSystemPanel : public IEditorPanel
{
public:
	explicit ParticleSystemPanel(EditorHUD* hud);
	void Draw() override;

private:
	ParticleSystemComponent* GetSelectedParticleSystemComponent() const;
	void DrawParticleSystemEditor(ParticleSystemComponent* particleSystemComponent);
	void OpenAssetSelector(ParticleSystemComponent* particleSystemComponent, ParticleSystemAssetSelectionTarget selectionTarget);
	void OnAssetSelected(const std::string& path);
	void ResetAssetSelectionState();

	ParticleSystemComponent* assetSelectionComponent_{ nullptr };
	ParticleSystemAssetSelectionTarget assetSelectionTarget_{ ParticleSystemAssetSelectionTarget::None };
};
