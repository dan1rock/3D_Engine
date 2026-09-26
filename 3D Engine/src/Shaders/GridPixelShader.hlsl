// Кількість каскадів карти тіней; те саме значення визначено в GlobalResources.h
#define SHADOW_CASCADE_COUNT 4

cbuffer constant: register(b0)
{
	row_major float4x4 world;
	row_major float4x4 model;
    row_major float4x4 invTransModel;
	row_major float4x4 view;
	row_major float4x4 projection;
    row_major float4x4 lightViewProjection[SHADOW_CASCADE_COUNT];
    float3 cameraPos;
    float cameraPosPadding;
    float3 lightPos;
    float lightPosPadding;
    float3 lightColor;
    float lightColorPadding;
    float3 lightDir;
    float lightDirPadding;
    float4 shadowParams;
    float4 cascadeSplits;
    float4 cascadeBias;
    float4 cascadeParams;
	unsigned int time;
}

struct PS_INPUT
{
    float4 pos : SV_POSITION0;
    float3 worldPos : TEXCOORD0;
};

// Колір ліній та їхня непрозорість: дрібні лінії ледь помітні, кожна десята - виразніша
static const float3 LINE_COLOR = float3(0.62f, 0.62f, 0.62f);
static const float MINOR_ALPHA = 0.28f;
static const float MAJOR_ALPHA = 0.5f;

// Повертає покриття пікселя лініями сітки з вказаним кроком: одиниця на лінії, нуль між ними
float gridLines(float2 coord, float cell)
{
    float2 scaled = coord / cell;

	// Скільки клітинок припадає на один піксель екрана вздовж кожної осі
    float2 width = max(fwidth(scaled), 0.00001f);

	// Відстань до найближчої лінії у пікселях дає згладжену лінію завтовшки близько пікселя
    float2 distance = abs(frac(scaled - 0.5f) - 0.5f) / width;
    float lines = 1.0f - saturate(min(distance.x, distance.y));

	// Коли клітинки стають дрібнішими за кілька пікселів, лінії зливаються в брижі, тож гаснуть
    float density = max(width.x, width.y);

    return lines * (1.0f - smoothstep(0.15f, 0.45f, density));
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float minor = gridLines(input.worldPos.xz, 1.0f) * MINOR_ALPHA;
    float major = gridLines(input.worldPos.xz, 10.0f) * MAJOR_ALPHA;

    float alpha = max(minor, major);

	// Що вище камера, то дальше видно сітку; межа лишається ближчою за дальню площину відсікання
    float height = abs(cameraPos.y);
    float fadeEnd = clamp(40.0f + height * 6.0f, 40.0f, 180.0f);

    float distance = length(input.worldPos - cameraPos);
    alpha *= 1.0f - smoothstep(fadeEnd * 0.35f, fadeEnd, distance);

	// Під малим кутом площина стискається в смужку, і лінії там лише мерехтіли б
    float3 viewDir = normalize(input.worldPos - cameraPos);
    alpha *= saturate(abs(viewDir.y) * 6.0f);

    if (alpha <= 0.001f) discard;

    return float4(LINE_COLOR, alpha);
}
