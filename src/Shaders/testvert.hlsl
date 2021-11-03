#pragma shader_stage(vertex)

#include "ShaderDefines.h"

struct VSInput {
    [[vk::location(VAT_POSITION)]] float3 Position : POSITION;
    [[vk::location(VAT_COLOR)]] float3 Color : COLOR;
};

struct VSOutput {
    float4 Position : SV_POSITION;
    [[vk::location(0)]] float3 Color : COLOR;
};

struct UBO {
    float4x4 model;
    float4x4 vp;
};

cbuffer ubo : register(b0) 
{ 
    UBO ubo; 
}

VSOutput main(VSInput input) {
    VSOutput output = (VSOutput)0;
    output.Position = mul(ubo.vp, mul(ubo.model, float4(input.Position, 1.0)));
    output.Color = input.Color;
    return output;
}