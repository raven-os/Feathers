#pragma once

# include "Wlroots.hpp"
# include "Output.hpp"
# include "Listeners.hpp"

#include <vector>
#include <memory>

/* Used to move all of the data necessary to render a surface from the top-level
 * frame handler to the per-surface render function. */
struct render_data
{
  struct wlr_output *output;
  struct wlr_renderer *renderer;
  View *view;
  struct timespec *when;
  bool fullscreen;
};

struct OutputManagerListeners
{
  struct wl_listener new_output;
};

class OutputManager : public OutputManagerListeners
{
public:
  OutputManager();
  ~OutputManager() = default;

  void output_frame(struct wl_listener *listener, void *data);
  void server_new_output(struct wl_listener *listener, void *data);
  static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data);

  struct wlr_output_layout *getLayout() const noexcept;
  std::vector<std::unique_ptr<Output>> const& getOutputs() const;

  Output &getOutput(wlr_output *wlr_output) noexcept;
  Output const &getOutput(wlr_output *wlr_output) const noexcept;

private:
  struct wlr_output_layout *output_layout;
  std::vector<std::unique_ptr<Output>> outputs;
};