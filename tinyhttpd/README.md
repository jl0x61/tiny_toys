# tinyhttpd
a tiny http server for only GET request. \
It was implemented by thread pool and epoll. 


## usage
use `gcc -o httpd httpd.c -lpthread` to compile. \
use `./httpd <path> <port> &` to run. \
`<path>` is the path of http server. \
`<port>` is the port number to provide http service. `<port>` is usually 80. 

## known issues
1. httpd occupy nearly 100% CPU.
2. httpd will killed by SIGSEGV when many connection in a short time.

## TODO list:
1. Make every thread in thread pool have a buffer. Because buffer was initialized at the beginning of many function. That will degrade perfermance largely. (DONE, use thread-specific data to solve it, every buffer will be initialized only once)
2. Implement http download.
3. Add a test program to test the perfermance.
