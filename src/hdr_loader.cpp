#include "hdr_loader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

static bool startsWith(const char *s, const char *p)
{
    return std::strncmp(s, p, std::strlen(p)) == 0;
}

uint32_t mipCount2D(uint32_t w, uint32_t h)
{
    uint32_t m = 1;
    uint32_t d = std::max(w, h);
    while (d > 1)
    {
        d >>= 1;
        ++m;
    }
    return m;
}

// RGBE to float
static void rgbeToFloat(const uint8_t rgbe[4], float &r, float &g, float &b)
{
    if (rgbe[3] == 0)
    {
        r = g = b = 0.0f;
        return;
    }
    // value = mantissa * 2^(exp-128) / 256
    const int e = int(rgbe[3]) - 128;
    const float f = std::ldexp(1.0f, e) / 256.0f;
    r = float(rgbe[0]) * f;
    g = float(rgbe[1]) * f;
    b = float(rgbe[2]) * f;
}

static void readLine(FILE *f, char *buf, size_t cap)
{
    if (!std::fgets(buf, (int)cap, f))
        throw std::runtime_error("Unexpected EOF while reading HDR header");
}

std::vector<float> loadRadianceHDR_RGBA32F(const std::string &path, int &outW, int &outH)
{
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        throw std::runtime_error("Failed to open HDR: " + path);

    char line[512] = {};
    readLine(f, line, sizeof(line));
    if (!startsWith(line, "#?RADIANCE") && !startsWith(line, "#?RGBE"))
    {
        std::fclose(f);
        throw std::runtime_error("Not a Radiance HDR file: " + path);
    }

    // read until blank line
    bool formatOk = false;
    while (true)
    {
        readLine(f, line, sizeof(line));
        if (line[0] == '\n' || line[0] == '\r' || line[0] == 0)
            break;
        if (startsWith(line, "FORMAT="))
        {
            if (std::strstr(line, "32-bit_rle_rgbe"))
                formatOk = true;
        }
    }
    if (!formatOk)
    {
        // many HDRs will prob still work
    }

    // resolution line: -Y <h> +X <w>
    readLine(f, line, sizeof(line));
    int w = 0, h = 0;
    if (std::sscanf(line, "-Y %d +X %d", &h, &w) != 2 && std::sscanf(line, "+X %d -Y %d", &w, &h) != 2)
    {
        std::fclose(f);
        throw std::runtime_error("Failed to parse HDR resolution line: " + std::string(line));
    }
    if (w <= 0 || h <= 0)
    {
        std::fclose(f);
        throw std::runtime_error("Invalid HDR dimensions");
    }

    outW = w;
    outH = h;

    std::vector<float> rgba;
    rgba.resize(size_t(w) * size_t(h) * 4u);

    std::vector<uint8_t> scanline;
    scanline.resize(size_t(w) * 4u);

    for (int y = 0; y < h; ++y)
    {
        uint8_t rgbe[4];
        if (std::fread(rgbe, 1, 4, f) != 4)
        {
            std::fclose(f);
            throw std::runtime_error("Unexpected EOF in HDR pixel data");
        }

        if (rgbe[0] != 2 || rgbe[1] != 2 || (rgbe[2] & 0x80))
        {
            // put first pixel into scanline then read remaining
            scanline[0] = rgbe[0];
            scanline[1] = rgbe[1];
            scanline[2] = rgbe[2];
            scanline[3] = rgbe[3];
            const size_t remain = size_t(w - 1) * 4u;
            if (remain > 0)
            {
                if (std::fread(scanline.data() + 4, 1, remain, f) != remain)
                {
                    std::fclose(f);
                    throw std::runtime_error("Unexpected EOF in old HDR pixel data");
                }
            }
        }
        else
        {
            const int scanW = (int(rgbe[2]) << 8) | int(rgbe[3]);
            if (scanW != w)
            {
                std::fclose(f);
                throw std::runtime_error("HDR scanline width mismatch");
            }
            for (int c = 0; c < 4; ++c)
            {
                int x = 0;
                while (x < w)
                {
                    uint8_t count = 0;
                    if (std::fread(&count, 1, 1, f) != 1)
                    {
                        std::fclose(f);
                        throw std::runtime_error("Unexpected EOF in HDR RLE");
                    }
                    if (count > 128)
                    {
                        const int run = int(count) - 128;
                        uint8_t val = 0;
                        if (std::fread(&val, 1, 1, f) != 1)
                        {
                            std::fclose(f);
                            throw std::runtime_error("Unexpected EOF in HDR RLE run");
                        }
                        for (int i = 0; i < run; ++i)
                        {
                            scanline[size_t(c) * size_t(w) + size_t(x)] = val;
                            ++x;
                        }
                    }
                    else
                    {
                        const int lit = int(count);
                        if (std::fread(scanline.data() + size_t(c) * size_t(w) + size_t(x), 1, size_t(lit), f) != size_t(lit))
                        {
                            std::fclose(f);
                            throw std::runtime_error("Unexpected EOF in HDR RLE literal");
                        }
                        x += lit;
                    }
                }
            }

            for (int x = 0; x < w; ++x)
            {
                rgbe[0] = scanline[size_t(0) * size_t(w) + size_t(x)];
                rgbe[1] = scanline[size_t(1) * size_t(w) + size_t(x)];
                rgbe[2] = scanline[size_t(2) * size_t(w) + size_t(x)];
                rgbe[3] = scanline[size_t(3) * size_t(w) + size_t(x)];

                float r, g, b;
                rgbeToFloat(rgbe, r, g, b);
                const size_t idx = (size_t(y) * size_t(w) + size_t(x)) * 4u;
                rgba[idx + 0] = r;
                rgba[idx + 1] = g;
                rgba[idx + 2] = b;
                rgba[idx + 3] = 1.0f;
            }
            continue;
        }
        for (int x = 0; x < w; ++x)
        {
            const uint8_t *p = scanline.data() + size_t(x) * 4u;
            float r, g, b;
            rgbeToFloat(p, r, g, b);
            const size_t idx = (size_t(y) * size_t(w) + size_t(x)) * 4u;
            rgba[idx + 0] = r;
            rgba[idx + 1] = g;
            rgba[idx + 2] = b;
            rgba[idx + 3] = 1.0f;
        }
    }

    std::fclose(f);

    // flip to convert top left to bottom left
    for (int y = 0; y < h / 2; ++y)
    {
        int oy = h - 1 - y;
        for (int x = 0; x < w; ++x)
        {
            size_t a = (size_t(y) * size_t(w) + size_t(x)) * 4u;
            size_t b = (size_t(oy) * size_t(w) + size_t(x)) * 4u;
            for (int c = 0; c < 4; ++c)
                std::swap(rgba[a + c], rgba[b + c]);
        }
    }

    return rgba;
}
