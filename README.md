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

Deno sandboxed in TinyKVM:
```sh
$ ./wrk -c1 -t1 http://127.00.1:8080
Running 10s test @ http://127.00.1:8080
  1 threads and 1 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    16.42us    3.03us 277.00us   92.06%
    Req/Sec    60.26k     4.81k   63.15k    91.09%
  605804 requests in 10.10s, 89.55MB read
Requests/sec:  59981.22
Transfer/sec:      8.87MB

$ ./wrk -c64 -t64 http://127.00.1:8080
Running 10s test @ http://127.00.1:8080
  64 threads and 64 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    69.96us   58.90us   5.53ms   97.43%
    Req/Sec    14.72k     3.48k   37.18k    75.40%
  9465008 requests in 10.10s, 1.37GB read
Requests/sec: 937194.37
Transfer/sec:    138.54MB
```
Deno in TinyKVM has around ~5us in context switching and safety overhead per request. But has good scaling with forked VMs.
