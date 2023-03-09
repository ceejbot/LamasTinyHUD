#pragma once

namespace control {
    class common {
    public:
        enum : uint32_t {
            k_invalid = static_cast<uint32_t>(-1),
            k_keyboard_offset = 0,
            k_mouse_offset = 256,
            k_gamepad_offset = 266
        };

        static void get_key_id(const RE::ButtonEvent* a_button, uint32_t& a_key);

        static bool is_key_valid(uint32_t a_key);
        static bool is_key_valid_and_matches(uint32_t a_key, uint32_t a_key_to_check);

    private:
        static uint32_t get_gamepad_index(RE::BSWin32GamepadDevice::Key a_key);
    };
}  // control
