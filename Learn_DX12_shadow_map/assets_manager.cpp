#include "assets_manager.h"

#include <cstring>

AssetsManager& AssetsManager::GetSharedInstance()
{
  static AssetsManager instance;
  return instance;
}

void AssetsManager::GetMergedVerticesAndIndices(std::unique_ptr<Asset::Model::Vertex[]>& vertices_data, std::unique_ptr<DWORD[]>& indices_data)
{
  auto total_vertex_number = GetTotalModelVertexNumber();
  vertices_data = std::make_unique<Asset::Model::Vertex[]>(total_vertex_number);

  auto total_index_number = GetTotalModelIndexNumber();
  indices_data = std::make_unique<DWORD[]>(total_index_number);
  
  size_t current_merged_vertex_number = 0;
  size_t current_merged_index_number = 0;
  std::for_each(models_.cbegin(), models_.cend(), [&vertices_data, &indices_data, &current_merged_vertex_number, &current_merged_index_number](const std::unique_ptr<Asset::Model>& model) {
    Asset::Model::Vertex* current_vertex_insert_ptr = vertices_data.get() + current_merged_vertex_number;
    std::unique_ptr<Asset::Model::Vertex[]> single_model_vertices_data = model->GetVertexData();
    std::memcpy(current_vertex_insert_ptr, single_model_vertices_data.get(), model->GetVertexDataSize());
    current_merged_vertex_number += model->GetVertexNumber();

    DWORD* current_index_insert_ptr = indices_data.get() + current_merged_index_number;
    std::unique_ptr<DWORD[]> single_model_indices_data = model->GetIndexData();
    std::memcpy(current_index_insert_ptr, single_model_indices_data.get(), model->GetIndexDataSize());
    current_merged_index_number += model->GetIndexNumber();
  });
}

AssetsManager::AssetsManager()
{

}

AssetsManager::~AssetsManager()
{
}