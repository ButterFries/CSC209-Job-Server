# CSC209 - Job Server 

For the final project, the class was tasked to create a server which would allow clients to request the server to run certain C files
and return their results. This was completed through the use of forks, pipes, and dup2 in order to run the code and successfully return
the correct result. The compiled client and Makefiles were provided for the project.

## Getting Started

### Prerequisites

The application was designed for Ubuntu 18.10

### Installing

Step 1 - Open the repo folder in terminal and enter

```
make
```

### Running

(server)
Step 1 - Open the folder containing the repo in terminal and enter

Step 2 (optional) - To change the port, edit the PORT flag in Makefile

```
./jobserver
```

Step 3 - To stop the server enter CTRL+C


(client)
Step 1 - Open another terminal and enter

```
./jobclient 127.0.0.1 55555
```

Client commands:
```
run [JOBNAME]
jobs
kill [PID]
exit
```
Availible jobs are located in the jobs folder

## License

Copyright <2020>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Acknowledgments

* Thank You PurpleBooth for a README.md template: https://gist.github.com/PurpleBooth/109311bb0361f32d87a2

