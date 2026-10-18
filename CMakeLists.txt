# SPDX-License-Identifier: MIT
# Copyright (c) 2026 ALOLAN
cmake_minimum_required(VERSION 3.16)
project(util_ringbuf_template LANGUAGES C)

add_library(util_ringbuf STATIC
    User/Utils/utils_ringbuf.c
    User/ThirdParty/lwrb/src/lwrb/lwrb.c)
target_include_directories(util_ringbuf PUBLIC
    User/Utils
    User/ThirdParty/lwrb/src/include)
# 必须传播到所有包含公共结构体的编译单元，避免后端布局不一致。
# 默认仅承诺串行或由调用方完整同步的使用方式。
target_compile_definitions(util_ringbuf PUBLIC LWRB_DISABLE_ATOMIC)
set_target_properties(util_ringbuf PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
