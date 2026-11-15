#include "net/favicon.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>

#include "misc/logger.h"
#include "misc/utf8_path.h"

namespace loppe_browser {
namespace {

std::uint32_t ReadU16(const unsigned char* p)
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8);
}

std::uint32_t ReadU32(const unsigned char* p)
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

bool IsPng(const std::string& b)
{
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    return b.size() >= 8 && std::memcmp(b.data(), sig, 8) == 0;
}

// ICO 与 CUR 的头一样（只差 reserved/type 两个字段的取值），解码逻辑通用
bool IsIconContainer(const std::string& b)
{
    if (b.size() < 6) return false;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(b.data());
    return ReadU16(p) == 0 && (ReadU16(p + 2) == 1 || ReadU16(p + 2) == 2) && ReadU16(p + 4) > 0;
}

bool WriteFile(const std::string& path, const std::string& bytes)
{
    std::FILE* f = loppe::utf8path::Open(path, "wb");
    if (!f) return false;
    const std::size_t n = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return n == bytes.size();
}

// 未压缩 32 位 BGRA、原点在左上：stb_image 认这个格式，写起来只有 18 字节头
std::string EncodeTga(int w, int h, const std::vector<unsigned char>& rgba)
{
    std::string out(18 + static_cast<std::size_t>(w) * h * 4, '\0');
    unsigned char* p = reinterpret_cast<unsigned char*>(out.data());
    p[2] = 2;   // 图像类型：未压缩真彩色
    p[12] = static_cast<unsigned char>(w & 0xFF);
    p[13] = static_cast<unsigned char>((w >> 8) & 0xFF);
    p[14] = static_cast<unsigned char>(h & 0xFF);
    p[15] = static_cast<unsigned char>((h >> 8) & 0xFF);
    p[16] = 32;     // 每像素位数
    p[17] = 0x28;   // 8 位 alpha + 原点在左上
    unsigned char* q = p + 18;
    for (int i = 0; i < w * h; ++i) {
        q[i * 4 + 0] = rgba[i * 4 + 2];   // TGA 是 BGRA
        q[i * 4 + 1] = rgba[i * 4 + 1];
        q[i * 4 + 2] = rgba[i * 4 + 0];
        q[i * 4 + 3] = rgba[i * 4 + 3];
    }
    return out;
}

// ICO 里那张图的载荷：一个 BITMAPINFOHEADER + XOR 位图 + AND 遮罩（无文件头）。
// 头和独立 BMP 一样，只是高度写的是实际高度的两倍（位图 + 遮罩各一半）。
bool DecodeIcoBitmap(const unsigned char* src, std::size_t size,
                     int* outW, int* outH, std::vector<unsigned char>* outRgba)
{
    if (size < 40) return false;
    const std::uint32_t headerSize = ReadU32(src);
    if (headerSize < 40 || headerSize > size) return false;
    if (ReadU32(src + 16) != 0) return false;   // 只认 BI_RGB；压缩格式不做

    const std::uint32_t w = ReadU32(src + 4);
    const std::uint32_t doubledH = ReadU32(src + 8);
    const std::uint16_t bpp = static_cast<std::uint16_t>(ReadU16(src + 14));
    if (bpp != 32 && bpp != 24) return false;   // 调色板（1/4/8 位）不做

    const int width = static_cast<int>(w);
    const int height = static_cast<int>(doubledH / 2);
    if (width <= 0 || width > 256 || height <= 0 || height > 256) return false;

    const std::size_t xorStride = (static_cast<std::size_t>(width) * bpp / 8 + 3) & ~static_cast<std::size_t>(3);
    const std::size_t andStride = (static_cast<std::size_t>(width) + 31) / 32 * 4;
    if (headerSize + xorStride * height > size) return false;

    const unsigned char* xorBits = src + headerSize;
    const unsigned char* andBits = xorBits + xorStride * height;
    const bool hasMask = headerSize + xorStride * height + andStride * height <= size;

    outRgba->assign(static_cast<std::size_t>(width) * height * 4, 255);
    bool anyAlpha = false;
    const int bytesPerPixel = bpp / 8;
    for (int y = 0; y < height; ++y) {
        const unsigned char* row = xorBits + xorStride * (height - 1 - y);   // 位图是底朝上
        for (int x = 0; x < width; ++x) {
            const unsigned char* px = row + static_cast<std::size_t>(x) * bytesPerPixel;
            unsigned char* out = outRgba->data() + (static_cast<std::size_t>(y) * width + x) * 4;
            out[0] = px[2];
            out[1] = px[1];
            out[2] = px[0];
            if (bpp == 32) {
                out[3] = px[3];
                if (px[3] != 0) anyAlpha = true;
            }
        }
    }
    // 老图标（Windows XP 时代那批）：32 位 alpha 全是 0，真正的透明信息在 AND 遮罩里
    // （每像素 1 位，1 = 全透明）。24 位图标没有 alpha 通道，也只能靠遮罩。
    if (hasMask && (bpp == 24 || !anyAlpha)) {
        for (int y = 0; y < height; ++y) {
            const unsigned char* row = andBits + andStride * (height - 1 - y);
            for (int x = 0; x < width; ++x) {
                const bool transparent = ((row[x >> 3] >> (7 - (x & 7))) & 1) != 0;
                if (transparent) {
                    outRgba->data()[(static_cast<std::size_t>(y) * width + x) * 4 + 3] = 0;
                }
            }
        }
    }

    *outW = width;
    *outH = height;
    return true;
}

struct PickedImage {
    bool isPng = false;            // true = payload 就是 PNG，原样落盘
    std::string pngBytes;
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
};

// 从 ICO 容器里挑一张：优先 32×32（桌面 1x 下缩到 14dp 仍然清楚），其次 16×16，再其次最大。
bool PickFromIconContainer(const std::string& body, PickedImage* out)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(body.data());
    const std::uint32_t count = ReadU16(p + 4);
    if (count == 0 || count > 64) return false;
    if (body.size() < 6 + static_cast<std::size_t>(count) * 16) return false;

    int bestEntry = -1;
    int bestScore = -1;
    for (std::uint32_t i = 0; i < count; ++i) {
        const unsigned char* e = p + 6 + i * 16;
        const int w = e[0] == 0 ? 256 : e[0];
        const int h = e[1] == 0 ? 256 : e[1];
        const std::uint16_t bpp = static_cast<std::uint16_t>(ReadU16(e + 6));
        // 打分：尺寸优先（32 最好），同位深作次判据；调色板小图标排最后
        const int sizeScore = (w == 32) ? 300 : (w == 16 ? 200 : (w == 48 ? 150 : w));
        const int bppScore = (bpp >= 24) ? 30 : 0;
        const int score = sizeScore + bppScore;
        if (score > bestScore) {
            bestScore = score;
            bestEntry = static_cast<int>(i);
        }
        (void)h;
    }
    if (bestEntry < 0) return false;

    const unsigned char* e = p + 6 + static_cast<std::size_t>(bestEntry) * 16;
    const std::uint32_t bytesInRes = ReadU32(e + 8);
    const std::uint32_t offset = ReadU32(e + 12);
    if (offset > body.size() || bytesInRes > body.size() - offset) return false;

    const unsigned char* payload = p + offset;
    if (bytesInRes >= 8 && (payload[0] == 0x89 && payload[1] == 'P' && payload[2] == 'N' && payload[3] == 'G')) {
        out->isPng = true;
        out->pngBytes.assign(reinterpret_cast<const char*>(payload), bytesInRes);
        return true;
    }
    return DecodeIcoBitmap(payload, bytesInRes, &out->width, &out->height, &out->rgba);
}

} // namespace

std::string FaviconCacheDir()
{
    static const std::string dir = [] {
        const char* tmp = std::getenv("TEMP");
        if (!tmp || !*tmp) tmp = std::getenv("TMP");
        if (!tmp || !*tmp) tmp = std::getenv("TMPDIR");
        const std::string root = (tmp && *tmp) ? std::string(tmp) : std::string(".");
        const std::string path = root + "/loppe-browser-favicon";
        // Windows 上 fs::path(窄串) 会按 ANSI 代码页解 UTF-8 字节 → 用户名带中文时抛异常，
        // 所以走 u8path 并整段兜住：建不出目录就当没有图标，不影响页面。
        try {
            std::filesystem::create_directories(std::filesystem::u8path(path));
        } catch (...) {
            LOPPE_WARN("[favicon] 建不出缓存目录 " + path);
        }
        return path;
    }();
    return dir;
}

std::string SaveFaviconImage(const std::string& body)
{
    if (body.size() < 8) return std::string();

    PickedImage img;
    if (IsPng(body)) {
        img.isPng = true;
        img.pngBytes = body;
    } else if (IsIconContainer(body)) {
        if (!PickFromIconContainer(body, &img)) {
            LOPPE_INFO("[favicon] ICO 里没有能解的图（调色板/压缩格式），用默认图标");
            return std::string();
        }
    } else {
        LOPPE_INFO("[favicon] 不是 PNG/ICO，用默认图标");
        return std::string();
    }

    // 每次一个新名字：引擎按路径缓存纹理，同路径换了内容不会再读
    static std::atomic<int> serial{ 0 };
    const std::string base = FaviconCacheDir() + "/f" + std::to_string(serial.fetch_add(1));

    if (img.isPng) {
        const std::string path = base + ".png";
        if (!WriteFile(path, img.pngBytes)) {
            LOPPE_WARN("[favicon] 写不进 " + path);
            return std::string();
        }
        return path;
    }

    const std::string path = base + ".tga";
    if (!WriteFile(path, EncodeTga(img.width, img.height, img.rgba))) {
        LOPPE_WARN("[favicon] 写不进 " + path);
        return std::string();
    }
    return path;
}

} // namespace loppe_browser
