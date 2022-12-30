﻿#pragma once


#include <windows.h>
#include <WinUser.h>

namespace ui {
    class ui_renderer {
        struct wnd_proc_hook {
            static LRESULT thunk(HWND h_wnd, UINT u_msg, WPARAM w_param, LPARAM l_param);
            static inline WNDPROC func;
        };

        struct d_3d_init_hook {
            static void thunk();
            static inline REL::Relocation<decltype(thunk)> func;

            static constexpr auto id = REL::RelocationID(75595, 77226);
            static constexpr auto offset = REL::VariantOffset(0x9, 0x275, 0x00); // VR unknown

            static inline std::atomic<bool> initialized = false;
        };

        struct dxgi_present_hook {
            static void thunk(std::uint32_t a_p1);
            static inline REL::Relocation<decltype(thunk)> func;

            static constexpr auto id = REL::RelocationID(75461, 77246);
            static constexpr auto offset = REL::Offset(0x9);
        };

        ui_renderer();

        static void draw_ui();
        static bool load_texture_from_file(const char* filename,
            ID3D11ShaderResourceView** out_srv,
            std::int32_t& out_width,
            std::int32_t& out_height);
        static void message_callback(SKSE::MessagingInterface::Message* msg);

        static inline bool show_ui_ = false;
        static inline ID3D11Device* device_ = nullptr;
        static inline ID3D11DeviceContext* context_ = nullptr;

    public:
        static bool install();

        static float get_resolution_scale_width();
        static float get_resolution_scale_height();

        static float get_resolution_width();
        static float get_resolution_height();
    };
}