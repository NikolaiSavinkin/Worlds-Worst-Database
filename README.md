# C Playground
This is a test application in C to help familiarise myself with the language as well as common system calls that make up the application layer of many networked systems.

## The Application
This test application is a networked data store where a client takes in commands to add, remove, and view data and sends it over a simple network protocol.
The server manages the tcp connections of multiple clients and processes them synchronously.

## Improvements and To Do
There are some TODOs in the source, but since I am only using this project as a playground to learn more about C, I am mainly concerned with trying out different things, not making a complete and feature rich tool.
My next focus will probably be on all the issues Valgrind has found.
I'm also debating if I prefer the more dynamic approach to memory allocation that I used for the client pfds or the static approach used in the protocol buffer.
It might be interesting to see how easy it is to migrate one approach to the other.
I'm also probably going to try and add multithreading or libevent or something.
So far, I have had almost no concern for performance so it might be interesting to stress test this with many client requests at a time and profile it.
