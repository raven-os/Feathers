#include "XdgView.hpp"
#include "Server.hpp"
#include "Output.hpp"
#include "Commands.hpp"
#include <unistd.h>

#include <cassert>

XdgView::XdgView(wlr_surface *surface, Workspace *workspace) noexcept
  : View(surface)
  , workspace(workspace)
{
  if (wlr_surface_is_xdg_surface_v6(surface))
    {
      wlr_xdg_surface_v6 *xdg_surface = wlr_xdg_surface_v6_from_wlr_surface(surface);
      wlr_xdg_toplevel_v6 *toplevel = xdg_surface->toplevel;

      SET_LISTENER(XdgView, ViewListeners, map, xdg_surface_map<SurfaceType::xdg_v6>);
      wl_signal_add(&xdg_surface->events.map, &map);
      SET_LISTENER(XdgView, ViewListeners, unmap, xdg_surface_unmap<SurfaceType::xdg_v6>);
      wl_signal_add(&xdg_surface->events.unmap, &unmap);
      // external function (not class member) so manual assignement necessary
      destroy.notify = [](wl_listener *listener, void *data) { Server::getInstance().xdgShellV6->xdg_surface_destroy(listener, data); };
      wl_signal_add(&xdg_surface->events.destroy, &destroy);
      SET_LISTENER(XdgView, ViewListeners, request_move, xdg_toplevel_request_move<SurfaceType::xdg_v6>);
      wl_signal_add(&toplevel->events.request_move, &request_move);
      SET_LISTENER(XdgView, ViewListeners, request_resize, xdg_toplevel_request_resize<SurfaceType::xdg_v6>);
      wl_signal_add(&toplevel->events.request_resize, &request_resize);
      SET_LISTENER(XdgView, ViewListeners, request_fullscreen, xdg_toplevel_request_fullscreen<SurfaceType::xdg_v6>);
      wl_signal_add(&toplevel->events.request_fullscreen, &request_fullscreen);
      SET_LISTENER(XdgView, ViewListeners, request_maximize, xdg_toplevel_request_fullscreen<SurfaceType::xdg_v6>);
      wl_signal_add(&toplevel->events.request_maximize, &request_maximize);
      SET_LISTENER(XdgView, ViewListeners, new_popup, xdg_handle_new_popup<SurfaceType::xdg_v6>);
      wl_signal_add(&xdg_surface->events.new_popup, &new_popup);
    }
  else if (wlr_surface_is_xdg_surface(surface))
    {
      wlr_xdg_surface *xdg_surface = wlr_xdg_surface_from_wlr_surface(surface);
      wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;

      SET_LISTENER(XdgView, ViewListeners, map, xdg_surface_map<SurfaceType::xdg>);
      wl_signal_add(&xdg_surface->events.map, &map);
      SET_LISTENER(XdgView, ViewListeners, unmap, xdg_surface_unmap<SurfaceType::xdg>);
      wl_signal_add(&xdg_surface->events.unmap, &unmap);
      // external function (not class member) so manual assignement necessary
      destroy.notify = [](wl_listener *listener, void *data) { Server::getInstance().xdgShell->xdg_surface_destroy(listener, data); };
      wl_signal_add(&xdg_surface->events.destroy, &destroy);
      SET_LISTENER(XdgView, ViewListeners, request_move, xdg_toplevel_request_move<SurfaceType::xdg>);
      wl_signal_add(&toplevel->events.request_move, &request_move);
      SET_LISTENER(XdgView, ViewListeners, request_resize, xdg_toplevel_request_resize<SurfaceType::xdg>);
      wl_signal_add(&toplevel->events.request_resize, &request_resize);
      SET_LISTENER(XdgView, ViewListeners, request_fullscreen, xdg_toplevel_request_fullscreen<SurfaceType::xdg>);
      wl_signal_add(&toplevel->events.request_fullscreen, &request_fullscreen);
      SET_LISTENER(XdgView, ViewListeners, request_maximize, xdg_toplevel_request_fullscreen<SurfaceType::xdg>);
      wl_signal_add(&toplevel->events.request_maximize, &request_maximize);
      SET_LISTENER(XdgView, ViewListeners, new_popup, xdg_handle_new_popup<SurfaceType::xdg>);
      wl_signal_add(&xdg_surface->events.new_popup, &new_popup);
    }
  else
    {
      assert(!"Unknown surface type");
    }
}

XdgView::~XdgView() noexcept
{
  wl_list_remove(&map.link);
  wl_list_remove(&unmap.link);
  wl_list_remove(&destroy.link);
  if (wl_list_empty(&request_move.link) > 0)
    wl_list_remove(&request_move.link);
  if (wl_list_empty(&request_resize.link) > 0)
    wl_list_remove(&request_resize.link);
  wl_list_remove(&new_popup.link);
}

template<SurfaceType surfaceType>
void XdgView::set_tiled(uint32_t tiled_edges)
{
  if constexpr (surfaceType == SurfaceType::xdg_v6)
    wlr_xdg_toplevel_set_tiled(wlr_xdg_surface_from_wlr_surface(surface), tiled_edges);
  else
    ; // not supported in v6
}


void XdgView::set_tiled(uint32_t tiled_edges)
{
  if (wlr_surface_is_xdg_surface(surface))
    set_tiled<SurfaceType::xdg>(tiled_edges);
  // don't forward in v6 case since unsupported.
}

template<SurfaceType surfaceType>
void XdgView::xdg_surface_map(wl_listener *listener, void *data)
{
  Server &server = Server::getInstance();
  wlr_box box[1];

  mapped = true;
  focus_view();
  if constexpr (surfaceType == SurfaceType::xdg_v6)
    wlr_xdg_surface_v6_get_geometry(wlr_xdg_surface_v6_from_wlr_surface(surface), box);
  else if constexpr (surfaceType == SurfaceType::xdg)
    wlr_xdg_surface_get_geometry(wlr_xdg_surface_from_wlr_surface(surface), box);
  previous_size = {box->width, box->height};

  auto &windowTree(server.getActiveWindowTree());

  if (server.openType == OpenType::dontCare && server.configuration.getBool("enable default floating"))
    server.openType = OpenType::floating;
  if (server.openType != OpenType::floating &&
      (server.getViews().size() == 1 || server.getViews()[1]->windowNode == wm::nullNode)) // node: we are at least ourselves in the tree
    {
      auto rootNode(windowTree.getRootIndex());
      auto &rootNodeData(windowTree.getData(rootNode));

      windowNode = rootNodeData.getContainer().addChild(rootNode, windowTree, wm::ClientData{this});
      set_tiled<surfaceType>(0x15);
    }
  else
    {
      switch (server.openType)
	{
	case OpenType::dontCare:
	  {
	    auto prevNode(server.getViews()[1]->windowNode);
	    auto parentNode(windowTree.getParent(prevNode));
	    auto &parentNodeData(windowTree.getData(parentNode));

	    windowNode = parentNodeData.getContainer().addChild(parentNode, windowTree, prevNode, wm::ClientData{this});
	    set_tiled<surfaceType>(0x15);
	  }
	  break;
	case OpenType::below:
	case OpenType::right:
	  {
	    auto prevNode(server.getViews()[1]->windowNode);
	    auto &prevData(windowTree.getData(prevNode));
	    auto position(prevData.getPosition());
	    auto size(prevData.getSize());

	    prevData.data.emplace<std::unique_ptr<wm::Container>>(new wm::Container{wm::Rect{position, {FixedPoint<0, uint32_t>(size[0]), FixedPoint<0, uint32_t>(size[1])}}});

	    auto &container(prevData.getContainer());

	    container.direction = (server.openType == OpenType::below) ? wm::Container::verticalTiling : wm::Container::horizontalTiling;
	    server.getViews()[1]->windowNode = container.addChild(prevNode, windowTree, wm::ClientData{server.getViews()[1].get()});
	    windowNode = container.addChild(prevNode, windowTree, server.getViews()[1]->windowNode, wm::ClientData{this});
	    set_tiled<surfaceType>(0x15);
	    server.openType = OpenType::dontCare;
	  }
	  break;
	case OpenType::floating:
	  {
	    windowNode = wm::nullNode;
	    server.openType = OpenType::dontCare;
	  }
	  break;
	default:
	  assert("Unexpected openType value.");
	  break;
	}
    }
}

template<SurfaceType surfaceType>
void XdgView::xdg_surface_unmap(wl_listener *listener, void *data)
{
  auto &output(Server::getInstance().outputManager.getOutput(getWlrOutput()));

  mapped = false;

  if (workspace->getFullscreenView() == this)
    {
      xdg_toplevel_request_fullscreen<surfaceType>(nullptr, nullptr);
    }
  if (windowNode == wm::nullNode)
    return;

  wm::Container::removeFromParent(workspace->getWindowTree(), windowNode);
};

template<SurfaceType surfaceType>
void XdgView::xdg_toplevel_request_move(wl_listener *listener, void *data)
{
  if (windowNode == wm::nullNode)
    begin_interactive(CursorMode::CURSOR_MOVE, 0);
};

template<SurfaceType surfaceType>
void XdgView::xdg_toplevel_request_fullscreen(wl_listener *listener, void *data)
{
  auto *event = static_cast<wlr_xdg_toplevel_set_fullscreen_event *>(data);
  Server &server = Server::getInstance();
  auto &output = server.outputManager.getOutput(getWlrOutput());

  if (event && event->fullscreen)
    {
      wlr_box *outputBox = wlr_output_layout_get_box(server.outputManager.getLayout(), getWlrOutput());
      Commands::new_workspace(true);
      Commands::switch_window_from_workspace(Workspace::RIGHT);

      if constexpr (surfaceType == SurfaceType::xdg_v6)
	{
	  wlr_xdg_surface_v6 *xdg_surface = wlr_xdg_surface_v6_from_wlr_surface(surface);

	  wlr_xdg_surface_v6_get_geometry(xdg_surface, &output.saved);
	  wlr_xdg_toplevel_v6_set_size(xdg_surface, outputBox->width, outputBox->height);
	  wlr_xdg_toplevel_v6_set_fullscreen(xdg_surface, true);
	}
      else if constexpr (surfaceType == SurfaceType::xdg)
	{
	  wlr_xdg_surface *xdg_surface = wlr_xdg_surface_from_wlr_surface(surface);

	  wlr_xdg_surface_get_geometry(xdg_surface, &output.saved);
	  wlr_xdg_toplevel_set_size(xdg_surface, outputBox->width, outputBox->height);
	  wlr_xdg_toplevel_set_fullscreen(xdg_surface, true);
	}

      workspace->setFullscreenView(this);
      fullscreen = true;
    }
  else
    {
      if constexpr (surfaceType == SurfaceType::xdg_v6)
        {
	  wlr_xdg_surface_v6 *xdg_surface = wlr_xdg_surface_v6_from_wlr_surface(surface);

	  wlr_xdg_toplevel_v6_set_fullscreen(xdg_surface, false);
	  wlr_xdg_toplevel_v6_set_size(xdg_surface, output.saved.width, output.saved.height);
	}
      else if constexpr (surfaceType == SurfaceType::xdg)
        {
	  wlr_xdg_surface *xdg_surface = wlr_xdg_surface_from_wlr_surface(surface);

	  wlr_xdg_toplevel_set_fullscreen(xdg_surface, false);
	  wlr_xdg_toplevel_set_size(xdg_surface, output.saved.width, output.saved.height);
	}
      workspace->setFullscreenView(nullptr);
      fullscreen = false;
   
      set_tiled(0x15);
      Workspace *w = Server::getInstance().outputManager.getActiveWorkspace();
      Commands::switch_window_from_workspace(Workspace::LEFT);
      Commands::close_workspace(w);
    }
}

template<SurfaceType surfaceType>
void XdgView::xdg_toplevel_request_resize(wl_listener *listener, void *data)
{
  wlr_xdg_toplevel_v6_resize_event *event = static_cast<wlr_xdg_toplevel_v6_resize_event *>(data);
  begin_interactive(CursorMode::CURSOR_RESIZE, event->edges);
}

void XdgView::requestFullscreen()
{
  wlr_xdg_toplevel_set_fullscreen_event data;

  data.output = nullptr;
  data.fullscreen = !fullscreen;
  if (wlr_surface_is_xdg_surface_v6(surface))
    xdg_toplevel_request_fullscreen<SurfaceType::xdg_v6>(nullptr, &data);
  else if (wlr_surface_is_xdg_surface(surface))
    xdg_toplevel_request_fullscreen<SurfaceType::xdg>(nullptr, &data);
}

void XdgView::close()
{
  if (wlr_surface_is_xdg_surface_v6(surface))
    wlr_xdg_surface_v6_send_close(wlr_xdg_surface_v6_from_wlr_surface(surface));
  else if (wlr_surface_is_xdg_surface(surface))
    wlr_xdg_toplevel_send_close(wlr_xdg_surface_from_wlr_surface(surface));
}

void XdgView::begin_interactive(CursorMode mode, uint32_t edges)
{
  Server &server = Server::getInstance();
  wlr_surface *focused_surface = server.seat.getSeat()->pointer_state.focused_surface;
  if (surface != focused_surface)
    {
      return;
    }
  server.grabbed_view = this;
  server.cursor.cursor_mode = mode;
  wlr_box geo_box;

  if (wlr_surface_is_xdg_surface_v6(surface))
    wlr_xdg_surface_v6_get_geometry(wlr_xdg_surface_v6_from_wlr_surface(surface), &geo_box);
  else if (wlr_surface_is_xdg_surface(surface))
    wlr_xdg_surface_get_geometry(wlr_xdg_surface_from_wlr_surface(surface), &geo_box);

  if (mode == CursorMode::CURSOR_MOVE)
    {
      server.grab_x = server.cursor.cursor->x - x.getDoubleValue();
      server.grab_y = server.cursor.cursor->y - y.getDoubleValue();
    }
  else
    {
      server.grab_x = server.cursor.cursor->x + geo_box.x;
      server.grab_y = server.cursor.cursor->y + geo_box.y;
    }
  server.grab_width = geo_box.width;
  server.grab_height = geo_box.height;
  server.resize_edges = edges;
}


wlr_output *XdgView::getWlrOutput()
{
  Server &server = Server::getInstance();
  wlr_box viewBox;
  {
    if (wlr_surface_is_xdg_surface_v6(surface))
      wlr_xdg_surface_v6_get_geometry(wlr_xdg_surface_v6_from_wlr_surface(surface), &viewBox);
    else if (wlr_surface_is_xdg_surface(surface))
      wlr_xdg_surface_get_geometry(wlr_xdg_surface_from_wlr_surface(surface), &viewBox);
    // handle layer surface;
  }

  double outputX;
  double outputY;
  wlr_output_layout_closest_point(server.outputManager.getLayout(), nullptr,
				  x.getDoubleValue() + static_cast<double>(viewBox.width/2),
				  y.getDoubleValue() + static_cast<double>(viewBox.height/2),
				  &outputX, &outputY);
  return wlr_output_layout_output_at(server.outputManager.getLayout(), outputX, outputY);
}

void XdgView::resize(wm::WindowNodeIndex, wm::WindowTree &, std::array<uint16_t, 2u> size)
{
  resize(size);
}

void XdgView::resize(std::array<uint16_t, 2u> size)
{
  if (!fullscreen)
    {
      if (wlr_surface_is_xdg_surface_v6(surface))
	wlr_xdg_toplevel_v6_set_size(wlr_xdg_surface_v6_from_wlr_surface(surface), size[0], size[1]);
      else if (wlr_surface_is_xdg_surface(surface))
	wlr_xdg_toplevel_set_size(wlr_xdg_surface_from_wlr_surface(surface), size[0], size[1]);
    }
  else
    {
      auto &output(Server::getInstance().outputManager.getOutput(getWlrOutput()));

      output.saved.width = size[0];
      output.saved.height = size[1];
    }
}

void XdgView::move(wm::WindowNodeIndex, wm::WindowTree &, std::array<FixedPoint<-4, int32_t>, 2u> position)
{
  move(position);
}

void XdgView::move(std::array<FixedPoint<-4, int32_t>, 2u> position)
{
  x = position[0];
  y = position[1];
}

std::array<FixedPoint<-4, int32_t>, 2u> XdgView::getPosition() const noexcept
{
  std::array<FixedPoint<-4, int32_t>, 2u> result{{x, y}};

  return result;
}

std::array<uint16_t, 2u> XdgView::getSize() const noexcept
{
  wlr_box box[1];

  if (wlr_surface_is_xdg_surface_v6(surface))
    wlr_xdg_surface_v6_get_geometry(wlr_xdg_surface_v6_from_wlr_surface(surface), box);
  else if (wlr_surface_is_xdg_surface(surface))
    wlr_xdg_surface_get_geometry(wlr_xdg_surface_from_wlr_surface(surface), box);

  return {uint16_t(box->width), uint16_t(box->height)};
}

std::array<FixedPoint<-4, int32_t>, 2u> XdgView::getMinSize() const noexcept
{
  std::array<FixedPoint<-4, int32_t>, 2u> result;
  if (wlr_surface_is_xdg_surface_v6(surface))
    {
      result[0] = FixedPoint<0, int32_t>(wlr_xdg_surface_v6_from_wlr_surface(surface)->toplevel->current.min_width);
      result[1] = FixedPoint<0, int32_t>(wlr_xdg_surface_v6_from_wlr_surface(surface)->toplevel->current.min_height);
    }
  else if (wlr_surface_is_xdg_surface(surface))
    {
      result[0] = FixedPoint<0, int32_t>(wlr_xdg_surface_from_wlr_surface(surface)->toplevel->current.min_width);
      result[1] = FixedPoint<0, int32_t>(wlr_xdg_surface_from_wlr_surface(surface)->toplevel->current.min_height);
    }
  return result;
}

std::array<FixedPoint<-4, int32_t>, 2u> XdgView::getMinSize(wm::WindowNodeIndex, wm::WindowTree &) const noexcept
{
  return getMinSize();
}

// Note: bases itself on the active workspace, which is actually not a good idea
XdgView *XdgView::desktop_view_at(double lx, double ly,
				  wlr_surface **surface, double *sx, double *sy)
{
  Server &server = Server::getInstance();

  for (auto &view : server.outputManager.getActiveWorkspace()->getViews())
    {
      if (view->at(lx, ly, surface, sx, sy))
	{
	  return view.get();
	}
    }
  return nullptr;
}
