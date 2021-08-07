#include "directional_light.h"

DirectionalLight::DirectionalLight() :
  world_direction_(),
  light_color_(1.0f, 1.0f, 1.0f)
{
  XMVECTOR world_direction_vector = XMVector3Normalize(XMVectorSet(1.0f, -1.732f, -0.5f, 0.0f));
  XMStoreFloat3(&world_direction_, world_direction_vector);
}
