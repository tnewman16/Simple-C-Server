# Simple-C-Server
This project was created in the Fall 2016 semester for the CSCI 340: Operating Systems course at the College of Charleston.

This program was made to run in a Linux environment. In order to run the server on your local machine, simply cd into the folder, type "make" followed by "make run". This will start the server on localhost port 8080. You can connect through a browser by navigating to "http://localhost:8080/".

This server uses a thread pool with 8 threads to manage incoming requests, and provides a very simple display to show the IP addresses and timestamps of both requests and responses, along with which thread handled the request. Currently there is only one HTML page (index.html).
