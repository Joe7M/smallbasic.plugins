// This file is part of SmallBASIC
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//
// Copyright(C) 2024 Chris Warren-Smith

#include "config.h"
#include "include/var.h"
#include "include/module.h"
#include "include/param.h"

#include <cstring>
#include <cstdio>
#include <jni.h>
#include <pthread.h>
#include "robin-hood-hashing/src/include/robin_hood.h"

JNIEnv *env;
JavaVM *jvm;
int nextId = 1;

#define CLASS_DIGITAL_INPUT "net/sourceforge/smallbasic/ioio/DigitalOutput"
#define CLASS_ANALOG_INPUT "net/sourceforge/smallbasic/ioio/AnalogInput"
#define CLASS_IOCLASS 1
#define METHOD_BEGIN_BATCH "beginBatch"
#define METHOD_DISCONNECT "disconnect"
#define METHOD_END_BATCH "endBatch"
#define METHOD_HARD_RESET "hardReset"
#define METHOD_OPEN  "open"
#define METHOD_READY "isReady"
#define METHOD_SOFT_RESET "softReset"
#define METHOD_SYNC "sync"
#define METHOD_WAIT_FOR_CONNECT "waitForConnect"
#define METHOD_WAIT_FOR_DISCONNECT "waitForDisconnect"
#define METHOD_WRITE "write"

struct IOClass {
  IOClass(): _clazz(nullptr), _instance(nullptr) {}

  virtual ~IOClass() {
    _clazz = nullptr;
    _instance = nullptr;
  }

  bool create(const char *path) {
    bool result;
    if (_instance != nullptr) {
      // error when already constructed
      result = false;
    } else {
      _clazz = env->FindClass(path);
      if (_clazz == nullptr) {
        env->ExceptionDescribe();
      } else {
        jmethodID constructor = env->GetMethodID(_clazz, "<init>", "()V");
        if (constructor == nullptr) {
          env->ExceptionDescribe();
        } else {
          _instance = env->NewObject(_clazz, constructor);
        }
      }
      result = _instance != nullptr;
    }
    return result;
  }

  bool checkException() {
    auto exc = env->ExceptionOccurred();
    if (exc) {
      env->ExceptionDescribe();
      env->ExceptionClear();
    }
    return exc;
  }

  bool invokeIV(const char *name, int value) {
    bool result = false;
    if (_instance != nullptr) {
      jmethodID method = env->GetMethodID(_clazz, name, "(I)V");
      if (method != nullptr) {
        env->CallVoidMethod(_instance, method, value);
      }
      if (!checkException()) {
        result = (method != nullptr);
      }
    }
    return result;
  }

  int invokeI(const char *name) {
    int result = 0;
    if (_instance != nullptr) {
      jmethodID method = env->GetMethodID(_clazz, name, "()I");
      if (method != nullptr) {
        result = env->CallIntMethod(_instance, method);
      }
      checkException();
    }
    return result;
  }

  void invokeV(const char *name) {
    if (_instance != nullptr) {
      jmethodID method = env->GetMethodID(_clazz, name, "()V");
      if (method != nullptr) {
        env->CallVoidMethod(_instance, method);
      }
      checkException();
    }
  }

  int isReady() {
    return invokeI(METHOD_READY);
  }

  bool open(int pin) {
    return invokeIV(METHOD_OPEN, pin);
  }

  void beginBatch() {
    invokeV(METHOD_BEGIN_BATCH);
  }

  void endBatch() {
    invokeV(METHOD_END_BATCH);
  }

  void disconnect() {
    invokeV(METHOD_DISCONNECT);
  }

  void hardReset() {
    invokeV(METHOD_HARD_RESET);
  }

  void softReset() {
    invokeV(METHOD_SOFT_RESET);
  }

  void sync() {
    invokeV(METHOD_SYNC);
  }

  void waitForConnect() {
    invokeV(METHOD_WAIT_FOR_CONNECT);
  }

  void waitForDisconnect() {
    invokeV(METHOD_WAIT_FOR_DISCONNECT);
  }

  bool write(int value) {
    return invokeIV(METHOD_WRITE, value);
  }

  private:
  jclass _clazz;
  jobject _instance;
};

robin_hood::unordered_map<int, IOClass> _classMap;

static int get_io_class_id(var_s *map, var_s *retval) {
  int result = -1;
  if (is_map(map)) {
    int id = map->v.m.id;
    if (id != -1 && _classMap.find(id) != _classMap.end()) {
      result = id;
    }
  }
  if (result == -1) {
    error(retval, "IOClass not found");
  }
  return result;
}

static int cmd_begin_batch(var_s *self, int param_count, slib_par_t *params, var_s *retval) {
  int result = 0;
  if (param_count != 0) {
    error(retval, METHOD_BEGIN_BATCH, 0);
  } else {
    int id = get_io_class_id(self, retval);
    if (id != -1) {
      _classMap.at(id).beginBatch();
      result = 1;
    }
  }
  return result;
}

static int cmd_disconnect(var_s *self, int param_count, slib_par_t *params, var_s *retval) {
  int result = 0;
  if (param_count != 0) {
    error(retval, METHOD_DISCONNECT, 0);
  } else {
    int id = get_io_class_id(self, retval);
    if (id != -1) {
      _classMap.at(id).disconnect();
      result = 1;
    }
  }
  return result;
}

static int cmd_end_batch(var_s *self, int param_count, slib_par_t *params, var_s *retval) {
  int result = 0;
  if (param_count != 0) {
    error(retval, METHOD_END_BATCH, 0);
  } else {
    int id = get_io_class_id(self, retval);
    if (id != -1) {
      _classMap.at(id).endBatch();
      result = 1;
    }
  }
  return result;
}

static int cmd_hard_reset(var_s *self, int param_count, slib_par_t *params, var_s *retval) {
  int result = 0;
  if (param_count != 0) {
    error(retval, METHOD_HARD_RESET, 0);
  } else {
    int id = get_io_class_id(self, retval);
    if (id != -1) {
      _classMap.at(id).hardReset();
      result = 1;
    }
  }
  return result;
}

static int cmd_is_ready(var_s *self, int param_count, slib_par_t *params, var_s *retval) {
  int result = 0;
  if (param_count != 0) {
    error(retval, METHOD_READY, 0);
  } else {
    int id = get_io_class_id(self, retval);
    if (id != -1) {
      v_setint(retval, _classMap.at(id).isReady());
      result = 1;
    }
  }
  return result;
}

static int cmd_soft_reset(var_s *self, int param_count, slib_par_t *params, var_s *retval) {
  int result = 0;
  if (param_count != 0) {
    error(retval, METHOD_SOFT_RESET, 0);
  } else {
    int id = get_io_class_id(self, retval);
    if (id != -1) {
      _classMap.at(id).softReset();
      result = 1;
    }
  }
  return result;
}

static int cmd_sync(var_s *self, int param_count, slib_par_t *params, var_s *retval) {
  int result = 0;
  if (param_count != 0) {
    error(retval, METHOD_SYNC, 0);
  } else {
    int id = get_io_class_id(self, retval);
    if (id != -1) {
      _classMap.at(id).sync();
      result = 1;
    }
  }
  return result;
}

static int cmd_wait_for_connect(var_s *self, int param_count, slib_par_t *params, var_s *retval) {
  int result = 0;
  if (param_count != 0) {
    error(retval, METHOD_WAIT_FOR_CONNECT, 0);
  } else {
    int id = get_io_class_id(self, retval);
    if (id != -1) {
      _classMap.at(id).waitForConnect();
      result = 1;
    }
  }
  return result;
}

static int cmd_wait_for_disconnect(var_s *self, int param_count, slib_par_t *params, var_s *retval) {
  int result = 0;
  if (param_count != 0) {
    error(retval, METHOD_WAIT_FOR_DISCONNECT, 0);
  } else {
    int id = get_io_class_id(self, retval);
    if (id != -1) {
      _classMap.at(id).waitForDisconnect();
      result = 1;
    }
  }
  return result;
}

static int cmd_digital_output_write(var_s *self, int param_count, slib_par_t *params, var_s *retval) {
  int result = 0;
  if (param_count != 1) {
    error(retval, METHOD_WRITE, 1);
  } else {
    int id = get_io_class_id(self, retval);
    if (id != -1) {
      int value = get_param_int(param_count, params, 0, 0);
      _classMap.at(id).write(value);
      result = 1;
    }
  }
  return result;
}

static void create_io_class(var_t *map, int id) {
  map_init_id(map, id, CLASS_IOCLASS);
  v_create_callback(map, METHOD_BEGIN_BATCH, cmd_begin_batch);
  v_create_callback(map, METHOD_DISCONNECT, cmd_disconnect);
  v_create_callback(map, METHOD_END_BATCH, cmd_end_batch);
  v_create_callback(map, METHOD_HARD_RESET, cmd_hard_reset);
  v_create_callback(map, METHOD_READY, cmd_is_ready);
  v_create_callback(map, METHOD_SOFT_RESET, cmd_soft_reset);
  v_create_callback(map, METHOD_SYNC, cmd_sync);
  v_create_callback(map, METHOD_WAIT_FOR_CONNECT, cmd_wait_for_connect);
  v_create_callback(map, METHOD_WAIT_FOR_DISCONNECT, cmd_wait_for_disconnect);
}

static int cmd_openanaloginput(int argc, slib_par_t *params, var_t *retval) {
  int result;
  int pin = get_param_int(argc, params, 0, 0);
  int id = ++nextId;
  IOClass &input = _classMap[id];
  if (input.create(CLASS_ANALOG_INPUT) &&
      input.open(pin)) {
    create_io_class(retval, id);
    //v_create_func(retval, "write", cmd_digital_output_write);
    result = 1;
  } else {
    _classMap.erase(id);
    error(retval, "openAnalogInput() failed");
    result = 0;
  }
  return result;
}

static int cmd_opendigitaloutput(int argc, slib_par_t *params, var_t *retval) {
  int result;
  int pin = get_param_int(argc, params, 0, 0);
  int id = ++nextId;
  IOClass &output = _classMap[id];
  if (output.create(CLASS_DIGITAL_INPUT) &&
      output.open(pin)) {
    create_io_class(retval, id);
    v_create_callback(retval, METHOD_WRITE, cmd_digital_output_write);
    result = 1;
  } else {
    _classMap.erase(id);
    error(retval, "openDigitalOutput() failed");
    result = 0;
  }
  return result;
}

FUNC_SIG lib_func[] = {
  {1, 1, "OPENANALOGINPUT", cmd_openanaloginput},
  {1, 1, "OPENDIGITALOUTPUT", cmd_opendigitaloutput},
};

FUNC_SIG lib_proc[] = {};

SBLIB_API int sblib_proc_count() {
  return (sizeof(lib_proc) / sizeof(lib_proc[0]));
}

SBLIB_API int sblib_func_count() {
  return (sizeof(lib_func) / sizeof(lib_func[0]));
}

int sblib_init(const char *sourceFile) {
  JavaVMInitArgs vm_args;
  JavaVMOption options[2];
  options[0].optionString = (char *)"-Djava.class.path=./target/ioio-1.0-jar-with-dependencies.jar";
  options[1].optionString = (char *)"-Dioio.SerialPorts=IOIO0";
  //options[2].optionString = "-Xdebug";
  //options[3].optionString = "-agentlib:jdwp=transport=dt_socket,server=y,address=5005,suspend=y";
  //options[2].optionString = (char *)"-Xcheck:jni";
  vm_args.version = JNI_VERSION_1_8;
  vm_args.nOptions = 2;
  vm_args.options = options;
  vm_args.ignoreUnrecognized = 0;
  int result = (JNI_CreateJavaVM(&jvm, (void **)&env, &vm_args) == JNI_OK &&
                jvm->AttachCurrentThread((void **)&env, nullptr) == JNI_OK);
  if (!result) {
    fprintf(stderr, "Failed to create JVM\n");
  }
  return result;
}

SBLIB_API void sblib_free(int cls_id, int id) {
  if (id != -1) {
    switch (cls_id) {
    case CLASS_IOCLASS:
      _classMap.erase(id);
      break;
    }
  }
}

void sblib_close(void) {
  if (!_classMap.empty()) {
    fprintf(stderr, "IOClass leak detected\n");
    _classMap.clear();
  }
  jvm->DetachCurrentThread();
  // calling this hangs
  //jvm->DestroyJavaVM();
  env = nullptr;
  jvm = nullptr;
}

