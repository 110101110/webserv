#!/usr/bin/env python3
import os
import sys
method = os.environ.get("REQUEST_METHOD", "GET")
query  = os.environ.get("QUERY_STRING", "")
content_type = os.environ.get("CONTENT_TYPE", "")
script_name  = os.environ.get("SCRIPT_NAME", "")

body_input = ""
if method == "POST":
    length = int(os.environ.get("CONTENT_LENGTH", 0))
    if length > 0:
        body_input = sys.stdin.read(length)

html = """<html>
<body>
<h1>CGI Test Script</h1>
<table border="1">
    <tr><th>Variable</th><th>Valeur</th></tr>
    <tr><td>REQUEST_METHOD</td><td>{method}</td></tr>
    <tr><td>QUERY_STRING</td><td>{query}</td></tr>
    <tr><td>CONTENT_TYPE</td><td>{content_type}</td></tr>
    <tr><td>SCRIPT_NAME</td><td>{script_name}</td></tr>
    <tr><td>BODY RECU</td><td>{body}</td></tr>
</table>
</body>
</html>""".format(
    method=method,
    query=query,
    content_type=content_type,
    script_name=script_name,
    body=body_input if body_input else "aucun"
)

sys.stdout.write("Content-Type: text/html\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(html)