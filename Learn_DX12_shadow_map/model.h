#pragma once

#include <memory>

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

 private:

};  // class Model

}  // namespace Asset