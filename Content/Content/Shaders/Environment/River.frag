#version 440 core
// Base Material Variables
uniform vec3 ambientReflectance;
uniform vec3 specularReflectance;
uniform float phongExponent;

uniform float translucency;

uniform float deltaTime;
uniform float elapsedTime;

out vec4 fragmentColor;
in mat4 finalModelMatrix;
in vec4 fragmentPositionWorldSpace;
in vec4 fragmentPositionScreenSpace;
in vec4 fragmentPositionLightSpace_DirectionalLight0;
in vec3 vertexNormal;
in vec2 textureUV;
uniform vec3 viewPosition;

//---------------------------------------- SHADOW MAPS ----------------------------------------
uniform sampler2DShadow shadowMap_DirectionalLight0;
//----------------------------------------------------------------------------------------------

vec3 DirectionalLight0Direction = vec3(0.577350, 0.577350, -0.577350);
vec3 DirectionalLight0Intensity = vec3(1.000000, 0.990000, 0.830000);
uniform bool DirectionalLight0IsCastingShadow;
uniform sampler2D texture1;
vec4 diffuseReflectance;
vec3 sceneAmbient = vec3(0.392157, 0.392157, 0.392157);

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
        vec3 specularColor = specularReflectance * cosAlphaPrimeToThePowerOfPhongExponent;
        specularColor *= max(0, normalDotLightDirection) * intensity;

        vec3 finalIntensity = specularColor + diffuseColor;

        if(isBackface && isTranslucent)
        {
                finalIntensity *= translucency;
        }

        return finalIntensity;
}


void main()
{
        vec4 texture1Color = texture(texture1, vec2(textureUV.x + elapsedTime * 0.25f, textureUV.y)); 
        diffuseReflectance = vec4(texture1Color); 

        vec3 lightIntensity = sceneAmbient * ambientReflectance;


        if(DirectionalLight0IsCastingShadow)
        {
                vec3 lightSpaceScreenCoordinate_DirectionalLight0 = fragmentPositionLightSpace_DirectionalLight0.xyz / fragmentPositionLightSpace_DirectionalLight0.w;

                if(     0.f <= lightSpaceScreenCoordinate_DirectionalLight0.x && lightSpaceScreenCoordinate_DirectionalLight0.x <= 1.f &&
                        0.f <= lightSpaceScreenCoordinate_DirectionalLight0.y && lightSpaceScreenCoordinate_DirectionalLight0.y <= 1.f)
                {
                        float shadowValue_DirectionalLight0 = textureProj(shadowMap_DirectionalLight0, fragmentPositionLightSpace_DirectionalLight0);

                        if(0.f < shadowValue_DirectionalLight0)
                        {
                                vec3 lightIntensity_DirectionalLight0 = CalculateDirectionalLightColor(DirectionalLight0Direction, DirectionalLight0Intensity); 
                                lightIntensity += shadowValue_DirectionalLight0 * lightIntensity_DirectionalLight0;
                        }
                }
                else
                {
                        lightIntensity +=  CalculateDirectionalLightColor(DirectionalLight0Direction, DirectionalLight0Intensity);
                }
        }
        else
        {
                lightIntensity +=  CalculateDirectionalLightColor(DirectionalLight0Direction, DirectionalLight0Intensity);
        }

        fragmentColor = vec4(lightIntensity, 0.8f) * diffuseReflectance;
}