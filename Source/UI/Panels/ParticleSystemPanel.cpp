#include "ParticleSystemPanel.h"

#include <algorithm>

#include "imgui.h"

#include "Goknar/ObjectBase.h"
#include "Goknar/Components/ParticleSystemComponent.h"

#include "UI/EditorAssetPathUtils.h"
#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/EditorWidgets.h"
#include "UI/Panels/AssetSelectorPanel.h"

namespace
{
	constexpr float PARTICLE_SCALAR_INPUT_WIDTH = 120.f;
	constexpr float PARTICLE_RANGE_INPUT_WIDTH = 150.f;
	constexpr float PARTICLE_VECTOR_COMPONENT_WIDTH = 72.f;
	constexpr float PARTICLE_COLOR_INPUT_WIDTH = 220.f;
}

ParticleSystemPanel::ParticleSystemPanel(EditorHUD* hud) :
	IEditorPanel("Particle System", hud)
{
	isOpen_ = false;
}

void ParticleSystemPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_);

	if (EditorContext::Get()->selectedObjectType != EditorSelectionType::Object)
	{
		ImGui::TextWrapped("Select a game object to edit its particle system.");
		ImGui::End();
		return;
	}

	ObjectBase* selectedObject = EditorContext::Get()->GetSelectionAs<ObjectBase>();
	if (!selectedObject)
	{
		ImGui::TextWrapped("Select a game object to edit its particle system.");
		ImGui::End();
		return;
	}

	ParticleSystemComponent* particleSystemComponent = GetSelectedParticleSystemComponent();
	if (!particleSystemComponent)
	{
		ImGui::TextWrapped("The selected object does not have a particle system component.");
		if (ImGui::Button("Add Billboard Particle System"))
		{
			selectedObject->AddSubComponent<BillboardParticleSystemComponent>();
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Static Mesh Particle System"))
		{
			selectedObject->AddSubComponent<StaticMeshParticleSystemComponent>();
		}

		ImGui::End();
		return;
	}

	DrawParticleSystemEditor(particleSystemComponent);

	ImGui::End();
}

ParticleSystemComponent* ParticleSystemPanel::GetSelectedParticleSystemComponent() const
{
	if (EditorContext::Get()->selectedObjectType != EditorSelectionType::Object)
	{
		return nullptr;
	}

	ObjectBase* selectedObject = EditorContext::Get()->GetSelectionAs<ObjectBase>();
	return selectedObject ? selectedObject->GetFirstComponentOfType<ParticleSystemComponent>() : nullptr;
}

void ParticleSystemPanel::DrawParticleSystemEditor(ParticleSystemComponent* particleSystemComponent)
{
	if (!particleSystemComponent)
	{
		return;
	}

	const std::string specialPostfix = "##ParticleSystem_" + std::to_string(particleSystemComponent->GetGUID());

	bool isActive = particleSystemComponent->GetIsActive();
	ImGui::Checkbox(("Active" + specialPostfix).c_str(), &isActive);
	particleSystemComponent->SetIsActive(isActive);

	BillboardParticleSystemComponent* billboardParticleSystemComponent = dynamic_cast<BillboardParticleSystemComponent*>(particleSystemComponent);
	StaticMeshParticleSystemComponent* staticMeshParticleSystemComponent = dynamic_cast<StaticMeshParticleSystemComponent*>(particleSystemComponent);
	ImGui::Text("Render Mode: %s", staticMeshParticleSystemComponent ? "Static Mesh" : "Billboard");


	int maxParticleCount = static_cast<int>(particleSystemComponent->GetMaxParticleCount());
	ImGui::Text("Max Particle Count:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_SCALAR_INPUT_WIDTH);
	EditorWidgets::DrawInputInt("##MaxParticleCount" + specialPostfix, maxParticleCount);
	ImGui::PopItemWidth();
	particleSystemComponent->SetMaxParticleCount(static_cast<std::uint32_t>((std::max)(1, maxParticleCount)));

	float particleSize = particleSystemComponent->GetParticleSize();
	ImGui::Text("Particle Size:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_SCALAR_INPUT_WIDTH);
	EditorWidgets::DrawInputFloat("##ParticleSize" + specialPostfix, particleSize);
	ImGui::PopItemWidth();
	particleSystemComponent->SetParticleSize(particleSize);

	Vector3 gravity = particleSystemComponent->GetGravity();
	ImGui::Text("Gravity:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_VECTOR_COMPONENT_WIDTH);
	EditorWidgets::DrawInputVector3("##Gravity" + specialPostfix, gravity);
	ImGui::PopItemWidth();
	particleSystemComponent->SetGravity(gravity);

	ImGui::Separator();

	if (staticMeshParticleSystemComponent)
	{
		ImGui::Text("Static Mesh:");
		ImGui::TextWrapped("%s", staticMeshParticleSystemComponent->GetStaticMeshPath().empty() ? "" : staticMeshParticleSystemComponent->GetStaticMeshPath().c_str());
		if (ImGui::Button(("Select Static Mesh" + specialPostfix).c_str()))
		{
			OpenAssetSelector(particleSystemComponent, ParticleSystemAssetSelectionTarget::StaticMesh);
		}

		ImGui::SameLine();
		if (ImGui::Button(("Clear Static Mesh" + specialPostfix).c_str()))
		{
			staticMeshParticleSystemComponent->SetStaticMeshPath("");
		}
	}
	else if (billboardParticleSystemComponent)
	{
		ImGui::TextWrapped("Billboards can render either with a material-driven shader graph or a fallback texture-driven particle shader.");

		ImGui::Text("Billboard Material:");
		ImGui::TextWrapped("%s", billboardParticleSystemComponent->GetBillboardMaterialPath().empty() ? "" : billboardParticleSystemComponent->GetBillboardMaterialPath().c_str());
		if (ImGui::Button(("Select Billboard Material" + specialPostfix).c_str()))
		{
			OpenAssetSelector(particleSystemComponent, ParticleSystemAssetSelectionTarget::BillboardMaterial);
		}

		ImGui::SameLine();
		if (ImGui::Button(("Clear Billboard Material" + specialPostfix).c_str()))
		{
			billboardParticleSystemComponent->SetBillboardMaterialPath("");
		}

		ImGui::Text("Billboard Texture Fallback:");
		ImGui::TextWrapped("%s", billboardParticleSystemComponent->GetBillboardTexturePath().empty() ? "" : billboardParticleSystemComponent->GetBillboardTexturePath().c_str());
		if (ImGui::Button(("Select Billboard Texture" + specialPostfix).c_str()))
		{
			OpenAssetSelector(particleSystemComponent, ParticleSystemAssetSelectionTarget::BillboardTexture);
		}

		ImGui::SameLine();
		if (ImGui::Button(("Clear Billboard Texture" + specialPostfix).c_str()))
		{
			billboardParticleSystemComponent->SetBillboardTexturePath("");
		}
	}

	ImGui::Separator();

	GPUParticleSpawnDesc spawnDesc = particleSystemComponent->GetSpawnDesc();

	bool looping = spawnDesc.looping;
	ImGui::Checkbox(("Looping" + specialPostfix).c_str(), &looping);
	spawnDesc.looping = looping;

	float spawnInterval = spawnDesc.spawnInterval;
	ImGui::Text("Spawn Interval:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_SCALAR_INPUT_WIDTH);
	EditorWidgets::DrawInputFloat("##SpawnInterval" + specialPostfix, spawnInterval);
	ImGui::PopItemWidth();
	spawnDesc.spawnInterval = spawnInterval;

	int spawnCountPerInterval = static_cast<int>(spawnDesc.spawnCountPerInterval);
	ImGui::Text("Spawn Count / Interval:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_SCALAR_INPUT_WIDTH);
	EditorWidgets::DrawInputInt("##SpawnCountPerInterval" + specialPostfix, spawnCountPerInterval);
	ImGui::PopItemWidth();
	spawnDesc.spawnCountPerInterval = static_cast<std::uint32_t>((std::max)(1, spawnCountPerInterval));

	Vector3 spawnBoxExtents = spawnDesc.spawnBoxExtents;
	ImGui::Text("Spawn Box Extents:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_VECTOR_COMPONENT_WIDTH);
	EditorWidgets::DrawInputVector3("##SpawnBoxExtents" + specialPostfix, spawnBoxExtents);
	ImGui::PopItemWidth();
	spawnDesc.spawnBoxExtents = spawnBoxExtents;

	float lifetimeRange[2] = { spawnDesc.lifetime.minValue, spawnDesc.lifetime.maxValue };
	ImGui::Text("Lifetime Range:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_RANGE_INPUT_WIDTH);
	ImGui::DragFloat2(("##LifetimeRange" + specialPostfix).c_str(), lifetimeRange, 0.01f);
	ImGui::PopItemWidth();
	spawnDesc.lifetime = GPUParticleValueRange<float>(lifetimeRange[0], lifetimeRange[1]);

	Vector3 initialVelocityMin = spawnDesc.initialVelocity.minValue;
	ImGui::Text("Initial Velocity Min:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_VECTOR_COMPONENT_WIDTH);
	EditorWidgets::DrawInputVector3("##InitialVelocityMin" + specialPostfix, initialVelocityMin);
	ImGui::PopItemWidth();
	spawnDesc.initialVelocity.minValue = initialVelocityMin;

	Vector3 initialVelocityMax = spawnDesc.initialVelocity.maxValue;
	ImGui::Text("Initial Velocity Max:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_VECTOR_COMPONENT_WIDTH);
	EditorWidgets::DrawInputVector3("##InitialVelocityMax" + specialPostfix, initialVelocityMax);
	ImGui::PopItemWidth();
	spawnDesc.initialVelocity.maxValue = initialVelocityMax;

	Vector3 initialRotationMin = spawnDesc.initialRotation.minValue;
	ImGui::Text("Initial Rotation Min:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_VECTOR_COMPONENT_WIDTH);
	EditorWidgets::DrawInputVector3("##InitialRotationMin" + specialPostfix, initialRotationMin);
	ImGui::PopItemWidth();
	spawnDesc.initialRotation.minValue = initialRotationMin;

	Vector3 initialRotationMax = spawnDesc.initialRotation.maxValue;
	ImGui::Text("Initial Rotation Max:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_VECTOR_COMPONENT_WIDTH);
	EditorWidgets::DrawInputVector3("##InitialRotationMax" + specialPostfix, initialRotationMax);
	ImGui::PopItemWidth();
	spawnDesc.initialRotation.maxValue = initialRotationMax;

	Vector3 angularVelocityMin = spawnDesc.angularVelocity.minValue;
	ImGui::Text("Angular Velocity Min:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_VECTOR_COMPONENT_WIDTH);
	EditorWidgets::DrawInputVector3("##AngularVelocityMin" + specialPostfix, angularVelocityMin);
	ImGui::PopItemWidth();
	spawnDesc.angularVelocity.minValue = angularVelocityMin;

	Vector3 angularVelocityMax = spawnDesc.angularVelocity.maxValue;
	ImGui::Text("Angular Velocity Max:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_VECTOR_COMPONENT_WIDTH);
	EditorWidgets::DrawInputVector3("##AngularVelocityMax" + specialPostfix, angularVelocityMax);
	ImGui::PopItemWidth();
	spawnDesc.angularVelocity.maxValue = angularVelocityMax;

	Vector3 accelerationMin = spawnDesc.acceleration.minValue;
	ImGui::Text("Acceleration Min:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_VECTOR_COMPONENT_WIDTH);
	EditorWidgets::DrawInputVector3("##AccelerationMin" + specialPostfix, accelerationMin);
	ImGui::PopItemWidth();
	spawnDesc.acceleration.minValue = accelerationMin;

	Vector3 accelerationMax = spawnDesc.acceleration.maxValue;
	ImGui::Text("Acceleration Max:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_VECTOR_COMPONENT_WIDTH);
	EditorWidgets::DrawInputVector3("##AccelerationMax" + specialPostfix, accelerationMax);
	ImGui::PopItemWidth();
	spawnDesc.acceleration.maxValue = accelerationMax;

	float velocityLimit = spawnDesc.velocityLimit;
	ImGui::Text("Velocity Limit:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_SCALAR_INPUT_WIDTH);
	EditorWidgets::DrawInputFloat("##VelocityLimit" + specialPostfix, velocityLimit);
	ImGui::PopItemWidth();
	spawnDesc.velocityLimit = velocityLimit;

	float sizeByLifetime[2] = { spawnDesc.sizeByLifetime.startValue, spawnDesc.sizeByLifetime.endValue };
	ImGui::Text("Size by Lifetime:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_RANGE_INPUT_WIDTH);
	ImGui::DragFloat2(("##SizeByLifetime" + specialPostfix).c_str(), sizeByLifetime, 0.01f);
	ImGui::PopItemWidth();
	spawnDesc.sizeByLifetime = GPUParticleFloatCurve{ sizeByLifetime[0], sizeByLifetime[1] };

	float sizeBySpeedRange[2] = { spawnDesc.sizeBySpeedRange.x, spawnDesc.sizeBySpeedRange.y };
	ImGui::Text("Size by Speed Range:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_RANGE_INPUT_WIDTH);
	ImGui::DragFloat2(("##SizeBySpeedRange" + specialPostfix).c_str(), sizeBySpeedRange, 0.01f);
	ImGui::PopItemWidth();
	spawnDesc.sizeBySpeedRange = Vector2(sizeBySpeedRange[0], sizeBySpeedRange[1]);

	float sizeBySpeed[2] = { spawnDesc.sizeBySpeed.startValue, spawnDesc.sizeBySpeed.endValue };
	ImGui::Text("Size by Speed:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_RANGE_INPUT_WIDTH);
	ImGui::DragFloat2(("##SizeBySpeed" + specialPostfix).c_str(), sizeBySpeed, 0.01f);
	ImGui::PopItemWidth();
	spawnDesc.sizeBySpeed = GPUParticleFloatCurve{ sizeBySpeed[0], sizeBySpeed[1] };

	float colorByLifetimeStart[4] = { spawnDesc.colorByLifetime.startValue.x, spawnDesc.colorByLifetime.startValue.y, spawnDesc.colorByLifetime.startValue.z, spawnDesc.colorByLifetime.startValue.w };
	ImGui::Text("Color by Lifetime Start:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_COLOR_INPUT_WIDTH);
	ImGui::ColorEdit4(("##ColorByLifetimeStart" + specialPostfix).c_str(), colorByLifetimeStart);
	ImGui::PopItemWidth();
	spawnDesc.colorByLifetime.startValue = Vector4(colorByLifetimeStart[0], colorByLifetimeStart[1], colorByLifetimeStart[2], colorByLifetimeStart[3]);

	float colorByLifetimeEnd[4] = { spawnDesc.colorByLifetime.endValue.x, spawnDesc.colorByLifetime.endValue.y, spawnDesc.colorByLifetime.endValue.z, spawnDesc.colorByLifetime.endValue.w };
	ImGui::Text("Color by Lifetime End:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_COLOR_INPUT_WIDTH);
	ImGui::ColorEdit4(("##ColorByLifetimeEnd" + specialPostfix).c_str(), colorByLifetimeEnd);
	ImGui::PopItemWidth();
	spawnDesc.colorByLifetime.endValue = Vector4(colorByLifetimeEnd[0], colorByLifetimeEnd[1], colorByLifetimeEnd[2], colorByLifetimeEnd[3]);

	float colorBySpeedRange[2] = { spawnDesc.colorBySpeedRange.x, spawnDesc.colorBySpeedRange.y };
	ImGui::Text("Color by Speed Range:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_RANGE_INPUT_WIDTH);
	ImGui::DragFloat2(("##ColorBySpeedRange" + specialPostfix).c_str(), colorBySpeedRange, 0.01f);
	ImGui::PopItemWidth();
	spawnDesc.colorBySpeedRange = Vector2(colorBySpeedRange[0], colorBySpeedRange[1]);

	float colorBySpeedStart[4] = { spawnDesc.colorBySpeed.startValue.x, spawnDesc.colorBySpeed.startValue.y, spawnDesc.colorBySpeed.startValue.z, spawnDesc.colorBySpeed.startValue.w };
	ImGui::Text("Color by Speed Start:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_COLOR_INPUT_WIDTH);
	ImGui::ColorEdit4(("##ColorBySpeedStart" + specialPostfix).c_str(), colorBySpeedStart);
	ImGui::PopItemWidth();
	spawnDesc.colorBySpeed.startValue = Vector4(colorBySpeedStart[0], colorBySpeedStart[1], colorBySpeedStart[2], colorBySpeedStart[3]);

	float colorBySpeedEnd[4] = { spawnDesc.colorBySpeed.endValue.x, spawnDesc.colorBySpeed.endValue.y, spawnDesc.colorBySpeed.endValue.z, spawnDesc.colorBySpeed.endValue.w };
	ImGui::Text("Color by Speed End:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_COLOR_INPUT_WIDTH);
	ImGui::ColorEdit4(("##ColorBySpeedEnd" + specialPostfix).c_str(), colorBySpeedEnd);
	ImGui::PopItemWidth();
	spawnDesc.colorBySpeed.endValue = Vector4(colorBySpeedEnd[0], colorBySpeedEnd[1], colorBySpeedEnd[2], colorBySpeedEnd[3]);

	particleSystemComponent->SetSpawnDesc(spawnDesc);

	ImGui::Separator();

	int previewParticleCount = static_cast<int>(particleSystemComponent->GetPreviewParticleCount());
	ImGui::Text("Preview Burst Count:");
	ImGui::SameLine();
	ImGui::PushItemWidth(PARTICLE_SCALAR_INPUT_WIDTH);
	EditorWidgets::DrawInputInt("##PreviewParticleCount" + specialPostfix, previewParticleCount);
	ImGui::PopItemWidth();
	particleSystemComponent->SetPreviewParticleCount(static_cast<std::uint32_t>((std::max)(0, previewParticleCount)));

	if (ImGui::Button(("Regenerate Preview Burst" + specialPostfix).c_str()))
	{
		particleSystemComponent->RegeneratePreviewParticles();
	}

	ImGui::SameLine();
	if (ImGui::Button(("Clear Particles" + specialPostfix).c_str()))
	{
		particleSystemComponent->ClearParticles();
	}
}

void ParticleSystemPanel::OpenAssetSelector(ParticleSystemComponent* particleSystemComponent, ParticleSystemAssetSelectionTarget selectionTarget)
{
	assetSelectionComponent_ = particleSystemComponent;
	assetSelectionTarget_ = selectionTarget;

	switch (selectionTarget)
	{
	case ParticleSystemAssetSelectionTarget::StaticMesh:
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::StaticMesh;
		break;
	case ParticleSystemAssetSelectionTarget::BillboardMaterial:
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::Material;
		break;
	case ParticleSystemAssetSelectionTarget::BillboardTexture:
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::Texture;
		break;
	case ParticleSystemAssetSelectionTarget::None:
	default:
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::None;
		break;
	}

	AssetSelectorPanel::OnAssetSelected =
		Delegate<void(const std::string&)>::Create<ParticleSystemPanel, &ParticleSystemPanel::OnAssetSelected>(this);

	hud_->ShowPanel<AssetSelectorPanel>();
}

void ParticleSystemPanel::OnAssetSelected(const std::string& path)
{
	ParticleSystemComponent* particleSystemComponent = assetSelectionComponent_ ? assetSelectionComponent_ : GetSelectedParticleSystemComponent();
	if (!particleSystemComponent)
	{
		ResetAssetSelectionState();
		return;
	}

	const std::string normalizedPath = EditorAssetPathUtils::ToContentRelativePath(path);
	switch (assetSelectionTarget_)
	{
	case ParticleSystemAssetSelectionTarget::StaticMesh:
		if (StaticMeshParticleSystemComponent* staticMeshParticleSystemComponent = dynamic_cast<StaticMeshParticleSystemComponent*>(particleSystemComponent))
		{
			staticMeshParticleSystemComponent->SetStaticMeshPath(normalizedPath);
		}
		break;
	case ParticleSystemAssetSelectionTarget::BillboardTexture:
		if (BillboardParticleSystemComponent* billboardParticleSystemComponent = dynamic_cast<BillboardParticleSystemComponent*>(particleSystemComponent))
		{
			billboardParticleSystemComponent->SetBillboardTexturePath(normalizedPath);
		}
		break;
	case ParticleSystemAssetSelectionTarget::BillboardMaterial:
		if (BillboardParticleSystemComponent* billboardParticleSystemComponent = dynamic_cast<BillboardParticleSystemComponent*>(particleSystemComponent))
		{
			billboardParticleSystemComponent->SetBillboardMaterialPath(normalizedPath);
		}
		break;
	case ParticleSystemAssetSelectionTarget::None:
	default:
		break;
	}

	ResetAssetSelectionState();
}

void ParticleSystemPanel::ResetAssetSelectionState()
{
	assetSelectionComponent_ = nullptr;
	assetSelectionTarget_ = ParticleSystemAssetSelectionTarget::None;
	EditorContext::Get()->assetSelectorFilter = EditorAssetType::None;
}
