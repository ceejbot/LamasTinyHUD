﻿#pragma once
#include "page/page_setting.h"

namespace handle {
    class page_handle {
    public:
        static page_handle* get_singleton();
        void init_page([[maybe_unused]] uint32_t a_page,
            page_setting::position a_position,
            RE::TESForm* a_form,
            util::selection_type a_type,
            float a_slot_offset,
            float a_key_offset);
        [[nodiscard]] page_setting* get_page_setting(page_setting::position a_position) const;
        [[nodiscard]] std::map<page_setting::position, page_setting*> get_page() const;

        page_handle(const page_handle&) = delete;
        page_handle(page_handle&&) = delete;

        page_handle& operator=(const page_handle&) const = delete;
        page_handle& operator=(page_handle&&) const = delete;

    private:
        page_handle()
            : data_(nullptr) {}

        ~page_handle() = default;

        static void get_offset_values(page_setting::position a_position,
            float a_setting,
            float& offset_x,
            float& offset_y);

        struct page_handle_data {
            std::map<page_setting::position, page_setting*> page_settings;
        };

        page_handle_data* data_;
    };
}