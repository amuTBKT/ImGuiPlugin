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

	void SetVertexOffset(FRHICommandList& RHICmdList, uint32 VertexOffset)
	{
		SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), GlobalVertexOffsetParam, VertexOffset);
	}

	void SetProjectionMatrix(FRHICommandList& RHICmdList, const FMatrix44f& ProjectionMatrix)
	{
		SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), ProjectionMatrixParam, ProjectionMatrix);
	}

	void SetVertexBuffer(FRHICommandList& RHICmdList, FRHIShaderResourceView* VertexSRV)
	{
		SetSRVParameter(RHICmdList, RHICmdList.GetBoundVertexShader(), VertexBufferParam, VertexSRV);
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
	}
	FImGuiPS() {}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	void SetTexture(FRHICommandList& RHICmdList, FRHITexture* Texture)
	{
		SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), TextureParam, Texture);
	}

	void SetTextureSampler(FRHICommandList& RHICmdList, FRHISamplerState* SamplerState)
	{
		SetSamplerParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), TextureSamplerParam, SamplerState);
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, TextureParam);
	LAYOUT_FIELD(FShaderResourceParameter, TextureSamplerParam);
};
