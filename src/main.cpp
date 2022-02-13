#define EV_COMPAT3 1
#define EV_FEATURES 1
#define EV_MULTIPLICITY 1
#define EV_USE_MONOTONIC 1
#define EV_USE_FLOOR 1
#define EV_USE_EVENTFD 1
#define EV_USE_TIMERFD 1
#define EV_USE_EPOLL 1
#define EV_USE_INOTIFY 1
#define EV_NO_THREADS 1
#define EV_ASYNC_ENABLE 1

#define EV_MINPRI -2
#define EV_MAXPRI 2

#include <ev++.h>
#include <fcntl.h>
#include <unistd.h>

struct stdin_handler {
  ev::timer tm_watch;
  ev::loop_ref _loop;
  const char *_1 = "IS WRITE";
  const char *_2 = "BUFFER";
  const char *exit_cmd = "EXIT\n";
  char buff[256];
  char msg[512];
  const char *reminder = "Last valid input was entered 5 sec ago.\n\0";

  stdin_handler(ev::loop_ref loop) : _loop{loop} {
    tm_watch.set(loop);
    tm_watch.set(this);
  }

  void operator()(ev::timer &t, int revents) {
    ::fprintf(stdout, reminder, sizeof(reminder));
    ::fflush(stdout);
  }

  void operator()(ev::io &io, int revents) {
    // Flash the buffer on data
    ::memset(buff, '\0', sizeof(buff));
    int res = ::read(io.fd, (void *)buff, sizeof(buff));
    if (res < 0) {
      return;
    }
    ::fflush(stdin);
    if (!::strcmp(buff, exit_cmd) || !::strcmp(buff, "\0")) {
      _loop.break_loop();
      return;
    }

    if (::strcmp(buff, "\n")) { /* Ignore new line input */
      tm_watch.stop();
      char *token = ::strtok(buff, "\n");
      do {
        ::memset(msg, '\0', sizeof(msg));

        ::sprintf(msg, "%s:%d\n%s:%s\n", _1, revents & ev::WRITE, _2, token);
        ::fprintf(stdout, msg, sizeof(msg));
        ::fflush(stdout);
      } while ((token = ::strtok(NULL, "\n")));
      tm_watch.start(5., 0.);
    }
  }
};


int main() {

  // Open stdin fd in a non-blocking mode
  ::fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

  ev::dynamic_loop loop(EVBACKEND_EPOLL);

  stdin_handler stdin_handl(loop);

  ev::io stdin_watch;

  // Register callback
  stdin_watch.set(&stdin_handl);

  // Add watcher to the loop
  stdin_watch.set(loop);

  // Wait until stdin becomes readable
  // Start watching fd and events -> invoke callback
  stdin_watch.start(STDIN_FILENO, EV_WRITE);


  struct periodic_hi {
    void operator()(ev::timer &, int revents) { printf("Hi!\n"); }
  } hi;

  ev::timer dummy_greeter;
  dummy_greeter.set(&hi);
  dummy_greeter.set(loop);
  dummy_greeter.start(0.,5.);

  loop.run();

  return 0;
}