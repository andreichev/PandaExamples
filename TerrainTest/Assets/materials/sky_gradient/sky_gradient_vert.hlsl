cbuffer UBO_VERT {
    float4x4 projViewMtx;
    float4x4 model;
};

struct VSInput {
    float3 position : POSITION;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(projViewMtx, mul(model, float4(input.position, 1.0)));
    output.direction = input.position;
    return output;
}
