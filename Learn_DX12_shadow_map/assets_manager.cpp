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

void AssetsManager::GetModelDrawArguments(std::vector<DrawArgument>& draw_arguments)
{
  if (models_.empty()) {
    return;
  }

  draw_arguments.clear();
  draw_arguments.resize(models_.size());

  draw_arguments[0].index_count = static_cast<UINT>(models_[0]->GetIndexNumber());
  draw_arguments[0].index_start = 0;
  draw_arguments[0].vertex_base = 0;
  if (models_[0]->GetTextureImageFileName() != "") {
    draw_arguments[0].diffuse_texture_index = 0;
  }

  UINT accumulated_index_start = static_cast<UINT>(models_[0]->GetIndexNumber());
  UINT accumulated_vertex_base = static_cast<UINT>(models_[0]->GetVertexNumber());
  UINT accumulated_diffuse_texture_index = 1;
  for (auto i = 1; i < draw_arguments.size(); ++i) {
    draw_arguments[i].index_count = static_cast<UINT>(models_[i]->GetIndexNumber());
    draw_arguments[i].index_start = accumulated_index_start;
    draw_arguments[i].vertex_base = accumulated_vertex_base;

    accumulated_index_start += static_cast<UINT>(models_[i]->GetIndexNumber());
    accumulated_vertex_base += static_cast<UINT>(models_[i]->GetVertexNumber());

    if (models_[i]->GetTextureImageFileName() != "") {
      draw_arguments[i].diffuse_texture_index = accumulated_diffuse_texture_index;
      accumulated_diffuse_texture_index++;  // Note: may be changed for extra textures, such as normal map
    }
  }
}

AssetsManager::AssetsManager()
{

}

AssetsManager::~AssetsManager()
{
}