cbuffer SceneConstantBuffer : register(b0)
{
  float4x4 light_view_proj_transform;
};

float4 main( float3 pos : POSITION ) : SV_POSITION
{
	return mul(float4(pos, 1.0f), light_view_proj_transform);
}