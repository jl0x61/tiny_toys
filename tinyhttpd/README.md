# tinyhttpd
a tiny http server for only GET request. \
It was implemented by thread pool and epoll. \

\
\
TODO list:
1. Make every thread in thread pool have a buffer. Because buffer was initialized at the beginning of many function. That will degrade perfermance largely. 
2. To implement more request and improve the robustness.
