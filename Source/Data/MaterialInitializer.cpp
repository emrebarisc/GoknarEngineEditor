#include "MaterialInitializer.h"

#include "Goknar/Engine.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Model/StaticMesh.h"

MaterialInitializer::MaterialInitializer()
{
	Skills_InitializeTargetObjectMaterials();
	Skills_InitializeFireBurstCastedObjectMaterials();
	Environment_InitializePortalMaterials();
	Environment_InitializeGrassMaterials();
	Environment_InitializeMushroomMaterials();
}

void MaterialInitializer::Skills_InitializeTargetObjectMaterials()
{
	ResourceManager* resourceManager = engine->GetResourceManager();

	StaticMesh* targetIndicatorStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/SM_SkillTargetIndicator.fbx");

	std::string trailImageTextureName = "trailTexture";
	Image* trailImage = resourceManager->GetContent<Image>("Textures/VFX/T_Trail01.jpg");
	trailImage->SetName(trailImageTextureName);

	Material* targetIndicatorStaticMeshMaterial = targetIndicatorStaticMesh->GetMaterial();
	targetIndicatorStaticMeshMaterial->SetBaseColor(Vector3{ 0.8f, 1.f, 0.8f });
	targetIndicatorStaticMeshMaterial->SetEmmisiveColor(Vector3{ 0.8f, 1.f, 0.8f });
	targetIndicatorStaticMeshMaterial->AddTextureImage(trailImage);

	MaterialInitializationData* materialInitializationData = targetIndicatorStaticMeshMaterial->GetInitializationData();

	materialInitializationData->baseColor.calculation = R"(
	vec2 modifiedUV = )" + std::string(SHADER_VARIABLE_NAMES::TEXTURE::UV) + R"(;
	modifiedUV *= vec2(1.f, 1.f);
	modifiedUV += vec2(0.25f, 0.f) * )" + SHADER_VARIABLE_NAMES::TIMING::ELAPSED_TIME + R"(;
	vec4 textureValue = texture2D()" + trailImageTextureName + R"(, modifiedUV);
	
	float opacity = 0.25f * textureValue.r;

	vec3 firstColor = vec3(0.89803921568f, 0.8f, 1.f);
	vec3 secondColor = vec3(0.8f, 1.f, 0.8f);

	vec2 colorChangeUV = )" + std::string(SHADER_VARIABLE_NAMES::TEXTURE::UV) + R"(;
	colorChangeUV += elapsedTime * vec2(0.5f, -0.1f);
	colorChangeUV = fract(colorChangeUV);

	vec3 colorInterpolator = vec3(colorChangeUV, 1.f);

	vec3 color = secondColor;//smoothstep(firstColor, secondColor, colorInterpolator);
)";

	materialInitializationData->baseColor.result = "vec4(color, opacity);";
}

void MaterialInitializer::Skills_InitializeFireBurstCastedObjectMaterials()
{
	ResourceManager* resourceManager = engine->GetResourceManager();

	{
		StaticMesh* skillObjectStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/Skills/SkillObjects/SM_FireBurstCastedSkillObject.fbx");

		std::string noiseImageTextureName = "flameTexture";
		Image* flowImage = resourceManager->GetContent<Image>("Textures/VFX/T_Flame.png");
		flowImage->SetTextureUsage(TextureUsage::None);
		flowImage->SetName(noiseImageTextureName);

		std::string trailImageTextureName = "trailTexture";
		Image* trailImage = resourceManager->GetContent<Image>("Textures/VFX/T_Trail01.jpg");
		trailImage->SetName(trailImageTextureName);

		Material* newMaterial = new Material();
		newMaterial->SetBlendModel(MaterialBlendModel::Transparent);
		newMaterial->SetBaseColor(Vector4{ 0.75f, 0.75f, 0.75f, 0.9f });
		newMaterial->SetEmmisiveColor(2.f * Vector3{ 1.f, 0.064f, 0.f });
		newMaterial->AddTextureImage(flowImage);
		newMaterial->AddTextureImage(trailImage);

		MaterialInitializationData* materialInitializationData = newMaterial->GetInitializationData();

		materialInitializationData->baseColor.calculation =
			R"(
	vec2 scaledUV = vec2(2.f, 2.f) * textureUV;

	vec2 flowUVMoveSpeed = vec2(-0.1, 1.f);
	vec2 flowUV = scaledUV * vec2(1.f, 0.25f);
	flowUV += elapsedTime * flowUVMoveSpeed;

	vec2 flowUVReversedMoveSpeed = vec2(0.2f, 0.8f);
	vec2 flowUVReversed = scaledUV * vec2(1.f, 0.25f);
	flowUVReversed += elapsedTime * flowUVReversedMoveSpeed;

	vec4 noiseTextureColor = texture2D()" + noiseImageTextureName + R"(, flowUV);
	vec4 noiseTextureColorReversed = texture2D()" + noiseImageTextureName + R"(, flowUVReversed);
	float noiseTextureColorMultipliedR = noiseTextureColor.r * noiseTextureColorReversed.r;
	float fireColorInterpolator = smoothstep(0.f, 1.f, 1.f - pow(noiseTextureColorMultipliedR, 0.125f));
	fireColorInterpolator = smoothstep(fireColorInterpolator * 4.f, 1.f, 0.f);
	
	vec3 fireColor = mix(vec3(0.7f, 0.2f, 0.2f), vec3(0.275f, 0.08f, 0.05f), fireColorInterpolator);

	vec2 worldPosToView2D = normalize(viewPosition.xy - fragmentPositionWorldSpace.xy);
	float cosAngleToViewPosition2D = dot(worldPosToView2D, normalize(vertexNormal.xy));

	float lightBeamForMiddle = smoothstep(0.f, 1.f, cosAngleToViewPosition2D);
	lightBeamForMiddle = pow(lightBeamForMiddle, 64.f);

	float lightBeamForEdges = (1.f - pow(abs(sin(3.141593f * cosAngleToViewPosition2D / 2.f)), 0.5f));

	float lightBeam = clamp(lightBeamForEdges + lightBeamForMiddle, 0.f, 1.f);
	lightBeam = smoothstep(0.f, 1.f, lightBeam);

	float distanceFromGroundMultiplier = clamp(fragmentPositionWorldSpace.z * 0.1f + 0.5f, 0.f, 1.f);

	vec3 emmisiveColorResult =
		(1.f - fireColorInterpolator) * 
		lightBeam * 
		vec3(10.f, 5.f, 1.5f) * 
		distanceFromGroundMultiplier + 
		fireColor;
)";
		materialInitializationData->baseColor.result = std::string(SHADER_VARIABLE_NAMES::MATERIAL::BASE_COLOR) + "; \n";

		materialInitializationData->emmisiveColor.result = "emmisiveColorResult;\n";

		skillObjectStaticMesh->SetMaterial(newMaterial);
	}

	{
		StaticMesh* skillObjectStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/Skills/SkillObjects/SM_FireBurstCastedSkillFloorFlameObject.fbx");

		std::string flameNoiseTextureName = "flameTexture";
		Image* flameImage = resourceManager->GetContent<Image>("Textures/VFX/T_Flame.png");
		flameImage->SetTextureUsage(TextureUsage::None);
		flameImage->SetName(flameNoiseTextureName);

		Material* newMaterial = new Material();
		newMaterial->SetBlendModel(MaterialBlendModel::Masked);
		newMaterial->SetBaseColor(Vector3{ 0.23529411764, 0.03921568627f, 0.03921568627f });
		newMaterial->SetEmmisiveColor(Vector3{ 1.f });
		newMaterial->AddTextureImage(flameImage);
		newMaterial->SetShadingModel(MaterialShadingModel::TwoSided);

		MaterialInitializationData* materialInitializationData = newMaterial->GetInitializationData();
		materialInitializationData->baseColor.calculation =
			R"(
	vec2 uv1 = textureUV * vec2(8.f, 1.f) + vec2(0.7f, 0.8f);
	uv1 += vec2(0.3f, 1.2f) * elapsedTime;
	vec4 texture1 = texture2D()" + flameNoiseTextureName + R"(, uv1);

	vec2 uv2 = textureUV * vec2(8.f, 1.f) + vec2(0.5f, 1.0f);
	uv2 += vec2(-0.2f, 0.8f) * elapsedTime;
	vec4 texture2 = texture2D()" + flameNoiseTextureName + R"(, uv2);

	float combinedTextureR = texture1.r * texture2.r;
	combinedTextureR += textureUV.y;
	combinedTextureR *= 2.f;

	float opacity = smoothstep(1.f, 2.f, combinedTextureR);
)";
		materialInitializationData->baseColor.result = "vec4(" + std::string(SHADER_VARIABLE_NAMES::MATERIAL::BASE_COLOR) + ".xyz, opacity); ";

		//materialInitializationData->emmisiveColor.result = "mix(vec3(0.7f, 0.f, 0.f), vec3(1.f, 0.5f, 0.15f), multipliedTextureR);\n";

		skillObjectStaticMesh->SetMaterial(newMaterial);
	}
}

void MaterialInitializer::Environment_InitializePortalMaterials()
{
	ResourceManager* resourceManager = engine->GetResourceManager();

	StaticMesh* portalStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/Environment/Portals/SM_Portal.fbx");

	Material* targetIndicatorStaticMeshMaterial = portalStaticMesh->GetMaterial();

	Vector3 color = Vector3{ 0.3725490196f, 0.73725490196f, 0.95294117647f };
	targetIndicatorStaticMeshMaterial->SetBaseColor(Vector3{ 0.1f });
	targetIndicatorStaticMeshMaterial->SetTranslucency(1.f);
	targetIndicatorStaticMeshMaterial->SetEmmisiveColor(color);
	targetIndicatorStaticMeshMaterial->SetBlendModel(MaterialBlendModel::Transparent);

	MaterialInitializationData* materialInitializationData = targetIndicatorStaticMeshMaterial->GetInitializationData();

	materialInitializationData->vertexPositionOffset.calculation = R"(
    const float PI = 3.1415f;

    vec3 scaledWorldPosition = position;

    vec3 origin1 = scaledWorldPosition + vec3(3.f, 3.f, 1.5f);
    vec3 origin2 = scaledWorldPosition + vec3(-3.f, 3.f, -1.5f);
    vec3 origin3 = scaledWorldPosition + vec3(3.f, -3.f, 1.5f);
    vec3 origin4 = scaledWorldPosition + vec3(-3.f, -3.f, -1.5f); 

    float distance1 = length(origin1);
    float distance2 = length(origin2);
    float distance3 = length(origin3);
    float distance4 = length(origin4);

	float scaledTime = elapsedTime * 2.f;

    float wave =    sin(3.3f * PI * distance1 * 0.13f + scaledTime) * 0.025f +
                    sin(3.2f * PI * distance2 * 0.12f + scaledTime) * 0.025f +
                    sin(3.1f * PI * distance3 * 0.24f + scaledTime) * 0.025f +
                    sin(3.5f * PI * distance4 * 0.32f + scaledTime) * 0.025f;
)";
	materialInitializationData->vertexPositionOffset.result = "vec3(wave)";

	materialInitializationData->baseColor.calculation = R"(
	vec3 worldPosToView = normalize(viewPosition - fragmentPositionWorldSpace.xyz);
	float cosAngleToViewPosition = dot(worldPosToView, normalize(vertexNormal));

	float opacity = (1.f - abs(sin(3.141593f * cosAngleToViewPosition)));
)";
	materialInitializationData->baseColor.result = "vec4(" + std::string(SHADER_VARIABLE_NAMES::MATERIAL::BASE_COLOR) + ".xyz, opacity); ";
}

void MaterialInitializer::Environment_InitializeGrassMaterials()
{
	ResourceManager* resourceManager = engine->GetResourceManager();
	StaticMesh* grassStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/Environment/Plants/SM_Grass.fbx");

	Material* grassMaterial = grassStaticMesh->GetMaterial();
	grassMaterial->SetBaseColor(Vector3{ 0.25f, 0.275f, 0.25f });
	grassMaterial->SetEmmisiveColor(Vector3{ 0.f });
	grassMaterial->SetTranslucency(1.f);
	grassMaterial->SetShadingModel(MaterialShadingModel::TwoSided);

	MaterialInitializationData* materialInitializationData = grassMaterial->GetInitializationData();

	materialInitializationData->vertexPositionOffset.calculation = R"(
    const float PI = 3.1415f;

    vec3 scaledWorldPosition = position;

    vec3 origin1 = scaledWorldPosition + vec3(1.f, 1.f, 0.5f);
    vec3 origin2 = scaledWorldPosition + vec3(-1.f, 1.f, -0.5f);
    vec3 origin3 = scaledWorldPosition + vec3(1.f, -1.f, 0.5f);
    vec3 origin4 = scaledWorldPosition + vec3(-1.f, -1.f, -0.5f); 

    float distance1 = length(origin1);
    float distance2 = length(origin2);
    float distance3 = length(origin3);
    float distance4 = length(origin4);

	float scaledTime = elapsedTime * 2.f;

    float wave =    sin(3.3f * PI * distance1 * 0.13f + scaledTime) +
                    sin(3.2f * PI * distance2 * 0.12f + scaledTime) +
                    sin(3.1f * PI * distance3 * 0.24f + scaledTime) +
                    sin(3.5f * PI * distance4 * 0.32f + scaledTime);

	wave *= 0.01f;
)";
	materialInitializationData->vertexPositionOffset.result = std::string("vec3(wave * ") + SHADER_VARIABLE_NAMES::VERTEX::COLOR + ")";
}

void MaterialInitializer::Environment_InitializeMushroomMaterials()
{
	ResourceManager* resourceManager = engine->GetResourceManager();
	StaticMesh* mushroomStaticMesh = resourceManager->GetContent<StaticMesh>("Meshes/Environment/Plants/SM_Mushrooms_01.fbx");

	Material* mushroomMaterial = mushroomStaticMesh->GetMaterial();
	mushroomMaterial->SetEmmisiveColor(Vector3{ 5.f });
	mushroomMaterial->SetTranslucency(0.f);

	std::string emmisiveTextureName = "emmisiveTexture";
	Image* emmisiveImage = resourceManager->GetContent<Image>("Textures/Environment/T_MushroomEmmisive.png");
	emmisiveImage->SetTextureUsage(TextureUsage::Emmisive);
	emmisiveImage->SetName(emmisiveTextureName);

	mushroomMaterial->AddTextureImage(emmisiveImage);

	MaterialInitializationData* materialInitializationData = mushroomMaterial->GetInitializationData();
	materialInitializationData->emmisiveColor.result = std::string("texture(" + emmisiveTextureName + ", " + SHADER_VARIABLE_NAMES::TEXTURE::UV + ").xyz * ") + SHADER_VARIABLE_NAMES::MATERIAL::EMMISIVE_COLOR + "; ";
}
