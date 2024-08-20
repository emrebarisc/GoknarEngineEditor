#version 440 core


// Base Material Variables
//---------------------------------PBR------------------------------------
uniform float metallic;
uniform float roughness;
uniform float ambientOcclusion;
//------------------------------------------------------------------------


//------------------------------------------------------------------------


uniform float deltaTime;
uniform float elapsedTime;

uniform vec3 viewPosition;
in mat4 finalModelMatrix;
in vec4 fragmentPositionScreenSpace;
in vec2 textureUV;

// Base Material Variables
out vec4 fragmentColor;



uniform sampler2D position_GBuffer;
uniform sampler2D normal_GBuffer;
uniform sampler2D diffuse_GBuffer;
uniform sampler2D specularAndPhong_GBuffer;
uniform sampler2D emmisiveColor_GBuffer;

vec4 fragmentPositionWorldSpace;
vec4 baseColor;
vec3 emmisiveColor;
vec3 specular;
vec3 vertexNormal;
float phongExponent;
float translucency;

layout (std140, binding = 3) uniform DirectionalLightViewMatrixUniformBuffer 
{
	mat4 directionalLightViewMatrixArray[4];
};

layout (std140, binding = 4) uniform SpotLightViewMatrixUniformBuffer 
{
	mat4 spotLightViewMatrixArray[8];
};

struct DirectionalLight
{
	vec3 direction;
	float shadowIntensity;
	vec3 intensity;
	bool isCastingShadow;
};

struct PointLight
{
	vec3 position;
	float radius ;
	vec3 intensity;
	bool isCastingShadow;
	float shadowIntensity;
};


struct SpotLight
{
	vec3 position;
	float coverageAngle;
	vec3 direction;
	float falloffAngle;
	vec3 intensity;
	float shadowIntensity;
	bool isCastingShadow;
};

layout (std140, binding = 0) uniform DirectionalLightUniform
{
	DirectionalLight directionalLights[4];
	int directionalLightCount;
};

layout (std140, binding = 1) uniform PointLightUniform 
{
	PointLight pointLights[16];
	int pointLightCount;
};

layout (std140, binding = 2) uniform SpotLightUniform
{
	SpotLight spotLights[8];
	int spotLightCount;
};

uniform sampler2DShadow directionalLightShadowMaps[4];
uniform samplerCubeShadow pointLightShadowMaps[16];
uniform sampler2DShadow spotLightShadowMaps[8];
vec4 directionalLightSpaceFragmentPositions[4];
vec4 spotLightSpaceFragmentPositions[8];

vec3 CalculateDirectionalLightColor(vec3 direction, vec3 intensity)
{
	vec3 wi = normalize(-direction);

	float normalDotLightDirection = dot(wi, vertexNormal);

	bool isTranslucent = 0.f < translucency;
	bool isBackface = normalDotLightDirection < 0.f;

	if(isBackface)
	{
		if(!isTranslucent)
		{
			return vec3(0.f);
		}
		else
		{
			normalDotLightDirection = -normalDotLightDirection;
		}
	}
	
	if(isTranslucent)
	{
		normalDotLightDirection = clamp(normalDotLightDirection * (1.f +translucency), 0.f, 1.f);
	}

	vec3 diffuseColor = intensity * normalDotLightDirection;

	// To viewpoint vector
	vec3 wo = normalize(viewPosition - vec3(fragmentPositionWorldSpace));

	// Half vector
	vec3 halfVector = (wi + wo) * 0.5f;

	// Specular
	float cosAlphaPrimeToThePowerOfPhongExponent = pow(max(0.f, dot(vertexNormal, halfVector)), phongExponent);
	vec3 specularColor = specular * cosAlphaPrimeToThePowerOfPhongExponent;
	specularColor *= max(0, normalDotLightDirection) * intensity;
	
	vec3 finalIntensity = specularColor + diffuseColor;

	if(isBackface && isTranslucent)
	{
		finalIntensity *= translucency;
	}

	return finalIntensity;
}

vec3 CalculatePointLightColor(vec3 position, vec3 intensity, float radius)
{
	// To light vector
	vec3 wi = position - vec3(fragmentPositionWorldSpace);
	float wiLength = length(wi);

	if(radius < wiLength)
	{
		return vec3(0.f);
	}

	wi /= wiLength;

	float normalDotLightDirection = dot(vertexNormal, wi);
	
	bool isTranslucent = 0.f < translucency;
	bool isBackface = normalDotLightDirection < 0.f;

	if(isBackface)
	{
		if(!isTranslucent)
		{
			return vec3(0.f);
		}
		else
		{
			normalDotLightDirection = -normalDotLightDirection;
			normalDotLightDirection = clamp(normalDotLightDirection + translucency, 0.f, 1.f);
		}
	}

	// To viewpoint vector
	vec3 wo = normalize(viewPosition - vec3(fragmentPositionWorldSpace));

	// Half vector
	vec3 halfVector = (wi + wo) * 0.5f;

	vec3 intensityOverDistanceSquare = intensity / (wiLength * wiLength);

	// Diffuse
	float cosThetaPrime = max(0.f, normalDotLightDirection);
	vec3 diffuseColor = cosThetaPrime * intensityOverDistanceSquare;

	// Specular
	float cosAlphaPrimeToThePowerOfPhongExponent = pow(max(0.f, dot(vertexNormal, halfVector)), phongExponent);
	vec3 specularColor = specular * cosAlphaPrimeToThePowerOfPhongExponent;

	specularColor *= cosThetaPrime * intensityOverDistanceSquare;

	vec3 finalIntensity = specularColor + diffuseColor;

	if(isBackface && isTranslucent)
	{
		finalIntensity *= translucency;
	}

	return finalIntensity;
}

vec3 CalculateSpotLightColor(vec3 position, vec3 direction, vec3 intensity, float coverageAngle, float falloffAngle)
{
	float lightMultiplier = 0.f;

	// To light vector
	vec3 wi = vec3(fragmentPositionWorldSpace) - position;
	float wiLength = length(wi);
	wi /= wiLength;

	float normalDotLightDirection = dot(wi, vertexNormal);
	
	bool isTranslucent = 0.f < translucency;
	
	bool isBackface = 0.f < normalDotLightDirection;

	if(isBackface)
	{
		if(!isTranslucent)
		{
			return vec3(0.f);
		}
		else
		{
			normalDotLightDirection = -normalDotLightDirection;
			normalDotLightDirection = -clamp(-normalDotLightDirection + translucency, 0.f, 1.f);
		}
	}
	
	vec3 intensityOverDistanceSquare = intensity / (wiLength * wiLength);

	vec3 diffuseColor = -normalDotLightDirection * intensityOverDistanceSquare;

	float cosCoverage = cos(coverageAngle);
	float cosFalloff = cos(falloffAngle);
	float cosTheta = abs(dot(wi, direction));
	
	if(cosTheta < cosCoverage)
	{
		return vec3(0.f);
	}

	if(cosFalloff < cosTheta)
	{
		lightMultiplier = 1.f;
	}
	else
	{
		lightMultiplier = pow((cosTheta - cosCoverage) / (cosFalloff - cosCoverage), 4);
	}

	// To viewpoint vector
	vec3 wo = normalize(viewPosition - vec3(fragmentPositionWorldSpace));

	// Half vector
	vec3 halfVector = (-wi + wo) * 0.5f;

	// Specular
	float cosAlphaPrimeToThePowerOfPhongExponent = pow(max(0.f, dot(vertexNormal, halfVector)), phongExponent);
	vec3 specularColor = specular * cosAlphaPrimeToThePowerOfPhongExponent;

	specularColor *= intensityOverDistanceSquare;
	
	vec3 finalIntensity = (specularColor + diffuseColor) * lightMultiplier;

	if(isBackface && isTranslucent)
	{
		finalIntensity *= translucency;
	}

	return finalIntensity;
}

void main()
{

	vec4 finalBaseColor = texture(diffuse_GBuffer, textureUV);

	fragmentPositionWorldSpace = vec4(texture(position_GBuffer, textureUV).xyz, 1.f);

	vec4 fragmentNormalAndPhongExponent = texture(normal_GBuffer, textureUV);
	vertexNormal= fragmentNormalAndPhongExponent.xyz;
	phongExponent= pow(2.f, fragmentNormalAndPhongExponent.a);

	vec4 specularAndTranslucency = texture(specularAndPhong_GBuffer, textureUV);
	specular = specularAndTranslucency.xyz;
	translucency = specularAndTranslucency.a;

	vec3 finalEmmisiveColor = texture(emmisiveColor_GBuffer, textureUV).xyz;

	
	for(int directionalLightIndex = 0; directionalLightIndex < directionalLightCount; ++directionalLightIndex)
	{
		directionalLightSpaceFragmentPositions[directionalLightIndex] = fragmentPositionWorldSpace * directionalLightViewMatrixArray[directionalLightIndex];
	}

	for(int spotLightIndex = 0; spotLightIndex < spotLightCount; ++spotLightIndex)
	{
		spotLightSpaceFragmentPositions[spotLightIndex] = fragmentPositionWorldSpace * spotLightViewMatrixArray[spotLightIndex];
	}

	 vec3 lightIntensity = finalEmmisiveColor; 

	for(int directionalLightIndex = 0; directionalLightIndex < directionalLightCount; ++directionalLightIndex)
	{
		if(directionalLights[directionalLightIndex].isCastingShadow)
		{
			vec3 lightSpaceScreenCoordinate = directionalLightSpaceFragmentPositions[directionalLightIndex].xyz / directionalLightSpaceFragmentPositions[directionalLightIndex].w;

			if(	0.f <= lightSpaceScreenCoordinate.x && lightSpaceScreenCoordinate.x <= 1.f &&
				0.f <= lightSpaceScreenCoordinate.y && lightSpaceScreenCoordinate.y <= 1.f)
			{
				float shadowValue = textureProj(directionalLightShadowMaps[directionalLightIndex], directionalLightSpaceFragmentPositions[directionalLightIndex]);
				shadowValue += directionalLights[directionalLightIndex].shadowIntensity;
				shadowValue = clamp(shadowValue, 0.f, 1.f);
				if(0.f < shadowValue)
				{
					vec3 currentLightIntensity = CalculateDirectionalLightColor(

						directionalLights[directionalLightIndex].direction,
						directionalLights[directionalLightIndex].intensity);

					lightIntensity += shadowValue * currentLightIntensity;
				}
			}
			else
			{
				lightIntensity += CalculateDirectionalLightColor(
					directionalLights[directionalLightIndex].direction,
					directionalLights[directionalLightIndex].intensity);
			}
		}
		else
		{
			lightIntensity += CalculateDirectionalLightColor(
				directionalLights[directionalLightIndex].direction,
				directionalLights[directionalLightIndex].intensity);
		}
	}

	for(int pointLightIndex = 0; pointLightIndex < pointLightCount; ++pointLightIndex)
	{
		if(pointLights[pointLightIndex].isCastingShadow)
		{
			vec3 fragmentToLightVector = fragmentPositionWorldSpace.xyz + vertexNormal * 0.025f - pointLights[pointLightIndex].position;
			float shadowValue = texture(pointLightShadowMaps[pointLightIndex], vec4(fragmentToLightVector, 0.025f)).x;
			shadowValue *= pointLights[pointLightIndex].radius;
			if(length(fragmentToLightVector) < shadowValue)
			{
				vec3 currentLightIntensity = CalculatePointLightColor(
					pointLights[pointLightIndex].position,
					pointLights[pointLightIndex].intensity,
					pointLights[pointLightIndex].radius);
				lightIntensity += currentLightIntensity;
			}
			else
			{	
				lightIntensity += pointLights[pointLightIndex].shadowIntensity;			
			}
		}
		else
		{
			lightIntensity += CalculatePointLightColor(
				pointLights[pointLightIndex].position,
				pointLights[pointLightIndex].intensity,
				pointLights[pointLightIndex].radius);
		}
	}

	for(int spotLightIndex = 0; spotLightIndex < spotLightCount; ++spotLightIndex)
	{
		if(spotLights[spotLightIndex].isCastingShadow)
		{
			vec3 lightSpaceScreenCoordinate = spotLightSpaceFragmentPositions[spotLightIndex].xyz / spotLightSpaceFragmentPositions[spotLightIndex].w;

			if(	0.f <= lightSpaceScreenCoordinate.x && lightSpaceScreenCoordinate.x <= 1.f &&
				0.f <= lightSpaceScreenCoordinate.y && lightSpaceScreenCoordinate.y <= 1.f)
			{
				float shadowValue = textureProj(spotLightShadowMaps[spotLightIndex], spotLightSpaceFragmentPositions[spotLightIndex]);
				shadowValue += spotLights[spotLightIndex].shadowIntensity;
				shadowValue = clamp(shadowValue, 0.f, 1.f);
				if(0.f < shadowValue)
				{
					vec3 currentLightIntensity = CalculateSpotLightColor(
						spotLights[spotLightIndex].position,
						spotLights[spotLightIndex].direction,
						spotLights[spotLightIndex].intensity,
						spotLights[spotLightIndex].coverageAngle,
						spotLights[spotLightIndex].falloffAngle); 
					lightIntensity += shadowValue * currentLightIntensity;
				}
			}
			else
			{
				lightIntensity += CalculateSpotLightColor(
					spotLights[spotLightIndex].position,
					spotLights[spotLightIndex].direction,
					spotLights[spotLightIndex].intensity,
					spotLights[spotLightIndex].coverageAngle,
					spotLights[spotLightIndex].falloffAngle);
			}
		}
		else
		{
			lightIntensity += CalculateSpotLightColor(
				spotLights[spotLightIndex].position,
				spotLights[spotLightIndex].direction,
				spotLights[spotLightIndex].intensity,
				spotLights[spotLightIndex].coverageAngle,
				spotLights[spotLightIndex].falloffAngle);
		}
	}
	fragmentColor = vec4(lightIntensity, 1.f) * finalBaseColor;
}