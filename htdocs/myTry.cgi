#!/usr/bin/python3

import sys,os
# import cgi
# import cgitb

# cgi.test()
length = os.getenv('CONTENT_LENGTH')
# form = cgi.FieldStorage()
print("Content-type:text/html\n")
print("<html>")
if length :
    userdata = sys.stdin.read(int(length))
    print("<p>Your input is:" + userdata)
    # color = form.getvalue("color")
    # print("<p>We see your color is " + color)

print("<p>Hello, world!")
print("</html>")
