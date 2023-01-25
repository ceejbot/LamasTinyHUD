﻿#pragma once
#include "ui/image_path.h"

namespace handle {
    class position_draw_setting {
    public:
        float key_icon_scale_width = 0.f;
        float key_icon_scale_height = 0.f;

        float icon_scale_width = 0.f;
        float icon_scale_height = 0.f;

        uint32_t background_icon_transparency = ui::draw_full;
        uint32_t icon_transparency = ui::draw_full;
        uint32_t key_transparency = ui::draw_full;
        uint32_t text_transparency = ui::draw_full;

        float offset_slot_x = 0.f;
        float offset_slot_y = 0.f;
        float offset_key_x = 0.f;
        float offset_key_y = 0.f;
        float offset_text_x = 0.f;
        float offset_text_y = 0.f;

        float offset_name_text_x = 0.f;
        float offset_name_text_y = 0.f;

        float width_setting = 0.f;
        float height_setting = 0.f;

        float hud_image_scale_width = 0.f;
        float hud_image_scale_height = 0.f;

        [[maybe_unused]] uint32_t background_transparency = ui::draw_full;
    };
}
