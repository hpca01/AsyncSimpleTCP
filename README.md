## Simple TCP server
As title states, this is a simple TCP server written in C.



**To start project use the Makefile**

```
make
```


You will see
```
./server

~~~~~~~~~Waiting to accept a new conn~~~~~~~~

```
After which you can create a seperate terminal and invoke the client:
```
./client

```

When you're done and want to clean up the artifacts:
```
make clean
```

To-Do:

- [x] Write simple TCP server
- [x] Write client
- [x] Parse Headers
- [x] Assemble return with correct HTTP code.
- [x] Change main loop to be async using EPOLL.
- [ ] Write out client to do a load test.
- [ ] Valgrind Test on server after client is written
