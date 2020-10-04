#! /bin/sh

kill -9 $(pidof ./server)
kill -9 $(pidof /vagrant/alg/key-value-storage-approach-1/server)
kill -9 $(pidof "[server] <defunct>")
