#pragma once

#include <vector>

#include "common_headers.h"
#include "d3dx12.h"
#include "camera.h"
#include "directional_light.h"
#include "point_light.h"
#include "spot_light.h"

using Microsoft::WRL::ComPtr;

struct InputState
{
  bool rightArrowPressed;
  bool leftArrowPressed;
  bool upArrowPressed;
  bool downArrowPressed;
};

struct SceneConstantBuffer {
  XMFLOAT4X4 model;
  XMFLOAT4X4 view;
  XMFLOAT4X4 proj;
  XMFLOAT4 light_world_direction_or_position;  // direction: directional_light, position: point light or spot light
  XMFLOAT4 light_color;
  XMFLOAT4 camera_world_pos;
  XMFLOAT4X4 light_view_proj_transform;
  int light_type;
};

class Scene {
public:
  Scene(UINT frame_count, UINT width, UINT height);
  ~Scene();
  
  void Initialize(ID3D12Device* device, ID3D12CommandQueue* command_queue, UINT frame_index);
  void LoadSizeDependentResources(ID3D12Device* device, ComPtr<ID3D12Resource>* render_targets, UINT width, UINT height);
  void Update();
  void Render(ID3D12CommandQueue* command_queue);
  void KeyDown(UINT8 key);
  void KeyUp(UINT8 key);

  void SetFrameIndex(UINT frame_index) {
    current_frame_index_ = frame_index;
  }

private:
  enum class LightType {
    kDirectionLight = 0,
    kPointLight = 1,
    kSpotLight = 2,
    kLightTypeNumber = 3,
  };

  void CreateConstanfBuffer(ID3D12Device* device, UINT size, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initState);

  void CreateDescriptorHeaps(ID3D12Device* device);
  void CreatePipelineStates(ID3D12Device* device);
  void CreateAndMapConstantBuffers(ID3D12Device* device);
  void CreateShadowPipelineState(ID3D12Device* device);
  void CreateAndMapShadowConstantBuffer(ID3D12Device* device);
  void CreateScenePipelineState(ID3D12Device* device);
  void CreateAndMapSceneConstantBuffer(ID3D12Device* device);
  void CreateCameraDrawPipelineState(ID3D12Device* device);
  void LoadAssets(ID3D12Device* device);
  void LoadModelVerticesAndIndices(ID3D12Device* device);
  void LoadTextures(ID3D12Device* device);
  void CreateCameraPoints(ID3D12Device* device);
  void UpdateConstantBuffers();
  void CommitConstantBuffers(UINT object_index);
  void CommitConstantBuffersForAllObjects();
  void SetCameras();
  void PopulateCommandLists();
  void ShadowPass();
  void ScenePass();
  void DrawCameras();

  UINT GetCbvSrvUavDescriptorsNumber() const {
    return  kDepthBufferCount_ + 1;  // 1 means the number of diffuse textures is 1.
  }

  // Update vertices of camera points
  void UpdateVerticesOfCameraPoints();

  UINT frame_count_ = 0;
  UINT current_frame_index_ = 0;
  static constexpr UINT kTotalCameraCount_ = 4;
  static constexpr UINT kDepthBufferCount_ = 2;

  // D3D objects
  ComPtr<ID3D12RootSignature> shadow_root_signature_;
  ComPtr<ID3D12PipelineState> shadow_pipeline_state_;
  ComPtr<ID3D12RootSignature> scene_root_signature_;
  ComPtr<ID3D12PipelineState> scene_pipeline_state_;
  std::vector<ComPtr<ID3D12Resource>> scene_constant_buffer_views_;  // each frame has its own constant buffer view
  ComPtr<ID3D12RootSignature> camera_draw_root_signature_;
  ComPtr<ID3D12PipelineState> camera_draw_pipeline_state_;
  std::vector<ComPtr<ID3D12CommandAllocator>> command_allocators_;
  ComPtr<ID3D12GraphicsCommandList> command_list_;
  std::vector<ComPtr<ID3D12Resource>> render_targets_;
  ComPtr<ID3D12Resource> vertex_buffer_;
  ComPtr<ID3D12Resource> vertex_upload_heap_;
  ComPtr<ID3D12Resource> index_buffer_;
  ComPtr<ID3D12Resource> index_upload_heap_;
  D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view_{};
  D3D12_INDEX_BUFFER_VIEW index_buffer_view_{};
  ComPtr<ID3D12Resource> camera_points_vertex_buffer_;
  ComPtr<ID3D12Resource> camera_points_vertex_upload_heap_;
  D3D12_VERTEX_BUFFER_VIEW camera_points_vertex_buffer_view_{};
  std::vector<ComPtr<ID3D12Resource>> model_textures_;
  std::vector<ComPtr<ID3D12Resource>> model_textures_upload_heap_;
  std::vector<ComPtr<ID3D12Resource>> depth_textures_;  // 0: shadow depth texture; 1: scene depth texture

  // Heap objects
  ComPtr<ID3D12DescriptorHeap> rtv_descriptor_heap_;
  UINT rtv_descriptor_increment_size_ = 0;
  ComPtr<ID3D12DescriptorHeap> dsv_descriptor_heap_;
  UINT dsv_descriptor_size_ = 0;
  ComPtr<ID3D12DescriptorHeap> cbv_srv_descriptor_heap_;
  UINT cbv_srv_descriptor_increment_size_ = 0;

  CD3DX12_VIEWPORT view_port_;
  CD3DX12_RECT scissor_rect_;

  InputState keyboard_input_;

  std::vector<Camera> cameras_;
  UINT camera_index_ = 0;  // camera index of current viewing camera

  SceneConstantBuffer scene_constant_buffer_;
  std::vector<void*> scene_constant_buffer_pointers_;
  UINT scene_constant_buffer_aligned_size_ = 0;

  // light related
  DirectionalLight directional_light_;
  PointLight point_light_;
  SpotLight spot_light_;
  LightType light_type_ = LightType::kDirectionLight;
  Camera light_camera_;  // for shadow mapping
};