/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <string>
#include <map>
#include <set>
#include <memory>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "loppe_export.h"
#include "render/graphics_types.h"

class IDrawer;

/**
 * @brief 路径是否用了 DSL 的 `~/` 前缀：`~/xxx` 相对当前 .lui 文件所在目录（`~\xxx` 同样接受）。
 */
bool isDocumentRelativePath(const std::string& path);

/**
 * @brief 字体资源信息
 */
struct FontResource {
    std::string name;           // 字体名称 (作为 key)
    std::string filePath;       // 字体文件的完整路径
    std::string fileName;       // 原始文件名
    
    FontResource() = default;
    FontResource(const std::string& n, const std::string& path, const std::string& file);
};

struct ImageRawData {
    std::string path;
    unsigned char* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
    
    ~ImageRawData(); // Custom destructor to free data
};

/**
 * @brief 资源管理器类
 *
 * 按名加载/缓存资源（字体等）；字体由调用方用 loadFont() 显式注册，引擎不扫磁盘。
 */
class LOPPE_API ResourceManager {
public:
    /**
     * @brief 获取 ResourceManager 的单例实例
     */
    static ResourceManager& getInstance();
    
    ~ResourceManager();

    /**
     * @brief 初始化资源管理器（不扫磁盘：字体由 loadFont() 显式注册）
     *
     * @return 当前恒为 true（保留返回值以便将来扩展）
     */
    bool initialize();
    
    /**
     * @brief 根据名称获取字体资源
     * 
     * @param name 字体名称 (不包含扩展名)
     * @return 字体资源指针，如果未找到返回 nullptr
     */
    const FontResource* getFontByName(const std::string& name) const;

    // 异步加载图片；加载完成后引擎会自己安排一次重绘（调用方不必轮询或手动请求重绘）。
    void preloadImage(const std::string& path);

    // ===== 图片生命周期（加载 → 纹理缓存 → 淘汰）=====

    // 注入绘制器：纹理上传（createImageFromMemory）与释放（unloadImage）经它完成。
    // 宿主图形上下文初始化后调用；换上下文前先 detachDrawer()。
    void attachDrawer(IDrawer* drawer);

    // 解绑 drawer 并卸载全部图片纹理（宿主图形上下文关闭前调用，缓存里不留悬空句柄）。
    void detachDrawer();

    // 查询路径对应的纹理；未就绪返回 0，并限流警告一次（大概率调用方漏了 preloadImage）。
    // 主线程调用（与 uploadReadyImages/unloadImage 同一线程，无需加锁）。
    ImageHandle getImage(const std::string& path);

    // 帧首由 Compositor 调用。
    // 把已解码就绪的图片上传为纹理（需先 attachDrawer）。
    void uploadReadyImages();

    // 按路径完整卸载：未出队的加载请求取消（出队）、已解码未上传的就绪数据丢弃、
    // 已上传的纹理释放、加载记录清除。之后对该路径可重新 preloadImage。主线程调用。
    void unloadImage(const std::string& path);

    // 卸载并清空所有图片纹理与加载状态（字体缓存不受影响）。主线程调用。
    void clearImages();
    
    // 获取已加载完成的图片数据（如果有），并从队列中移除
    // 返回 nullptr 表示队列为空（加载失败的图不入队、也不占位）
    std::unique_ptr<ImageRawData> popReadyImage();

    /**
     * @brief 获取所有已缓存的字体名称列表
     * 
     * @return 字体名称列表
     */
    std::vector<std::string> getAllFontNames() const;
    
    /**
     * @brief 获取已缓存的字体数量
     * 
     * @return 字体数量
     */
    size_t getFontCount() const;
    
    /**
     * @brief 手动加载单个字体文件
     * 
     * @param fontPath 字体文件的相对路径 (相对于项目根目录)
     * @param fontName 字体名称 (如果为空，则从文件名自动提取)
     * @return true 成功加载，false 加载失败
     */
    bool loadFont(const std::string& fontPath, const std::string& fontName = "");
    
    /**
     * @brief 加载一个可用的系统字体并设为默认字体（Windows / Linux 桌面通用入口）。
     *        其它平台（Android / 嵌入式 / macOS）→ false（字体由各自平台层/资源提供）。
     * @param fontName 注册名（同时设为默认字体名）
     * @param fontSize 默认字号
     * @return true = 已 loadFont + setDefaultFont；false = 没找到（可自行 loadFont 指定字体）
     */
    bool loadSystemDefaultFont(const std::string& fontName, float fontSize);
    
    /**
     * @brief 清除所有缓存的字体资源
     */
    void clear();
    
    /**
     * @brief 打印所有已缓存的字体信息 (调试用)
     */
    void printFontCache() const;
    
    /**
     * @brief 获取默认字体名称
     * 
     * @return 默认字体名称
     */
    std::string getDefaultFontName() const;
    
    /**
     * @brief 获取默认字体大小
     * 
     * @return 默认字体大小
     */
    float getDefaultFontSize() const;
    
    /**
     * @brief 设置默认字体
     * 
     * @param fontName 字体名称
     * @param fontSize 字体大小
     */
    void setDefaultFont(const std::string& fontName, float fontSize);
    void setResourceRoot(const std::string& resourceRoot);
    std::string resolvePath(const std::string& path) const;
    std::string getResourceRoot() const;

    /**
     * @brief 设置「当前界面文件（.lui）所在目录」：DSL 里 `~/` 前缀路径的解析基准。
     *
     * `~/xxx` = 相对本 .lui 文件所在目录；其余路径不受影响（仍按资源根解析）。
     * 宿主在加载/预览一份 .lui 之前调用；传空串 = 清除（`~/` 退化为普通相对路径）。
     *
     * @param documentRoot .lui 文件所在目录（空串 = 无 .lui 上下文）
     */
    void setDocumentRoot(const std::string& documentRoot);

    /**
     * @brief 当前 .lui 文件所在目录（未设置时为空串）
     */
    std::string getDocumentRoot() const;

    /**
     * @brief 把 DSL 里的路径解析成运行时路径：`~/` 前缀 → 相对当前 .lui 目录；
     *        其余路径**原样返回**（由 resolvePath 在加载时按资源根规则解析）。
     *
     * @param path DSL 里写的路径（如 `~/images/logo.png`）
     * @return 运行时路径（未设置 .lui 目录时 = 去掉 `~/` 前缀的原路径）
     */
    std::string resolveDocumentPath(const std::string& path) const;
private:
    ResourceManager();
    
    // 禁止拷贝和赋值
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    void loadThreadFunc();

    // 图片就绪 → 请求一帧重绘
    void requestFrameForReadyImage();

    void scanFontsInDirectory(const std::string& dirPath, bool recursive = true);
    
    /**
     * @brief 从文件名中提取字体名称 (去除扩展名和路径)
     * 
     * @param filePath 文件路径
     * @return 字体名称
     */
    std::string extractFontName(const std::string& filePath) const;
    
    /**
     * @brief 判断文件是否为字体文件
     * 
     * @param fileName 文件名
     * @return true 是字体文件，false 不是字体文件
     */
    bool isFontFile(const std::string& fileName) const;

private:
    std::map<std::string, FontResource> fonts_;      // 字体缓存 (key: 字体名称)
    std::string resourceRoot_;
    std::string documentRoot_;                       // `~/` 的解析基准（.lui 文件所在目录）
    mutable std::mutex documentRootMutex_;
    bool initialized_;                                // 是否已初始化
    
    // 默认字体配置
    std::string defaultFontName_;                    // 默认字体名称
    float defaultFontSize_;                          // 默认字体大小

    std::deque<std::string> loadQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::atomic<bool> running_{true};
    std::thread loaderThread_;

    std::deque<std::unique_ptr<ImageRawData>> readyQueue_;
    std::mutex readyMutex_;

    std::map<std::string, bool> pendingOrLoaded_;

    // ===== 纹理层（attachDrawer 注入后启用；以下成员仅主线程访问）=====
    IDrawer* drawer_ = nullptr;                        // 纹理上传/释放工具
    std::map<std::string, ImageHandle> textureCache_;  // path → 已上传纹理
    std::set<std::string> warnedMissing_;              // getImage 未命中已警告过的路径
};

#endif // RESOURCE_MANAGER_H