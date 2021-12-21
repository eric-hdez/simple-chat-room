# simple-chat-room
An overly simplified chatroom in C that can be run on localhost. Tested on MacOS only, should work on linux.

## build
    $ make
    $ make all

## usage
    $ ./server [-p portnumber]
    $ ./client [-u username] [-p portnumber]

## clean
    $ make clean

## action items
    * try to connect clients over different IP addrs
    * clean up code
    * work out disconnections from server on client
    * encrypt messages (if used over different networks)
