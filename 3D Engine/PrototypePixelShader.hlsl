Texture2D Texture : register(t0);
sampler TextureSampler : register(s0);

struct PS_INPUT
{
    float4 pos : SV_POSITION0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD0;
    float3 cameraDir : TEXCOORD2;
    float3 lightDir : TEXCOORD3;
};

cbuffer constant : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 model;
    row_major float4x4 invTransModel;
    row_major float4x4 view;
    row_major float4x4 projection;
    float3 cameraPos;
    float3 lightPos;
    float3 lightColor;
    unsigned int time;
};

cbuffer material : register(b1)
{
    float ambient;
    float diffuse;
    float specular;
    float shininess;
    float textureScale;
    float4 color;
    bool isTextured;
};

float3 calculateLighting(float ambient, float diffuse, float specular, float shininess, float3 lightColor, float3 normal, float3 lightDir, float3 cameraDir)
{
	//AMBIENT LIGHT
    float3 ambient_light = ambient * lightColor;

	//DIFFUSE LIGHT
    float amount_diffuse_light = (dot(lightDir, normal) + 1.0f) * 0.5f;
    float3 diffuse_light = diffuse * lightColor * amount_diffuse_light;

	//SPECULAR LIGHT
    float3 reflected_light = reflect(lightDir, normal);
    float amount_specular_light = pow(max(0.0, dot(reflected_light, cameraDir)), shininess);

    float3 specular_light = specular * amount_specular_light * lightColor;

    float3 final_light = ambient_light + diffuse_light + specular_light;
	
    return final_light;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float3 Normal = normalize(mul(input.normal, (float3x3) invTransModel));
    
    float3 lighting = calculateLighting(ambient, diffuse, specular, shininess, lightColor, Normal, input.lightDir, input.cameraDir);
    
    float4 baseColor = color;
    
    if (isTextured)
    {
        float4 textureSample = Texture.Sample(TextureSampler, input.texCoord * textureScale);
        baseColor = color * (1.0f - textureSample.a) + textureSample * textureSample.a;
    }
    
    float4 result = baseColor * float4(lighting, 1.0f);
    
    return result;
}
