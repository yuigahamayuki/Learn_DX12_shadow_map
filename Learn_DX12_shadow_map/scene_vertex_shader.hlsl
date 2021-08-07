cbuffer SceneConstantBuffer : register(b0)
{
  float4x4 model;
  float4x4 view;
  float4x4 proj;
};

struct PSInput {
	float4 pos : SV_POSITION;
	float3 color : COLOR;
	float3 world_pos : POSITION;
	float3 world_normal : NORMAL;
};

PSInput main( float3 pos : POSITION, float3 normal : NORMAL, float3 color : COLOR)
{
	PSInput ps_input;
	ps_input.pos = float4(pos, 1.0f);
	ps_input.pos = mul(ps_input.pos, model);
	ps_input.world_pos = ps_input.pos.xyz;
	ps_input.pos = mul(ps_input.pos, view);
	ps_input.pos = mul(ps_input.pos, proj);
	ps_input.color = color;

	ps_input.world_normal = normalize(mul(float4(normal, 0.0f), model)).xyz;
	return ps_input;
}