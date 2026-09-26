struct PS_INPUT
{
    float4 pos : SV_POSITION0;
};

cbuffer outlineMask : register(b0)
{
    // Канал, у який пише поточний об'єкт: червоний - вибраний, зелений - його нащадки
    float4 maskValue;
};

// Записує силует об'єкта в маску виділення; колір і світло тут не потрібні
float4 main(PS_INPUT input) : SV_TARGET
{
    return maskValue;
}
