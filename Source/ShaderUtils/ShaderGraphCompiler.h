#pragma once

#include "Goknar/Core.h"
#include "Goknar/Materials/Material.h"
#include "UI/Panels/ShaderEditor/ShaderEditorTypes.h"

#include <vector>

struct GOKNAR_API ShaderGraphCompileInput
{
    const std::vector<ShaderNode>* nodes{ nullptr };
    const std::vector<ShaderLink>* links{ nullptr };
    const std::vector<ShaderEditorTextureInfo>* textures{ nullptr };
    int masterNodeId{ -1 };
};

class GOKNAR_API ShaderGraphCompiler
{
public:
    void CompileMaterial(const ShaderGraphCompileInput& input, MaterialInitializationData* outMaterialData) const;
};
