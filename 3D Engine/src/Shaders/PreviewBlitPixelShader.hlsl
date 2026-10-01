Texture2D Source : register(t0);
sampler SourceSampler : register(s3);

struct PS_INPUT
{
    float4 pos : SV_POSITION0;
    float2 texCoord : TEXCOORD0;
};

// Копіює текстуру в мініатюру як є: мініатюра зберігає ті самі закодовані значення кольору, що й файл
float4 main(PS_INPUT input) : SV_TARGET
{
    return Source.Sample(SourceSampler, input.texCoord);
}
