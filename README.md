# KVM server

blah blah blah sandboxing

## Benchmarks

Deno native:
```sh
$ ./wrk -c1 -t1 http://127.00.1:8080
Running 10s test @ http://127.00.1:8080
  1 threads and 1 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    12.04us   17.13us   1.89ms   99.24%
    Req/Sec    85.20k     9.94k   96.70k    72.28%
  855810 requests in 10.10s, 126.51MB read
Requests/sec:  84739.32
Transfer/sec:     12.53MB

$ ./wrk -c64 -t64 http://127.00.1:8080
Running 10s test @ http://127.00.1:8080
  64 threads and 64 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   323.48us   29.34us   2.08ms   89.37%
    Req/Sec     3.11k    85.66     3.99k    97.00%
  1997161 requests in 10.10s, 295.22MB read
Requests/sec: 197741.36
Transfer/sec:     29.23MB
```
Deno has good performance. 12us on average single-threaded.

> $ deno serve --parallel --allow-all --unstable-net deno/main.ts

Deno serve seems to be a wrapper around fork():
```sh
$ ./wrk -c64 -t64 http://127.00.1:8080
Running 10s test @ http://127.00.1:8080
  64 threads and 64 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    84.89us  335.01us  19.62ms   98.35%
    Req/Sec    17.48k     2.47k   35.03k    83.03%
  11240457 requests in 10.10s, 1.62GB read
Requests/sec: 1112864.20
Transfer/sec:    164.50MB
```

Deno sandboxed in TinyKVM:
```sh
$ ./wrk -c1 -t1 http://127.0.0.1:8000
Running 10s test @ http://127.0.0.1:8000
  1 threads and 1 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    15.50us    1.72us 126.00us   93.03%
    Req/Sec    63.61k     2.35k   65.78k    88.12%
  638961 requests in 10.10s, 94.45MB read
Requests/sec:  63264.87
Transfer/sec:      9.35MB

$ ./wrk -c64 -t64 http://127.0.0.1:8000
Running 10s test @ http://127.0.0.1:8000
  64 threads and 64 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    68.30us   97.70us  11.34ms   99.04%
    Req/Sec    15.61k     6.90k   32.44k    77.84%
  10038128 requests in 10.10s, 1.45GB read
Requests/sec: 993829.98
Transfer/sec:    146.91MB
```
Deno in TinyKVM has around ~4us in context-switching and safety overhead per request. But has good scaling with forked VMs.

## React benchmark

Rendering a React page we get:
```sh
$ ./wrk -c1 -t1 http://127.0.0.1:8000
Running 10s test @ http://127.0.0.1:8000
  1 threads and 1 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   533.24us   22.99us   0.91ms   88.97%
    Req/Sec     1.88k     9.42     1.90k    68.00%
  18741 requests in 10.00s, 558.53MB read
Requests/sec:   1874.07
Transfer/sec:     55.85MB

$ ./wrk -c8 -t8 http://127.0.0.1:8000
Running 10s test @ http://127.0.0.1:8000
  8 threads and 8 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   589.57us   39.86us   1.26ms   81.93%
    Req/Sec     1.70k    60.49     1.81k    55.45%
  136925 requests in 10.10s, 3.99GB read
Requests/sec:  13556.71
Transfer/sec:    404.02MB
```
This scales well over many cores, as can be seen above. Producing 13.5k server-rendered pages per second in a sandbox is quite insane.
