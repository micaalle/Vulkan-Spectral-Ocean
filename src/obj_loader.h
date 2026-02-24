#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct ObjVertex
{
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

struct ObjBounds
{
    float minx, miny, minz;
    float maxx, maxy, maxz;
};

bool loadObjTriangulated(const std::string &path,
                         std::vector<ObjVertex> &outVerts,
                         std::vector<uint32_t> &outIndices,
                         ObjBounds *outBounds = nullptr,
                         bool flipV = true);
