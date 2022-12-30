﻿#include "ui_renderer.h"

#include <d3d11.h>

#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <dxgi.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "event/sink_event.h"
#include "handle/set_data.h"
#include "setting/file_setting.h"
#include "setting/mcm_setting.h"
#include "util/constant.h"

namespace ui {
    struct image {
        ID3D11ShaderResourceView* texture = nullptr;
        int32_t width = 0;
        int32_t height = 0;
    };

    static image image_struct[static_cast<int32_t>(util::image_type::total)];

    LRESULT ui_renderer::wnd_proc_hook::thunk(const HWND h_wnd,
        const UINT u_msg,
        const WPARAM w_param,
        const LPARAM l_param) {
        auto& io = ImGui::GetIO();
        if (u_msg == WM_KILLFOCUS) {
            io.ClearInputCharacters();
            io.ClearInputKeys();
        }

        return func(h_wnd, u_msg, w_param, l_param);
    }

    void ui_renderer::d_3d_init_hook::thunk() {
        func();

        logger::info("D3DInit Hooked"sv);
        const auto render_manager = RE::BSRenderManager::GetSingleton();
        if (!render_manager) {
            logger::error("Cannot find render manager. Initialization failed."sv);
            return;
        }

        const auto [forwarder, context, unk58, unk60, unk68, swapChain, unk78, unk80, renderView, resourceView] =
            render_manager->GetRuntimeData();

        logger::info("Getting swapchain..."sv);
        const auto swapchain = swapChain;
        if (!swapchain) {
            logger::error("Cannot find render manager. Initialization failed."sv);
            return;
        }

        logger::info("Getting swapchain desc..."sv);
        DXGI_SWAP_CHAIN_DESC sd{};
        if (swapchain->GetDesc(std::addressof(sd)) < 0) {
            logger::error("IDXGISwapChain::GetDesc failed."sv);
            return;
        }

        device_ = forwarder;
        context_ = context;

        logger::info("Initializing ImGui..."sv);
        ImGui::CreateContext();
        if (!ImGui_ImplWin32_Init(sd.OutputWindow)) {
            logger::error("ImGui initialization failed (Win32)");
            return;
        }
        if (!ImGui_ImplDX11_Init(device_, context_)) {
            logger::error("ImGui initialization failed (DX11)"sv);
            return;
        }
        logger::info("ImGui initialized.");

        initialized.store(true);

        wnd_proc_hook::func = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(
                sd.OutputWindow,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(wnd_proc_hook::thunk)));
        if (!wnd_proc_hook::func) {
            logger::error("SetWindowLongPtrA failed"sv);
        }
    }

    void ui_renderer::dxgi_present_hook::thunk(std::uint32_t a_p1) {
        func(a_p1);

        if (!d_3d_init_hook::initialized.load()) {
            return;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        draw_ui();

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    // Simple helper function to load an image into a DX11 texture with common settings
    bool ui_renderer::load_texture_from_file(const char* filename,
        ID3D11ShaderResourceView** out_srv,
        std::int32_t& out_width,
        std::int32_t& out_height) {
        auto render_manager = RE::BSRenderManager::GetSingleton();
        if (!render_manager) {
            logger::error("Cannot find render manager. Initialization failed."sv);
            return false;
        }

        auto [forwarder, context, unk58, unk60, unk68, swapChain, unk78, unk80, renderView, resourceView] =
            render_manager->GetRuntimeData();

        // Load from disk into a raw RGBA buffer
        int image_width = 0;
        int image_height = 0;
        unsigned char* image_data = stbi_load(filename, &image_width, &image_height, nullptr, 4);
        if (image_data == nullptr) {
            return false;
        }

        // Create texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = image_width;
        desc.Height = image_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        ID3D11Texture2D* p_texture = nullptr;
        D3D11_SUBRESOURCE_DATA sub_resource;
        sub_resource.pSysMem = image_data;
        sub_resource.SysMemPitch = desc.Width * 4;
        sub_resource.SysMemSlicePitch = 0;
        device_->CreateTexture2D(&desc, &sub_resource, &p_texture);

        // Create texture view
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
        ZeroMemory(&srv_desc, sizeof srv_desc);
        srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = desc.MipLevels;
        srv_desc.Texture2D.MostDetailedMip = 0;
        forwarder->CreateShaderResourceView(p_texture, &srv_desc, out_srv);
        p_texture->Release();

        out_width = image_width;
        out_height = image_height;
        stbi_image_free(image_data);

        return true;
    }

    ui_renderer::ui_renderer() = default;

    void ui_renderer::draw_element(ID3D11ShaderResourceView* a_texture,
        const ImVec2 a_center,
        const ImVec2 a_size,
        const float a_angle) {
        const float cos_a = cosf(a_angle);
        const float sin_a = sinf(a_angle);
        const ImVec2 pos[4] =
        {
            a_center + ImRotate(ImVec2(-a_size.x * 0.5f, -a_size.y * 0.5f), cos_a, sin_a),
            a_center + ImRotate(ImVec2(+a_size.x * 0.5f, -a_size.y * 0.5f), cos_a, sin_a),
            a_center + ImRotate(ImVec2(+a_size.x * 0.5f, +a_size.y * 0.5f), cos_a, sin_a),
            a_center + ImRotate(ImVec2(-a_size.x * 0.5f, +a_size.y * 0.5f), cos_a, sin_a)

        };
        constexpr ImVec2 uvs[4] =
        {
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            ImVec2(0.0f, 1.0f)
        };


        
        ImGui::GetWindowDrawList()->AddImageQuad(a_texture,
            pos[0],
            pos[1],
            pos[2],
            pos[3],
            uvs[0],
            uvs[1],
            uvs[2],
            uvs[3],
            IM_COL32_WHITE);
    }

    void ui_renderer::draw_hud(const float a_x, const float a_y) {
        constexpr auto angle = 0.f;

        const auto width_setting = config::mcm_setting::get_hud_image_position_width();
        const auto height_setting = config::mcm_setting::get_hud_image_position_height();

        ImVec2 center;
        if (width_setting > a_x || height_setting > a_y) {
            center = ImVec2(0.f, 0.f);
        } else {
            center = ImVec2(width_setting, height_setting);
        }

        const auto [texture, width, height] = image_struct[static_cast<int32_t>(util::image_type::hud)];

        const auto size = ImVec2(
            static_cast<float>(width) * config::mcm_setting::get_hud_image_scale_width(),
            static_cast<float>(height) * config::mcm_setting::get_hud_image_scale_height());

        draw_element(texture, center, size, angle);
    }


    void ui_renderer::draw_slot(const float a_x, const float a_y, const float a_offset_x, const float a_offset_y) {
        constexpr auto angle = 0.f;

        //get data from normal hud, and add an offset config for each image
        const auto width_setting = config::mcm_setting::get_hud_image_position_width();
        const auto height_setting = config::mcm_setting::get_hud_image_position_height();

        ImVec2 center;
        if (width_setting > a_x || height_setting > a_y) {
            center = ImVec2(0.f, 0.f);
        } else {
            center = ImVec2(width_setting + a_offset_x, height_setting + a_offset_y);
        }

        const auto [texture, width, height] = image_struct[static_cast<int32_t>(util::image_type::round)];

        //for now use scale from normal setting, but later add separate config
        const auto size = ImVec2(
            static_cast<float>(width) * config::mcm_setting::get_hud_image_scale_width() +
            config::file_setting::get_extra_size_for_image(),
            static_cast<float>(height) * config::mcm_setting::get_hud_image_scale_height() +
            config::file_setting::get_extra_size_for_image());

        draw_element(texture, center, size, angle);
    }

    void ui_renderer::draw_slots(const float a_x, const float a_y) {
        const auto offset = config::mcm_setting::get_hud_slot_position_offset();
        //T -> -y
        //R -> +X
        //D -> +Y
        //L -> -X
        draw_slot(a_x, a_y, 0.f, -offset);
        draw_slot(a_x, a_y, offset, 0.f);
        draw_slot(a_x, a_y, 0.f, offset);
        draw_slot(a_x, a_y, -offset, 0.f);
    }


    void ui_renderer::draw_key(const float a_x, const float a_y, const float a_offset_x, const float a_offset_y) {
        constexpr auto angle = 0.f;

        //get data from normal hud, and add an offset config for each image
        const auto width_setting = config::mcm_setting::get_hud_image_position_width();
        const auto height_setting = config::mcm_setting::get_hud_image_position_height();

        ImVec2 center;
        if (width_setting > a_x || height_setting > a_y) {
            center = ImVec2(0.f, 0.f);
        } else {
            center = ImVec2(width_setting + a_offset_x, height_setting + a_offset_y);
        }

        const auto [texture, width, height] = image_struct[static_cast<int32_t>(util::image_type::key)];

        //for now use scale from normal setting, but later add separate config
        //add hardcoded 1px 
        const auto size = ImVec2(
            static_cast<float>(width) * config::mcm_setting::get_hud_image_scale_width() +
            config::file_setting::get_extra_size_for_image(),
            static_cast<float>(height) * config::mcm_setting::get_hud_image_scale_height() +
            config::file_setting::get_extra_size_for_image());

        draw_element(texture, center, size, angle);
    }

    void ui_renderer::draw_keys(const float a_x, const float a_y) {
        const auto offset = config::mcm_setting::get_hud_key_position_offset();
        draw_key(a_x, a_y, 0.f, -offset);
        draw_key(a_x, a_y, offset, 0.f);
        draw_key(a_x, a_y, 0.f, offset);
        draw_key(a_x, a_y, -offset, 0.f);
    }


    void ui_renderer::draw_ui() {
        if (!show_ui_)
            return;

        if (const auto ui = RE::UI::GetSingleton();
            !ui || ui->GameIsPaused() || !ui->IsCursorHiddenWhenTopmost() || !ui->IsShowingMenus() || !ui->GetMenu<
                RE::HUDMenu>()) {
            return;
        }

        if (const auto control_map = RE::ControlMap::GetSingleton();
            !control_map || !control_map->IsMovementControlsEnabled() ||
            control_map->contextPriorityStack.back() != RE::UserEvents::INPUT_CONTEXT_ID::kGameplay) {
            return;
        }

        static constexpr ImGuiWindowFlags window_flag =
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs;


        const float screen_size_x = ImGui::GetIO().DisplaySize.x, screen_size_y = ImGui::GetIO().DisplaySize.y;


        ImGui::SetNextWindowSize(ImVec2(screen_size_x, screen_size_y));
        ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));

        ImGui::Begin(util::hud_name, nullptr, window_flag);

        draw_hud(screen_size_x, screen_size_y);
        draw_slots(screen_size_x, screen_size_y);
        draw_keys(screen_size_x, screen_size_y);

        ImGui::End();
    }


    void ui_renderer::message_callback(SKSE::MessagingInterface::Message* msg)
    //CallBack & LoadTextureFromFile should called after resource loaded.
    {
        if (msg->type == SKSE::MessagingInterface::kDataLoaded && d_3d_init_hook::initialized) {
            // Read Texture only after game engine finished load all it renderer resource.

            for (auto [type, path] : util::image_type_path_map) {
                const auto idx = static_cast<int32_t>(type);
                if (load_texture_from_file(path,
                    &image_struct[idx].texture,
                    image_struct[idx].width,
                    image_struct[idx].height)) {
                    logger::info("loaded texture {}"sv, path);
                } else {
                    logger::error("failed to load texture {}"sv, path);
                }

                image_struct[idx].width *= static_cast<int32_t>(get_resolution_scale_width());
                image_struct[idx].height *= static_cast<int32_t>(get_resolution_scale_height());
            }


            show_ui_ = true;
            event::sink_events();
            //config is already loaded
            handle::set_data::set_slot_data();
            logger::info("done with data loaded");
        }
    }

    bool ui_renderer::install() {
        auto g_message = SKSE::GetMessagingInterface();
        if (!g_message) {
            logger::error("Messaging Interface Not Found. return."sv);
            return false;
        }

        g_message->RegisterListener(message_callback);

        SKSE::AllocTrampoline(14 * 2);

        stl::write_thunk_call<d_3d_init_hook>();
        stl::write_thunk_call<dxgi_present_hook>();

        logger::info("installed callback for ui. return.");
        return true;
    }

    float ui_renderer::get_resolution_scale_width() {
        return ImGui::GetIO().DisplaySize.x / 1920.f;
    }

    float ui_renderer::get_resolution_scale_height() {
        return ImGui::GetIO().DisplaySize.y / 1080.f;
    }

    float ui_renderer::get_resolution_width() {
        return ImGui::GetIO().DisplaySize.x;
    }

    float ui_renderer::get_resolution_height() {
        return ImGui::GetIO().DisplaySize.y;
    }
}
