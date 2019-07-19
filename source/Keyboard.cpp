#include <unistd.h>
#include <iostream>
#include <algorithm>

#include "Keyboard.hpp"
#include "Commands.hpp"
#include "Server.hpp"


std::map<std::string, uint32_t> modifiersLst = {
  {"Alt", WLR_MODIFIER_ALT},
  {"Ctrl", WLR_MODIFIER_CTRL}
};

Keyboard::Keyboard(struct wlr_input_device *device)
  : keymap(nullptr)
  , device(device)
{
  key_repeat_source = wl_event_loop_add_timer(Server::getInstance().wl_event_loop, keyboard_handle_repeat, this);

  shortcuts["Alt+Return"] = {"Terminal", [](){ Commands::open_terminal(); }};
  shortcuts["Alt+F4"] = {"destroy", [](){
    Server &server = Server::getInstance();
    for (auto &view : server.views)
      {
	if (view->xdg_surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL &&
	    view->xdg_surface->toplevel->server_pending.activated)
	  {
	    view->close();
	    break;
	  }
      }
  }};
  shortcuts["Alt+F2"] = {"Toggle fullscreen", [](){ Commands::toggle_fullscreen(); }};
  shortcuts["Alt+Tab"] = {"Switch window", [](){ Commands::switch_window(); }};
  shortcuts["Alt+Escape"] = {"Leave", [](){ Commands::close_compositor(); }};
  shortcuts["Alt+Space"] = {"Toggle float", [](){ Commands::toggle_float_window(); }};
  shortcuts["Alt+E"] = {"Switch position", [](){ Commands::switch_container_direction(); }};
  shortcuts["Alt+H"] = {"Open below", [](){ Server::getInstance().openType = OpenType::below; }};
  shortcuts["Alt+V"] = {"Open right", [](){ Server::getInstance().openType = OpenType::right; }};
  shortcuts["Alt+Up"] = {"Switch focus up", [](){ Commands::switch_focus_up(); }};
  shortcuts["Alt+Left"] = {"Switch focus left", [](){ Commands::switch_focus_left(); }};
  shortcuts["Alt+Down"] = {"Switch focus Down", [](){ Commands::switch_focus_down(); }};
  shortcuts["Alt+Right"] = {"Switch focus Right", [](){ Commands::switch_focus_right(); }};

  //Allowing keyboard debug
  shortcuts["Alt+D"] = {"Debug", [this](){debug = !debug;}};
  shortcuts["Alt+F1"] = {"Open config editore", [](){ Commands::open_config_editor(); }};
}

Keyboard::~Keyboard() {
  if (keymap) {
    xkb_keymap_unref(keymap);
  }
  wl_list_remove(&key.link);
  wl_list_remove(&modifiers.link);
  wl_event_source_remove(key_repeat_source);
}

bool Keyboard::handle_keybinding()
{
  for (auto const & shortcut : shortcuts) {
    std::string key = shortcut.first;
    std::vector<std::string> splitStr;
    size_t i = 0;
    size_t j = key.find("+");
    uint32_t sum = 0;
    uint32_t mod = 0;

    while (j != std::string::npos) {
      key[j] = 0;
      splitStr.push_back(key.data() + i);
      i = j + 1;
      j = key.find("+", j + 1);
    }
    splitStr.push_back(key.data() + i);

    for (std::string tmp : splitStr) {
      if (modifiersLst.find(tmp) != modifiersLst.end())
        mod |= modifiersLst[tmp];
      else
        sum += xkb_keysym_from_name(tmp.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
    }

    if (debug)
      std::cout << "Shortcuts Code: " << sum << " + " << mod << " -> " << shortcut.second.name << std::endl;

    bool isBinding = keycodes_states.last_raw_modifiers == mod && sum == keycodes_states.sum;

    if (isBinding) {
      if (debug)
        std::cout << keycodes_states.sum << " + " << keycodes_states.last_raw_modifiers << std::endl;
      shortcut.second.action();
      return true;
    }
  }
  if (debug)
    std::cout << keycodes_states.sum << " + " << keycodes_states.last_raw_modifiers << std::endl << std::endl;
  return false;
}

void Keyboard::keyboard_handle_modifiers([[maybe_unused]]struct wl_listener *listener, [[maybe_unused]]void *data)
{
  struct wlr_seat *seat = Server::getInstance().seat.getSeat();
  wlr_seat_set_keyboard(seat, device);
  wlr_seat_keyboard_notify_modifiers(seat,
				     &device->keyboard->modifiers);
}

void Keyboard::keyboard_handle_key([[maybe_unused]]struct wl_listener *listener, void *data)
{
  struct wlr_event_keyboard_key *event = static_cast<struct wlr_event_keyboard_key *>(data);
  struct wlr_seat *seat = Server::getInstance().seat.getSeat();

  uint32_t keycode = event->keycode + 8;
  const xkb_keysym_t *syms;
  int nsyms = xkb_state_key_get_syms(device->keyboard->xkb_state, keycode, &syms);

  bool handled = false;
  uint32_t modifiers = wlr_keyboard_get_modifiers(device->keyboard);
  keycodes_states.update_state(event, keycode, modifiers, device->keyboard->xkb_state);
  char name[30] = {0};
  //std::cout << keycode << " " <<  "'A' sym:" <<xkb_keysym_from_name("a", XKB_KEYSYM_CASE_INSENSITIVE) << " " << xkb_state_key_get_utf32(device->keyboard->xkb_state, keycode) << std::endl;
  if (event->state == WLR_KEY_PRESSED)
  {
    for (int i = 0; i < nsyms; i++)
    {
      if (debug) {
        xkb_keysym_get_name(syms[i], name, 30);
        std::cout << name << ": " << syms[i] << std::endl;
      }

      handled = handle_keybinding();
    }
  }

  if (!handled)
  {
    wlr_seat_set_keyboard(seat, device);
    wlr_seat_keyboard_notify_key(seat, event->time_msec,
      event->keycode, event->state);
  }
}

void Keyboard::configure() {
    struct xkb_rule_names rules = { NULL, NULL, NULL, NULL, NULL };
    struct xkb_context *context;
    struct xkb_keymap *key_map;


  	if (!rules.layout) {
  		rules.layout = getenv("XKB_DEFAULT_LAYOUT");
  	}
  	if (!rules.model) {
  		rules.model = getenv("XKB_DEFAULT_MODEL");
  	}
  	if (!rules.options) {
  		rules.options = getenv("XKB_DEFAULT_OPTIONS");
  	}
  	if (!rules.rules) {
  		rules.rules = getenv("XKB_DEFAULT_RULES");
  	}
  	if (!rules.variant) {
  		rules.variant = getenv("XKB_DEFAULT_VARIANT");
  	}

    if (!(context = xkb_context_new(XKB_CONTEXT_NO_FLAGS))) {
      std::cerr << "Cannot create the xkb context" << std::endl;
      return ;
    }
    if (!(key_map = xkb_map_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS))) {
      std::cerr << "Cannot configure keyboard: keymap not found" << std::endl;
      xkb_context_unref(context);
      return ;
    }

    xkb_keymap_unref(this->keymap);
    this->keymap = key_map;
    if (debug)
      std::cout << xkb_keymap_get_as_string(this->keymap, XKB_KEYMAP_USE_ORIGINAL_FORMAT) << std::endl;
    wlr_keyboard_set_keymap(device->keyboard, this->keymap);

    //TODO get info from config
    int repeat_rate = 25;
    int repeat_delay = 600;

    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(device->keyboard, repeat_rate, repeat_delay);
}

void Keyboard::setModifiersListener()
{
  SET_LISTENER(Keyboard, KeyboardListeners, modifiers, keyboard_handle_modifiers);
  wl_signal_add(&device->keyboard->events.modifiers, &modifiers);
}

void Keyboard::setKeyListener()
{
  SET_LISTENER(Keyboard, KeyboardListeners, key, keyboard_handle_key);
  wl_signal_add(&device->keyboard->events.key, &key);
}

int Keyboard::keyboard_handle_repeat(void *data)
{
  return 0;
}
