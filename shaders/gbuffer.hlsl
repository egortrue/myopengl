#include "pack.h"

struct VertexInput
{
    float3 position : POSITION;
    uint32_t tangent;
    uint32_t normal;
    uint32_t uv;
};

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

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 tangent;
    float3 normal;
    float3 wPos;
    float2 uv;
};

struct InstancePushConstants 
{
    int32_t instanceId;
};
[[vk::push_constant]] ConstantBuffer<InstancePushConstants> pconst;

cbuffer ubo
{
    float4x4 viewToProj;
    float4x4 worldToView;
    float4x4 lightSpaceMatrix;
    float4 lightPosition;
    float3 CameraPos;
    float pad;
    uint32_t debugView;
}

Texture2D textures[];
SamplerState gSampler;
StructuredBuffer<Material> materials;
StructuredBuffer<InstanceConstants> instanceConstants;

[shader("vertex")]
PS_INPUT vertexMain(VertexInput vi)
{
    PS_INPUT out;
    InstanceConstants constants = instanceConstants[NonUniformResourceIndex(pconst.instanceId)];
    float4 wpos = mul(constants.model, float4(vi.position, 1.0f));
    out.pos = mul(viewToProj, mul(worldToView, wpos));
    out.uv = unpackUV(vi.uv);
    // assume that we don't use non-uniform scales
    // TODO:
    out.normal = unpackNormal(vi.normal);
    out.tangent = unpackTangent(vi.tangent);
    out.wPos = wpos.xyz / wpos.w; 
    return out;
}

float3 CalcBumpedNormal(PS_INPUT inp, uint32_t texId)
{
    float3 Normal = normalize(inp.normal);
    float3 Tangent = -normalize(inp.tangent);
    Tangent = normalize(Tangent - dot(Tangent, Normal) * Normal);
    float3 Bitangent = cross(Normal, Tangent);

    float3 BumpMapNormal = textures[NonUniformResourceIndex(texId)].Sample(gSampler, inp.uv).xyz;
    BumpMapNormal = BumpMapNormal * 2.0 - 1.0;

    float3x3 TBN = transpose(float3x3(Tangent, Bitangent, Normal));
    float3 NewNormal = normalize(mul(TBN, BumpMapNormal));

    return NewNormal;
}

#define INVALID_INDEX -1
#define PI 3.1415926535897

struct FSOutput
{
    float4 wPos : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 tangent : SV_TARGET2;
    float2 uv : SV_TARGET3;
    int instId : SV_TARGET4;
};

// Fragment Shader
[shader("fragment")]
FSOutput fragmentMain(PS_INPUT inp) : SV_TARGET
{
    InstanceConstants constants = instanceConstants[NonUniformResourceIndex(pconst.instanceId)];
    Material material = materials[NonUniformResourceIndex(constants.materialId)];

    int32_t texNormalId = material.texNormalId;

    float3 N = normalize(inp.normal);
    if (texNormalId != INVALID_INDEX)
    {
        N = CalcBumpedNormal(inp, texNormalId);
    }

    FSOutput ret;
    ret.wPos = float4(inp.wPos, 1.0);
    ret.normal = float4(N, 0.0);
    ret.tangent = float4(inp.tangent, 0.0);
    ret.uv = inp.uv;
    ret.instId = pconst.instanceId;

    return ret;
}
