#pragma once

#include "model.h"

namespace Asset {

class CubeModel : public Model {
 public:
   std::unique_ptr<Vertex[]> GetVertexData() const override {
     std::unique_ptr<Vertex[]> vertices_data = std::make_unique<Vertex[]>(8);

     vertices_data[0].position = XMFLOAT3(-0.5f, -0.5f, -0.5f);
     XMVECTOR temp_vector = XMLoadFloat3(&(vertices_data[0].position));
     temp_vector = XMVector3Normalize(temp_vector);
     XMStoreFloat3(&(vertices_data[0].normal), temp_vector);
     vertices_data[0].color = XMFLOAT3(0.74118f, 0.49020f, 0.45490f);

     vertices_data[1].position = XMFLOAT3(-0.5f, 0.5f, -0.5f);
     temp_vector = XMLoadFloat3(&(vertices_data[1].position));
     temp_vector = XMVector3Normalize(temp_vector);
     XMStoreFloat3(&(vertices_data[1].normal), temp_vector);
     vertices_data[1].color = XMFLOAT3(0.74118f, 0.49020f, 0.45490f);

     vertices_data[2].position = XMFLOAT3(0.5f, -0.5f, -0.5f);
     temp_vector = XMLoadFloat3(&(vertices_data[2].position));
     temp_vector = XMVector3Normalize(temp_vector);
     XMStoreFloat3(&(vertices_data[2].normal), temp_vector);
     vertices_data[2].color = XMFLOAT3(0.74118f, 0.49020f, 0.45490f);

     vertices_data[3].position = XMFLOAT3(0.5f, 0.5f, -0.5f);
     temp_vector = XMLoadFloat3(&(vertices_data[3].position));
     temp_vector = XMVector3Normalize(temp_vector);
     XMStoreFloat3(&(vertices_data[3].normal), temp_vector);
     vertices_data[3].color = XMFLOAT3(0.74118f, 0.49020f, 0.45490f);

     vertices_data[4].position = XMFLOAT3(-0.5f, -0.5f, 0.5f);
     temp_vector = XMLoadFloat3(&(vertices_data[4].position));
     temp_vector = XMVector3Normalize(temp_vector);
     XMStoreFloat3(&(vertices_data[4].normal), temp_vector);
     vertices_data[4].color = XMFLOAT3(0.74118f, 0.49020f, 0.45490f);

     vertices_data[5].position = XMFLOAT3(-0.5f, 0.5f, 0.5f);
     temp_vector = XMLoadFloat3(&(vertices_data[5].position));
     temp_vector = XMVector3Normalize(temp_vector);
     XMStoreFloat3(&(vertices_data[5].normal), temp_vector);
     vertices_data[5].color = XMFLOAT3(0.74118f, 0.49020f, 0.45490f);

     vertices_data[6].position = XMFLOAT3(0.5f, -0.5f, 0.5f);
     temp_vector = XMLoadFloat3(&(vertices_data[6].position));
     temp_vector = XMVector3Normalize(temp_vector);
     XMStoreFloat3(&(vertices_data[6].normal), temp_vector);
     vertices_data[6].color = XMFLOAT3(0.74118f, 0.49020f, 0.45490f);

     vertices_data[7].position = XMFLOAT3(0.5f, 0.5f, 0.5f);
     temp_vector = XMLoadFloat3(&(vertices_data[7].position));
     temp_vector = XMVector3Normalize(temp_vector);
     XMStoreFloat3(&(vertices_data[7].normal), temp_vector);
     vertices_data[7].color = XMFLOAT3(0.74118f, 0.49020f, 0.45490f);

     return vertices_data;
   }

   size_t GetVertexDataSize() const override {
     return sizeof(Vertex) * 8;
   }

   size_t GetVertexNumber() const override {
     return 8;
   }

   std::unique_ptr<DWORD[]> GetIndexData() const override {
     std::unique_ptr<DWORD[]> indices_data = std::make_unique<DWORD[]>(36);

     // front
     indices_data[0] = 0;
     indices_data[1] = 1;
     indices_data[2] = 2;
     indices_data[3] = 2;
     indices_data[4] = 1;
     indices_data[5] = 3;

     // back
     indices_data[6] = 6;
     indices_data[7] = 7;
     indices_data[8] = 4;
     indices_data[9] = 4;
     indices_data[10] = 7;
     indices_data[11] = 5;

     // left
     indices_data[12] = 4;
     indices_data[13] = 5;
     indices_data[14] = 0;
     indices_data[15] = 0;
     indices_data[16] = 5;
     indices_data[17] = 1;

     // right
     indices_data[18] = 2;
     indices_data[19] = 3;
     indices_data[20] = 6;
     indices_data[21] = 6;
     indices_data[22] = 3;
     indices_data[23] = 7;

     // top
     indices_data[24] = 1;
     indices_data[25] = 5;
     indices_data[26] = 3;
     indices_data[27] = 3;
     indices_data[28] = 5;
     indices_data[29] = 7;

     // bottom
     indices_data[30] = 4;
     indices_data[31] = 0;
     indices_data[32] = 6;
     indices_data[33] = 6;
     indices_data[34] = 0;
     indices_data[35] = 2;

     return indices_data;
   }

   size_t GetIndexDataSize() const override {
     return sizeof(DWORD) * 36;
   }

   size_t GetIndexNumber() const override {
     return 36;
   }

 private:

};

}  // namespace Asset