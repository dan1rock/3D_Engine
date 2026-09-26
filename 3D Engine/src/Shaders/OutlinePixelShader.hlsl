Texture2D Mask : register(t2);

struct PS_INPUT
{
    float4 pos : SV_POSITION0;
    float2 texCoord : TEXCOORD0;
};

// Кольори обведення, як у Unity: помаранчевий для вибраного об'єкта, синій для його нащадків
static const float4 SELECTED_COLOR = float4(1.0f, 0.4f, 0.0f, 1.0f);
static const float4 CHILDREN_COLOR = float4(0.37f, 0.58f, 1.0f, 1.0f);

// Товщина обведення у пікселях
static const int RADIUS = 2;

// Малює обведення там, де піксель лежить поза силуетом, але поруч з ним
float4 main(PS_INPUT input) : SV_TARGET
{
    int2 pixel = int2(input.pos.xy);

    float2 self = Mask.Load(int3(pixel, 0)).rg;

	// Найбільше значення маски в колі навколо пікселя; за межами текстури Load дає нуль
    float2 nearby = float2(0.0f, 0.0f);

    [unroll]
    for (int y = -RADIUS; y <= RADIUS; y++)
    {
        [unroll]
        for (int x = -RADIUS; x <= RADIUS; x++)
        {
			// Кути квадрата відкидаються, щоб обведення мало заокруглені, а не зрізані кути
            if (x * x + y * y > RADIUS * RADIUS + 1) continue;

            nearby = max(nearby, Mask.Load(int3(pixel + int2(x, y), 0)).rg);
        }
    }

	// Сам силует лишається незафарбованим, як у Unity, видно лише його край
    if (self.r < 0.5f && nearby.r > 0.5f) return SELECTED_COLOR;
    if (self.r < 0.5f && self.g < 0.5f && nearby.g > 0.5f) return CHILDREN_COLOR;

    discard;
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
