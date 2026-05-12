#pragma once

#include "Goknar/Math/GoknarMath.h"
#include "Goknar/Materials/MaterialBase.h"
#include "Goknar/Materials/MaterialFunction.h"

#include "imgui.h"

#include <string>
#include <variant>
#include <vector>

using ShaderPinType = MaterialFunctionPinType;
using ShaderValue = std::variant<float, Vector2, Vector3, Vector4, std::string>;

enum class ShaderEditorAssetMode
{
    Material = 0,
    MaterialFunction
};

enum class ShaderPinKind
{
    Input,
    Output
};

struct ShaderPin
{
    int id{ -1 };
    int nodeId{ -1 };
    std::string name{};
    ShaderPinType type{ ShaderPinType::None };
    ShaderPinKind kind{ ShaderPinKind::Input };

    ShaderValue defaultValue{ 0.0f };
};

struct ShaderNode
{
    int id{ -1 };
    std::string name{};
    std::string typeCategory{};
    ImVec2 pos{ 0.0f, 0.0f };
    ImVec2 size{ 0.0f, 0.0f };

    std::vector<ShaderPin> inputs{};
    std::vector<ShaderPin> outputs{};

    std::string stringData{};
    bool isUniform{ true };
    std::vector<ShaderValue> arrayDefaultValues{};
    bool useTextureAtlas{ true };
};

struct ShaderLink
{
    int id{ -1 };
    int startPinId{ -1 }; // Always an Output pin
    int endPinId{ -1 };   // Always an Input pin
};

struct ShaderEditorTextureInfo
{
    std::string name{};
    std::string path{};
    bool useTextureAtlas{ true };
};
