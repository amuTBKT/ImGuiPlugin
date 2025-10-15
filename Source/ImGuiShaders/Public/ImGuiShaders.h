// Copyright 2024-25 Amit Kumar Mehar. All Rights Reserved.

#pragma once

#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "ShaderParameterStruct.h"

class IMGUISHADERS_API FImGuiVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FImGuiVS, Global);

public:
	FImGuiVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ProjectionMatrixParam.Bind(Initializer.ParameterMap, TEXT("ProjectionMatrix"));
	}
	FImGuiVS() {}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	void SetParameters(
		FRHIBatchedShaderParameters& BatchedParameters,
		const FMatrix44f& ProjectionMatrix)
	{
		SetShaderValue(BatchedParameters, ProjectionMatrixParam, ProjectionMatrix);
	}

private:
	LAYOUT_FIELD(FShaderParameter, ProjectionMatrixParam);
};

class IMGUISHADERS_API FImGuiPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FImGuiPS, Global);

public:
	FImGuiPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TextureParam.Bind(Initializer.ParameterMap, TEXT("Texture"));
		TextureSamplerParam.Bind(Initializer.ParameterMap, TEXT("TextureSampler"));
		SrgbParam.Bind(Initializer.ParameterMap, TEXT("IsTextureSrgb"));
		ShaderStateOverridesParam.Bind(Initializer.ParameterMap, TEXT("ShaderStateOverrides"));
	}
	FImGuiPS() {}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	void SetParameters(
		FRHIBatchedShaderParameters& BatchedParameters,
		FRHITexture* Texture,
		FRHISamplerState* SamplerState,
		bool EnableSrgb,
		uint32 ShaderStateOverrides)
	{
		SetTextureParameter(BatchedParameters, TextureParam, Texture);
		SetSamplerParameter(BatchedParameters, TextureSamplerParam, SamplerState);
		SetShaderValue(BatchedParameters, SrgbParam, EnableSrgb ? 1 : 0);
		SetShaderValue(BatchedParameters, ShaderStateOverridesParam, ShaderStateOverrides);
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, TextureParam);
	LAYOUT_FIELD(FShaderResourceParameter, TextureSamplerParam);
	LAYOUT_FIELD(FShaderParameter, SrgbParam);
	LAYOUT_FIELD(FShaderParameter, ShaderStateOverridesParam);
};
