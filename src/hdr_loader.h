#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Minimal Radiance .hdr loader (RGBE + RLE). Returns RGBA float32 pixels (linear), row-major.
// Throws std::runtime_error on failure.
std::vector<float> loadRadianceHDR_RGBA32F(const std::string& path, int& outW, int& outH);

// Utility: compute mip count for a 2D image.
uint32_t mipCount2D(uint32_t w, uint32_t h);
