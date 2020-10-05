import hashlib
import random
import socket
import string

def readuntil(f, delim=':'):
	buf = ''
	while not buf.endswith(delim):
		buf += f.read(1)
	return buf

s = socket.create_connection(("localhost", 1337))
f = s.makefile('rw', bufsize=0)

for i in range(100000):
	key = ''.join(random.choice(string.ascii_uppercase + string.lowercase + string.digits) for _ in range(10))
	val = hashlib.sha224(key).hexdigest()

	# do SET operation
	f.write('SET {} {}'.format(key, val) + chr(0x0a))

	# thrash the result
	readuntil(f, chr(0x0a))

	# do GET operation
	f.write('GET {}'.format(key) + chr(0x0a))

	# log the result
	buf = readuntil(f, chr(0x0a)).rstrip(chr(0x0a))
	print(buf)

# close the socket
s.close()
