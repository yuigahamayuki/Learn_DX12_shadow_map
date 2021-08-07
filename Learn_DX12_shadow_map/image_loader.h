#pragma once

#include <vector>
#include <string>

#include "common_headers.h"

class ImageLoader {
public:
  static int LoadImageDataFromFile(std::vector<uint8_t>& imageData, D3D12_RESOURCE_DESC& texture_desc, const std::string& filename, int& bytesPerRow);
};  // class ImageLoader