cbuffer MATERIAL_FIELDS : register(b0) {
    float4 baseColorFactor;
    float alphaCutoff;
};

cbuffer PANDA_FIELDS : register(b1) {
    float time;
};

struct PSInput {
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float2 fragTexCoord : TEXCOORD2;
    float4 fragColor : TEXCOORD3;
    float fragLight : TEXCOORD4;
    nointerpolation uint fragId : TEXCOORD5;
    bool isFrontFace : SV_IsFrontFace;
};

cbuffer SCENE_LIGHTS {
    float4 ambientColorIntensity;
    float4 directionalDirectionIntensity;
    float4 directionalColor;
    uint4 sceneLightCounts;
    float4 pointLightPositionsRange[8];
    float4 pointLightColorsIntensity[8];
};

Texture2D baseColorTexture;
SamplerState baseColorTextureSampler;

float computeSpecular(float3 normal, float3 lightDirection, float roughness, float metallic) {
    float3 viewDirection = normalize(float3(0.0, 0.0, 1.0));
    float3 halfVector = normalize(lightDirection + viewDirection);
    float specularPower = lerp(64.0, 4.0, roughness);
    return pow(saturate(dot(normal, halfVector)), specularPower) * lerp(0.04, 1.0, metallic);
}

float4 main(PSInput input) : SV_Target0 {
    float4 baseColorSample = baseColorTexture.Sample(baseColorTextureSampler, input.fragTexCoord);

    float3 surfaceNormal = normalize(input.worldNormal);

    float4 baseColor = baseColorSample * baseColorFactor * input.fragColor;
    float alpha = baseColor.a;
    if (alpha < alphaCutoff) {
        discard;
    }

    float3 lighting = ambientColorIntensity.rgb * ambientColorIntensity.w;

    if (directionalDirectionIntensity.w > 0.0) {
        float3 lightDirection = normalize(directionalDirectionIntensity.xyz);
        float3 radiance = directionalColor.rgb * directionalDirectionIntensity.w;
        float diffuse = saturate(dot(surfaceNormal, lightDirection));
        float specular = computeSpecular(surfaceNormal, lightDirection, 0.0, 0.0);
        lighting += radiance * diffuse;
        lighting += radiance * specular;
    }

    float3 litColor = baseColor.xyz * lighting * input.fragLight;
    return float4(litColor, alpha);
}
