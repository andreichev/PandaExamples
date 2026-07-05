// Postprocess pass fragment shader. The engine feeds:
//   inputTexture    — frame from the previous pass (or the scene for the first pass),
//   originalTexture — frame before the whole chain,
//   POST_DATA       — resolutionTime.xy = frame size in pixels, .z = time in seconds.
// Any extra uniforms/textures you declare become editable material fields.
cbuffer POST_DATA {
    float4 resolutionTime;
};

Texture2D inputTexture;
SamplerState inputTextureSampler;
Texture2D originalTexture;
SamplerState originalTextureSampler;

struct PSInput {
    float2 fragTexCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0 {
    float4 color = inputTexture.Sample(inputTextureSampler, input.fragTexCoord);
    // Example effect: vignette. Replace with your own.
    float2 centered = input.fragTexCoord - 0.5;
    float vignette = 1.0 - dot(centered, centered) * 0.9;
    return float4(color.rgb * vignette, color.a);
}
