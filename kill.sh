#! /bin/sh

kill -9 $(pidof ./server) 2>/dev/null
kill -9 $(pidof /vagrant/alg/key-value-storage-approach-1/server) 2>/dev/null
kill -9 $(pidof "[server] <defunct>") 2>/dev/null
