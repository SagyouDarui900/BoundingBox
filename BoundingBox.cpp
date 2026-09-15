#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>

#include "filter2.h"
#include "module2.h"

namespace {

bool is_occupied(const PIXEL_RGBA& pixel, int threshold) {
    return pixel.a >= threshold;
}

void set_box_pixel(PIXEL_RGBA& pixel, int r, int g, int b, int opacity) {
    pixel.r = static_cast<unsigned char>(std::clamp(r, 0, 255));
    pixel.g = static_cast<unsigned char>(std::clamp(g, 0, 255));
    pixel.b = static_cast<unsigned char>(std::clamp(b, 0, 255));
    pixel.a = static_cast<unsigned char>(std::clamp(opacity, 0, 255));
}

void bounding_box(SCRIPT_MODULE_PARAM* param) {
    if (param->get_param_num() != 10) {
        param->set_error("bounding_box expects 10 arguments");
        return;
    }

    auto* pixels = static_cast<PIXEL_RGBA*>(param->get_param_data(0));
    const int width = param->get_param_int(1);
    const int height = param->get_param_int(2);
    const int threshold = std::clamp(param->get_param_int(3), 0, 255);
    const int opacity = std::clamp(param->get_param_int(4), 0, 255);
    const bool binary = param->get_param_int(5) != 0;
    const bool only = param->get_param_int(6) != 0;
    const int r = param->get_param_int(7);
    const int g = param->get_param_int(8);
    const int b = param->get_param_int(9);

    if (!pixels || width <= 0 || height <= 0) {
        param->set_error("invalid image data or image size");
        return;
    }

    int left = width;
    int top = height;
    int right = -1;
    int bottom = -1;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto& pixel = pixels[y * width + x];
            if (binary) {
                pixel.a = pixel.a >= threshold ? 255 : 0;
            }
            if (!is_occupied(pixel, threshold)) {
                continue;
            }
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }

    if (right < left || bottom < top) {
        return;
    }

    if (only) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                pixels[y * width + x].a = 0;
            }
        }
    }

    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const bool edge = x == left || x == right || y == top || y == bottom;
            auto& pixel = pixels[y * width + x];
            if (only || edge || pixel.a == 0) {
                set_box_pixel(pixel, r, g, b, opacity);
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
