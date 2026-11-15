# main/CMakeLists.txt — 本工程的**板级**代码（app_main + ST7789 驱动）编成一个 IDF 组件。
#
# 引擎不在本工程里编译：它是**预编译静态库包**
#     ../../thirdparty/loppe-embedded-v1.0.0-preview.2/esp32/{lib/libloppe.a, include/}
# 随工程走（发布物里不出现引擎源码；布局同 IDE 新建的工程 = 解决方案根 thirdparty/loppe-<平台>-v<引擎版本>/<目标>）。
set(ENGINE_PKG "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty/loppe-embedded-v1.0.0-preview.2/esp32")

if(NOT EXISTS "${ENGINE_PKG}/lib/libloppe.a")
    message(FATAL_ERROR
        "缺少引擎预编译包：${ENGINE_PKG}/lib/libloppe.a\n"
        "  从 IDE 的 thirdparty/loppe-embedded-v1.0.0-preview.2/esp32 拷进来（lib/ + include/ + VERSION.txt）。")
endif()

# 浏览器内核 + QuickJS：与桌面 / 安卓**同一份源码**（../../kernel/src、../../thirdparty/quickjs-ng），
# 由 IDF 用 xtensa 工具链直接编进固件 —— 不产 .a，也不拷源码副本。
# 二级上溯：esp32s3/main → esp32s3 → loppe-browser → kernel/ | thirdparty/
set(KERNEL_DIR "${CMAKE_CURRENT_LIST_DIR}/../../kernel/src")
set(QUICKJS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty/quickjs-ng")

# 板级源码自动发现：main/ 下（含子目录）的 .c/.cpp 全编进来，新增文件不用改这里。
# 不用 CONFIGURE_DEPENDS：IDF 读组件依赖时会把本文件当脚本执行一遍，那里不允许它。
# idf.py build 每次都会重跑 configure，glob 照样能发现新文件。
file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

# 内核源码同理（自动发现，加文件不用改本文件）
file(GLOB_RECURSE KERNEL_SOURCES
    "${KERNEL_DIR}/*.c"
    "${KERNEL_DIR}/*.cpp")

# QuickJS 核心四个 .c（模块系统 / qjs 可执行 / libc 都不要）
set(QUICKJS_SOURCES
    "${QUICKJS_DIR}/quickjs.c"
    "${QUICKJS_DIR}/dtoa.c"
    "${QUICKJS_DIR}/libregexp.c"
    "${QUICKJS_DIR}/libunicode.c")

# QuickJS 是第三方代码，printf 的 %u/%d 是按「int 就是 32 位」写的；xtensa 上 uint32_t 是
# unsigned long，IDF 的 -Werror=format 直接判错。这两个告警降级到「警告」（不能只对这四个文件
# 生效：IDF 读组件依赖时会把本文件当脚本跑，那里不许 set_source_files_properties）。

idf_component_register(
    SRCS ${APP_SOURCES} ${KERNEL_SOURCES} ${QUICKJS_SOURCES}
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_spi esp_timer
    # 界面：EMBED_FILES 里的 .lui 编进固件，符号名 = 路径（/ 换成 _）+ _start / _end。
    #   "main.lui"     → _binary_main_lui_start / _end
    #   "ui/home.lui"  → _binary_ui_home_lui_start / _end
    # 界面**不限于一份**：想做多界面就多列几份（例如外壳 + 每页一份，见 main/main.lui 顶部说明），
    # 在 app_main.cpp 里按符号取出文本、运行期挂载或整份切换。新增条目要同时在 app_main.cpp 里
    # 加一对 extern 符号。
    EMBED_FILES "main.lui"
)

# IDF 会把 INCLUDE_DIRS 里的绝对路径相对化（相对组件目录），而 ninja 工作目录是 build 根，
# 导致跨目录 include（platform/framebuffer.h、engine/xxx.h）解析失败
# → 用 -I 绝对路径直接拼进编译选项。
target_compile_options(${COMPONENT_LIB} PRIVATE
    "-I${ENGINE_PKG}/include"
    "-I${KERNEL_DIR}"
    "-I${QUICKJS_DIR}"
)

# 嵌入式编译宏（引擎头里按它选嵌入式分支；与包里的编译口径一致）
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LOPPE_PLATFORM_EMBEDDED
    LOPPE_STATIC
)

# 板级 / 内核代码会带引擎头，warning 在 ESP-IDF 默认 -Werror 下会变错误，这里降级为警告。
# -Wno-error=format / implicit-fallthrough 是给 QuickJS 的（见上面那段说明）。
target_compile_options(${COMPONENT_LIB} PRIVATE
    -fexceptions
    -frtti
    -Wno-error=switch
    -Wno-error=reorder
    -Wno-error=format
    -Wno-error=implicit-fallthrough
)

# 链接引擎静态库：必须 INTERFACE —— 组件自己就是个静态库，PRIVATE 不会把它带到最终 app 链接。
target_link_libraries(${COMPONENT_LIB} INTERFACE "${ENGINE_PKG}/lib/libloppe.a")
