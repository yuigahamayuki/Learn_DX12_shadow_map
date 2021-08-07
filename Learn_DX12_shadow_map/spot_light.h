#pragma once

#include "common_headers.h"

using namespace DirectX;

class SpotLight {
public:
  SpotLight();
  ~SpotLight() = default;

  SpotLight(const SpotLight&) = default;
  SpotLight& operator=(const SpotLight&) = default;

  XMFLOAT4 world_pos() const {
    return XMFLOAT4(world_pos_.x, world_pos_.y, world_pos_.z, 0.0f);
  }

  XMFLOAT4 light_color() const {
    return XMFLOAT4(light_color_.x, light_color_.y, light_color_.z, 1.0f);
  }

private:
  XMFLOAT3 world_pos_;
  XMFLOAT3 light_color_;
};