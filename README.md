# HttpSrv

| Linux  | [![Linux Build](https://api.travis-ci.org/eantcal/httpsrv.svg?branch=master)](https://travis-ci.org/eantcal/httpsrv)  |

HttpSrv is a retailed version of a lightweight HTTP server and derives from [thttpd](https://github.com/eantcal/thttpd), originally designed to implement HTTP GET/HEAD methods.
It has been implemented in modern C++ and portable on several platforms including Linux, MacOs and Windows.

HttpSrv is capable to serve multiple clients supporting GET and POST methods and has been designed to be a standalone application containing an embedded web server which exposes the following HTTP API for storing and retrieving text files and their associated metadata:
* `POST` `some_file.txt` to `/store`: returns a JSON payload with file metadata containing name, size (in bytes), request timestamp and an auto-generated ID
* `GET` `/files`: returns a JSON payload containing an array of files metadata containing file name, size (in bytes), timestamp and ID
* `GET` `/files/{id}`: returns a JSON payload with file metadata containing name, size (in bytes), timestamp and ID for the provided ID `id`
* `GET` `/files/{id}/zip`: returns a zip archive containing the file which corresponds to the provided ID `id`
* `GET` `/mrufiles`: returns a JSON payload with an array of files metadata containing file name, size (in bytes), timestamp and ID for the top `N` most recently accessed files via the `/files/{id}` and `/files/{id}/zip` endpoints. `N` should be a configurable parameter for this application.
* `GET` `/mrufiles/zip`: returns a zip archive containing the top `N` most recently accessed files via the `/files/{id}` and `/files/{id}/zip` endpoints. `N` should be a configurable parameter for this application.

## HttpSrv educational purpose

This is a very simple application can be used for educational purposes. 
You can just copy and modify it as you desire and reusing part of source code according the limitation of license (see COPYING file).

## HttpSrv architecture

HttpSrv is a console application (which can be easily daemonized via external command like [daemonize](http://manpages.ubuntu.com/manpages/eoan/man1/daemonize.1.html), if required).
Its main thread executes the main HTTP server loop. Each client request, a separate HTTP session, is then handled in a specific thread context. Once the session terminates, the thread and related resources are freed.

When a client POSTs a new file, that file is stored in a specific `repository` (which is basically a dedicated directory).
`File metadata` is always generated by reading repository stored file attributes, while an id-to-filename thread-safe map (`FilenameMap` instance) is shared among the HTTP sessions (threads), and used to retreive the `filename` for a given `id`. The `id` is generated by a hash code function (which executes the SHA256 algorithm).
When a zip archive is required, that is generated on-the-fly in a temporary directory and sent to the client. Eventually the temporary directory and its content is cleaned up.

### Configuration and start-up

HttpSrv entry point, the application `main()` function, creates a `class Application` object which is in turn a builder for classes `FileRepository` and `HttpServer`:

* `FileRepository` provides methods for:
  * accessing the filesystem,
  * creating the repository directory,
  * managing the temporary directories required for sending the zip archives,
  * creating zip archives,
  * creating and handling the `FilenameMap` object
  * formatting the JSON metadata
* `HttpServer` implements the HTTP server main loop:
  * accepting a new TCP connection and
  * finally creating HTTP sessions (`HTTPSession`) where the API business logic is implemented.

The file repository directory (if not already existent) is created in the context `FileRepository` initialization.
The repository directory path can be configured at start-up. By default, it is a subdir of user home directory named `“.httpsrv”` for ‘unix’ platforms, `“httpsrv”` for Windows.

When the initialization is completed, the application method `run()` calls the method `run()` of `HttpServer`. Such method is blocking for the caller, so unless the application is executed as background process or daemon, it will block the caller process (typically the shell).
The server is designed to bind on any interfaces and a specific and configurable TCP port (which is `8080`, by default).

### Main server loop

The method `HttpServer::run()` executes a loop that, for each iteration, waits for incoming connection by calling `accept()` method, that will eventually result in calling `accept()` function of socket library.
When `accept()` accepts a connection, `run()` gets a new `TcpSocket` handle (a smart pointer to the actual object), which represents the TCP session, and creates a new HTTP session handled by a dedicated instance of class `HttpSession`.
Each HTTP session will run in a separate thread, so multiple requests can be served concurrently.

### Concurrent operations

* Concurrent `GET` operations not altering the timestamp can be executed without any conflicts.
* Concurrent `POST`, `GET/files/<id>` and `GET/files/<id>/zip` for the **same** files might produce a JSON metadata which does not reflect -- for some concurrent clients -- the actual repository status. This is what commonly happens in a shared and unlocked unix filesystem that implements an optimistic non-locking policy. While running on locked filesystem which implements an exclusive access policy, one of the concurrent operation might fail generating an error to the client.
Zip file genaration also for the same files can be done cuncurrently: the zip file is created in a unique `temporary-directory` whose name is generated randomically, so its integrity is always preserved.
In absence of specific requirements it has been decided not to implement a strictly F/S locking mechanism, adopting indeed an optimistic policy. The rationale is to maintain the design simple, also considering the remote chance of conflicts and their negligible effects.
The file `id` is a hash code (SHA256) of file name (which is in turn assumed to be unique), so any conflicts would have no impact on its validity, moreover the class `FilenameMap` (which provides the methods to resolve the filename for a given id) is designed to be thread-safe (r/w locking mechanism is used for the purposes). So the design should avoid that concurrent requests resulted in server crash or asymptotic instability.

### Resiliency

The application is also designed to be recovered from intentional or unintentional restart.
For such reason, HttpSrv updates the `FilenameMap` object at start-up (reading the repository files list), as the http requests are not accepted yet, getting the status of any files present in the configured repository path. This allows the server to restart from a given repository state.

Multiple instances of HttpSrv could be concurrently executed on the same system, binding on separate ports. In case they share the same repository, it is not guaranteed that a file posted from a server can be visible to another server (until such server executes a request for file list or is restarted) because the `FilenameMap` object would be stored in each (isolated) process memory.

### More details about `GET` and `POST` processing

`GET` and `POST` requests are processed within a `HTTPSession` loop which consists in:

* reading receiving and parsing an HTTP request (`HttpRequest`)
* validating and classifieng the request
* executing the business logic related to the request (GET/POST)
* creating a response header + a body (`HTTPResponse`)
* cleaning-up resourses (deleting for example the temporary directory of a zip file sent to the client) and ending the session

#### POST

When a file is uploaded, `POST` request is handled as following:

* (in absence of errors) writes the file in the repository;
* updating id-filename map (`FilenameMap`);
* generates a JSON file metadata from stored file attribute;
* replies to the client either sending back a JSON metadata or HTTP/HTML error response depending on success or failure of one of previous steps.

#### GET

When `GET` request is processed the business logic performs the following action:

* `/files`: formats a JSON formatted body containing a list of metadata corrisponding to file attributes read from repository;
* `/mrufiles`: likewise in `/file`, but the metadata list is generated by a timeordered list and limited to max number of mru files configured (3, by default)
* `/files/<id>`:
  * resolves the id via `FilenameMap` object,
  * updates the file timestamp,
  * reads the file attributes,
  * writes in the HTTP response body the JSON metadata reppresenting the file attributes
* `/files/<id>/zip`:
  * resolves the id via `FilenameMap` object,
  * updates the file timestamp,
  * reads the file attributes,
  * creates a zip archive containing the file in a unique temporary directory
  * writes the zip binary in the HTTP response body
  * cleans up the temporary directory
* `/mrufiles/zip`:
  * creates a list of mru files
  * adds each file in a new zip archive stored in unique temporary directory
  * writes the zip binary in the HTTP response body
  * cleans up the temporary directory

### HTTP Errors

HttpSrv notifies errors to a client by using a standard HTTP error code and a related error description, formatted in HTML body.
Empty repository or zero file size is not considered an error. In the first scenario just an empty JSON list `[]` will be sent back by server on both `GET` `/files` and `GET` `/mrufiles` valid requests.
If the URI does not respect the given syntax, an `HTTP 400 Bad Request` error will be sent to the client.
If the URI is valid but the `id` not found, an `HTTP 404 Not Found` error will be sent to the client.
Building a `release` version of HttpSrv binary strips out `assert()` calls, so in case of bugs, hardware failures or resources (e.g. memory) exhausted, `HTTP 500 Internal Server Error` might be sent to the client.

### Summary of HttpSrv classes and functions

#### HttpServer Management

* Class `HttpServer` accepts client request and generates HttpSession in separate worker thread
* Class `HttpSession` handles the single GET/POST request and executes the related business logic
* Class `HttpSocket` provides metadata extractor for HTTP message
* Class `HttpRequest` encapsulates an HTTP request providing a parser for supported request message.
* Class `HttpResponse` encapsulates an HTTP response providing a formatter for supported response message
* Class `TransportSocket` and `TcpSocket` classes expose basic socket functions including `send/recv` APIs
* Class `TcpListener` provides a wrapper of some passive TCP functions such as `listen` and `accept`.

#### Repository Management

* Class `FileRepository` provides the support for handlig the files, reading attributes, building MRU list, formatting the JSON metadata
* Class `FilenameMap` provides id to file name resolver
* Class `ZipArchive` provides a wrapper for zip functions

#### Additional Helper functions

* Functions in namepace `FileUtils` provide some f/s helpers
* Functions in namepace `StrUtils` provide some string manipulation helpers
* Functions in namepace `SysUtils` provide some helpers not belonging to previous two categories

#### 3pp Libraries

HttpSrv relies on C++ standard library (which is part of language) and other few 3pp part libraries such as:

* [PicoSHA2](https://github.com/okdshin/PicoSHA2), single header file SHA256 hash generator
* [zip](https://github.com/kuba--/zip), a portable simple zip library written in C

Source code of such libraries has been copied in HttpSrv source tree in 3pp subdir.
Related source code has been directly listed as part of src/include reference in the CMakeLists.txt and VS project file.

Wrapper function/class for such libraries have been provided:

* Class `ZipArchive` is a wrapper on employed `zip` functions
* `hashCode()` function part of `FileUtils.h` is a wrapper for `picosha2::hash256_hex_string` function

## Known Limitations

* HTTP protocol has been supported only for providing the specific API exposed.
* Timestamp precision of some filesystem implementation might not support the microseconds field as result two files will have different timestamps if they differ at least for 1 second and the metadata `timestamp` field can result rounded to the second.

## Build Instructions

### C++ Compiler Prerequisites

To compile HttpSrv you will need a compiler supporting modern C++. As for example GCC/G++ 8.3, Microsoft Visual C++ 2019, (Apple) Clang V.11.

`CMakeLists.txt` and [Visual Studio 2019 C++ project](https://github.com/MicrosoftDocs/cpp-docs/blob/master/docs/build/creating-and-managing-visual-cpp-projects.md) project file (`httpsrv.vcxproj`) and solution file (`httpsrv.sln`) have been provided.

### CMake

To build HttpSrv from source directory using CMake just type

```console
$ mkdir build
$ cd build
$ cmake ..
$ make
```

As result, a binary file named `httpsrv` will be generated.

For further build instructions, see the blog post [How to Build a CMake-Based Project](http://preshing.com/20170511/how-to-build-a-cmake-based-project).

### Compile and build in Visual Studio

See [Compile and build in Visual Studio](https://docs.microsoft.com/en-us/cpp/build/projects-and-build-systems-cpp?view=vs-2019).


### HttpSrv Usage

HttpSrv accept optional paramters, such as the following:

```
Usage:
	./httpsrv
		-p | --port <port>
			Bind server to a TCP port number (default is 8080)
		-n | --mrufiles <N>
			MRU Files N (default is 3)
		-w | --storedir <repository-path>
			Set a repository directory (default is ~/.httpsrv)
		-vv | --verbose
			Enable logging on stderr
		-v | --version
			Show software version
		-h | --help
			Show this help

```

### Windows Execution Prerequisites

To run successfully HttpSrv the following software component is required on the installation computer:

* Visual C++ Redistributable Packages are required

## Tests

*Functional tests* have been implemented as [BASH script](test/functional_test.sh) in order to verify the main use scenarios.
It relies on a number of well-known 3pp commands/tools including `grep`, `awk`, `sed`, `unzip`, `curl`, `jsonlint`, `sha256sum`.
To execute the functional tests `httpsrv` program must be running (by default bound on localhost:8080).
The script accepts as an optional parameter in the format `hostname:port` (same syntax of `curl`) to override the default setting.
The test shows a detailed log during the execution.
If the test completes sucessfully it prints out a summary as shown in this [misc/example_of_positive_test_result.txt](misc/example_of_positive_test_result.txt)
In case of error the test stops showing a related error message.

### Tested Platforms

The server has been built and tested on Linux Ubuntu, MacOS and Windows, more precisely it has been tested on:

* `Darwin (18.7.0 Darwin Kernel Version 18.7.0) on MacBook Pro, built using Apple clang version 11.0.0 (clang-1100.0.20.17), Target: x86_64-apple-darwin18.7.0, Thread model: posix, CMake version 3.12.2`
* `Linux 5.0.0-38-generic #41-Ubuntu SMP Tue Dec 3 00:27:35 UTC 2019 x86_64 GNU/Linux, built using g++ (Ubuntu 8.3.0-6ubuntu1) 8.3.0, CMake version 3.13.4`
* `Microsoft Windows [Version 10.0.18363.535], Visual Studio 2019 (Version 16.4.2)`

Valgrind 3.15.0 on Ubuntu has been used to check for issues and memory leaks.

### Log files

HttpSrv can generate debugging log information on the standard output.
An example of log output is shown here [misc/example_of_server_log.txt](misc/example_of_server_log.txt)

### Example on how to run tests on Ubuntu

* Make sure your environment is configured to build and test the application, on Ubuntu you can run the following command:

```console
$ sudo apt install build-essential g++ cmake jsonlint curl unzip
```

* Then, build the application, so from project directory execute the command:

```console
$ rm -rf build && mkdir build && cmake .. && make
$ cd -
```

* Run HttpSrv in background (enabling logging on a file) 

```console
$ ./build/httpsrv -vv > /tmp/httpsrv.log &
```

* Check if the log file contains something like in the following example

```console
$ tail /tmp/httpsrv.log 

Sun Jan 12 20:35:03 2020 GMT
Command line :'./build/httpsrv -vv'
httpsrv is listening on TCP port 8080
Working directory is '~/.httpsrv'

```

* Finally run the tests *from test directory*

```console
$ cd test && ./functional_test.sh
```

When the test completes you will see on the screen a related [report](misc/example_of_positive_test_result.txt)

### build_and_run_all_test.sh

You may also use the bash script [build_and_run_all_tests.sh](build_and_run_all_tests.sh) which basically executes the previous steps.

## License

HttpSrv (c) antonino.calderone@gmail.com - 2020

HttpSrv can be distributed under MIT. See also [COPYING](COPYING).
See 3pp related license files for futher information
