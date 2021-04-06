struct VertexInput
{
    float3 position : POSITION;
    float3 tangent;
    uint32_t normal;
    uint32_t uv;
    uint16_t materialId;
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
  uint32_t texDiffuseId; // map_diffuse

  uint32_t texAmbientId; // map_ambient
  uint32_t texSpecularId; // map_specular
  uint32_t texNormalId; // map_normal - map_Bump
  uint32_t pad;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 tangent;
    float3 normal;
    float3 wPos;
    float2 uv;
    nointerpolation uint32_t materialId;
};

cbuffer ubo
{
    float4x4 modelToWorld;
    float4x4 modelViewProj;
    float4x4 worldToView;
    float4x4 inverseModelToWorld;
    float4 lightDirect;
    float3 CameraPos;
}

Texture2D textures[];
SamplerState gSampler;
StructuredBuffer<Material> materials;

//  valid range of coordinates [-1; 1]
float3 unpackNormal(uint32_t val)
{
   float3 normal;
   normal.z = ((val & 0xfff00000) >> 20) / 511.99999f * 2.0f - 1.0f;
   normal.y = ((val & 0x000ffc00) >> 10) / 511.99999f * 2.0f - 1.0f;
   normal.x = (val & 0x000003ff) / 511.99999f * 2.0f - 1.0f;

   return normal;
}

//  valid range of coordinates [-6; 6]
float2 unpackUV(uint32_t val)
{
   float2 uv;
   uv.y = ((val & 0xffff0000) >> 16) / 16383.99999f * 12.0f - 6.0f;
   uv.x = (val & 0x0000ffff) / 16383.99999f * 12.0f  - 6.0f;

   return uv;
}

[shader("vertex")]
PS_INPUT vertexMain(VertexInput vi)
{
    PS_INPUT out;
    out.pos = mul(modelViewProj, float4(vi.position, 1.0f));

    out.uv = unpackUV(vi.uv);
    out.normal = mul((float3x3)inverseModelToWorld, unpackNormal(vi.normal));
    out.tangent = mul((float3x3)inverseModelToWorld, normalize(vi.tangent));
    out.materialId = vi.materialId;
    out.wPos = mul(vi.position, (float3x3)modelToWorld);

    return out;
}

float3 diffuseLambert(float3 kD, float3 n, float3 l)
{
    return kD * saturate(dot(l, n));
}

float3 specularPhong(float3 kS, float3 r, float3 v)
{
    return kS * pow(saturate(dot(r, v)), 30);
}

float3 CalcBumpedNormal(PS_INPUT inp, uint32_t texId)
{
    float3 Normal = normalize(inp.normal);
    float3 Tangent = normalize(inp.tangent);
    Tangent = normalize(Tangent - dot(Tangent, Normal) * Normal);

    float3 Bitangent = cross(Tangent, Normal);
    float3 BumpMapNormal = textures[texId].Sample(gSampler, inp.uv).xyz;
    BumpMapNormal = normalize(2.0 * BumpMapNormal - float3(1.0, 1.0, 1.0));

    float3 NewNormal;
    float3x3 TBN = transpose(float3x3(Tangent, Bitangent, Normal));
    NewNormal = mul(TBN, BumpMapNormal);
    NewNormal = normalize(NewNormal);

    return NewNormal;
}

// Fragment Shader
[shader("fragment")]
float4 fragmentMain(PS_INPUT inp) : SV_TARGET
{
   float3 emissive = float3(materials[inp.materialId].emissive.rgb);
   float opticalDensity = float(materials[inp.materialId].opticalDensity);
   float shininess = float(materials[inp.materialId].shininess);
   float3 transparency = float3(materials[inp.materialId].transparency.rgb);
   uint32_t illum = 2;

   uint32_t texAmbientId = materials[inp.materialId].texAmbientId;
   uint32_t texDiffuseId = materials[inp.materialId].texDiffuseId;
   uint32_t texSpecularId = materials[inp.materialId].texSpecularId;
   uint32_t texNormalId = materials[inp.materialId].texNormalId;

   float3 kA = materials[inp.materialId].ambient.rgb;
   float3 kD = materials[inp.materialId].diffuse.rgb;
   float3 kS = materials[inp.materialId].specular.rgb;

   if (texAmbientId != (uint32_t) -1)
   {
      kA *= textures[texAmbientId].Sample(gSampler, inp.uv).rgb;
   }
   if (texDiffuseId != (uint32_t) -1)
   {
      kD *= textures[texDiffuseId].Sample(gSampler, inp.uv).rgb;
   }
   if (texSpecularId != (uint32_t) -1)
   {
      kS *= textures[texSpecularId].Sample(gSampler, inp.uv).rgb;
   }

   float3 N = normalize(inp.normal);
   if (texNormalId != (uint32_t) -1)
   {
      N = CalcBumpedNormal(inp, texNormalId);
   }

   float3 lightPos = float3(100.0f,100.0f,100.0f);
   
   //float3 L = normalize(lightPos - inp.wPos);
   float3 L = normalize(lightDirect.xyz);
   float3 diffuse = diffuseLambert(kD, L, N);

   float3 R = reflect(-L, N);
   float3 V = normalize(CameraPos - inp.wPos);
   float3 specular = specularPhong(kS, R, V);

  return float4(saturate(kA + diffuse + specular), 1.0f);
}
