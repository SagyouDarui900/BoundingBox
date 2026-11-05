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

// Straight Alpha Porter-Duff "Over" 合成:
// 前景 src を 背景 dst の上に重ねて dst に書き込む
inline void blend_over(PIXEL_RGBA& dst, const PIXEL_RGBA& src) {
    if (src.a == 0) {
        return;
    }
    if (src.a == 255 || dst.a == 0) {
        dst = src;
        return;
    }

    const uint32_t src_a = src.a;
    const uint32_t dst_a = dst.a;
    const uint32_t inv_src_a = 255 - src_a;
    const uint32_t out_a = src_a * 255 + dst_a * inv_src_a;

    const uint32_t pr = src.r * src_a * 255 + dst.r * dst_a * inv_src_a;
    const uint32_t pg = src.g * src_a * 255 + dst.g * dst_a * inv_src_a;
    const uint32_t pb = src.b * src_a * 255 + dst.b * dst_a * inv_src_a;

    dst.r = static_cast<uint8_t>((pr + out_a / 2) / out_a);
    dst.g = static_cast<uint8_t>((pg + out_a / 2) / out_a);
    dst.b = static_cast<uint8_t>((pb + out_a / 2) / out_a);
    dst.a = static_cast<uint8_t>((out_a + 127) / 255);
}

// Straight Alpha Porter-Duff "Under" 合成:
// 前景 dst (文字) の背後に 背景 bg (ボックス) を敷いて dst に書き込む
inline void blend_under(PIXEL_RGBA& dst, const PIXEL_RGBA& bg) {
    if (bg.a == 0) {
        return;
    }
    if (dst.a == 0) {
        dst = bg;
        return;
    }
    if (dst.a == 255) {
        return;
    }
    if (bg.a == 255) {
        const uint32_t dst_a = dst.a;
        const uint32_t inv_dst_a = 255 - dst_a;
        dst.r = static_cast<uint8_t>((dst.r * dst_a + bg.r * inv_dst_a + 127) / 255);
        dst.g = static_cast<uint8_t>((dst.g * dst_a + bg.g * inv_dst_a + 127) / 255);
        dst.b = static_cast<uint8_t>((dst.b * dst_a + bg.b * inv_dst_a + 127) / 255);
        dst.a = 255;
        return;
    }

    const uint32_t dst_a = dst.a;
    const uint32_t bg_a = bg.a;
    const uint32_t inv_dst_a = 255 - dst_a;
    const uint32_t out_a = dst_a * 255 + bg_a * inv_dst_a;

    const uint32_t pr = dst.r * dst_a * 255 + bg.r * bg_a * inv_dst_a;
    const uint32_t pg = dst.g * dst_a * 255 + bg.g * bg_a * inv_dst_a;
    const uint32_t pb = dst.b * dst_a * 255 + bg.b * bg_a * inv_dst_a;

    dst.r = static_cast<uint8_t>((pr + out_a / 2) / out_a);
    dst.g = static_cast<uint8_t>((pg + out_a / 2) / out_a);
    dst.b = static_cast<uint8_t>((pb + out_a / 2) / out_a);
    dst.a = static_cast<uint8_t>((out_a + 127) / 255);
}

inline void apply_box_pixel(PIXEL_RGBA& pixel, const PIXEL_RGBA& box_pixel, bool text_front) {
    if (text_front) {
        blend_under(pixel, box_pixel);
    } else {
        blend_over(pixel, box_pixel);
    }
}

int get_int_value(SCRIPT_MODULE_PARAM* param, int index) {
    auto type = param->get_param_type ? param->get_param_type(index) : PARAM_TYPE::NONE;
    if (type == PARAM_TYPE::BOOLEAN) {
        return param->get_param_boolean(index) ? 1 : 0;
    }
    int v = param->get_param_int(index);
    if (v == 0) {
        double dv = param->get_param_double(index);
        if (dv != 0.0) {
            v = static_cast<int>(dv + 0.5);
        }
    }
    return v;
}

bool get_bool_value(SCRIPT_MODULE_PARAM* param, int index) {
    auto type = param->get_param_type ? param->get_param_type(index) : PARAM_TYPE::NONE;
    if (type == PARAM_TYPE::BOOLEAN) {
        return param->get_param_boolean(index);
    }
    return get_int_value(param, index) != 0;
}

void bounding_box(SCRIPT_MODULE_PARAM* param) {
    if (param->get_param_num() != 13) {
        param->set_error("bounding_box expects 13 arguments");
        return;
    }

    auto* pixels = static_cast<PIXEL_RGBA*>(param->get_param_data(0));
    const int width = get_int_value(param, 1);
    const int height = get_int_value(param, 2);
    const int threshold = std::clamp(get_int_value(param, 3), 0, 255);
    const int line_width = std::max(0, get_int_value(param, 4));
    const int opacity = std::clamp(get_int_value(param, 5), 0, 255);
    const bool fill = get_bool_value(param, 6);
    const bool text_front = get_bool_value(param, 7);
    const bool binary = get_bool_value(param, 8);
    const bool only = get_bool_value(param, 9);
    const uint8_t r = static_cast<uint8_t>(std::clamp(get_int_value(param, 10), 0, 255));
    const uint8_t g = static_cast<uint8_t>(std::clamp(get_int_value(param, 11), 0, 255));
    const uint8_t b = static_cast<uint8_t>(std::clamp(get_int_value(param, 12), 0, 255));

    if (!pixels || width <= 0 || height <= 0) {
        param->set_error("invalid image data or image size");
        return;
    }

    // 1. 外接矩形の検出
    // 完全に透明なピクセルを除外するため最小走査しきい値を1とする
    const int scan_threshold = std::max(1, threshold);
    int top = height;
    int bottom = -1;

    for (int y = 0; y < height; ++y) {
        const PIXEL_RGBA* row = pixels + y * width;
        for (int x = 0; x < width; ++x) {
            if (row[x].a >= scan_threshold) {
                top = y;
                goto found_top;
            }
        }
    }
found_top:;

    if (top == height) {
        return;
    }

    for (int y = height - 1; y >= top; --y) {
        const PIXEL_RGBA* row = pixels + y * width;
        for (int x = 0; x < width; ++x) {
            if (row[x].a >= scan_threshold) {
                bottom = y;
                goto found_bottom;
            }
        }
    }
found_bottom:;

    int left = width;
    int right = -1;

    for (int y = top; y <= bottom; ++y) {
        const PIXEL_RGBA* row = pixels + y * width;
        for (int x = 0; x < left; ++x) {
            if (row[x].a >= scan_threshold) {
                left = x;
                break;
            }
        }
        for (int x = width - 1; x > right; --x) {
            if (row[x].a >= scan_threshold) {
                right = x;
                break;
            }
        }
    }

    if (right < left || bottom < top) {
        return;
    }

    // Straight Alpha 形式のボックスピクセル
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
        // 「通常（元オブジェクトと合成）」
        if (fill || all_border) {
            for (int y = top; y <= bottom; ++y) {
                PIXEL_RGBA* row = pixels + y * width;
                for (int x = left; x <= right; ++x) {
                    apply_box_pixel(row[x], box_pixel, text_front);
                }
            }
        } else if (line_width > 0) {
            for (int y = top; y <= bottom; ++y) {
                PIXEL_RGBA* row = pixels + y * width;
                if (y < top + line_width || y > bottom - line_width) {
                    for (int x = left; x <= right; ++x) {
                        apply_box_pixel(row[x], box_pixel, text_front);
                    }
                } else {
                    const int l_end = left + line_width - 1;
                    const int r_start = right - line_width + 1;
                    for (int x = left; x <= l_end; ++x) {
                        apply_box_pixel(row[x], box_pixel, text_front);
                    }
                    for (int x = r_start; x <= right; ++x) {
                        apply_box_pixel(row[x], box_pixel, text_front);
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
