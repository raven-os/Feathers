#pragma once

#include <array>
#include <vector>
#include <memory>

# include "Wlroots.hpp"
# include "ServerCursor.hpp"
# include "Popup.hpp"
# include "wm/WindowNodeIndex.hpp"
# include "util/FixedPoint.hpp"

namespace wm
{
  struct WindowTree;
}

class Server;
class Workspace;

enum class SurfaceType
  {
   xdg,
   xdg_v6,
   unkonwn
  };

class View
{
public:
  View(wlr_surface *surface) noexcept;
  View(View const &) = delete;
  View(View &&) = delete;
  ~View();

  wlr_surface *surface;
  bool mapped;
  FixedPoint<-4, int> x, y;
  std::unique_ptr<Popup> popup;

  template<SurfaceType surfaceType>
  void xdg_handle_new_popup(wl_listener *listener, void *data);
  void focus_view();

  // Also takes into account layer shell views
  static View *view_at(double lx, double ly, wlr_surface **surface, double *sx, double *sy);


protected:
  bool at(double lx, double ly, wlr_surface **surface, double *sx, double *sy);
};
