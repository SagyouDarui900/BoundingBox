#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "filter2.h"
#include "module2.h"

namespace {

inline void blend_pixel(PIXEL_RGBA& dst, const PIXEL_RGBA& src) {
    if (src.a == 255) {
        dst = src;
    } else if (src.a == 0) {
        return;
    } else {
        const uint32_t a = src.a;
        const uint32_t inv_a = 255 - a;
        dst.r = static_cast<uint8_t>((src.r * a + dst.r * inv_a) / 255);
        dst.g = static_cast<uint8_t>((src.g * a + dst.g * inv_a) / 255);
        dst.b = static_cast<uint8_t>((src.b * a + dst.b * inv_a) / 255);
        dst.a = static_cast<uint8_t>(src.a + (dst.a * inv_a) / 255);
    }
}

void bounding_box(SCRIPT_MODULE_PARAM* param) {
    if (param->get_param_num() != 13) {
        param->set_error("bounding_box expects 13 arguments");
        return;
    }

    auto* pixels = static_cast<PIXEL_RGBA*>(param->get_param_data(0));
    const int width = param->get_param_int(1);
    const int height = param->get_param_int(2);
    const int threshold = std::clamp(param->get_param_int(3), 0, 255);
    const int line_width = std::max(0, param->get_param_int(4));
    const int opacity = std::clamp(param->get_param_int(5), 0, 255);
    const bool fill = param->get_param_int(6) != 0;
    const bool text_front = param->get_param_int(7) != 0;
    const bool binary = param->get_param_int(8) != 0;
    const bool only = param->get_param_int(9) != 0;
    const uint8_t r = static_cast<uint8_t>(std::clamp(param->get_param_int(10), 0, 255));
    const uint8_t g = static_cast<uint8_t>(std::clamp(param->get_param_int(11), 0, 255));
    const uint8_t b = static_cast<uint8_t>(std::clamp(param->get_param_int(12), 0, 255));

    if (!pixels || width <= 0 || height <= 0) {
        param->set_error("invalid image data or image size");
        return;
    }

    // 1. 外接矩形の検出（高速スキャン）
    int top = height;
    int bottom = -1;

    // 上から走査
    for (int y = 0; y < height; ++y) {
        const PIXEL_RGBA* row = pixels + y * width;
        for (int x = 0; x < width; ++x) {
            if (row[x].a >= threshold) {
                top = y;
                goto found_top;
            }
        }
    }
found_top:;

    if (top == height) {
        return;
    }

    // 下から走査
    for (int y = height - 1; y >= top; --y) {
        const PIXEL_RGBA* row = pixels + y * width;
        for (int x = 0; x < width; ++x) {
            if (row[x].a >= threshold) {
                bottom = y;
                goto found_bottom;
            }
        }
    }
found_bottom:;

    int left = width;
    int right = -1;

    // top から bottom までの行について left と right を更新
    for (int y = top; y <= bottom; ++y) {
        const PIXEL_RGBA* row = pixels + y * width;
        for (int x = 0; x < left; ++x) {
            if (row[x].a >= threshold) {
                left = x;
                break;
            }
        }
        for (int x = width - 1; x > right; --x) {
            if (row[x].a >= threshold) {
                right = x;
                break;
            }
        }
    }

    if (right < left || bottom < top) {
        return;
    }

    const uint8_t box_alpha = binary ? 255 : static_cast<uint8_t>(opacity);
    const PIXEL_RGBA box_pixel = { r, g, b, box_alpha };

    // 2. 描画処理
    const int rect_w = right - left + 1;
    const int rect_h = bottom - top + 1;
    const bool all_border = (line_width > 0) && ((line_width * 2 >= rect_w) || (line_width * 2 >= rect_h));

    if (only) {
        // 「ボックスのみ」: 元オブジェクトを消去して矩形のみ描画
        std::memset(pixels, 0, sizeof(PIXEL_RGBA) * width * height);

        if (fill || all_border) {
            for (int y = top; y <= bottom; ++y) {
                PIXEL_RGBA* row = pixels + y * width + left;
                std::fill_n(row, rect_w, box_pixel);
            }
        } else if (line_width > 0) {
            for (int y = top; y <= bottom; ++y) {
                PIXEL_RGBA* row = pixels + y * width;
                if (y < top + line_width || y > bottom - line_width) {
                    std::fill_n(row + left, rect_w, box_pixel);
                } else {
                    std::fill_n(row + left, line_width, box_pixel);
                    std::fill_n(row + right - line_width + 1, line_width, box_pixel);
                }
            }
        }
    } else {
        // 「通常（元オブジェクトを表示）」
        for (int y = top; y <= bottom; ++y) {
            PIXEL_RGBA* row = pixels + y * width;
            const bool is_y_border = (line_width > 0) && (y < top + line_width || y > bottom - line_width);

            if (all_border || is_y_border) {
                // 上下の枠線行
                if (text_front) {
                    // 文字を前面: 文字部分(a >= threshold)は保持、余白のみ枠線色
                    for (int x = left; x <= right; ++x) {
                        if (row[x].a < threshold) {
                            row[x] = box_pixel;
                        }
                    }
                } else {
                    // 図形を前面: 文字の上から枠線を描画（ブレンド）
                    for (int x = left; x <= right; ++x) {
                        blend_pixel(row[x], box_pixel);
                    }
                }
            } else {
                // 内部行の左右枠線
                if (line_width > 0) {
                    const int l_end = left + line_width - 1;
                    const int r_start = right - line_width + 1;

                    if (text_front) {
                        // 文字を前面
                        for (int x = left; x <= l_end; ++x) {
                            if (row[x].a < threshold) row[x] = box_pixel;
                        }
                        for (int x = r_start; x <= right; ++x) {
                            if (row[x].a < threshold) row[x] = box_pixel;
                        }
                    } else {
                        // 図形を前面
                        for (int x = left; x <= l_end; ++x) {
                            blend_pixel(row[x], box_pixel);
                        }
                        for (int x = r_start; x <= right; ++x) {
                            blend_pixel(row[x], box_pixel);
                        }
                    }
                }

                // 枠線の内側（背景塗りつぶし）
                if (fill) {
                    const int inner_start = left + line_width;
                    const int inner_end = right - line_width;
                    for (int x = inner_start; x <= inner_end; ++x) {
                        if (row[x].a < threshold) {
                            row[x] = box_pixel;
                        }
                    }
                }
            }
        }
    }
}

SCRIPT_MODULE_FUNCTION functions[] = {
    {L"bounding_box", bounding_box},
    {nullptr, nullptr},
};

SCRIPT_MODULE_TABLE module = {
    L"BoundingBox for AviUtl ExEdit2",
    functions,
};

} // namespace

extern "C" __declspec(dllexport) bool InitializePlugin(DWORD) {
    return true;
}

extern "C" __declspec(dllexport) void UninitializePlugin() {
}

extern "C" __declspec(dllexport) SCRIPT_MODULE_TABLE* GetScriptModuleTable() {
    return &module;
}
