#include "cadical.hpp"
#include "resources.hpp"
#include "signal.hpp"

/*------------------------------------------------------------------------*/

#include <csignal>
#include <cassert>

/*------------------------------------------------------------------------*/

extern "C" {
#include <unistd.h>
}

/*------------------------------------------------------------------------*/

// Signal handlers for printing statistics even if solver is interrupted.

namespace CaDiCaL {

static volatile bool caught_signal = false;
static Handler * signal_handler;

#ifndef __WIN32

static volatile bool caught_alarm = false;
static volatile bool alarm_set = false;
static int alarm_time = -1;

void Handler::catch_alarm () { catch_signal (SIGALRM); }

#endif

#define SIGNALS \
SIGNAL(SIGABRT) \
SIGNAL(SIGINT) \
SIGNAL(SIGSEGV) \
SIGNAL(SIGTERM) \

#define SIGNAL(SIG) \
static void (*SIG ## _handler)(int);
SIGNALS
// SIGNAL(SIGABRT), SIGNAL(SIGINT), SIGNAL(SIGSEGV), SIGNAL(SIGTERM) に等しい
// よって以下のように展開される：
// SIGNAL(SIGABRT) => static void (*SIGABRT_hander)(int);
// SIGNAL(SIGINT ) => static void (*SIGINT_hander)(int);
// SIGNAL(SIGSEGV) => static void (*SIGSEGV_hander)(int);
// SIGNAL(SIGTERM) => static void (*SIGTERM_hander)(int);
// これらは static だが，大域変数なので，複数の CaDiCaL オブジェクトを保持したときにハンドラはどうなる？
// → App の catch_signal メソッドがハンドラなので問題ない？
#undef SIGNAL
static void (*SIGALRM_handler)(int);

#ifndef __WIN32

void Signal::reset_alarm () {
  if (!alarm_set) return;
  (void) signal (SIGALRM, SIGALRM_handler);
  SIGALRM_handler = 0;
  caught_alarm = false;
  alarm_set = false;
  alarm_time = -1;
}

#endif

void Signal::reset () {
  signal_handler = 0;
#define SIGNAL(SIG) \
  (void) signal (SIG, SIG ## _handler); \
  SIG ## _handler = 0;
SIGNALS
#undef SIGNAL
#ifndef __WIN32
  reset_alarm ();
#endif
  caught_signal = false;
}

const char * Signal::name (int sig) {
#define SIGNAL(SIG) \
  if (sig == SIG) return # SIG;
  SIGNALS
#undef SIGNAL
#ifndef __WIN32
  if (sig == SIGALRM) return "SIGALRM";
#endif
  return "UNKNOWN";
}

// TODO printing is not reentrant and might lead to deadlock if the signal
// is raised during another print attempt (and locked IO is used).  To avoid
// this we have to either run our own low-level printing routine here or in
// 'Message' or just dump those statistics somewhere else were we have
// exclusive access to.  All these solutions are painful and not elegant.

static void catch_signal (int sig) {
#ifndef __WIN32
  if (sig == SIGALRM && absolute_real_time () >= alarm_time) {
    if (!caught_alarm) {
      caught_alarm = true;
      if (signal_handler) signal_handler->catch_alarm ();
    }
    Signal::reset_alarm ();
  } else 
#endif
  {
    if (!caught_signal) {
      caught_signal = true;
      if (signal_handler) signal_handler->catch_signal (sig);
    }
    Signal::reset ();
    ::raise (sig);
  }
}

void Signal::set (Handler * h) {
  signal_handler = h;
#define SIGNAL(SIG) \
  SIG ## _handler = signal (SIG, catch_signal);
SIGNALS
// SIGNAL(SIGABRT), SIGNAL(SIGINT), SIGNAL(SIGSEGV), SIGNAL(SIGTERM) に等しい
// よって以下のように展開される：
// SIGNAL(SIGABRT) => SIGABRT_handler = signal(SIGABRT, catch_signal);
// SIGNAL(SIGINT ) => SIGINT_handler  = signal(SIGINT , catch_signal);
// SIGNAL(SIGSEGV) => SIGSEGV_handler = signal(SIGSEGV, catch_signal);
// SIGNAL(SIGTERM) => SIGTERM_handler = signal(SIGTERM, catch_signal);
#undef SIGNAL
}

#ifndef __WIN32

void Signal::alarm (int seconds) {
  assert (seconds >= 0);
  assert (!alarm_set);
  assert (alarm_time < 0);
  SIGALRM_handler = signal (SIGALRM, catch_signal);
  alarm_set = true;
  alarm_time = absolute_real_time () + seconds;
  ::alarm (seconds);
}

#endif

}
