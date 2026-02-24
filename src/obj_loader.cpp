#include "obj_loader.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <tuple>
#include <cmath>
#include <iostream>

static inline void updateBounds(ObjBounds &b, float x, float y, float z)
{
    b.minx = std::min(b.minx, x);
    b.miny = std::min(b.miny, y);
    b.minz = std::min(b.minz, z);
    b.maxx = std::max(b.maxx, x);
    b.maxy = std::max(b.maxy, y);
    b.maxz = std::max(b.maxz, z);
}

struct Idx
{
    int v = -1, vt = -1, vn = -1;
};

static bool parseFaceToken(const std::string &tok, Idx &out)
{
    int v = 0, vt = 0, vn = 0;
    char c;
    std::stringstream ss(tok);
    if (!(ss >> v))
        return false;
    out.v = v;
    if (ss.peek() == '/')
    {
        ss.get();
        if (ss.peek() == '/')
        {
            ss.get();
            if (ss >> vn)
                out.vn = vn;
        }
        else
        {
            if (ss >> vt)
                out.vt = vt;
            if (ss.peek() == '/')
            {
                ss.get();
                if (ss >> vn)
                    out.vn = vn;
            }
        }
    }
    return true;
}

static inline int fixIndex(int idx, int n)
{
    if (idx > 0)
        return idx - 1;
    if (idx < 0)
        return n + idx;
    return -1;
}

struct Key
{
    int v, vt, vn;
    bool operator==(const Key &o) const noexcept { return v == o.v && vt == o.vt && vn == o.vn; }
};
struct KeyHash
{
    size_t operator()(const Key &k) const noexcept
    {
        size_t h = 1469598103934665603ull;
        auto mix = [&](int x)
        {
            h ^= (uint64_t)(x + 0x9e3779b9);
            h *= 1099511628211ull;
        };
        mix(k.v);
        mix(k.vt);
        mix(k.vn);
        return h;
    }
};

bool loadObjTriangulated(const std::string &path,
                         std::vector<ObjVertex> &outVerts,
                         std::vector<uint32_t> &outIndices,
                         ObjBounds *outBounds,
                         bool flipV)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        std::cerr << "Failed to open OBJ: " << path << "\n";
        return false;
    }

    std::vector<float> pos;
    pos.reserve(100000);
    std::vector<float> nrm;
    nrm.reserve(100000);
    std::vector<float> uv;
    uv.reserve(100000);

    ObjBounds bounds{};
    bounds.minx = bounds.miny = bounds.minz = 1e30f;
    bounds.maxx = bounds.maxy = bounds.maxz = -1e30f;

    outVerts.clear();
    outIndices.clear();
    std::unordered_map<Key, uint32_t, KeyHash> map;

    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty())
            continue;
        if (line[0] == '#')
            continue;
        std::stringstream ss(line);
        std::string t;
        ss >> t;
        if (t == "v")
        {
            float x, y, z;
            ss >> x >> y >> z;
            pos.push_back(x);
            pos.push_back(y);
            pos.push_back(z);
            updateBounds(bounds, x, y, z);
        }
        else if (t == "vn")
        {
            float x, y, z;
            ss >> x >> y >> z;
            nrm.push_back(x);
            nrm.push_back(y);
            nrm.push_back(z);
        }
        else if (t == "vt")
        {
            float a, b;
            ss >> a >> b;
            if (flipV)
                b = 1.0f - b;
            uv.push_back(a);
            uv.push_back(b);
        }
        else if (t == "f")
        {
            std::vector<Idx> face;
            std::string tok;
            while (ss >> tok)
            {
                Idx id;
                if (parseFaceToken(tok, id))
                    face.push_back(id);
            }
            if (face.size() < 3)
                continue;

            // fan triangulate
            for (size_t k = 1; k + 1 < face.size(); k++)
            {
                Idx tri[3] = {face[0], face[k], face[k + 1]};
                for (int j = 0; j < 3; j++)
                {
                    int iv = fixIndex(tri[j].v, (int)(pos.size() / 3));
                    int it = fixIndex(tri[j].vt, (int)(uv.size() / 2));
                    int in = fixIndex(tri[j].vn, (int)(nrm.size() / 3));
                    if (iv < 0)
                        continue;

                    Key key{iv, it, in};
                    auto itMap = map.find(key);
                    uint32_t vi;
                    if (itMap == map.end())
                    {
                        ObjVertex v{};
                        v.px = pos[iv * 3 + 0];
                        v.py = pos[iv * 3 + 1];
                        v.pz = pos[iv * 3 + 2];
                        if (in >= 0)
                        {
                            v.nx = nrm[in * 3 + 0];
                            v.ny = nrm[in * 3 + 1];
                            v.nz = nrm[in * 3 + 2];
                        }
                        else
                        {
                            v.nx = v.ny = 0.0f;
                            v.nz = 1.0f;
                        }
                        if (it >= 0)
                        {
                            v.u = uv[it * 2 + 0];
                            v.v = uv[it * 2 + 1];
                        }
                        else
                        {
                            v.u = v.v = 0.0f;
                        }

                        vi = (uint32_t)outVerts.size();
                        outVerts.push_back(v);
                        map.emplace(key, vi);
                    }
                    else
                    {
                        vi = itMap->second;
                    }
                    outIndices.push_back(vi);
                }
            }
        }
    }

    // if missing compute smooth-ish by accumulating face normals
    bool anyMissing = false;
    for (auto &v : outVerts)
    {
        if (std::fabs(v.nx) + std::fabs(v.ny) + std::fabs(v.nz) < 1e-6f)
        {
            anyMissing = true;
            break;
        }
    }
    if (anyMissing)
    {
        std::vector<float> acc(outVerts.size() * 3, 0.0f);
        for (size_t i = 0; i + 2 < outIndices.size(); i += 3)
        {
            uint32_t a = outIndices[i], b = outIndices[i + 1], c = outIndices[i + 2];
            float ax = outVerts[a].px, ay = outVerts[a].py, az = outVerts[a].pz;
            float bx = outVerts[b].px, by = outVerts[b].py, bz = outVerts[b].pz;
            float cx = outVerts[c].px, cy = outVerts[c].py, cz = outVerts[c].pz;
            float ux = bx - ax, uy = by - ay, uz = bz - az;
            float vx = cx - ax, vy = cy - ay, vz = cz - az;
            float nx = uy * vz - uz * vy;
            float ny = uz * vx - ux * vz;
            float nz = ux * vy - uy * vx;
            acc[a * 3 + 0] += nx;
            acc[a * 3 + 1] += ny;
            acc[a * 3 + 2] += nz;
            acc[b * 3 + 0] += nx;
            acc[b * 3 + 1] += ny;
            acc[b * 3 + 2] += nz;
            acc[c * 3 + 0] += nx;
            acc[c * 3 + 1] += ny;
            acc[c * 3 + 2] += nz;
        }
        for (size_t i = 0; i < outVerts.size(); i++)
        {
            float nx = acc[i * 3 + 0], ny = acc[i * 3 + 1], nz = acc[i * 3 + 2];
            float l = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (l > 1e-6f)
            {
                nx /= l;
                ny /= l;
                nz /= l;
            }
            outVerts[i].nx = nx;
            outVerts[i].ny = ny;
            outVerts[i].nz = nz;
        }
    }

    if (outBounds)
        *outBounds = bounds;
    std::cout << "Loaded OBJ: " << path << " verts=" << outVerts.size() << " tris=" << (outIndices.size() / 3) << "\n";
    return !outVerts.empty() && !outIndices.empty();
}
