/**
 * stb_image 实现文件
 *
 * ============================================================
 *  为什么需要单独的 .cpp 文件？
 * ============================================================
 * stb_image.h 是一个「单头文件库」（Single Header Library），
 * 它的实现代码需要恰好在一个 .cpp 中通过
 *   #define STB_IMAGE_IMPLEMENTATION
 * 来展开。如果把这个宏写在 main.cpp 中，其他文件再 include
 * stb_image.h 就会重复定义。
 *
 * 正确做法：单独建一个 .cpp，只写这两行。
 */
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/include/stb_image.h"
