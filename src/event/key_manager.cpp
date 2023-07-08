#include "key_manager.h"
#include "handle/ammo_handle.h"
#include "handle/extra_data_holder.h"
#include "handle/page_handle.h"
#include "processing/game_menu_setting.h"
#include "processing/setting_execute.h"
#include "setting/mcm_setting.h"
#include "ui/ui_renderer.h"

namespace event {
    using event_result = RE::BSEventNotifyControl;
    using position_type = handle::position_setting::position_type;
    using common = control::common;
    using mcm = config::mcm_setting;
    using setting_execute = processing::setting_execute;

    key_manager* key_manager::get_singleton() {
        static key_manager singleton;
        return std::addressof(singleton);
    }

    void key_manager::sink() {
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(get_singleton());
        logger::info("start listening for input events."sv);
    }

    event_result key_manager::ProcessEvent(RE::InputEvent* const* a_event,
        [[maybe_unused]] RE::BSTEventSource<RE::InputEvent*>* a_event_source) {
        button_press_modify_ = mcm::get_slot_button_feedback();
        auto* key_binding = control::binding::get_singleton();

        if (!a_event) {
            return event_result::kContinue;
        }

        //top execute btn is bound to the shout key, no need to check here
        if (!key_binding->keys_configured()) {
            return event_result::kContinue;
        }

        // If we can't ask questions about the state of the UI, we bail.
        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            return event_result::kContinue;
        }

        // If the console is open, we bail.
        const auto* interface_strings = RE::InterfaceStrings::GetSingleton();
        if (ui->IsMenuOpen(interface_strings->console)) {
            return event_result::kContinue;
        }

        // Check if one of our relevant menus is open: inventory, magic, or favorites.
        // If so, we don't handle this event.
        if (processing::game_menu_setting::relevant_menu_open(ui)) {
            return event_result::kContinue;
        }

        handle::extra_data_holder::get_singleton()->reset_data();

        // We might get a list of events to handle.
        for (auto* event = *a_event; event; event = event->next) {
            if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                continue;
            }

            //this stays static_cast
            const auto* button =
                static_cast<RE::ButtonEvent*>(event);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

            key_ = button->idCode;
            if (key_ == common::k_invalid) {
                continue;
            }

            common::get_key_id(button, key_);

            if (const auto* control_map = RE::ControlMap::GetSingleton(); !control_map->IsMovementControlsEnabled()) {
                continue;
            }

            /*if the game is not paused with the menu, it triggers the menu always in the background*/
            if (ui->GameIsPaused() || !ui->IsCursorHiddenWhenTopmost() || !ui->IsShowingMenus() ||
                !ui->GetMenu<RE::HUDMenu>()) {
                continue;
            }

            if (RE::UI::GetSingleton()->IsMenuOpen("LootMenu") && mcm::get_disable_input_quick_loot()) {
                continue;
            }

            // If the player can't move, e.g., we're in the opening scene, we bail.
            const auto* control_map = RE::ControlMap::GetSingleton();
            if (!control_map || !control_map->IsMovementControlsEnabled() ||
                control_map->contextPriorityStack.back() != RE::UserEvents::INPUT_CONTEXT_ID::kGameplay) {
                continue;
            }

            //get shout key
            auto elden = mcm::get_elden_demon_souls();
            if (elden) {
                RE::UserEvents* user_events = RE::UserEvents::GetSingleton();
                key_binding->set_top_execute(control_map->GetMappedKey(user_events->shout, button->device.get()));
            }

            // These are the buttons for cycling through positional lists.
            auto is_position_button = key_binding->is_position_button(key_);
            auto is_showhide_key = common::is_key_valid_and_matches(key_, key_binding->get_hide_show());
            auto is_power_key = common::is_key_valid_and_matches(key_, key_binding->get_top_execute());
            auto is_utility_key = key_ == key_binding->get_bottom_action();
            auto is_toggle_key =
                common::is_key_valid_and_matches(key_, key_binding->get_bottom_execute_or_toggle_action());
            auto execute_requires_modifier = mcm::get_bottom_execute_key_combo_only();

            if (mcm::get_hide_outside_combat() && !ui::ui_renderer::get_fade()) {
                if ((is_position_button || is_toggle_key || (elden && is_power_key)) &&
                    (button->IsDown() || button->IsPressed())) {
                    ui::ui_renderer::set_fade(true, 1.f);
                }
            }

            if (button->IsDown() && is_position_button) {
                logger::debug("configured key ({}) is down"sv, key_);
                auto* position_setting = setting_execute::get_position_setting_for_key(key_);
                if (!position_setting) {
                    logger::warn("setting for key {} is null. break."sv, key_);
                    continue;
                }
                do_button_down(position_setting);
            }

            if (button->IsUp() && is_position_button) {
                logger::debug("configured Key ({}) is up"sv, key_);
                //set slot back to normal color
                // Look up the current thing-we-would-do for this keypress, then do it.
                // E.g., equip the next item in the cycle.
                auto* position_setting = setting_execute::get_position_setting_for_key(key_);
                if (!position_setting) {
                    logger::warn("setting for key {} is null. break."sv, key_);
                    continue;
                }
                position_setting->button_press_modify = ui::draw_full;
                if (position_setting->position == position_type::left) {
                    if (auto* current_ammo = handle::ammo_handle::get_singleton()->get_current()) {
                        current_ammo->button_press_modify = ui::draw_full;
                    }
                }
            }

            // Is this key the toggle key for soulsy mode?
            if (elden && execute_requires_modifier && is_toggle_key) {
                // If we previously recorded a toggle key down, check if it's up now
                if (button->IsUp() && mToggleModeEntered) {
                    mToggleModeEntered = false;
                }
                // The toggle button has been pressed.
                if (button->IsDown()) {
                    mToggleModeEntered = true;
                }
            }

            // We've handled all the button-up cases. Button-down only from here on.
            if (!button->IsDown()) {
                continue;
            }

            // Note to self: figure out the difference between button down and is-pressed
            if (button->IsPressed() && is_showhide_key) {
                ui::ui_renderer::toggle_show_ui();
            }

            // For the normal mode, which we'll be deleting, the toggle key is the page key.
            if (button->IsPressed() && !elden && is_toggle_key) {
                logger::debug("configured toggle key ({}) is pressed"sv, key_);

                const auto* handler = handle::page_handle::get_singleton();
                handler->set_active_page(handler->get_next_page_id());
            }

            // We're always in soulsy-mode for our version.
            // We will also be deleting the concept of a modifier key needed to execute the utility.
            // So this will clean up some.

            auto toggle_key_is_enough = !execute_requires_modifier && is_toggle_key;
            auto utility_execute_requested = execute_requires_modifier && mToggleModeEntered && is_utility_key;

            if (elden && button->IsPressed()) {
                if (toggle_key_is_enough || utility_execute_requested) {
                    auto* page_setting = setting_execute::get_position_setting_for_key(key_);
                    if (!page_setting) {
                        logger::warn("setting for key {} is null. break."sv, key_);
                        break;
                    }
                    setting_execute::execute_settings(page_setting->slot_settings);
                }
                if (is_power_key) {
                    auto* page_setting = setting_execute::get_position_setting_for_key(key_);
                    if (!page_setting) {
                        logger::warn("setting for key {} is null. break."sv, key_);
                        break;
                    }
                    // only instant should need work, the default shout will be handled by the game
                    setting_execute::execute_settings(page_setting->slot_settings, false, true);
                }
            }

            if (is_position_button && button->IsPressed()) {
                do_button_press(key_, key_binding);
            }
        } // end event handling for loop

        return event_result::kContinue;
    }

    void key_manager::do_button_press(uint32_t a_key, control::binding*& a_binding) const {
        logger::debug("configured Key ({}) pressed"sv, a_key);
        auto* position_setting = setting_execute::get_position_setting_for_key(a_key);

        // Simple case first, then early return for readability.
        if (!mcm::get_elden_demon_souls()) {
            setting_execute::execute_settings(position_setting->slot_settings);
            return;
        }

        // From here on we're in Souls game mode.

        // If we're in utility execute requested mode, do nothing, because we already did it
        // in the function that called this one. This seems like it needs untangling.
        auto execute_requires_modifier = mcm::get_bottom_execute_key_combo_only();
        if (execute_requires_modifier && mToggleModeEntered && a_key == a_binding->get_bottom_action()) {
            return;
        }

        const auto* key_handler = handle::key_position_handle::get_singleton();
        const auto* page_handler = handle::page_handle::get_singleton();

        // Is this position locked? If so, we just check our ammo and exit.
        if (key_handler->is_position_locked(position_setting->position)) {
            logger::trace("position {} is locked, skip"sv, static_cast<uint32_t>(position_setting->position));
            //check ammo is set, might be a bow or crossbow present
            const auto* ammo_handle = handle::ammo_handle::get_singleton();
            if (const auto next_ammo = ammo_handle->get_next_ammo()) {
                setting_execute::execute_ammo(next_ammo);
                handle::ammo_handle::get_singleton()->get_current()->highlight_slot = true;
            }
            return;
        }

        // Advance our cycle one step. Why aren't we returning the settings from this call?
        page_handler->set_active_page_position(
            page_handler->get_next_non_empty_setting_for_position(position_setting->position),
            position_setting->position);
        
        // Get the new position setting. If we get nothing here, we are in a state where
        // we can't do anything useful.
        auto* new_position = setting_execute::get_position_setting_for_key(a_key);

        if (!new_position) {
            logger::warn("setting for key {} is null. break."sv, key_);
            return;
        }
        new_position->highlight_slot = true;
        if (!scroll_position(a_key, a_binding)) {
            setting_execute::execute_settings(new_position->slot_settings);
        } else if (new_position->position == position_type::top) {
            setting_execute::execute_settings(new_position->slot_settings, true);
        }
    }

    bool key_manager::scroll_position(const uint32_t a_key, control::binding*& a_binding) {
        if (a_key == a_binding->get_bottom_action() || a_key == a_binding->get_top_action()) {
            return true;
        }
        return false;
    }

    void key_manager::do_button_down(handle::position_setting*& a_position_setting) const {
        if (!a_position_setting) {
            return;
        }
        if (!handle::key_position_handle::get_singleton()->is_position_locked(a_position_setting->position)) {
            a_position_setting->button_press_modify = button_press_modify_;
        } else {
            if (a_position_setting->position == position_type::left) {
                if (auto* current_ammo = handle::ammo_handle::get_singleton()->get_current()) {
                    current_ammo->button_press_modify = button_press_modify_;
                }
            }
        }
    }
}
