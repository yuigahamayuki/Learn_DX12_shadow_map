#pragma once

#include "model.h"

namespace Asset {

class QuadModel : public Model {
 public:
  std::unique_ptr<Vertex[]> GetVertexData() const override {
    std::unique_ptr<Vertex[]> vertices_data = std::make_unique<Vertex[]>(4);

    vertices_data[0].position = XMFLOAT3(-1.0f, 0.0f, -1.0f);
    vertices_data[0].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
    vertices_data[0].uv = XMFLOAT2(0.0f, 1.0f);

    vertices_data[1].position = XMFLOAT3(-1.0f, 0.0f, 1.0f);
    vertices_data[1].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
    vertices_data[1].uv = XMFLOAT2(0.0f, 0.0f);

    vertices_data[2].position = XMFLOAT3(1.0f, 0.0f, -1.0f);
    vertices_data[2].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
    vertices_data[2].uv = XMFLOAT2(1.0f, 1.0f);

    vertices_data[3].position = XMFLOAT3(1.0f, 0.0f, 1.0f);
    vertices_data[3].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
    vertices_data[3].uv = XMFLOAT2(1.0f, 0.0f);

    return vertices_data;
  }

  size_t GetVertexDataSize() const override {
    return sizeof(Vertex) * 4;
  }

  size_t GetVertexNumber() const override {
    return 4;
  }

  std::unique_ptr<DWORD[]> GetIndexData() const override {
    std::unique_ptr<DWORD[]> indices_data = std::make_unique<DWORD[]>(6);

    indices_data[0] = 0;
    indices_data[1] = 1;
    indices_data[2] = 2;
    indices_data[3] = 2;
    indices_data[4] = 1;
    indices_data[5] = 3;

    return indices_data;
  }

  size_t GetIndexDataSize() const override {
    return sizeof(DWORD) * 6;
  }

  size_t GetIndexNumber() const override {
    return 6;
  }

 private:

};  // class QuadModel

}  // namespace Asset