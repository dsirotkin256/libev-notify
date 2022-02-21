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
#define EV_STAT_ENABLE 1

#define EV_MINPRI -2
#define EV_MAXPRI 2

#include <ev++.h>
#include <fcntl.h>
#include <liburing.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>


struct stdin_handler {
  ev::timer _tm_watch;
  ev::loop_ref _loop;
  double _timeout;
  const char *_1 = "IS WRITE";
  const char *_2 = "BUFFER";
  const char *exit_cmd = "EXIT\n";
  char buff[256];
  char msg[512];
  const char *reminder = "Last valid input was entered 5 sec ago.\n\0";

  stdin_handler(ev::loop_ref loop, double timeout = 5.)
      : _loop{loop}, _timeout(timeout) {
    _tm_watch.set(loop);
    _tm_watch.set(this);
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
      _tm_watch.stop();
      char *token = ::strtok(buff, "\n");
      do {
        ::memset(msg, '\0', sizeof(msg));

        ::sprintf(msg, "%s:%d\n%s:%s\n", _1, revents & ev::WRITE, _2, token);
        ::fprintf(stdout, msg, sizeof(msg));
        ::fflush(stdout);
      } while ((token = ::strtok(NULL, "\n")));
      _tm_watch.start(_timeout);
    }
  }
};

#define QUEUE_DEPTH 64
#define BLK_SIZE 4096

int main(int argc, char **argv) {

  int i = 0, fd, ret, pending = 0, done = 0;
  struct io_uring ring;
  ret = io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
  if (ret < 0) {
    fprintf(stderr, "queue_init: %s\n", strerror(-ret));
    return ret;
  }

  fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
      perror("open");
      return fd;
  }

  struct io_uring_sqe *sqe;
  struct io_uring_cqe *cqe;

  struct iovec *iovecs;
  struct stat sb;
  ssize_t fsize = 0;
  off_t offset = 0;
  void *buf;

  ret = fstat(fd, &sb);

  if (ret < 0) {
    perror("fstat");
    return ret;
  }

  iovecs = static_cast<struct iovec*>(calloc(QUEUE_DEPTH, sizeof(struct iovec)));

  for (i = 0; i < QUEUE_DEPTH; ++i) {
    if (posix_memalign(&buf, BLK_SIZE, BLK_SIZE)) {
      fprintf(stderr, "posix_memalign");
      return 1;
    }
    iovecs[i].iov_base = buf;
    iovecs[i].iov_len = BLK_SIZE;
    fsize += BLK_SIZE;
  }

  i = 0;

  do {
    sqe = io_uring_get_sqe(&ring);
    if (!sqe) break;
    io_uring_prep_readv(sqe, fd, &iovecs[i], 1, offset);
    offset += iovecs[i].iov_len;
    ++i;
    if (offset > sb.st_size) break;
  } while (true);

  ret = io_uring_submit(&ring);
  if (ret < 0) {
    fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
    return ret;
  } else if (ret != i) {
    fprintf(stderr, "io_uring_submit submitted less: %d\n", ret);
    return ret;
  }

  pending = ret;

  for (i = 0; i < pending; ++i) {
    ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
      fprintf(stderr, "io_uring_wait_cqe: %s\n", strerror(-ret));
      return ret;
    }
    done++;
    ret = 0;
    if (cqe->res != BLK_SIZE && cqe->res + fsize != sb.st_size) {
      fprintf(stderr, "ret=%d wanted 4096\n", cqe->res);
      ret = 1;
    }
    fsize += cqe->res;
    io_uring_cqe_seen(&ring, cqe);
    if (ret) break;
  }

  printf("submitted=%d completed=%d bytes=%lu\n", pending, done, (unsigned long) fsize);

  close(fd);
  io_uring_queue_exit(&ring);

/*
  // Open stdin fd in a non-blocking mode
  ::fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

  ev::dynamic_loop loop(EVBACKEND_IOURING);

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
  dummy_greeter.start(0., 500.);

  struct change_handler {
    void operator()(ev::stat &stat, int revents) {
      if (stat.attr.st_nlink) {
        printf("Path: %s", stat.path);
        printf("Total size in bytes: %ld\n", (long)stat.attr.st_size);
        printf("Time of last access: %ld\n", (long)stat.attr.st_atime);
        printf("Time of last modification: %ld\n", (long)stat.attr.st_mtime);
        printf("ID of device containing file: %ld\n", (long)stat.attr.st_dev);
        printf("Inode number: %ld\n", (long)stat.attr.st_ino);
        printf("File type and mode: %ld\n", (long)stat.attr.st_mode);
        printf("Number of hard links: %ld\n", (long)stat.attr.st_nlink);
        printf("User ID: %ld\n", (long)stat.attr.st_uid);
        printf("Group ID: %ld\n", (long)stat.attr.st_gid);
        printf("Device ID: %ld\n", (long)stat.attr.st_rdev);
        printf("Time of last status change: %ld\n", (long)stat.attr.st_ctime);
      } else {
        puts("File does not exit!\n");
      }
    }
  } chg_hdr;

  ev::stat dir_watch;
  dir_watch.set(&chg_hdr);
  dir_watch.set(loop);
  dir_watch.start("/mnt/extra/libev-notify");

  loop.run();
  */

  return 0;
}