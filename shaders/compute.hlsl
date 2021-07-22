cbuffer ubo
{
    float4x4 viewToProj;
    float4x4 worldToView;
    float4x4 lightSpaceMatrix;
    float4 lightPosition;
    float3 CameraPos;
    float pad0;
    int2 dimension;
    uint32_t debugView;
    float pad1;
}

struct Material
{
    float4 ambient; // Ka
    float4 diffuse; // Kd
    float4 specular; // Ks
    float4 emissive; // Ke
    float4 transparency; //  d 1 -- прозрачность/непрозрачность
    float opticalDensity; // Ni
    float shininess; // Ns 16 --  блеск материала
    uint32_t illum; // illum 2 -- модель освещения
    int32_t texDiffuseId; // map_diffuse

    int32_t texAmbientId; // map_ambient
    int32_t texSpecularId; // map_specular
    int32_t texNormalId; // map_normal - map_Bump
    float d; // alpha value
    //====PBR====
    float4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    int32_t metallicRoughnessTexture;
    int32_t texBaseColor;

    float3 emissiveFactor;
    int32_t texEmissive;

    int32_t texOcclusion;
    int32_t pad0;
    int32_t pad1;
    int32_t pad2;
};

struct InstanceConstants
{
    float4x4 model;
    float4x4 normalMatrix;
    int32_t materialId;
    int32_t pad0;
    int32_t pad1;
    int32_t pad2;
};


Texture2D<float> gbDepth;
Texture2D<float4> gbWPos;
Texture2D<float4> gbNormal;
Texture2D<float4> gbTangent;
Texture2D<float2> gbUV;
Texture2D<int> gbInstId;

Texture2D textures[]; // bindless

SamplerState gSampler;

StructuredBuffer<Material> materials;
StructuredBuffer<InstanceConstants> instanceConstants;

RWTexture2D<float4> output;

struct PointData
{
    float NL;
    float NV;
    float NH;
    float HV;
};

#define INVALID_INDEX -1
#define PI 3.1415926535897

float GGX_PartialGeometry(float cosThetaN, float alpha) 
{
    float cosTheta_sqr = saturate(cosThetaN*cosThetaN);
    float tan2 = (1.0 - cosTheta_sqr) / cosTheta_sqr;
    float GP = 2.0 / (1.0 + sqrt(1.0 + alpha * alpha * tan2));
    return GP;
}

float GGX_Distribution(float cosThetaNH, float alpha) 
{
    float alpha2 = alpha * alpha;
    float NH_sqr = saturate(cosThetaNH * cosThetaNH);
    float den = NH_sqr * alpha2 + (1.0 - NH_sqr);
    return alpha2 / (PI * den * den);
}

float3 FresnelSchlick(float3 F0, float cosTheta) 
{
    return F0 + (1.0 - F0) * pow(1.0 - saturate(cosTheta), 5.0);
}

float3 cookTorrance(in Material material, in PointData pd, in float2 uv)
{
    if (pd.NL <= 0.0 || pd.NV <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }

    float roughness = material.roughnessFactor;
    float roughness2 = roughness * roughness;

    float G = GGX_PartialGeometry(pd.NV, roughness2) * GGX_PartialGeometry(pd.NL, roughness2);
    float D = GGX_Distribution(pd.NH, roughness2);

    float3 f0 = float3(0.24, 0.24, 0.24);
    float3 F = FresnelSchlick(f0, pd.HV);
    //mix
    float3 specK = G * D * F * 0.25 / pd.NV;
    float3 diffK = saturate(1.0 - F);

    float3 albedo = material.baseColorFactor.rgb;
    if (material.texBaseColor != INVALID_INDEX)
    {
        albedo *= textures[NonUniformResourceIndex(material.texBaseColor)].Sample(gSampler, uv).rgb;
    }


    float3 result = max(0.0, albedo * diffK * pd.NL / PI + specK);
    return result;
}

float4 calc(uint2 pixelIndex)
{
    int instId = gbInstId[pixelIndex];
    if (instId < 0)
    {
        return float4(1.0);
    }
    InstanceConstants constants = instanceConstants[NonUniformResourceIndex(instId)];
    Material material = materials[NonUniformResourceIndex(constants.materialId)];

    float3 wpos = gbWPos[pixelIndex].xyz;
    float3 L = normalize(lightPosition.xyz - wpos);
    float3 N = gbNormal[pixelIndex].xyz;
    
    PointData pointData;
    pointData.NL = dot(N, L);
    float3 V = normalize(CameraPos - wpos);
    pointData.NV = dot(N, V);
    float3 H = normalize(V + L);
    pointData.HV = dot(H, V);
    pointData.NH = dot(N, H);
    float2 uv = gbUV[pixelIndex].xy;

    float3 result = cookTorrance(material, pointData, uv);

    if (material.texEmissive != INVALID_INDEX)
    {
        float3 emissive = textures[NonUniformResourceIndex(material.texEmissive)].Sample(gSampler, uv).rgb;
        result += emissive;
    }
    if (material.texOcclusion != INVALID_INDEX)
    {
        float occlusion = textures[NonUniformResourceIndex(material.texOcclusion)].Sample(gSampler, uv).r;
        result *= occlusion;
    }

    return float4(result, 1.0);
}

[numthreads(16, 16, 1)]
[shader("compute")]
void computeMain(uint2 pixelIndex : SV_DispatchThreadID)
{
    output[pixelIndex] = calc(pixelIndex);
}
