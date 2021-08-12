#pragma once

#include <memory>
#include <string>

#include "common_headers.h"

using namespace DirectX;

namespace Asset {

class Model {
 public:
  struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 uv;
    XMFLOAT3 color;
  };  // struct Vertex

  static size_t GetVertexStride() {
    return sizeof(Vertex);
  }

  virtual ~Model() {

  }

  virtual std::unique_ptr<Vertex[]> GetVertexData() const = 0;

  virtual size_t GetVertexDataSize() const = 0;

  virtual size_t GetVertexNumber() const = 0;

  virtual std::unique_ptr<DWORD[]> GetIndexData() const = 0;

  virtual size_t GetIndexDataSize() const = 0;

  virtual size_t GetIndexNumber() const = 0;

  virtual const std::string GetTextureImageFileName() const = 0;

  void SetModelTransform(const XMMATRIX& model_transform_matrix) {
    XMStoreFloat4x4(&model_transform_, model_transform_matrix);
  }

  XMFLOAT4X4 GetModelTransform() const {
    return model_transform_;
  }

 private:
   XMFLOAT4X4 model_transform_;
};  // class Model

}  // namespace Asset