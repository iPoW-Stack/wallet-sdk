cmake_minimum_required(VERSION 3.22.1)
project("tcccompiler")

# libtcc 使用 ONE_SOURCE 模式：只需编译 libtcc.c，它会 #include 所有子文件

# ── 公共编译选项 ──────────────────────────────────────────────
set(TCC_COMMON_DEFS
    ONE_SOURCE=1
    CONFIG_TCC_STATIC=1
    CONFIG_TCC_SEMLOCK=0
    # 禁用在 Android 上不可用的功能
    CONFIG_TCC_BACKTRACE=0
    CONFIG_TCC_BCHECK=0
    CONFIG_NEW_MACHO=0
    # 空路径（Android 上不使用系统头文件）
    CONFIG_SYSROOT=""
    CONFIG_TCCDIR=""
    CONFIG_TCC_SYSINCLUDEPATHS=""
    CONFIG_TCC_LIBPATHS=""
    CONFIG_TCC_CRTPREFIX=""
    CONFIG_TCC_ELFINTERP=""
)

set(TCC_COMMON_FLAGS -O2 -fPIC -Wno-unused-function -Wno-unused-variable)

# ── arm64-v8a 静态库 ──────────────────────────────────────────
add_library(tcc_arm64 STATIC libtcc/libtcc.c)
target_include_directories(tcc_arm64 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libtcc)
target_compile_definitions(tcc_arm64 PRIVATE
    ${TCC_COMMON_DEFS}
    TCC_TARGET_ARM64
)
target_compile_options(tcc_arm64 PRIVATE ${TCC_COMMON_FLAGS})

# ── x86_64 静态库（模拟器）────────────────────────────────────
add_library(tcc_x86_64 STATIC libtcc/libtcc.c)
target_include_directories(tcc_x86_64 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libtcc)
target_compile_definitions(tcc_x86_64 PRIVATE
    ${TCC_COMMON_DEFS}
    TCC_TARGET_X86_64
)
target_compile_options(tcc_x86_64 PRIVATE ${TCC_COMMON_FLAGS})

# ── 根据 ABI 选择对应静态库 ───────────────────────────────────
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(TCC_LIB tcc_arm64)
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(TCC_LIB tcc_x86_64)
else()
    message(FATAL_ERROR "Unsupported ABI: ${ANDROID_ABI}")
endif()

# ── JNI 桥接共享库 ────────────────────────────────────────────
add_library(tcc_bridge SHARED tcc_bridge.cpp)

target_include_directories(tcc_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libtcc
)

target_link_libraries(tcc_bridge
    ${TCC_LIB}
    android
    log
)
