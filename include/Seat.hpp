#pragma once

# include "Wlroots.hpp"
# include "Listeners.hpp"

class Server;

class Seat : public Listeners::SeatListeners
{
public:
  Seat(Server *server);
  ~Seat() = default;

  void seat_request_cursor(struct wl_listener *listener, void *data);
  struct wlr_seat *getSeat() const noexcept;

private:
  Server *server;
  struct wlr_seat *seat;
};
