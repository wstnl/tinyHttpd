# TinyHttp
## A socket programming proj by @wstnl
*notice: The author(maybe you could call me @youzi) is a very primary programmer and still in the progress of study, so if you find any bug in the program, feel free to create an issue!*
### Platform: Ubuntu 20.04
### Info
Based on the original version of [tinyHttpd](http://tinyhttpd.sourceforge.net/), I use the poll() function to solve the blocking problem caused by the recv() function, so that the web page can serve multiple clients at the same time.
To run the project, simply clone it and use the command `make`