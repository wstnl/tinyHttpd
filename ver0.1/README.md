# TinyHttp ver 0.1
## A socket programming proj by @wstnl
*notice: The author(maybe you could call me @youzi) is a very primary programmer and still in the progress of study, so if you find any bug in the program, feel free to create an issue!*
### Info
Based on the original version of [tinyHttpd](http://tinyhttpd.sourceforge.net/), I use the poll() function to solve the blocking problem caused by the recv() function, so that the web page can serve multiple clients at the same time.
To run the project, simply clone it and use the command `make`, and then `./httpd` to run the server.
Use your browser to visit `localhost:23333`, the home page will show up.