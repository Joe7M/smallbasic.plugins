// This file is part of SmallBASIC
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//
// Copyright(C) 2020 Chris Warren-Smith

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>

#include "config.h"
#include "param.h"
#include "hashmap.h"
#include "var_map.h"
#include "var.h"

float get_num(var_p_t var) {
  float result;
  switch (var->type) {
  case V_INT:
    result = var->v.i;
    break;
  case V_NUM:
    result = var->v.n;
    break;
  default:
    result = 0.0;
    break;
  }
  return result;
}

bool get_bool(var_p_t var) {
  bool result;
  switch (var->type) {
  case V_INT:
    result = (var->v.i != 0);
    break;
  case V_NUM:
    result = (var->v.n != 0);
    break;
  case V_STR:
    result = (strncasecmp(var->v.p.ptr, "true", 4));
    break;
  default:
    result = false;
    break;
  }
  return result;
}

int get_int(var_t *v) {
  int result;
  switch (v ? v->type : -1) {
  case V_INT:
    result = v->v.i;
    break;
  case V_NUM:
    result = v->v.n;
    break;
  default:
    result = 0;
    break;
  }
  return result;
}

void v_init(var_t *v) {
  v->type = V_INT;
  v->const_flag = 0;
  v->v.i = 0;
}

uint32_t v_get_capacity(uint32_t size) {
  return size + (size / 2) + 1;
}

void v_alloc_capacity(var_t *var, uint32_t size) {
  uint32_t capacity = v_get_capacity(size);
  v_capacity(var) = capacity;
  v_asize(var) = size;
  v_data(var) = (var_t *)malloc(sizeof(var_t) * capacity);
  if (v_data(var)) {
    for (uint32_t i = 0; i < capacity; i++) {
      var_t *e = v_elem(var, i);
      e->pooled = 0;
      v_init(e);
    }
  }
}

var_t *v_new() {
  var_t *result = (var_t *)malloc(sizeof(var_t));
  result->pooled = 0;
  v_init(result);
  return result;
}

void v_setint(var_t *var, var_int_t i) {
  assert(var->type == V_INT);
  var->type = V_INT;
  var->v.i = i;
}

void v_setreal(var_t *var, var_num_t n) {
  assert(var->type == V_INT);
  var->type = V_NUM;
  var->v.n = n;
}

void v_setstr(var_t *var, const char *str) {
  assert(var->type == V_INT);

  int length = strlen(str == nullptr ? 0 : str);
  var->type = V_STR;
  var->v.p.ptr = (char *)malloc(length + 1);
  var->v.p.ptr[0] = '\0';
  var->v.p.length = length + 1;
  var->v.p.owner = 1;
  strcpy(var->v.p.ptr, str);
}

int v_strlen(const var_t *v) {
  int result;
  if (v->type == V_STR) {
    result = v->v.p.length;
    if (result && v->v.p.ptr[result - 1] == '\0') {
      result--;
    }
  } else {
    result = 0;
  }
  return result;
}

void v_new_array(var_t *var, uint32_t size) {
  assert(var->type == V_INT);
  var->type = V_ARRAY;
  v_alloc_capacity(var, size);
}

void v_toarray1(var_t *v, uint32_t r) {
  v_new_array(v, r);
  v_maxdim(v) = 1;
  v_lbound(v, 0) = 0;
  v_ubound(v, 0) = r - 1;
}

void v_tomatrix(var_t *v, int r, int c) {
  v_new_array(v, r * c);
  v_maxdim(v) = 2;
  v_lbound(v, 0) = v_lbound(v, 1) = 0;
  v_ubound(v, 0) = r - 1;
  v_ubound(v, 1) = c - 1;
}

var_p_t map_add_var(var_p_t base, const char *name, int value) {
  var_p_t key = v_new();
  v_setstr(key, name);
  var_p_t var = hashmap_putv(base, key);
  v_setint(var, value);
  return var;
}

var_p_t map_get(var_p_t base, const char *name) {
  var_p_t result;
  if (base != NULL && base->type == V_MAP) {
    result = hashmap_get(base, name);
  } else {
    result = NULL;
  }
  return result;
}

void map_init(var_p_t map) {
  assert(map->type == V_INT);
  v_init(map);
  hashmap_create(map, 0);
}

int map_get_bool(var_p_t base, const char *name) {
  var_p_t var = map_get(base, name);
  return var != nullptr ? get_bool(var) : 0;
}

void map_set_int(var_p_t base, const char *name, var_int_t n) {
  var_p_t var = map_get(base, name);
  if (var != nullptr) {
    v_setint(var, n);
  }
}

int map_get_int(var_p_t base, const char *name, int def) {
  var_p_t var = map_get(base, name);
  return var != nullptr ? get_int(var) : def;
}

int is_param_array(int argc, slib_par_t *params, int n) {
  int result;
  if (n >= 0 && n < argc) {
    result = (params[n].var_p->type == V_ARRAY);
  } else {
    result = 0;
  }
  return result;
}

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

int is_param_nil(int argc, slib_par_t *params, int n) {
  int result;
  if (n >= 0 && n < argc) {
    result = (params[n].var_p->type == V_NIL);
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
      sprintf(buf, "%lld", params[n].var_p->v.i);
      result = buf;
      break;
    case V_NUM:
      sprintf(buf, "%f", params[n].var_p->v.n);
      result = buf;
      break;
    default:
      result = 0;
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
      result = nullptr;
    }
  } else {
    result = nullptr;
  }
  return result;
}

float get_map_num(var_p_t map, const char *name) {
  var_p_t var = map_get(map, name);
  return var != nullptr ? get_num(var) : 0;
}

float get_array_elem_num(var_p_t array, int index) {
  float result;
  int size = v_asize(array);
  if (index >= 0 && index < size) {
    result = get_num(v_elem(array, index));
  } else {
    result = 0.0;
  }
  return result;
}
