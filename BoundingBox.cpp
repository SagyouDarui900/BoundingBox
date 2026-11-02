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

inline void blend_pixel_premul(PIXEL_RGBA& dst, const PIXEL_RGBA& src) {
    if (src.a == 255) {
        dst = src;
    } else if (src.a == 0) {
        return;
    } else {
        const uint32_t inv_a = 255 - src.a;
        dst.r = static_cast<uint8_t>(std::min(255u, src.r + (dst.r * inv_a + 127) / 255));
        dst.g = static_cast<uint8_t>(std::min(255u, src.g + (dst.g * inv_a + 127) / 255));
        dst.b = static_cast<uint8_t>(std::min(255u, src.b + (dst.b * inv_a + 127) / 255));
        dst.a = static_cast<uint8_t>(std::min(255u, src.a + (dst.a * inv_a + 127) / 255));
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
    // NUMBER やその他の場合は整数値として判定（0はfalse）
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

    // 1. 外接矩形の検出（高速スキャン）
    int top = height;
    int bottom = -1;

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

    // アルファ値の決定 (binary有効時は255、無効時はopacity)
    const uint8_t box_alpha = binary ? 255 : static_cast<uint8_t>(opacity);

    // Premultiplied Alpha
    const uint8_t pr = static_cast<uint8_t>((r * box_alpha + 127) / 255);
    const uint8_t pg = static_cast<uint8_t>((g * box_alpha + 127) / 255);
    const uint8_t pb = static_cast<uint8_t>((b * box_alpha + 127) / 255);
    const PIXEL_RGBA box_pixel = { pr, pg, pb, box_alpha };

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
                    for (int x = left; x <= right; ++x) {
                        if (row[x].a < threshold) {
                            row[x] = box_pixel;
                        }
                    }
                } else {
                    for (int x = left; x <= right; ++x) {
                        blend_pixel_premul(row[x], box_pixel);
                    }
                }
            } else {
                // 内部行の左右枠線
                if (line_width > 0) {
                    const int l_end = left + line_width - 1;
                    const int r_start = right - line_width + 1;

                    if (text_front) {
                        for (int x = left; x <= l_end; ++x) {
                            if (row[x].a < threshold) row[x] = box_pixel;
                        }
                        for (int x = r_start; x <= right; ++x) {
                            if (row[x].a < threshold) row[x] = box_pixel;
                        }
                    } else {
                        for (int x = left; x <= l_end; ++x) {
                            blend_pixel_premul(row[x], box_pixel);
                        }
                        for (int x = r_start; x <= right; ++x) {
                            blend_pixel_premul(row[x], box_pixel);
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
