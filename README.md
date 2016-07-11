# RakNet RPC3 with C++14, without boost.

This is Boost-free version of RakNet's RPC3 plugin. Boost is replaced with features of standard C++ thanks to C++14.

Because C++11 introduced variadic templates, there is no limitation to the count of arguments.

This should work as drop-in replacement for your current RakNet project with the original RPC3.

Most of the code is unmodified biggest changes are in RPC3_STD.h, which replaces the old RPC3_Boost.h file.


Tested with GCC 5.3.1 and Clang 3.7.0 on Linux.


## Run the demo

Download RakNet to the root of this repository.

Go to demo folder and compile server and client:
```
./compile
```

Then run the server and client with (stop with Ctrl+C):
```
./run
```
