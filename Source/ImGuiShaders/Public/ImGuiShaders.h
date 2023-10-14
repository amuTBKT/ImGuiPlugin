#pragma once

#include "CoreMinimal.h"

#include "ShaderParameterStruct.h"
#include "ShaderParameterUtils.h"
#include "GlobalShader.h"
#include "Shader.h"

class IMGUISHADERS_API FImGuiVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FImGuiVS, Global);

public:
	FImGuiVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VertexBufferParam.Bind(Initializer.ParameterMap, TEXT("VertexBuffer"));
		ProjectionMatrixParam.Bind(Initializer.ParameterMap, TEXT("ProjectionMatrix"));
		GlobalVertexOffsetParam.Bind(Initializer.ParameterMap, TEXT("GlobalVertexOffset"));
	}
	FImGuiVS() {}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	void SetParameters(
		FRHIBatchedShaderParameters& BatchedParameters,
		FRHIShaderResourceView* VertexBufferSRV,
		const FMatrix44f& ProjectionMatrix,
		uint32 VertexOffset)
	{
		SetSRVParameter(BatchedParameters, VertexBufferParam, VertexBufferSRV);
		SetShaderValue(BatchedParameters, ProjectionMatrixParam, ProjectionMatrix);
		SetShaderValue(BatchedParameters, GlobalVertexOffsetParam, VertexOffset);
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, VertexBufferParam);
	LAYOUT_FIELD(FShaderParameter, ProjectionMatrixParam);
	LAYOUT_FIELD(FShaderParameter, GlobalVertexOffsetParam);
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
		bool EnableSrgb)
	{
		SetTextureParameter(BatchedParameters, TextureParam, Texture);
		SetSamplerParameter(BatchedParameters, TextureSamplerParam, SamplerState);
		SetShaderValue(BatchedParameters, SrgbParam, EnableSrgb ? 1 : 0);
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, TextureParam);
	LAYOUT_FIELD(FShaderResourceParameter, TextureSamplerParam);
	LAYOUT_FIELD(FShaderParameter, SrgbParam);
};
