# libev-notify
Linux based file system monitoring library based on libev and inotify


# Synopsis

[10s] -> [2s] -> [3s] x [4s] -> [1s] 

```
ev::dynamic_loop loop(EV_BACKEND_EPOLL);
Chain chain(loop);
chain.add([](auto res, auto err) {
   /* Collect something over the wire */
   res.set(data);
}, exp=10., repeat=20).add([](auto res, auto err){
    res.set(price);
});
```
