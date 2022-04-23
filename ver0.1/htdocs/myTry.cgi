#!/usr/bin/python3

import sys,os
# import cgi
# import cgitb

# cgitb.enable()

# cgi.test()
length = os.getenv('CONTENT_LENGTH')
# form = cgi.FieldStorage()
print("Content-type:text/html\n")
print("<html>")
print("<head>")
print("<title>My Python script</title>")
print("</head>")

if length :
    userdata = sys.stdin.read(int(length))
    index = userdata.find("value=")
    if index >= 0 :
        print("<p>Your input is: " + userdata[index + 6:])
    else :
        print("<p>Input data broken")
    # color = form.getvalue("color")
    # print("<p>We see your color is " + color)

print("<p>Hello, world!")
print("</html>")
