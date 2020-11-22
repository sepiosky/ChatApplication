#Chat application
This repository contains an impelmentation of a client-server chat application using socket programming in C. server runs on a port and clients can connect to server and chat in private mode or in group mode.
__For full specifications of project you can read `Course Project 1.pdf`__

## How to run
run server in a terminal window:
```bash
./server [listening_port]
```
now you can see logs of user activities


for each client in a seperate terminal window run:
```bash
./client [server_listening_port]
```
the client shows available commands in each step
