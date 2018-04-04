# tinyhttpd
a tiny http server for only GET request. \
It was implemented by thread pool and epoll. 

## detailed description


## usage
use `gcc -o httpd httpd.c -lpthread` to compile. \
use `./httpd <path> <port> &` to run. \
`<path>` is the path of http server. \
`<port>` is the port number to provide http service. `<port>` is usually 80. 

## known issues
httpd occupy nearly 100% CPU.

## TODO list:
1. Make every thread in thread pool have a buffer. Because buffer was initialized at the beginning of many function. That will degrade perfermance largely. 
2. Implement http download.
3. Add a test program to test the perfermance.
