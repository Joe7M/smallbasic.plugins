// This file is part of SmallBASIC
//
// Plugin for nukler GUI library - https://github.com/vurtun/nuklear.git
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//
// Copyright(C) 2018 Chris Warren-Smith

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_FONT_BAKING
#define NK_IMPLEMENTATION
#define NK_SDL_GL2_IMPLEMENTATION

#include "config.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gl2.h"
#include "var.h"
#include "var_map.h"
#include "module.h"

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 480
#define LOOP_DELAY 80
#define MAX_FLOATS 40
#define MAX_COMBOBOX_ITEMS 10

static SDL_Window *_window;
static SDL_GLContext _glContext;
static struct nk_context *_ctx;
static float _floats[MAX_FLOATS];
static const char *_comboboxItems[MAX_COMBOBOX_ITEMS];
static bool _sdlExit;
static struct nk_color _fg_color;
static struct nk_color _bg_color;
static float _line_thickness;

enum drawmode {DRAW_FILL, DRAW_LINE, DRAW_NONE};

const nk_color _colors[] = {
  {0  ,0  ,0  ,255}, // 0 black
  {0  ,0  ,128,255}, // 1 blue
  {0  ,128,0  ,255}, // 2 green
  {0  ,128,128,255}, // 3 cyan
  {128,0  ,0  ,255}, // 4 red
  {128,0  ,128,255}, // 5 magenta
  {128,128,0  ,255}, // 6 yellow
  {192,192,192,255}, // 7 white
  {128,128,128,255}, // 8 gray
  {0  ,0  ,255,255}, // 9 light blue
  {0  ,255,0  ,255}, // 10 light green
  {0  ,255,255,255}, // 11 light cyan
  {255,0  ,0  ,255}, // 12 light red
  {255,0  ,255,255}, // 13 light magenta
  {255,255,0  ,255}, // 14 light yellow
  {255,255,255,255}  // 15 bright white
};

typedef struct API {
  const char *name;
  int (*command)(int, slib_par_t *, var_t *retval);
} API;

int is_param_num(int argc, slib_par_t *params, int n) {
  int result;
  if (n >= 0 && n < argc) {
    result = (params[n].var_p->type == V_NUM ||
              params[n].var_p->type == V_INT);
  } else {
    result = 0;
  }
  return result;
}

int is_param_str(int argc, slib_par_t *params, int n) {
  int result;
  if (n >= 0 && n < argc) {
    result = (params[n].var_p->type == V_STR);
  } else {
    result = 0;
  }
  return result;
}

int is_param_map(int argc, slib_par_t *params, int n) {
  int result;
  if (n >= 0 && n < argc) {
    result = (params[n].var_p->type == V_MAP);
  } else {
    result = 0;
  }
  return result;
}

int get_param_int(int argc, slib_par_t *params, int n, int def) {
  int result;
  if (n >= 0 && n < argc) {
    switch (params[n].var_p->type) {
    case V_INT:
      result = params[n].var_p->v.i;
      break;
    case V_NUM:
      result = params[n].var_p->v.n;
      break;
    default:
      result = def;
    }
  } else {
    result = def;
  }
  return result;
}

var_num_t get_param_num(int argc, slib_par_t *params, int n, var_num_t def) {
  var_num_t result;
  if (n >= 0 && n < argc) {
    switch (params[n].var_p->type) {
    case V_INT:
      result = params[n].var_p->v.i;
      break;
    case V_NUM:
      result = params[n].var_p->v.n;
      break;
    default:
      result = def;
      break;
    }
  } else {
    result = def;
  }
  return result;
}

var_num_t get_param_num_field(int argc, slib_par_t *params, int n, const char *field) {
  var_num_t result;
  if (is_param_map(argc, params, n)) {
    var_p_t v_value = map_get(params[n].var_p, field);
    if (v_is_type(v_value, V_INT)) {
      result = v_value->v.i;
    } else if (v_is_type(v_value, V_NUM)) {
      result = v_value->v.n;
    } else {
      result = 0;
    }
  } else {
    result = 0;
  }
  return result;
}

const char *get_param_str(int argc, slib_par_t *params, int n, const char *def) {
  const char *result;
  static char buf[256];
  if (n >= 0 && n < argc) {
    switch (params[n].var_p->type) {
    case V_STR:
      result = params[n].var_p->v.p.ptr;
      break;
    case V_INT:
      sprintf(buf, "%ld", params[n].var_p->v.i);
      result = buf;
      break;
    case V_NUM:
      sprintf(buf, "%f", params[n].var_p->v.n);
      result = buf;
      break;
    }
  } else {
    result = def;
  }
  return result;
}

const char *get_param_str_field(int argc, slib_par_t *params, int n, const char *field) {
  const char *result;
  if (is_param_map(argc, params, n)) {
    var_p_t v_value = map_get(params[n].var_p, field);
    if (v_is_type(v_value, V_STR)) {
      result = v_value->v.p.ptr;
    } else {
      result = NULL;
    }
  } else {
    result = NULL;
  }
  return result;
}

nk_color get_param_color(int argc, slib_par_t *params, int n) {
  nk_color result;
  const char *color = get_param_str(argc, params, n, NULL);
  if (color != NULL) {
    result = nk_rgb_hex(color);
  } else {
    result = nk_black;
  }
  return result;
}

enum drawmode get_draw_mode(int argc, slib_par_t *params, int n) {
  const char *mode = get_param_str(argc, params, n, NULL);
  drawmode result;
  if (!strcasecmp(mode, "fill")) {
    result = DRAW_FILL;
  } else if (!strcasecmp(mode, "line")) {
    result = DRAW_LINE;
  } else {
    result = DRAW_NONE;
  }
  return result;
}

nk_flags get_alignment(const char *s) {
  nk_flags result;
  if (!strcasecmp(s, "left")) {
    result = NK_TEXT_LEFT;
  } else if (!strcasecmp(s, "centered")) {
    result = NK_TEXT_CENTERED;
  } else if (!strcasecmp(s, "right")) {
    result = NK_TEXT_RIGHT;
  } else if (!strcasecmp(s, "top left")) {
    result = NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_LEFT;
  } else if (!strcasecmp(s, "top centered")) {
    result = NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_CENTERED;
  } else if (!strcasecmp(s, "top right")) {
    result = NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_RIGHT;
  } else if (!strcasecmp(s, "bottom left")) {
    result = NK_TEXT_ALIGN_BOTTOM | NK_TEXT_ALIGN_LEFT;
  } else if (!strcasecmp(s, "bottom centered")) {
    result = NK_TEXT_ALIGN_BOTTOM | NK_TEXT_ALIGN_CENTERED;
  } else if (!strcasecmp(s, "bottom right")) {
    result = NK_TEXT_ALIGN_BOTTOM | NK_TEXT_ALIGN_RIGHT;
  } else {
    result = 0;
  }
  return result;
}

void process_events() {
  SDL_Event evt;
  nk_input_begin(_ctx);
  while (!_sdlExit) {
    if (SDL_PollEvent(&evt)) {
      nk_sdl_handle_event(&evt);
      if (evt.type == SDL_QUIT) {
        _sdlExit = true;
      }
      break;
    } else {
      SDL_Delay(LOOP_DELAY);
    }
  }
  nk_input_end(_ctx);
}

nk_flags get_window_flags(int argc, slib_par_t *params, int n) {
  nk_flags flags = 0;
  for (int i = n; i < argc; i++) {
    const char *flag = get_param_str(argc, params, i, NULL);
    if (flag != NULL && flag[0] != '\0') {
      if (!strcasecmp(flag, "border")) {
        flags |= NK_WINDOW_BORDER;
      } else if (!strcasecmp(flag, "movable")) {
        flags |= NK_WINDOW_MOVABLE;
      } else if (!strcasecmp(flag, "scalable")) {
        flags |= NK_WINDOW_SCALABLE;
      } else if (!strcasecmp(flag, "closable")) {
        flags |= NK_WINDOW_CLOSABLE;
      } else if (!strcasecmp(flag, "minimizable")) {
        flags |= NK_WINDOW_MINIMIZABLE;
      } else if (!strcasecmp(flag, "no_scrollbar")) {
        flags |= NK_WINDOW_NO_SCROLLBAR;
      } else if (!strcasecmp(flag, "title")) {
        flags |= NK_WINDOW_TITLE;
      } else if (!strcasecmp(flag, "scroll_auto_hide")) {
        flags |= NK_WINDOW_SCROLL_AUTO_HIDE;
      } else if (!strcasecmp(flag, "background")) {
        flags |= NK_WINDOW_BACKGROUND;
      } else if (!strcasecmp(flag, "scale_left")) {
        flags |= NK_WINDOW_SCALE_LEFT;
      } else if (!strcasecmp(flag, "no_input")) {
        flags |= NK_WINDOW_NO_INPUT;
      }
    }
  }
  return flags;
}

void nk_rgba_str(struct nk_color c, char *str) {
  if (c.a < 255) {
    sprintf(str, "#%02x%02x%02x%02x", c.r, c.g, c.b, c.a);
  } else {
    sprintf(str, "#%02x%02x%02x", c.r, c.g, c.b);
  }
}

int cmd_arc(int argc, slib_par_t *params, var_t *retval) {
  enum drawmode mode = get_draw_mode(argc, params, 0);
  float cx = get_param_num(argc, params, 1, 0);
  float cy = get_param_num(argc, params, 2, 0);
  float r = get_param_num(argc, params, 3, 0);
  float a0 = get_param_num(argc, params, 4, 0);
  float a1 = get_param_num(argc, params, 5, 0);
  if (mode == DRAW_FILL) {
    nk_fill_arc(&_ctx->current->buffer, cx, cy, r, a0, a1, _fg_color);
  } else if (mode == DRAW_LINE) {
    nk_stroke_arc(&_ctx->current->buffer, cx, cy, r, a0, a1, _line_thickness, _fg_color);
  }
  return (argc == 6);
}

int cmd_button(int argc, slib_par_t *params, var_t *retval) {
  const char *title = get_param_str(argc, params, 0, NULL);
  int result;
  if (title != NULL) {
    v_setint(retval, nk_button_label(_ctx, title));
    result = 1;
  } else {
    v_setstr(retval, "Invalid button input");
    result = 0;
  }
  return result;
}

int cmd_checkbox(int argc, slib_par_t *params, var_t *retval) {
  const char *text = get_param_str(argc, params, 0, "checkbox");
  if (is_param_num(argc, params, 1)) {
    int value = get_param_int(argc, params, 1, 0);
    value = nk_check_label(_ctx, text, value);
    v_setint(retval, value);
  } else if (is_param_map(argc, params, 1)) {
    var_t *map = params[1].var_p;
    int value = map_get_bool(map, "value");
    int changed = nk_checkbox_label(_ctx, text, &value);
    if (changed) {
      map_set_int(map, "value", !value);
    }
    v_setint(retval, changed);
  } else {
    v_setstr(retval, "Invalid checkbox input");
  }
  return (argc == 2);
}

int cmd_circle(int argc, slib_par_t *params, var_t *retval) {
  enum drawmode mode = get_draw_mode(argc, params, 0);
  float x = get_param_num(argc, params, 1, 0);
  float y = get_param_num(argc, params, 2, 0);
  float r = get_param_num(argc, params, 3, 0);
  if (mode == DRAW_FILL) {
    nk_fill_circle(&_ctx->current->buffer, nk_rect(x - r, y - r, r * 2, r * 2), _fg_color);
  } else if (mode == DRAW_LINE) {
    nk_stroke_circle(&_ctx->current->buffer, nk_rect(x - r, y - r, r * 2, r * 2), _line_thickness, _fg_color);
  }
  return (argc == 4);
}

int cmd_colorpicker(int argc, slib_par_t *params, var_t *retval) {
  int result = 1;
  char new_color_string[10];
  if (is_param_str(argc, params, 0)) {
    struct nk_colorf color = nk_color_cf(get_param_color(argc, params, 0));
    color = nk_color_picker(_ctx, color, NK_RGB);
    nk_rgba_str(nk_rgba_cf(color), new_color_string);
  } else if (is_param_map(argc, params, 0)) {
    const char *value = get_param_str_field(argc, params, 0, "value");
    if (value != NULL) {
      struct nk_colorf color = nk_color_cf(nk_rgb_hex(value));
      int changed = nk_color_pick(_ctx, &color, NK_RGB);
      if (changed) {
        nk_rgba_str(nk_rgba_cf(color), new_color_string);
        v_setstr(map_get(params[0].var_p, "value"), new_color_string);
      }
    } else {
      v_setstr(retval, "Invalid colorpicker input");
      result = 0;
    }
  } else {
    v_setstr(retval, "Invalid colorpicker input");
    result = 0;
  }
  return result;
}

int cmd_combobox(int argc, slib_par_t *params, var_t *retval) {
  int success = 0;
  if (is_param_map(argc, params, 0)) {
    var_t *map = params[0].var_p;
    var_p_t v_value = map_get(map, "value");
    var_p_t v_items = map_get(map, "items");
    if (v_is_type(v_value, V_INT) &&
        v_is_type(v_items, V_ARRAY)) {
      int len = v_asize(v_items);
      int count = 0;
      for (int i = 0; i < len && i < MAX_COMBOBOX_ITEMS; i++) {
        var_p_t elem = v_elem(v_items, i);
        if (v_is_type(elem, V_STR)) {
          _comboboxItems[count++] = elem->v.p.ptr;
        }
      }
      struct nk_rect bounds = nk_widget_bounds(_ctx);
      struct nk_vec2 size = nk_vec2(bounds.w, bounds.h * 8);
      int selected = v_value->v.i;
      nk_combobox(_ctx, _comboboxItems, count, &selected, bounds.h, size);
      v_value->v.i = selected;
      success = 1;
    }
  }
  if (!success) {
    v_setstr(retval, "Invalid combobox input");
  }
  return success;
}

int cmd_contextualbegin(int argc, slib_par_t *params, var_t *retval) {
  struct nk_vec2 size;
  struct nk_rect trigger;
  size.x = get_param_num(argc, params, 0, 0);
  size.y = get_param_num(argc, params, 1, 0);
  trigger.x = get_param_num(argc, params, 2, 0);
  trigger.y = get_param_num(argc, params, 3, 0);
  trigger.w = get_param_num(argc, params, 4, 0);
  trigger.h = get_param_num(argc, params, 5, 0);
  nk_flags flags = NK_WINDOW_NO_SCROLLBAR;
  v_setint(retval, nk_contextual_begin(_ctx, flags, size, trigger));
  return (argc == 6);
}

int cmd_contextualend(int argc, slib_par_t *params, var_t *retval) {
  nk_contextual_end(_ctx);
  return 1;
}

int cmd_contextualitem(int argc, slib_par_t *params, var_t *retval) {
  const char *text = get_param_str(argc, params, 0, NULL);
  if (text != NULL) {
    nk_contextual_item_symbol_label(_ctx, NK_SYMBOL_NONE, text, NK_TEXT_LEFT);
  }
  return (argc >= 1 && argc <= 3);
}

int cmd_curve(int argc, slib_par_t *params, var_t *retval) {
  float ax = get_param_num(argc, params, 0, 0);
  float ay = get_param_num(argc, params, 1, 0);
  float ctrl0x = get_param_num(argc, params, 2, 0);
  float ctrl0y = get_param_num(argc, params, 3, 0);
  float ctrl1x = get_param_num(argc, params, 4, 0);
  float ctrl1y = get_param_num(argc, params, 5, 0);
  float bx = get_param_num(argc, params, 6, 0);
  float by = get_param_num(argc, params, 7, 0);
  nk_stroke_curve(&_ctx->current->buffer, ax, ay, ctrl0x, ctrl0y, ctrl1x, ctrl1y, bx, by, _line_thickness, _fg_color);
  return argc == 8;
}

int cmd_draw(int argc, slib_par_t *params, var_t *retval) {
  return 1;
}

int cmd_edit(int argc, slib_par_t *params, var_t *retval) {
  return 1;
}

int cmd_ellipse(int argc, slib_par_t *params, var_t *retval) {
  enum drawmode mode = get_draw_mode(argc, params, 0);
  float x = get_param_num(argc, params, 1, 0);
  float y = get_param_num(argc, params, 2, 0);
  float rx = get_param_num(argc, params, 3, 0);
  float ry = get_param_num(argc, params, 4, 0);
  if (mode == DRAW_FILL) {
    nk_fill_circle(&_ctx->current->buffer, nk_rect(x - rx, y - ry, rx * 2, ry * 2), _fg_color);
  } else if (mode == DRAW_LINE) {
    nk_stroke_circle(&_ctx->current->buffer, nk_rect(x - rx, y - ry, rx * 2, ry * 2), _line_thickness, _fg_color);
  }
  return (argc == 5);
}

int cmd_framebegin(int argc, slib_par_t *params, var_t *retval) {
  return 1;
}

int cmd_frameend(int argc, slib_par_t *params, var_t *retval) {
  return 1;
}

int cmd_groupbegin(int argc, slib_par_t *params, var_t *retval) {
  const char *title = get_param_str(argc, params, 0, NULL);
  nk_flags flags = get_window_flags(argc, params, 1);
  return nk_group_begin(_ctx, title, flags);
}

int cmd_groupend(int argc, slib_par_t *params, var_t *retval) {
  nk_group_end(_ctx);
  return 1;
}

int cmd_image(int argc, slib_par_t *params, var_t *retval) {
  return 0;
}

int cmd_init(int argc, slib_par_t *params, var_t *retval) {
  return 0;
}

int cmd_keypressed(int argc, slib_par_t *params, var_t *retval) {
  return 0;
}

int cmd_label(int argc, slib_par_t *params, var_t *retval) {
  const char *label = get_param_str(argc, params, 0, NULL);
  const char *position = get_param_str(argc, params, 1, "left");
  int result;
  if (label != NULL) {
    nk_label(_ctx, label, get_alignment(position));
    result = 1;
  } else {
    v_setstr(retval, "Invalid label input");
    result = 0;
  }
  return result;
}

int cmd_layoutrow(int argc, slib_par_t *params, var_t *retval) {
  const char *format = get_param_str(argc, params, 0, NULL);
  var_num_t height = get_param_num(argc, params, 1, 0);
  int cols = get_param_int(argc, params, 2, 0);
  if (format != NULL) {
    if (!strcasecmp(format, "dynamic")) {
      nk_row_layout(_ctx, NK_DYNAMIC, height, cols, 0);
    } else if (!strcasecmp(format, "static")) {
      nk_row_layout(_ctx, NK_STATIC, height, cols, 0);
    }
  }
  return argc == 3;
}

int cmd_line(int argc, slib_par_t *params, var_t *retval) {
  for (int i = 0; i < argc && i < MAX_FLOATS; ++i) {
    _floats[i] = get_param_num(argc, params, i, 0);
  }
  nk_stroke_polyline(&_ctx->current->buffer, _floats, argc / 2, _line_thickness, _fg_color);
  return (argc >= 4 && argc % 2 == 0);
}

int cmd_menubegin(int argc, slib_par_t *params, var_t *retval) {
  int result;
  const char *text = get_param_str(argc, params, 0, NULL);
  if (text != NULL) {
    struct nk_vec2 size;
    size.x = get_param_num(argc, params, 2, 0);
    size.y = get_param_num(argc, params, 3, 0);
    v_setint(retval, nk_menu_begin_label(_ctx, text, NK_TEXT_LEFT, size));
    result = 1;
  } else {
    result = 0;
  }
  return result;
}

int cmd_menuend(int argc, slib_par_t *params, var_t *retval) {
  nk_menu_end(_ctx);
  return 1;
}

int cmd_menuitem(int argc, slib_par_t *params, var_t *retval) {
  int result;
  const char *text = get_param_str(argc, params, 0, NULL);
  if (text != NULL) {
    result = nk_menu_item_label(_ctx, text, NK_TEXT_LEFT);
  } else {
    v_setstr(retval, "Invalid menu input");
    result = 0;
  }
  return result;
}

int cmd_menubarbegin(int argc, slib_par_t *params, var_t *retval) {
  nk_menubar_begin(_ctx);
  return 1;
}

int cmd_menubarend(int argc, slib_par_t *params, var_t *retval) {
  nk_menubar_end(_ctx);
  return 1;
}

int cmd_mousemoved(int argc, slib_par_t *params, var_t *retval) {
  int x = get_param_int(argc, params, 0, 0);
  int y = get_param_int(argc, params, 2, 0);
  int dx = get_param_int(argc, params, 3, 0);
  int dy = get_param_int(argc, params, 4, 0);
  return 0;
}

int cmd_mousepressed(int argc, slib_par_t *params, var_t *retval) {
  int x = get_param_int(argc, params, 0, 0);
  int y = get_param_int(argc, params, 2, 0);
  int button = get_param_int(argc, params, 3, 0);
  return 0;
}

int cmd_mousereleased(int argc, slib_par_t *params, var_t *retval) {
  int x = get_param_int(argc, params, 0, 0);
  int y = get_param_int(argc, params, 2, 0);
  int button = get_param_int(argc, params, 3, 0);
  return 0;
}

int cmd_polygon(int argc, slib_par_t *params, var_t *retval) {
  enum drawmode mode = get_draw_mode(argc, params, 0);
  for (int i = 0; i < argc - 1 && i < MAX_FLOATS; ++i) {
    _floats[i] = get_param_num(argc, params, i + 1, 0);
  }
  if (mode == DRAW_FILL) {
    nk_fill_polygon(&_ctx->current->buffer, _floats, (argc - 1) / 2, _fg_color);
  } else if (mode == DRAW_LINE) {
    nk_stroke_polygon(&_ctx->current->buffer, _floats, (argc - 1) / 2, _line_thickness, _fg_color);
  }
  return (argc >= 2);
}

int cmd_progress(int argc, slib_par_t *params, var_t *retval) {
  return (argc >= 2 || argc <= 3);
}

int cmd_property(int argc, slib_par_t *params, var_t *retval) {
  const char *name = get_param_str(argc, params, 0, NULL);
  double min = get_param_num(argc, params, 1, 0);
  double max = get_param_num(argc, params, 3, 0);
  double step = get_param_num(argc, params, 4, 0);
  double inc_per_pixel = get_param_num(argc, params, 5, 0);
  if (is_param_num(argc, params, 2)) {
    double value = get_param_num(argc, params, 2, 0);
    value = nk_propertyd(_ctx, name, min, value, max, step, inc_per_pixel);
  } else if (is_param_map(argc, params, 2)) {
    double value = get_param_num_field(argc, params, 2, "value");
    double old = value;
    nk_property_double(_ctx, name, min, &value, max, step, inc_per_pixel);
    if (value != old) {
      v_setreal(map_get(params[2].var_p, "value"), value);
    }
  } else {
    v_setstr(retval, "Invalid property input");
  }
  return 1;
}

int cmd_radio(int argc, slib_par_t *params, var_t *retval) {
  int result;
  const char *name = get_param_str(argc, params, 0, NULL);
  if (name != NULL && is_param_map(argc, params, 1)) {
    const char *value = get_param_str_field(argc, params, 1, "value");
    int active = !strcasecmp(value, name);
    int changed = nk_radio_label(_ctx, value, &active);
    if (changed && active) {
      v_setstr(map_get(params[1].var_p, "value"), name);
    }
    result = 1;
  } else {
    v_setstr(retval, "Invalid radio input");
    result = 0;
  }
  return result;
}

int cmd_rectmulticolor(int argc, slib_par_t *params, var_t *retval) {
  float x = get_param_num(argc, params, 0, 0);
  float y = get_param_num(argc, params, 1, 0);
  float w = get_param_num(argc, params, 2, 0);
  float h = get_param_num(argc, params, 3, 0);
  struct nk_color topLeft = get_param_color(argc, params, 4);
  struct nk_color topRight = get_param_color(argc, params, 5);
  struct nk_color bottomLeft = get_param_color(argc, params, 6);
  struct nk_color bottomRight = get_param_color(argc, params, 7);
  nk_fill_rect_multi_color(&_ctx->current->buffer, nk_rect(x, y, w, h), topLeft, topRight, bottomLeft, bottomRight);
  return (argc == 8);
}

int cmd_selectable(int argc, slib_par_t *params, var_t *retval) {
  return (argc >= 2 && argc <= 4);
}

int cmd_slider(int argc, slib_par_t *params, var_t *retval) {
  float min = get_param_num(argc, params, 0, 0);
  float max = get_param_num(argc, params, 2, 0);
  float step = get_param_num(argc, params, 3, 0);
  int result = 1;
  if (is_param_num(argc, params, 1)) {
    float value = get_param_num(argc, params, 1, 0);
    value = nk_slide_float(_ctx, min, value, max, step);
  } else if (is_param_map(argc, params, 1)) {
    float value = get_param_num_field(argc, params, 1, "value");
    int changed = nk_slider_float(_ctx, min, &value, max, step);
    if (changed) {
      v_setreal(map_get(params[1].var_p, "value"), value);
    }
  } else {
    v_setstr(retval, "Invalid slider input");
    result = 0;
  }
  return result;
}

int cmd_spacing(int argc, slib_par_t *params, var_t *retval) {
  nk_spacing(_ctx, get_param_int(argc, params, 0, 0));
  return 1;
}

int cmd_text(int argc, slib_par_t *params, var_t *retval) {
  const char *text = get_param_str(argc, params, 0, NULL);
  float x = get_param_num(argc, params, 1, 0);
  float y = get_param_num(argc, params, 2, 0);
  float w = get_param_num(argc, params, 3, 0);
  float h = get_param_num(argc, params, 4, 0);
  //nk_draw_text(&_ctx->current->buffer, nk_rect(x, y, w, h), text, strlen(text),
  // &fonts[font_count++], nk_rgba(0, 0, 0, 0), _fg_color);
  return (argc == 5);
}

int cmd_textinput(int argc, slib_par_t *params, var_t *retval) {
  int len;
  char buf[256];
  const char *text = get_param_str(argc, params, 0, NULL);
  strcpy(buf, text);
  nk_edit_string(_ctx, NK_EDIT_SIMPLE, buf, &len, sizeof(buf) - 1, nk_filter_float);
  buf[len] = 0;
  v_setstr(retval, buf);
  return 1;
}

int cmd_tooltip(int argc, slib_par_t *params, var_t *retval) {
  const char *text = get_param_str(argc, params, 0, NULL);
  nk_tooltip(_ctx, text);
  return (argc == 1);
}

int cmd_treepop(int argc, slib_par_t *params, var_t *retval) {
  nk_tree_pop(_ctx);
  return (argc == 0);
}

int cmd_treepush(int argc, slib_par_t *params, var_t *retval) {
  return (argc >= 2 && argc <= 4);
}

int cmd_wheelmoved(int argc, slib_par_t *params, var_t *retval) {
  //  nk_input_scroll(_ctx, (float)y);
  v_setint(retval, nk_window_is_any_hovered(_ctx));
  return 1;
}

int cmd_widgetbounds(int argc, slib_par_t *params, var_t *retval) {
  struct nk_rect rc = nk_widget_bounds(_ctx);
  v_toarray1(retval, 4);
  v_setint(v_elem(retval, 0), rc.x);
  v_setint(v_elem(retval, 1), rc.y);
  v_setint(v_elem(retval, 2), rc.w);
  v_setint(v_elem(retval, 3), rc.h);
  return 1;
}

int cmd_widgetishovered(int argc, slib_par_t *params, var_t *retval) {
  v_setint(retval, nk_widget_is_hovered(_ctx));
  return 1;
}

int cmd_windowbegin(int argc, slib_par_t *params, var_t *retval) {
  const char *title = get_param_str(argc, params, 0, "Untitled");
  int x = get_param_int(argc, params, 1, 50);
  int y = get_param_int(argc, params, 2, 50);
  int w = get_param_int(argc, params, 3, 180);
  int h = get_param_int(argc, params, 4, 250);
  nk_flags flags = get_window_flags(argc, params, 5);
  process_events();
  v_setint(retval, nk_begin(_ctx, title, nk_rect(x, y, w, h), flags));
  return 1;
}

int cmd_windowend(int argc, slib_par_t *params, var_t *retval) {
  nk_end(_ctx);
  int width, height;
  SDL_GetWindowSize(_window, &width, &height);
  glViewport(0, 0, width, height);
  glClear(GL_COLOR_BUFFER_BIT);
  nk_sdl_render(NK_ANTI_ALIASING_ON);
  SDL_GL_SwapWindow(_window);
  return argc == 0;
}

int cmd_windowgetbounds(int argc, slib_par_t *params, var_t *retval) {
  struct nk_rect rc = nk_window_get_bounds(_ctx);
  v_toarray1(retval, 4);
  v_setint(v_elem(retval, 0), rc.x);
  v_setint(v_elem(retval, 1), rc.y);
  v_setint(v_elem(retval, 2), rc.w);
  v_setint(v_elem(retval, 3), rc.h);
  return 1;
}

API lib_proc[] = {
  {"BUTTON", cmd_button},
  {"ARC", cmd_arc},
  {"CHECKBOX", cmd_checkbox},
  {"CIRCLE", cmd_circle},
  {"COLORPICKER", cmd_colorpicker},
  {"COMBOBOX", cmd_combobox},
  {"CONTEXTUALEND", cmd_contextualend},
  {"CONTEXTUALITEM", cmd_contextualitem},
  {"CURVE", cmd_curve},
  {"DRAW", cmd_draw},
  {"EDIT", cmd_edit},
  {"ELLIPSE", cmd_ellipse},
  {"FRAMEBEGIN", cmd_framebegin},
  {"FRAMEEND", cmd_frameend},
  {"GROUPBEGIN", cmd_groupbegin},
  {"GROUPEND", cmd_groupend},
  {"IMAGE", cmd_image},
  {"INIT", cmd_init},
  {"KEYPRESSED", cmd_keypressed},
  {"LABEL", cmd_label},
  {"LAYOUTROW", cmd_layoutrow},
  {"LINE", cmd_line},
  {"MENUBARBEGIN", cmd_menubarbegin},
  {"MENUBAREND", cmd_menubarend},
  {"MENUEND", cmd_menuend},
  {"MENUITEM", cmd_menuitem},
  {"MOUSEMOVED", cmd_mousemoved},
  {"MOUSEPRESSED", cmd_mousepressed},
  {"MOUSERELEASED", cmd_mousereleased},
  {"POLYGON", cmd_polygon},
  {"PROGRESS", cmd_progress},
  {"PROPERTY", cmd_property},
  {"RADIO", cmd_radio},
  {"RECTMULTICOLOR", cmd_rectmulticolor},
  {"SELECTABLE", cmd_selectable},
  {"SLIDER", cmd_slider},
  {"SPACING", cmd_spacing},
  {"TEXT", cmd_text},
  {"TEXTINPUT", cmd_textinput},
  {"TOOLTIP", cmd_tooltip},
  {"TREEPOP", cmd_treepop},
  {"WHEELMOVED", cmd_wheelmoved},
  {"WINDOWEND", cmd_windowend},
};

API lib_func[] = {
  {"BUTTON", cmd_button},
  {"CONTEXTUALBEGIN", cmd_contextualbegin},
  {"MENUBEGIN", cmd_menubegin},
  {"TREEPUSH", cmd_treepush},
  {"WIDGETBOUNDS", cmd_widgetbounds},
  {"WIDGETISHOVERED", cmd_widgetishovered},
  {"WINDOWBEGIN", cmd_windowbegin},
  {"WINDOWGETBOUNDS", cmd_windowgetbounds}
};

int sblib_init(void) {
  return 1;
}

void sblib_devinit(const char *prog, int width, int height) {
  _window = NULL;
  _glContext = NULL;
  _ctx = NULL;
  _sdlExit = false;
  _bg_color = nk_black;
  _fg_color = nk_white;
  _line_thickness = 1;

  const char *name = strrchr(prog, '/');
  if (name == NULL) {
    name = prog;
  } else {
    name++;
  }
  int len = strlen(name) + 16;
  char *title = new char[len];
  sprintf(title, "%s - SmallBASIC", name);

  SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
  SDL_Init(SDL_INIT_VIDEO);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  _window = SDL_CreateWindow(title,
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             (width < 10) ? WINDOW_WIDTH : width,
                             (height < 10) ? WINDOW_HEIGHT : height,
                             SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|
                             SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_RESIZABLE);
  delete [] title;
  _glContext = SDL_GL_CreateContext(_window);
  glClearColor(0.10f, 0.18f, 0.24f, 1.0f);
  _ctx = nk_sdl_init(_window);
  struct nk_font_atlas *atlas;
  nk_sdl_font_stash_begin(&atlas);
  nk_sdl_font_stash_end();
}

int sblib_proc_count() {
  return (sizeof(lib_proc) / sizeof(lib_proc[0]));
}

int sblib_func_count() {
  return (sizeof(lib_func) / sizeof(lib_func[0]));
}

int sblib_proc_getname(int index, char *proc_name) {
  int result;
  if (index < sblib_proc_count()) {
    strcpy(proc_name, lib_proc[index].name);
    result = 1;
  } else {
    result = 0;
  }
  return result;
}

int sblib_func_getname(int index, char *proc_name) {
  int result;
  if (index < sblib_func_count()) {
    strcpy(proc_name, lib_func[index].name);
    result = 1;
  } else {
    result = 0;
  }
  return result;
}

int sblib_proc_exec(int index, int argc, slib_par_t *params, var_t *retval) {
  int result;
  if (index < sblib_proc_count()) {
    result = lib_proc[index].command(argc, params, retval);
  } else {
    result = 0;
  }
  return result;
}

int sblib_func_exec(int index, int argc, slib_par_t *params, var_t *retval) {
  int result;
  if (index < sblib_proc_count()) {
    result = lib_func[index].command(argc, params, retval);
  } else {
    result = 0;
  }

  return result;
}

int sblib_events(int wait_flag) {
  return _sdlExit ? -2 : 0;
}

void sblib_close() {
  if (_ctx) {
    nk_sdl_shutdown();
    SDL_GL_DeleteContext(_glContext);
    SDL_DestroyWindow(_window);
  }
}

struct nk_color sb_color(long c) {
  nk_color result;
  if (c >= 0 && c < 16) {
    result = _colors[c];
  } else {
    if (c < 0) {
      c = -c;
    }
    uint8_t r = (c & 0xff0000) >> 16;
    uint8_t g = (c & 0xff00) >> 8;
    uint8_t b = (c & 0xff);
    result = nk_rgba(r, g, b, 255);
  }
  return result;
}

void sblib_settextcolor(long fg, long bg) {
  _fg_color = sb_color(fg);
  _bg_color = sb_color(bg);
  struct nk_colorf c = nk_color_cf(_bg_color);
  glClearColor(c.r, c.g, c.b, c.a);
}

void sblib_setcolor(long fg) {
  _fg_color = sb_color(fg);
}
