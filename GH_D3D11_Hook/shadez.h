#pragma once

constexpr const char* szShadez = R"(
// Constant buffer
cbuffer ConstantBuffer : register(b0)
{
	matrix projection;
}

// PSI (PixelShaderInput)
struct PSI
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
};

// VertexShader
PSI VS( float4 pos : POSITION, float4 color : COLOR )
{
	PSI psi;
	psi.color = color;
	pos = mul( pos, projection );
	psi.pos = pos;
	return psi;
}

// PixelShader
float4 PS(PSI psi) : SV_TARGET
{
	return psi.color;
}
)";