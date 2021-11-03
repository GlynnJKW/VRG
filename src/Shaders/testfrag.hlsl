#pragma shader_stage(fragment)

struct VSOutput {
    float4 Position : SV_POSITION;
    [[vk::location(0)]] float3 Color : COLOR;
};

struct PSOutput {
    float4 Color : SV_TARGET0;
};

PSOutput main(VSOutput input) {
    PSOutput output = (PSOutput)0;
    output.Color = float4(input.Color, 1.0);
    return output;
}