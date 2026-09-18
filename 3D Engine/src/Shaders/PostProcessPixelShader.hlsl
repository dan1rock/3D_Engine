Texture2D Scene : register(t2);
Texture2D Bloom : register(t3);
sampler SourceSampler : register(s2);

struct PS_INPUT
{
    float4 pos : SV_POSITION0;
    float2 texCoord : TEXCOORD0;
};

cbuffer postProcess : register(b0)
{
    float4 texelSize;
    float4 blurDirection;
    float4 bloomParams;
    float4 vignetteParams;
    float4 vignetteColor;
};

// Затемнює зображення до країв кадру
float3 applyVignette(float3 color, float2 texCoord)
{
    float intensity = vignetteParams.x;
    if (intensity <= 0.0f) return color;

    float smoothness = saturate(vignetteParams.y);

	// Обидві осі нормуються незалежно, тому лінії однакового затемнення є овалом за формою екрана
    float2 offset = (texCoord - 0.5f) * 2.0f;

	// У центрі кадру відстань нульова, на серединах країв - близько 0.71, у кутах - одинична
    float distance = length(offset) * 0.70711f;

	// Затемнення починається не від центру, а на межі, заданій плавністю переходу
    float vignette = 1.0f - smoothstep(1.0f - smoothness, 1.0f, distance);

    return lerp(vignetteColor.rgb, color, lerp(1.0f, vignette, intensity));
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float3 color = Scene.Sample(SourceSampler, input.texCoord).rgb;

	// Світіння додається поверх кадру, бо є додатковим світлом, а не заміною кольору
    if (bloomParams.w > 0.0f)
    {
        color += Bloom.Sample(SourceSampler, input.texCoord).rgb * bloomParams.z;
    }

    color = applyVignette(color, input.texCoord);

    return float4(color, 1.0f);
}
