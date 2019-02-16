#include "ServerCursor.hpp"
#include "Server.hpp"

ServerCursor::ServerCursor(Server *server) : server(server){
  cursor = wlr_cursor_create();
  wlr_cursor_attach_output_layout(cursor, server->output_layout);

  cursor_mgr = wlr_xcursor_manager_create(nullptr, 24);
  wlr_xcursor_manager_load(cursor_mgr, 1);

  cursor_motion.notify = server_cursor_motion;
  wl_signal_add(&cursor->events.motion, &cursor_motion);
  cursor_motion_absolute.notify = server_cursor_motion_absolute;
  wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);
  cursor_button.notify = server_cursor_button;
  wl_signal_add(&cursor->events.button, &cursor_button);
  cursor_axis.notify = server_cursor_axis;
  wl_signal_add(&cursor->events.axis, &cursor_axis);

}

ServerCursor::~ServerCursor() {

}

void ServerCursor::process_cursor_move(uint32_t time)
{
  server->grabbed_view->x = cursor->x - server->grab_x;
  server->grabbed_view->y = cursor->y - server->grab_y;
}

void ServerCursor::process_cursor_resize(uint32_t time)
{
  View *view = server->grabbed_view;
  double dx = cursor->x - server->grab_x;
  double dy = cursor->y - server->grab_y;
  double x = view->x;
  double y = view->y;
  int width = server->grab_width;
  int height = server->grab_height;
  if (server->resize_edges & WLR_EDGE_TOP)
    {
      y = server->grab_y + dy;
      height -= dy;
      if (height < 1)
	{
	  y += height;
	}
    }
  else if (server->resize_edges & WLR_EDGE_BOTTOM)
    {
      height += dy;
    }
  if (server->resize_edges & WLR_EDGE_LEFT)
    {
      x = server->grab_x + dx;
      width -= dx;
      if (width < 1)
	{
	  x += width;
	}
    }
  else if (server->resize_edges & WLR_EDGE_RIGHT)
    {
      width += dx;
    }
  view->x = x;
  view->y = y;
  wlr_xdg_toplevel_set_size(view->xdg_surface, width, height);
}

void ServerCursor::process_cursor_motion(uint32_t time)
{
  if (cursor_mode == CursorMode::CURSOR_MOVE)
    {
      process_cursor_move(time);
      return;
    }
  else if (cursor_mode == CursorMode::CURSOR_RESIZE)
    {
      process_cursor_resize(time);
      return;
    }

  double sx, sy;
  struct wlr_seat *seat = server->seat;
  struct wlr_surface *surface = NULL;
  View *view = ServerView::desktop_view_at(server, cursor->x,
					   cursor->y, &surface, &sx, &sy);
  if (!view)
    {
      wlr_xcursor_manager_set_cursor_image(cursor_mgr, "left_ptr", cursor);
    }
  if (surface)
    {
      bool focus_changed = seat->pointer_state.focused_surface != surface;
      wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
      if (!focus_changed)
	{
	  wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	}
    }
  else
    {
      wlr_seat_pointer_clear_focus(seat);
    }
}

  void ServerCursor::server_cursor_motion(struct wl_listener *listener, void *data)
  {
    //Server *server = wl_container_of(listener, server, cursor->cursor_motion);
    struct wlr_event_pointer_motion *event = static_cast<struct wlr_event_pointer_motion *>(data);
    wlr_cursor_move(cursor, event->device, event->delta_x, event->delta_y);
    process_cursor_motion(event->time_msec);
  }

  void ServerCursor::server_cursor_motion_absolute(struct wl_listener *listener, void *data)
  {
    //Server *server = wl_container_of(listener, server, cursor_motion_absolute);
    struct wlr_event_pointer_motion_absolute *event = static_cast<struct wlr_event_pointer_motion_absolute *>(data);
    wlr_cursor_warp_absolute(cursor, event->device, event->x, event->y);
    process_cursor_motion(event->time_msec);
  }

  void ServerCursor::server_cursor_button(struct wl_listener *listener, void *data)
  {
  //  Server *server = wl_container_of(listener, server, cursor_button);
    struct wlr_event_pointer_button *event = static_cast<struct wlr_event_pointer_button *>(data);
    wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);
    double sx, sy;
    struct wlr_seat *seat = server->seat;
    struct wlr_surface *surface;
    View *view = ServerView::desktop_view_at(server, cursor->x, cursor->y, &surface, &sx, &sy);
    if (event->state == WLR_BUTTON_RELEASED)
      {
	cursor_mode = CursorMode::CURSOR_PASSTHROUGH;
      }
    else
      {
	ServerView::focus_view(view, surface);
      }
  }

  void ServerCursor::server_cursor_axis(struct wl_listener *listener, void *data)
  {
  //  Server *server = wl_container_of(listener, server, cursor->cursor_axis);
    struct wlr_event_pointer_axis *event = static_cast<struct wlr_event_pointer_axis *>(data);
    wlr_seat_pointer_notify_axis(server->seat,
				 event->time_msec, event->orientation, event->delta,
				 event->delta_discrete, event->source);
  }