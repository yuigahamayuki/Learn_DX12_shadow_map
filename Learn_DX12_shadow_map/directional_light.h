#pragma once

#include "common_headers.h"

using namespace DirectX;

class DirectionalLight {
public:
  DirectionalLight();
  ~DirectionalLight() = default;

  DirectionalLight(const DirectionalLight&) = default;
  DirectionalLight& operator=(const DirectionalLight&) = default;

  XMFLOAT4 world_direction() const {
    return XMFLOAT4(world_direction_.x, world_direction_.y, world_direction_.z, 0.0f);
  }

  XMFLOAT4 light_color() const {
    return XMFLOAT4(light_color_.x, light_color_.y, light_color_.z, 1.0f);
  }

private:
  XMFLOAT3 world_direction_;
  XMFLOAT3 light_color_;
};  // class DirectionalLight