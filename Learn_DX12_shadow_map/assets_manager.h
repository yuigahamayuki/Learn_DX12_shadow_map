#pragma once

#include <vector>
#include <memory>
#include <algorithm>

#include "model.h"

class AssetsManager {
 public:
  static AssetsManager& GetSharedInstance();

  ~AssetsManager();

  AssetsManager(const AssetsManager&) = delete;
  AssetsManager& operator=(const AssetsManager&) = delete;

  void InsertModel(std::unique_ptr<Asset::Model> model) {
    models_.emplace_back(std::move(model));
  }

  size_t GetTotalModelVertexNumber() const {
    size_t total_vertex_number = 0;
    std::for_each(models_.cbegin(), models_.cend(), [&total_vertex_number](const std::unique_ptr<Asset::Model>& model) {
      total_vertex_number += model->GetVertexNumber();
     });

    return total_vertex_number;
  }

  size_t GetTotalModelIndexNumber() const {
    size_t total_index_number = 0;
    std::for_each(models_.cbegin(), models_.cend(), [&total_index_number](const std::unique_ptr<Asset::Model>& model) {
      total_index_number += model->GetIndexNumber();
      });

    return total_index_number;
  }

  size_t GetTotalModelVertexSize() const {
    size_t total_vertex_size = 0;
    std::for_each(models_.cbegin(), models_.cend(), [&total_vertex_size](const std::unique_ptr<Asset::Model>& model) {
      total_vertex_size += model->GetVertexDataSize();
      });

    return total_vertex_size;
  }

  size_t GetTotalModelIndexSize() const {
    size_t total_index_size = 0;
    std::for_each(models_.cbegin(), models_.cend(), [&total_index_size](const std::unique_ptr<Asset::Model>& model) {
      total_index_size += model->GetIndexDataSize();
      });

    return total_index_size;
  }

  void GetMergedVerticesAndIndices(std::unique_ptr<Asset::Model::Vertex[]>& vertices_data, std::unique_ptr<DWORD[]>& indices_data);

 private:
  AssetsManager();
  
  std::vector<std::unique_ptr<Asset::Model>> models_;
};