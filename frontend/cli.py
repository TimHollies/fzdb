#! /usr/bin/env python

# Thanks to http://stackoverflow.com/questions/4342697/tcp-port-using-python-how-to-forward-command-output-to-tcp-port
import socket
import sys

TCP_IP = 'localhost'
TCP_PORT = 1407 

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((TCP_IP, TCP_PORT))
while True:
	line = sys.stdin.readline()
	if line:
		s.send(line)
	else:
		break

s.close()
