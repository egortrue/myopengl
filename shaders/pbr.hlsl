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

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 posLightSpace;
    float3 tangent;
    float3 normal;
    float3 wPos;
    float2 uv;
};

struct InstancePushConstants 
{
    float4x4 model;
    int32_t materialId;
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
Texture2D shadowMap;
SamplerState shadowSamp;

static const float4x4 biasMatrix = float4x4(
  0.5, 0.0, 0.0, 0.5,
  0.0, -0.5, 0.0, 0.5,
  0.0, 0.0, 1.0, 0.0,
  0.0, 0.0, 0.0, 1.0 );

[shader("vertex")]
PS_INPUT vertexMain(VertexInput vi)
{
    PS_INPUT out;
    float4 wpos = mul(pconst.model, float4(vi.position, 1.0f));
    out.pos = mul(viewToProj, mul(worldToView, wpos));
    out.posLightSpace = mul(biasMatrix, mul(lightSpaceMatrix, wpos));
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

float ShadowCalculation(float4 lightCoord)
{
    const float bias = 0.005;
    float shadow = 1.0;
    float3 projCoord = lightCoord.xyz / lightCoord.w;
    if (abs(projCoord.z) < 1.0)
    {
        float2 coord = float2(projCoord.x, projCoord.y);
        float distFromLight = shadowMap.Sample(shadowSamp, coord).r;
        if (distFromLight < projCoord.z)
        {
            shadow = 0.1;
        }
    }

    return shadow;
}

float ShadowCalculationPcf(float4 lightCoord)
{
    const float bias = 0.005;
    float shadow = 0.1;
    float3 projCoord = lightCoord.xyz / lightCoord.w;
    if (abs(projCoord.z) < 1.0)
    {
      float2 coord = float2(projCoord.x, projCoord.y);
      // pcf
      float2 texelSize = 1.0 / float2(1024, 1024);
        for (int x = -1; x <= 1; ++x)
        {
            for(int y = -1; y <= 1; ++y)
            {
                float pcfDepth = shadowMap.Sample(shadowSamp, coord + float2(x, y) * texelSize).r;
                shadow += projCoord.z > pcfDepth ? 0.1 : 1.0;
            }
        }
        shadow /= 9.0;
    }

    return shadow;
}

// Returns a random number based on a float3 and an int.
float random(float3 seed, int i){
	float4 seed4 = float4(seed,i);
	float dot_product = dot(seed4, float4(12.9898, 78.233, 45.164, 94.673));

	return frac(sin(dot_product) * 43758.5453);
}

float ShadowCalculationPoisson(float4 lightCoord, float3 wPos)
{
    float2 poissonDisk[16] = {
        float2(-0.94201624, -0.39906216),  float2(0.94558609, -0.76890725),
        float2(-0.094184101, -0.92938870), float2(0.34495938, 0.29387760),
        float2(-0.91588581, 0.45771432),   float2(-0.81544232, -0.87912464),
        float2(-0.38277543, 0.27676845),   float2(0.97484398, 0.75648379),
        float2(0.44323325, -0.97511554),   float2(0.53742981, -0.47373420),
        float2(-0.26496911, -0.41893023),  float2(0.79197514, 0.19090188),
        float2(-0.24188840, 0.99706507),   float2(-0.81409955, 0.91437590),
        float2(0.19984126, 0.78641367),    float2(0.14383161, -0.14100790)};

    float shadow = 1.0;
    float3 projCoord = lightCoord.xyz / lightCoord.w;
    if (abs(projCoord.z) < 1.0)
    {
        float2 coord = float2(projCoord.x, projCoord.y);
        // poisson
        for (int i = 0; i < 4; i++)
        {
            int index = int(16.0 * random(floor(wPos.xyz * 1000.0), i)) % 16;
            if (shadowMap.Sample(shadowSamp, coord + poissonDisk[index] / 700).r < projCoord.z)
            {
                shadow -= 0.2;
            }
        }
    }

    return shadow;
}

float ShadowCalculationPoissonPCF(float4 lightCoord, float3 wPos)
{
  float2 poissonDisk[16] = {
      float2(-0.94201624, -0.39906216),  float2(0.94558609, -0.76890725),
      float2(-0.094184101, -0.92938870), float2(0.34495938, 0.29387760),
      float2(-0.91588581, 0.45771432),   float2(-0.81544232, -0.87912464),
      float2(-0.38277543, 0.27676845),   float2(0.97484398, 0.75648379),
      float2(0.44323325, -0.97511554),   float2(0.53742981, -0.47373420),
      float2(-0.26496911, -0.41893023),  float2(0.79197514, 0.19090188),
      float2(-0.24188840, 0.99706507),   float2(-0.81409955, 0.91437590),
      float2(0.19984126, 0.78641367),    float2(0.14383161, -0.14100790)};

  float shadow = 1.0;
  float3 projCoord = lightCoord.xyz / lightCoord.w;
  float2 coord = float2(projCoord.x, projCoord.y);

  if (abs(projCoord.z) < 1.0)
  {
      float2 texelSize = 1.0 / float2(1024, 1024);
      // pcf
      for (int x = -1; x <= 1; ++x)
      {
          for(int y = -1; y <= 1; ++y)
          {
              float pcfDepth = shadowMap.Sample(shadowSamp, coord + float2(x, y) * texelSize).r;
              if (projCoord.z > pcfDepth)
              {
                  shadow += 0.1;
              }
              else
              {
                  shadow += 0.8;
                  // poisson
                  for (int i = 0; i < 4; i++)
                  {
                      int index = int(16.0 * random(floor(wPos.xyz * 1000.0), i)) % 16;
                      if (shadowMap.Sample(shadowSamp, coord + poissonDisk[index] / 700).r < projCoord.z)
                      {
                          shadow -= 0.2;
                      }
                  }
              }
          }
      }
      shadow /= 9.0;
  }

  return shadow;
}

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

struct PointData
{
    float NL;
    float NV;
    float NH;
    float HV;
};

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

// Fragment Shader
[shader("fragment")]
float4 fragmentMain(PS_INPUT inp) : SV_TARGET
{
    Material material = materials[NonUniformResourceIndex(pconst.materialId)];

    int32_t texNormalId = material.texNormalId;

    float3 N = normalize(inp.normal);
    if (texNormalId != INVALID_INDEX)
    {
        N = CalcBumpedNormal(inp, texNormalId);
    }
    float3 L = normalize(lightPosition.xyz - inp.wPos);

    PointData pointData;

    pointData.NL = dot(N, L);

    float3 V = normalize(CameraPos - inp.wPos);
    pointData.NV = dot(N, V);

    float3 H = normalize(V + L);
    pointData.HV = dot(H, V);
    pointData.NH = dot(N, H);

    float3 result = cookTorrance(material, pointData, inp.uv);

    if (material.texEmissive != INVALID_INDEX)
    {
        float3 emissive = textures[NonUniformResourceIndex(material.texEmissive)].Sample(gSampler, inp.uv).rgb;
        result += emissive;
    }
    if (material.texOcclusion != INVALID_INDEX)
    {
        float occlusion = textures[NonUniformResourceIndex(material.texOcclusion)].Sample(gSampler, inp.uv).r;
        result *= occlusion;
    }
    float shadow = ShadowCalculation(inp.posLightSpace);

    // Normals
    if (debugView == 1)
    {
      return float4(abs(N), 1.0);
    }
    // Shadow b&w
    if (debugView == 2)
    {
       return float4(dot(N, L) * shadow, 1.0);
    }
    // pcf shadow
    if (debugView == 3)
    {
        shadow = ShadowCalculationPcf(inp.posLightSpace);
    }
    // poisson shadow
    if (debugView == 4)
    {
        shadow = ShadowCalculationPoisson(inp.posLightSpace, inp.wPos);
    }
    // poisson + pcf shadow
    if (debugView == 5)
    {
        shadow = ShadowCalculationPoissonPCF(inp.posLightSpace, inp.wPos);
    }
    
    result *= shadow;

    float alpha = material.d;

    return float4(float3(result), alpha);
}
