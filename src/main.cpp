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

  ev::dynamic_loop loop(EVBACKEND_IOURING);

  loop.set_io_collect_interval(0.000001);
  loop.set_timeout_collect_interval(0.1);

  stdin_handler stdin_handl(loop);

  ev::io stdin_watch;

  // Register callback
  stdin_watch.set(&stdin_handl);

  // Wait until stdin becomes readable
  stdin_watch.set(STDIN_FILENO, ev::WRITE);

  // Add watcher to the loop
  stdin_watch.set(loop);

  // Start watching fd and events -> invoke callback
  stdin_watch.start();

  struct periodic_hi {
    void operator()(ev::timer &, int revents) { printf("Hi!\n"); }
  } hi;

  ev::timer dummy_greeter;
  dummy_greeter.set(&hi);
  dummy_greeter.set(0., 5.);
  dummy_greeter.set(loop);
  dummy_greeter.start();

  loop.run();

  return 0;
}