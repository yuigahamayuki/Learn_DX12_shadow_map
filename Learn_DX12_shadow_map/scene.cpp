#include "scene.h"

#include "dx_sample_helper.h"
#include "assets_manager.h"
#include "quad_model.h"
#include "cube_model.h"
#include "image_loader.h"

namespace {

inline HRESULT CreateDepthStencilTexture2D(
  ID3D12Device* device,
  UINT width,
  UINT height,
  DXGI_FORMAT typeless_format,
  DXGI_FORMAT dsv_format,
  DXGI_FORMAT srv_format,
  ID3D12Resource** pp_resource,
  D3D12_CPU_DESCRIPTOR_HANDLE dsv_cpu_descriptor_handle,
  D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu_descriptor_handle,
  D3D12_RESOURCE_STATES init_state = D3D12_RESOURCE_STATE_DEPTH_WRITE,
  float init_depth_value = 1.0f,
  UINT8 init_stencil_value = 0)
{
  try
  {
    *pp_resource = nullptr;

    CD3DX12_RESOURCE_DESC depth_texture_desc(
      D3D12_RESOURCE_DIMENSION_TEXTURE2D,
      0,
      width,
      height,
      1,
      1,
      typeless_format,
      1,
      0,
      D3D12_TEXTURE_LAYOUT_UNKNOWN,
      D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    CD3DX12_HEAP_PROPERTIES default_heap_properties(D3D12_HEAP_TYPE_DEFAULT);
    // Performance tip: Tell the runtime at resource creation the desired clear value.
    CD3DX12_CLEAR_VALUE depth_buffer_clear_value(dsv_format, init_depth_value, init_stencil_value);
    ThrowIfFailed(device->CreateCommittedResource(
      &default_heap_properties,
      D3D12_HEAP_FLAG_NONE,
      &depth_texture_desc,
      init_state,
      &depth_buffer_clear_value,
      IID_PPV_ARGS(pp_resource)));

    // Create a depth stencil view (DSV).
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format = dsv_format;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Texture2D.MipSlice = 0;
    device->CreateDepthStencilView(*pp_resource, &dsv_desc, dsv_cpu_descriptor_handle);

    // Create a shader resource view (SRV).
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = srv_format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(*pp_resource, &srv_desc, srv_cpu_descriptor_handle);
  }
  catch (HrException& e)
  {
    SAFE_RELEASE(*pp_resource);
    return e.Error();
  }
  return S_OK;

}

}  // namespace

Scene::Scene(UINT frame_count, UINT width, UINT height) : frame_count_(frame_count),
  view_port_(0.0f, 0.0f, (float)width, (float)height),
  scissor_rect_(0, 0, width, height)
{
  cameras_.resize(kTotalCameraCount_);
  depth_textures_.resize(kDepthBufferCount_);
}

Scene::~Scene()
{
}

void Scene::Initialize(ID3D12Device* device, ID3D12CommandQueue* command_queue, UINT frame_index)
{
  if (device == nullptr || command_queue == nullptr) {
    return;
  }

  SetFrameIndex(frame_index);

  SetCameras();

  CreateDescriptorHeaps(device);
  CreatePipelineStates(device);
  CreateAndMapConstantBuffers(device);

  command_allocators_.resize(frame_count_);
  for (UINT i = 0; i < frame_count_; ++i) {
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators_[i])));
  }
 
  ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators_[current_frame_index_].Get(), scene_pipeline_state_.Get(), IID_PPV_ARGS(&command_list_)));

  LoadAssets(device);

  CreateCameraPoints(device);

  ThrowIfFailed(command_list_->Close());
  ID3D12CommandList* command_lists[] = { command_list_.Get() };
  command_queue->ExecuteCommandLists(1, command_lists);
}

void Scene::LoadSizeDependentResources(ID3D12Device* device, ComPtr<ID3D12Resource>* render_targets, UINT width, UINT height)
{
  // Create render target views (RTVs).
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_cpu_descriptor_handle(rtv_descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
  for (UINT i = 0; i < frame_count_; ++i) {
    render_targets_.emplace_back(render_targets[i]);
    device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_cpu_descriptor_handle);
    rtv_cpu_descriptor_handle.Offset(rtv_descriptor_increment_size_);
  }

  // Create the depth stencil views (DSVs).
  CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_cpu_descriptor_handle(dsv_descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
  dsv_descriptor_size_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  CD3DX12_CPU_DESCRIPTOR_HANDLE depth_srv_descriptor_handle(cbv_srv_descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
  // i == 0: shadow; i == 1: scene
  for (auto i = 0; i < kDepthBufferCount_; ++i) {
    ThrowIfFailed(CreateDepthStencilTexture2D(device, width, height, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_FLOAT,
      &depth_textures_[i], dsv_cpu_descriptor_handle, depth_srv_descriptor_handle));

    dsv_cpu_descriptor_handle.Offset(dsv_descriptor_size_);
    depth_srv_descriptor_handle.Offset(cbv_srv_descriptor_increment_size_);
  }
}

void Scene::Update()
{
  const float angleChange = 0.002f;

  if (keyboard_input_.leftArrowPressed)
    cameras_[camera_index_].RotateAroundYAxis(-angleChange);
  if (keyboard_input_.rightArrowPressed)
    cameras_[camera_index_].RotateAroundYAxis(angleChange);
  if (keyboard_input_.upArrowPressed)
    cameras_[camera_index_].RotatePitch(-angleChange);
  if (keyboard_input_.downArrowPressed)
    cameras_[camera_index_].RotatePitch(angleChange);

  for (auto i = 0; i < cameras_.size(); ++i) {
    cameras_[i].UpdateDirections();
  }

  UpdateConstantBuffers();

  CommitConstantBuffers();
}

void Scene::Render(ID3D12CommandQueue* command_queue)
{
  PopulateCommandLists();

  ID3D12CommandList* command_lists[] = { command_list_.Get() };
  command_queue->ExecuteCommandLists(1, command_lists);
}

void Scene::KeyDown(UINT8 key)
{
  switch (key)
  {
  case VK_LEFT:
    keyboard_input_.leftArrowPressed = true;
    break;
  case VK_RIGHT:
    keyboard_input_.rightArrowPressed = true;
    break;
  case VK_UP:
    keyboard_input_.upArrowPressed = true;
    break;
  case VK_DOWN:
    keyboard_input_.downArrowPressed = true;
    break;
  case '1':
    camera_index_ = 0;
    break;
  case '2':
    camera_index_ = 1;
    break;
  case '3':
    camera_index_ = 2;
    break;
  case '4':
    camera_index_ = 3;
    break;
  case 'Q':
    light_type_ = LightType::kDirectionLight;
    break;
  case 'W':
    light_type_ = LightType::kPointLight;
    break;
  case 'E':
    light_type_ = LightType::kSpotLight;
    break;
  default:
    break;
  }
}

void Scene::KeyUp(UINT8 key)
{
  switch (key)
  {
  case VK_LEFT:
    keyboard_input_.leftArrowPressed = false;
    break;
  case VK_RIGHT:
    keyboard_input_.rightArrowPressed = false;
    break;
  case VK_UP:
    keyboard_input_.upArrowPressed = false;
    break;
  case VK_DOWN:
    keyboard_input_.downArrowPressed = false;
    break;
  default:
    break;
  }
}

void Scene::CreateConstanfBuffer(ID3D12Device* device, UINT size, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initState)
{
  *ppResource = nullptr;

  const UINT alignedSize = CalculateConstantBufferByteSize(size);
  auto upload_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto constant_buffer_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);
  ThrowIfFailed(device->CreateCommittedResource(
    &upload_heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &constant_buffer_resource_desc,
    initState,
    nullptr,
    IID_PPV_ARGS(ppResource)));

}

void Scene::CreateDescriptorHeaps(ID3D12Device* device)
{
  // Describe and create a render target view (RTV) descriptor heap.
  D3D12_DESCRIPTOR_HEAP_DESC rtv_descriptor_heap_desc{};
  rtv_descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtv_descriptor_heap_desc.NumDescriptors = frame_count_;
  rtv_descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  rtv_descriptor_heap_desc.NodeMask = 0;
  ThrowIfFailed(device->CreateDescriptorHeap(&rtv_descriptor_heap_desc, IID_PPV_ARGS(&rtv_descriptor_heap_)));
  rtv_descriptor_increment_size_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  // Describe and create a depth stencil view (DSV) descriptor heap.
  D3D12_DESCRIPTOR_HEAP_DESC dsv_descriptor_heap_desc{};
  dsv_descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsv_descriptor_heap_desc.NumDescriptors = kDepthBufferCount_;
  dsv_descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dsv_descriptor_heap_desc.NodeMask = 0;
  ThrowIfFailed(device->CreateDescriptorHeap(&dsv_descriptor_heap_desc, IID_PPV_ARGS(&dsv_descriptor_heap_)));

  // Describe and create a shader resource view (SRV) and constant 
  // buffer view (CBV) descriptor heap.  
  // Heap layout: 
  // 1) depth buffer views
  // 2) object diffuse textures views
  D3D12_DESCRIPTOR_HEAP_DESC cbv_descriptor_heap_desc = {};
  cbv_descriptor_heap_desc.NumDescriptors = GetCbvSrvUavDescriptorsNumber();
  cbv_descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbv_descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ThrowIfFailed(device->CreateDescriptorHeap(&cbv_descriptor_heap_desc, IID_PPV_ARGS(&cbv_srv_descriptor_heap_)));
  cbv_srv_descriptor_increment_size_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Scene::CreatePipelineStates(ID3D12Device* device)
{
  CreateShadowPipelineState(device);
  CreateScenePipelineState(device);
  CreateCameraDrawPipelineState(device);
}

void Scene::CreateAndMapConstantBuffers(ID3D12Device* device)
{
  CreateAndMapShadowConstantBuffer(device);
  CreateAndMapSceneConstantBuffer(device);
}

void Scene::CreateShadowPipelineState(ID3D12Device* device)
{
  D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
  // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
  featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

  if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
  {
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
  }

  CD3DX12_ROOT_PARAMETER1 root_parameters[1]{};
  root_parameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);
  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
  root_signature_desc.Init_1_1(_countof(root_parameters), root_parameters,
    0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
  );
  ComPtr<ID3DBlob> root_signature_blob;
  ComPtr<ID3DBlob> error;
  ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&root_signature_desc, featureData.HighestVersion, &root_signature_blob, &error));
  ThrowIfFailed(device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(), root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&shadow_root_signature_)));

  ComPtr<ID3DBlob> vertex_shader = CompileShader(L"shadow_vertex_shader.hlsl", nullptr, "main", "vs_5_0");
  ComPtr<ID3DBlob> pixel_shader = CompileShader(L"shadow_pixel_shader.hlsl", nullptr, "main", "ps_5_0");

  D3D12_INPUT_ELEMENT_DESC input_element_descs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
  };
  D3D12_INPUT_LAYOUT_DESC input_layout_desc{};
  input_layout_desc.pInputElementDescs = input_element_descs;
  input_layout_desc.NumElements = _countof(input_element_descs);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{};
  pipeline_state_desc.pRootSignature = shadow_root_signature_.Get();
  pipeline_state_desc.VS = CD3DX12_SHADER_BYTECODE(vertex_shader.Get());
  //pipeline_state_desc.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.Get());
  pipeline_state_desc.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
  pipeline_state_desc.SampleMask = UINT_MAX;
  pipeline_state_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());
  pipeline_state_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
  pipeline_state_desc.InputLayout = input_layout_desc;
  pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeline_state_desc.NumRenderTargets = 1;
  pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  pipeline_state_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  pipeline_state_desc.SampleDesc.Count = 1;
  pipeline_state_desc.NodeMask = 0;
  ThrowIfFailed(device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&shadow_pipeline_state_)));
}

void Scene::CreateAndMapShadowConstantBuffer(ID3D12Device* device)
{
  CreateConstanfBuffer(device, sizeof(light_view_proj_transform_), &shadow_constant_buffer_view_, D3D12_RESOURCE_STATE_GENERIC_READ);
  const CD3DX12_RANGE readRange(0, 0);
  shadow_constant_buffer_view_->Map(0, &readRange, &shadow_constant_buffer_pointer_);
}

void Scene::CreateScenePipelineState(ID3D12Device* device)
{
  D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

  // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
  featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

  if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
  {
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
  }

  CD3DX12_ROOT_PARAMETER1 root_parameters[2]{};
  // scene constant buffer
  root_parameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
  // texture
  CD3DX12_DESCRIPTOR_RANGE1 ranges[1]{};
  ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
  root_parameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

  // static sampler (Note: there is also dynamic sampler)
  CD3DX12_STATIC_SAMPLER_DESC static_sampler_desc{};
  static_sampler_desc.Init(0, D3D12_FILTER_MIN_MAG_MIP_POINT,
    D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
    0.0f, 0, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
    0.0f, D3D12_FLOAT32_MAX,
    D3D12_SHADER_VISIBILITY_PIXEL, 0);

  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
  root_signature_desc.Init_1_1(_countof(root_parameters), root_parameters,
    1, &static_sampler_desc,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
  );
  ComPtr<ID3DBlob> root_signature_blob;
  ComPtr<ID3DBlob> error;
  ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&root_signature_desc, featureData.HighestVersion, &root_signature_blob, &error));
  ThrowIfFailed(device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(), root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&scene_root_signature_)));

  ComPtr<ID3DBlob> vertex_shader = CompileShader(L"scene_vertex_shader.hlsl", nullptr, "main", "vs_5_0");
  ComPtr<ID3DBlob> pixel_shader = CompileShader(L"scene_pixel_shader.hlsl", nullptr, "main", "ps_5_0");

  D3D12_INPUT_ELEMENT_DESC input_element_descs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
  };
  D3D12_INPUT_LAYOUT_DESC input_layout_desc{};
  input_layout_desc.pInputElementDescs = input_element_descs;
  input_layout_desc.NumElements = _countof(input_element_descs);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{};
  pipeline_state_desc.pRootSignature = scene_root_signature_.Get();
  pipeline_state_desc.VS = CD3DX12_SHADER_BYTECODE(vertex_shader.Get());
  pipeline_state_desc.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.Get());
  pipeline_state_desc.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
  pipeline_state_desc.SampleMask = UINT_MAX;
  pipeline_state_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());
  pipeline_state_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
  pipeline_state_desc.InputLayout = input_layout_desc;
  pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeline_state_desc.NumRenderTargets = 1;
  pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  pipeline_state_desc.SampleDesc.Count = 1;
  pipeline_state_desc.NodeMask = 0;
  ThrowIfFailed(device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&scene_pipeline_state_)));
}

void Scene::CreateAndMapSceneConstantBuffer(ID3D12Device* device)
{
  CreateConstanfBuffer(device, sizeof(SceneConstantBuffer), &scene_constant_buffer_view_, D3D12_RESOURCE_STATE_GENERIC_READ);
  const CD3DX12_RANGE readRange(0, 0);
  scene_constant_buffer_view_->Map(0, &readRange, &scene_constant_buffer_pointer_);
}

void Scene::CreateCameraDrawPipelineState(ID3D12Device* device)
{
  D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

  // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
  featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

  if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
  {
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
  }

  CD3DX12_ROOT_PARAMETER1 root_parameters[1]{};
  root_parameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_GEOMETRY);

  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
  root_signature_desc.Init_1_1(1, root_parameters,
    0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS);
  ComPtr<ID3DBlob> root_signature_blob;
  ComPtr<ID3DBlob> error;
  ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&root_signature_desc, featureData.HighestVersion, &root_signature_blob, &error));
  ThrowIfFailed(device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(), root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&camera_draw_root_signature_)));

  ComPtr<ID3DBlob> vertex_shader = CompileShader(L"camera_draw_vertex_shader.hlsl", nullptr, "main", "vs_5_0");
  ComPtr<ID3DBlob> geometry_shader = CompileShader(L"camera_draw_geometry_shader.hlsl", nullptr, "main", "gs_5_1");
  ComPtr<ID3DBlob> pixel_shader = CompileShader(L"camera_draw_pixel_shader.hlsl", nullptr, "main", "ps_5_0");

  D3D12_INPUT_ELEMENT_DESC input_element_descs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOOR", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOOR", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };
  D3D12_INPUT_LAYOUT_DESC input_layout_desc{};
  input_layout_desc.NumElements = _countof(input_element_descs);
  input_layout_desc.pInputElementDescs = input_element_descs;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{};
  pipeline_state_desc.pRootSignature = camera_draw_root_signature_.Get();
  pipeline_state_desc.VS = CD3DX12_SHADER_BYTECODE(vertex_shader.Get());
  pipeline_state_desc.GS = CD3DX12_SHADER_BYTECODE(geometry_shader.Get());
  pipeline_state_desc.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.Get());
  pipeline_state_desc.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
  pipeline_state_desc.SampleMask = UINT_MAX;
  pipeline_state_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());
  pipeline_state_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
  pipeline_state_desc.InputLayout = input_layout_desc;
  pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
  pipeline_state_desc.NumRenderTargets = 1;
  pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  pipeline_state_desc.SampleDesc.Count = 1;
  pipeline_state_desc.NodeMask = 0;
  ThrowIfFailed(device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&camera_draw_pipeline_state_)));
}

void Scene::LoadAssets(ID3D12Device* device)
{
  LoadModelVerticesAndIndices(device);
  LoadTextures(device);
}

void Scene::LoadModelVerticesAndIndices(ID3D12Device* device)
{
  std::unique_ptr<Asset::Model> quad_model_ptr = std::make_unique<Asset::QuadModel>(Asset::QuadModel());
  std::unique_ptr<Asset::Model> cube_model_ptr = std::make_unique<Asset::CubeModel>(Asset::CubeModel());

  AssetsManager::GetSharedInstance().InsertModel(std::move(quad_model_ptr));
  AssetsManager::GetSharedInstance().InsertModel(std::move(cube_model_ptr));

  std::unique_ptr<Asset::Model::Vertex[]> vertices_data = nullptr;
  std::unique_ptr<DWORD[]> indices_data = nullptr;
  AssetsManager::GetSharedInstance().GetMergedVerticesAndIndices(vertices_data, indices_data);

  size_t vertex_data_size = AssetsManager::GetSharedInstance().GetTotalModelVertexSize();
  CD3DX12_HEAP_PROPERTIES default_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC vertex_buffer_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(vertex_data_size);
  ThrowIfFailed(device->CreateCommittedResource(&default_heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &vertex_buffer_resource_desc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr,
    IID_PPV_ARGS(&vertex_buffer_)));

  CD3DX12_HEAP_PROPERTIES upload_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  ThrowIfFailed(device->CreateCommittedResource(&upload_heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &vertex_buffer_resource_desc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&vertex_upload_heap_)));

  D3D12_SUBRESOURCE_DATA vertex_subresource_data{};
  vertex_subresource_data.pData = vertices_data.get();
  vertex_subresource_data.RowPitch = vertex_data_size;
  vertex_subresource_data.SlicePitch = vertex_data_size;
  UpdateSubresources(command_list_.Get(), vertex_buffer_.Get(), vertex_upload_heap_.Get(), 0, 0, 1, &vertex_subresource_data);

  vertex_buffer_view_.BufferLocation = vertex_buffer_->GetGPUVirtualAddress();
  vertex_buffer_view_.SizeInBytes = static_cast<UINT>(vertex_data_size);
  vertex_buffer_view_.StrideInBytes = static_cast<UINT>(Asset::Model::GetVertexStride());

  size_t index_data_size = AssetsManager::GetSharedInstance().GetTotalModelIndexSize();
  CD3DX12_RESOURCE_DESC index_buffer_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(index_data_size);
  ThrowIfFailed(device->CreateCommittedResource(&default_heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &index_buffer_resource_desc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr,
    IID_PPV_ARGS(&index_buffer_)));

  ThrowIfFailed(device->CreateCommittedResource(&upload_heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &index_buffer_resource_desc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&index_upload_heap_)));

  D3D12_SUBRESOURCE_DATA index_subresource_data{};
  index_subresource_data.pData = indices_data.get();
  index_subresource_data.RowPitch = index_data_size;
  index_subresource_data.SlicePitch = index_data_size;
  UpdateSubresources(command_list_.Get(), index_buffer_.Get(), index_upload_heap_.Get(), 0, 0, 1, &index_subresource_data);

  index_buffer_view_.BufferLocation = index_buffer_->GetGPUVirtualAddress();
  index_buffer_view_.SizeInBytes = static_cast<UINT>(index_data_size);
  index_buffer_view_.Format = DXGI_FORMAT_R32_UINT;
}

void Scene::LoadTextures(ID3D12Device* device)
{
  std::vector<std::string> model_textures_file_names;
  AssetsManager::GetSharedInstance().GetModelTexturesFileNames(model_textures_file_names);
  model_textures_.resize(model_textures_file_names.size());

  model_textures_upload_heap_.resize(model_textures_file_names.size());

  int texture_index = 0;
  CD3DX12_CPU_DESCRIPTOR_HANDLE cbv_srv_cpuHandle(cbv_srv_descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
  // See srv descriptor heap layout
  for (auto i = 0; i < kDepthBufferCount_; ++i) {
    cbv_srv_cpuHandle.Offset(cbv_srv_descriptor_increment_size_);
  }
  for (const auto& model_texture_file_name : model_textures_file_names) {
    if (!model_texture_file_name.empty()) {
      std::vector<uint8_t> texture_image_data;
      D3D12_RESOURCE_DESC texture_resource_desc{};
      int bytes_per_row = 0;
      auto image_size = ImageLoader::LoadImageDataFromFile(texture_image_data, texture_resource_desc, model_texture_file_name, bytes_per_row);

      CD3DX12_HEAP_PROPERTIES default_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
      ThrowIfFailed(device->CreateCommittedResource(&default_heap_properties,
        D3D12_HEAP_FLAG_NONE,
        &texture_resource_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&model_textures_[texture_index])));

      UINT64 texture_upload_buffer_size;
      // this function gets the size an upload buffer needs to be to upload a texture to the gpu.
      // each row must be 256 byte aligned except for the last row, which can just be the size in bytes of the row
      // eg. textureUploadBufferSize = ((((width * numBytesPerPixel) + 255) & ~255) * (height - 1)) + (width * numBytesPerPixel);
      //textureUploadBufferSize = (((imageBytesPerRow + 255) & ~255) * (textureDesc.Height - 1)) + imageBytesPerRow;
      device->GetCopyableFootprints(&texture_resource_desc, 0, 1, 0, nullptr, nullptr, nullptr, &texture_upload_buffer_size);

      CD3DX12_HEAP_PROPERTIES upload_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
      CD3DX12_RESOURCE_DESC upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(texture_upload_buffer_size);
      ThrowIfFailed(device->CreateCommittedResource(&upload_heap_properties,
        D3D12_HEAP_FLAG_NONE,
        &upload_buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&model_textures_upload_heap_[texture_index])));

      // Copy data to the intermediate upload heap and then schedule a copy
      // from the upload heap to the Texture2D.
      D3D12_SUBRESOURCE_DATA texture_data = {};
      texture_data.pData = texture_image_data.data();
      texture_data.RowPitch = bytes_per_row;
      texture_data.SlicePitch = image_size;  // size before aligned or aligned size?

      UpdateSubresources(command_list_.Get(), model_textures_[texture_index].Get(), model_textures_upload_heap_[texture_index].Get(), 0, 0, 1, &texture_data);

      // transition the texture default heap to a pixel shader resource (we will be sampling from this heap in the pixel shader to get the color of pixels)
      CD3DX12_RESOURCE_BARRIER texture_resource_barrier = CD3DX12_RESOURCE_BARRIER::Transition(model_textures_[texture_index].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      command_list_->ResourceBarrier(1, &texture_resource_barrier);

      // Describe and create an SRV.
      D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
      srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srv_desc.Format = texture_resource_desc.Format;
      srv_desc.Texture2D.MipLevels = texture_resource_desc.MipLevels;
      srv_desc.Texture2D.MostDetailedMip = 0;
      srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
      device->CreateShaderResourceView(model_textures_[texture_index].Get(), &srv_desc, cbv_srv_cpuHandle);
      cbv_srv_cpuHandle.Offset(cbv_srv_descriptor_increment_size_);
    }

    texture_index++;
  }

}

void Scene::CreateCameraPoints(ID3D12Device* device)
{
  CD3DX12_HEAP_PROPERTIES default_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  auto buffer_size = sizeof(Camera::Vertex) * (kTotalCameraCount_ - 1);
  CD3DX12_RESOURCE_DESC camera_points_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);
  ThrowIfFailed(device->CreateCommittedResource(&default_heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &camera_points_buffer_desc,
    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
    nullptr,
    IID_PPV_ARGS(&camera_points_vertex_buffer_)));
  NAME_D3D12_OBJECT(camera_points_vertex_buffer_);

  CD3DX12_HEAP_PROPERTIES upload_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  ThrowIfFailed(device->CreateCommittedResource(&upload_heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &camera_points_buffer_desc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&camera_points_vertex_upload_heap_)));

  camera_points_vertex_buffer_view_.BufferLocation = camera_points_vertex_buffer_->GetGPUVirtualAddress();
  camera_points_vertex_buffer_view_.SizeInBytes = static_cast<UINT>(buffer_size);
  camera_points_vertex_buffer_view_.StrideInBytes = static_cast<UINT>(sizeof(Camera::Vertex));
}

void Scene::UpdateConstantBuffers()
{
  XMStoreFloat4x4(&scene_constant_buffer_.model, XMMatrixIdentity());
  cameras_[camera_index_].Get3DViewProjMatricesLH(&scene_constant_buffer_.view, &scene_constant_buffer_.proj, 90.f, view_port_.Width, view_port_.Height, 0.01f, 10.0f);

  // update light related
  XMStoreFloat4(&scene_constant_buffer_.camera_world_pos, cameras_[camera_index_].mEye);
  scene_constant_buffer_.light_type = static_cast<int>(light_type_);

  switch (light_type_) {
    case LightType::kDirectionLight:
      scene_constant_buffer_.light_world_direction_or_position = directional_light_.world_direction();
      scene_constant_buffer_.light_color = directional_light_.light_color();
      break;

    case LightType::kPointLight:
      scene_constant_buffer_.light_world_direction_or_position = point_light_.world_pos();
      scene_constant_buffer_.light_color = point_light_.light_color();
      break;

    case LightType::kSpotLight:
      scene_constant_buffer_.light_world_direction_or_position = spot_light_.world_pos();
      scene_constant_buffer_.light_color = spot_light_.light_color();
      break;

    default:
      break;
  }

  // update shadow mapping related
  XMVECTOR light_camera_eye = XMLoadFloat4(&scene_constant_buffer_.light_world_direction_or_position);
  if (light_type_ == LightType::kDirectionLight) {
    light_camera_eye = XMVectorScale(light_camera_eye, -4.0f);
  }
  XMVECTOR light_camera_at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
  XMVECTOR light_camera_up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
  light_camera_.Set(light_camera_eye, light_camera_at, light_camera_up);
  XMFLOAT4X4 light_camera_view;
  XMFLOAT4X4 light_camera_proj;
  //light_camera_.GetOrthoProjMatricesLH(&light_camera_view, &light_camera_proj, view_port_.Width, view_port_.Height);
  light_camera_.Get3DViewProjMatricesLH(&light_camera_view, &light_camera_proj, 90.0f, view_port_.Width, view_port_.Height, 0.01f, 10.0f);
  XMMATRIX light_camera_view_matrix = XMLoadFloat4x4(&light_camera_view);
  XMMATRIX light_camera_proj_matrix = XMLoadFloat4x4(&light_camera_proj);
  XMMATRIX light_view_proj_transform_matrix = XMMatrixMultiply(light_camera_view_matrix, light_camera_proj_matrix);
  XMStoreFloat4x4(&light_view_proj_transform_, light_view_proj_transform_matrix);
}

void Scene::CommitConstantBuffers()
{
  memcpy(scene_constant_buffer_pointer_, &scene_constant_buffer_, sizeof(scene_constant_buffer_));
  memcpy(shadow_constant_buffer_pointer_, &light_view_proj_transform_, sizeof(light_view_proj_transform_));
}

void Scene::SetCameras()
{
  XMVECTOR eye = XMVectorSet(0.0f, 1.0f, 2.0f, 0.0f);
  XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
  XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
  cameras_[1].Set(eye, at, up);

  eye = XMVectorSet(-2.0f, 1.0f, 0.0f, 0.0f);
  cameras_[2].Set(eye, at, up);

  eye = XMVectorSet(2.0f, 1.0f, 0.0f, 0.0f);
  cameras_[3].Set(eye, at, up);
}

void Scene::PopulateCommandLists()
{
  ThrowIfFailed(command_allocators_[current_frame_index_]->Reset());
  ThrowIfFailed(command_list_->Reset(command_allocators_[current_frame_index_].Get(), nullptr));
  CD3DX12_RESOURCE_BARRIER resource_barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_targets_[current_frame_index_].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  command_list_->ResourceBarrier(1, &resource_barrier);

  ShadowPass();
  ScenePass();
  DrawCameras();

  resource_barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_targets_[current_frame_index_].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  command_list_->ResourceBarrier(1, &resource_barrier);



  ThrowIfFailed(command_list_->Close());
}

void Scene::ShadowPass()
{
  command_list_->SetPipelineState(shadow_pipeline_state_.Get());
  command_list_->SetGraphicsRootSignature(shadow_root_signature_.Get());
  command_list_->SetGraphicsRootConstantBufferView(0, shadow_constant_buffer_view_->GetGPUVirtualAddress());

  CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_cpu_descriptor_handle(dsv_descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
  command_list_->ClearDepthStencilView(dsv_cpu_descriptor_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
  dsv_cpu_descriptor_handle.Offset(dsv_descriptor_size_);
  command_list_->ClearDepthStencilView(dsv_cpu_descriptor_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  command_list_->IASetVertexBuffers(0, 1, &vertex_buffer_view_);
  command_list_->IASetIndexBuffer(&index_buffer_view_);
  command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  command_list_->RSSetViewports(1, &view_port_);
  command_list_->RSSetScissorRects(1, &scissor_rect_);

  dsv_cpu_descriptor_handle = dsv_descriptor_heap_->GetCPUDescriptorHandleForHeapStart();
  command_list_->OMSetRenderTargets(0, nullptr, false, &dsv_cpu_descriptor_handle);

  std::vector<AssetsManager::DrawArgument> draw_arguments;
  AssetsManager::GetSharedInstance().GetModelDrawArguments(draw_arguments);
  for (const auto& draw_argument : draw_arguments) {
    command_list_->DrawIndexedInstanced(draw_argument.index_count, 1, draw_argument.index_start, draw_argument.vertex_base, 0);
  }
}

void Scene::ScenePass()
{
  command_list_->SetPipelineState(scene_pipeline_state_.Get());
  // Set descriptor heaps.
  ID3D12DescriptorHeap* ppHeaps[] = { cbv_srv_descriptor_heap_.Get() };
  command_list_->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

  command_list_->SetGraphicsRootSignature(scene_root_signature_.Get());
  command_list_->SetGraphicsRootConstantBufferView(0, scene_constant_buffer_view_->GetGPUVirtualAddress());


  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_cpu_descriptor_handle(rtv_descriptor_heap_->GetCPUDescriptorHandleForHeapStart(), current_frame_index_, rtv_descriptor_increment_size_);
  const FLOAT clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
  command_list_->ClearRenderTargetView(rtv_cpu_descriptor_handle, clear_color, 0, nullptr);

  command_list_->IASetVertexBuffers(0, 1, &vertex_buffer_view_);
  command_list_->IASetIndexBuffer(&index_buffer_view_);
  command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  command_list_->RSSetViewports(1, &view_port_);
  command_list_->RSSetScissorRects(1, &scissor_rect_);

  command_list_->OMSetRenderTargets(1, &rtv_cpu_descriptor_handle, false, nullptr);

  std::vector<AssetsManager::DrawArgument> draw_arguments;
  AssetsManager::GetSharedInstance().GetModelDrawArguments(draw_arguments);
  const D3D12_GPU_DESCRIPTOR_HANDLE cbv_srv_heap_start = cbv_srv_descriptor_heap_->GetGPUDescriptorHandleForHeapStart();
  for (const auto& draw_argument : draw_arguments) {
    if (draw_argument.diffuse_texture_index >= 0) {
      CD3DX12_GPU_DESCRIPTOR_HANDLE texture_descritptor(cbv_srv_heap_start, kDepthBufferCount_ + draw_argument.diffuse_texture_index, cbv_srv_descriptor_increment_size_);
      command_list_->SetGraphicsRootDescriptorTable(1, texture_descritptor);
    }

    command_list_->DrawIndexedInstanced(draw_argument.index_count, 1, draw_argument.index_start, draw_argument.vertex_base, 0);
  }
}

void Scene::DrawCameras()
{
  command_list_->SetPipelineState(camera_draw_pipeline_state_.Get());
  command_list_->SetGraphicsRootSignature(camera_draw_root_signature_.Get());
  // TODO: need to call? yes
  command_list_->SetGraphicsRootConstantBufferView(0, scene_constant_buffer_view_->GetGPUVirtualAddress());
  
  UpdateVerticesOfCameraPoints();
  
  // TODO: slot 0 ok ? yes
  command_list_->IASetVertexBuffers(0, 1, &camera_points_vertex_buffer_view_);
  command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

  command_list_->DrawInstanced(kTotalCameraCount_ - 1, 1, 0, 0);
}

void Scene::UpdateVerticesOfCameraPoints()
{
  Camera::Vertex camera_draw_vertices[kTotalCameraCount_ - 1]{};
  int camera_point_index = 0;
  for (auto i = 0; i < kTotalCameraCount_; ++i) {
    if (i != camera_index_) {
      cameras_[i].GetCameraVertexData(&camera_draw_vertices[camera_point_index]);
      camera_point_index++;
    }
  }

  D3D12_SUBRESOURCE_DATA camera_points_subresource_data{};
  camera_points_subresource_data.pData = camera_draw_vertices;
  camera_points_subresource_data.RowPitch = sizeof(camera_draw_vertices);
  camera_points_subresource_data.SlicePitch = camera_points_subresource_data.RowPitch;

  CD3DX12_RESOURCE_BARRIER resource_barrier_1 = CD3DX12_RESOURCE_BARRIER::Transition(camera_points_vertex_buffer_.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST);
  command_list_->ResourceBarrier(1, &resource_barrier_1);
  UpdateSubresources(command_list_.Get(), camera_points_vertex_buffer_.Get(), camera_points_vertex_upload_heap_.Get(), 0, 0, 1, &camera_points_subresource_data);

  CD3DX12_RESOURCE_BARRIER resource_barrier_2 = CD3DX12_RESOURCE_BARRIER::Transition(camera_points_vertex_buffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
  command_list_->ResourceBarrier(1, &resource_barrier_2);
}
