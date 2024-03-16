/*
 * Copyright (c) 2020 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "log.h"
#include "serial.h"
#include "config.h"
#include "vcp.h"

#define MAX_CALLBACKS 32

typedef struct {
  log_LogFn fn;
  void *udata;
  int level;
} Callback;

static struct {
  void *udata;
  log_LockFn lock;
  int level;
  bool quiet;
  Callback callbacks[MAX_CALLBACKS];
} L;

UART_HandleTypeDef *uart;

static const char *level_strings[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
  "\x1b[94m", "\x1b[90m", "\x1b[0m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};
#endif

static void millis_to_timestamp(char *buf, int millis)
{
  uint8_t ms, sec, min, hr;
  hr =  millis / (1000 * 60 * 60);
  min = millis / (1000 *60) % 60;
  sec = millis / (1000) % 60;
  ms =  millis % 1000;
  sprintf(buf, "%02d:%02d:%02d.%03d", hr, min, sec, ms);
}

static void stdout_callback(log_Event *ev) {
  char fullBuf[MAX_MSG_LENGTH];
  char timeBuf[16];
  millis_to_timestamp(timeBuf, ev->time);
#ifdef LOG_USE_COLOR
  sprintf(
    fullBuf, "%s%s %-5s %16s:%-4d ",
    level_colors[ev->level], timeBuf, level_strings[ev->level],
    ev->file, ev->line);
#else
  sprintf(
    fullBuf, "%s %-5s %s:%d: ",
    timeBuf, level_strings[ev->level], ev->file, ev->line);
#endif
  vsprintf(fullBuf + strlen(fullBuf), ev->fmt, ev->ap);
  sprintf(fullBuf + strlen(fullBuf), "\x1b[0m\r\n");
  SerialWrite(uart, fullBuf);
  memset(fullBuf, 0, MAX_MSG_LENGTH);
}

static void vcp_callback(log_Event *ev) {
    char fullBuf[MAX_MSG_LENGTH];
    uint16_t len = sprintf(
        fullBuf, "%-5s %s:%d: ",
        level_strings[ev->level], ev->file, ev->line
    );
    VCPWriteDebugMsg(fullBuf, len);
    memset(fullBuf, 0, MAX_MSG_LENGTH);
}

static void lock(void)   {
  if (L.lock) { L.lock(true, L.udata); }
}


static void unlock(void) {
  if (L.lock) { L.lock(false, L.udata); }
}


const char* log_level_string(int level) {
  return level_strings[level];
}


void log_set_lock(log_LockFn fn, void *udata) {
  L.lock = fn;
  L.udata = udata;
}


void log_set_level(int level) {
  L.level = level;
}


void log_set_quiet(bool enable) {
  L.quiet = enable;
}


int log_add_callback(log_LogFn fn, void *udata, int level) {
  for (int i = 0; i < MAX_CALLBACKS; i++) {
    if (!L.callbacks[i].fn) {
      L.callbacks[i] = (Callback) { fn, udata, level };
      return 0;
    }
  }
  return -1;
}

void log_set_uart(UART_HandleTypeDef *new_uart)
{
  uart = new_uart;
}


static void init_event(log_Event *ev, void *udata) {
  if (!ev->time) {
    ev->time = HAL_GetTick();
  }
  ev->udata = udata;
}


void log_log(int level, const char *file, int line, const char *fmt, ...) {
  log_Event ev = {
    .fmt   = fmt,
    .file  = file,
    .line  = line,
    .level = level,
  };

  lock();

  if (!L.quiet && level >= L.level) {
    init_event(&ev, stderr);
    va_start(ev.ap, fmt);
    #ifdef UART2_LOGGING
    stdout_callback(&ev);
    #endif
    #ifdef VCP_LOGGING
    vcp_callback(&ev);
    #endif
    va_end(ev.ap);
  }

  for (int i = 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) {
    Callback *cb = &L.callbacks[i];
    if (level >= cb->level) {
      init_event(&ev, cb->udata);
      va_start(ev.ap, fmt);
      cb->fn(&ev);
      va_end(ev.ap);
    }
  }

  unlock();
}
