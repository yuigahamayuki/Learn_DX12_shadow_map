cbuffer SceneConstantBuffer : register(b0)
{
  float4x4 model;
  float4x4 view;
  float4x4 proj;
  float4 light_world_direction_or_position;
  float4 light_color;
  float4 camera_world_pos;
  float4x4 light_view_proj_transform;
  int light_type;  // 0: directional light; 1: point light; 2: spot light
};

float4 main( float3 pos : POSITION ) : SV_POSITION
{
	return mul(float4(pos, 1.0f), light_view_proj_transform);
}