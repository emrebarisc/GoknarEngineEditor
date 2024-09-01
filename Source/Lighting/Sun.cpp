#include "Sun.h"

#include "Application.h"
#include "Engine.h"
#include "Scene.h"
#include "Lights/DirectionalLight.h"
#include "Math/GoknarMath.h"

#include "Goknar/Renderer/ShaderBuilderNew.h"

Sun::Sun() :
	ObjectBase()
{
	SetIsTickable(false);

	SetName("__Editor__Sun");

	sunLight_ = new DirectionalLight();
	sunLight_->SetDirection(sunLightDirection_);
	sunLight_->SetColor(Vector3(1.f, 1.f, 1.f));
	sunLight_->SetIntensity(2.f);
	sunLight_->SetShadowIntensity(0.75f);
	sunLight_->SetIsShadowEnabled(true);
	sunLight_->SetLightMobility(LightMobility::Static);
	sunLight_->SetShadowWidth(8192);
	sunLight_->SetShadowHeight(8192);
	sunLight_->SetName("__Editor__Sun");
}

Sun::~Sun()
{
	delete sunLight_;
}

void Sun::BeginGame()
{
	sunLight_->SetDirection(sunLightDirection_);
	sunRotationAxis_ = Vector3::Cross(sunLightDirection_, Vector3::Cross(sunLightDirection_.GetOrthonormalBasis(), sunLightDirection_));
}

void Sun::Tick(float deltaTime)
{
	sunLightDirection_ = sunLightDirection_.RotateVectorAroundAxis(sunRotationAxis_, sunLightRotationSpeed_ * deltaTime);
	sunLight_->SetDirection(sunLightDirection_);
}
