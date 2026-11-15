# esp32s3 — ESP-IDF 工程入口（构建真相在这里，不在 build.lpj）
#
#   "${IDF_PYTHON}" "${IDF_PY}" -C <本目录> -B <本目录>/build build
#   产物：build/esp32s3.elf、build/esp32s3.bin（可烧写镜像）
#   构建目录 = IDF 缺省的 <工程>/build ⇒ 控制台里 `idf.py -p COMx flash` 直接复用，不必传 -B
#
# 目标芯片：sdkconfig.defaults 里已写 CONFIG_IDF_TARGET="esp32s3"（换 target 用 idf.py set-target）
# 引擎：../thirdparty/loppe-embedded-v1.0.0-preview.2/esp32（预编译静态库包，随工程走；main/CMakeLists.txt 里链接）
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32s3)
