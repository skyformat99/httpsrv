# httpsrv
httpsrv is a retailed version of a lightweight HTTP server and derives from thttpd (https://github.com/eantcal/thttpd), originally implemented to serve http GET/HEAD methods. 

![thttpd](https://github.com/eantcal/thttpd/blob/master/pics/tinyhttp1.png)

httpsrv has been implemented in modern C++, which means it requires a C++14 or even C++17 compiler to be successfully built.
It has been designed to run on Linux, but it can also run on MacOS or other Unix/Posix platforms other than Windows. 

httpsrv is capable to serve multiple clients supporting GET and POST methods and has been designed to respond to the following specifications:

* standalone application containing an embedded web server which exposes the following HTTP API for storing and retrieving text files and their associated metadata:
* `POST` `some_file.txt` to `/store`: returns a JSON payload with file metadata containing name, size (in bytes), request timestamp and an auto-generated ID
* `GET` `/files`: returns a JSON payload containing an array of files metadata containing file name, size (in bytes), timestamp and ID
* `GET` `/files/{id}`: returns a JSON payload with file metadata containing name, size (in bytes), timestamp and ID for the provided ID `id`
* `GET` `/files/{id}/zip`: returns a zip archive containing the file which corresponds to the provided ID `id`
* `GET` `/mrufiles`: returns a JSON payload with an array of files metadata containing file name, size (in bytes), timestamp and ID for the top `N` most recently accessed files via the `/files/{id}` and `/files/{id}/zip` endpoints. `N` should be a configurable parameter for this application.
* `GET` `/mrufiles/zip`: returns a zip archive containing the top `N` most recently accessed files via the `/files/{id}` and `/files/{id}/zip` endpoints. `N` should be a configurable parameter for this application.

## httpsrv architecture
httpsrv is a console application (which can be easily daemonized via external command like daemonize, if required).
Its main thread executes the main HTTP server loop. Each client request, a separate HTTP session, is then handled in a specific thread. Once the session completes, the thread and related resources are freed. 

When a client POST a new file, that file is stored in a specific `repository` (which is basically a dedicated directory). 
File metadata is always generated by reading the file attributes, while a id-to-filename thread-safe map (`FilenameMap`) is shared among the HTTP sessions (threads), and used to retreive the filename for a given id. The `id` is generated by an hash code function (SHA256).
When a zip archive is required, that is generated on-the-fly in a temporary directory and sent to the client. Eventually the temporary directory and its content is cleaned up.

### Configuration and start-up
httpsrv entry point, the application `main()` function, creates a `class Application` object which is in turn a builder for classes `FileRepository` and `HttpServer`:
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
    
The file repository is created in the context `FileRepository` initialization - if not already existent. 
The repository directory path can be configured. By default it is a subdir of user home directory named `“.httpsrv”` for ‘unix’ platforms, `“httpsrv”` for Windows.

When the initialization is completed, the application object calls the method `run()` of `HttpServer` object. Such method is blocking for the caller, so unless the application is executed as background process or daemon, it will block the caller process (typically the shell).
The server is designed to bind on any interfaces and a specific and configurable TCP port (which is `8080`, by default).

## Main server loop
The method `HttpServer::run()` executes a loop that, for each iteration, waits for incoming connection by calling `accept()` method which will eventually result in calling the `accept()` function of socket library. 
When `accept()` accepts a connection, `run()` gets a new `TcpSocket` object, which represents the TCP session, and creates a new HTTP Session handled by instance of class `HttpSession`. 
Each HTTP Session runs in a separate thread so multiple requests can be served concurrently.

### Concurrent operations by multiple clients
* Concurrent GET operations not altering the timestamp can be executed without any conflicts.
* Concurrent POST or GET (id or id/zip) for the **same** files might produce a JSON metadata which does not reflect - for some concurrent clients - the actual repository status. This is what commonly happens in a shared and unlocked unix filesystem that implements a optimistic non-locking policy. While running on locked filesystem which implements an exclusive access policy, one of the cuncurrent operation might fail generating an error to the client (typically `500 Internal Error Server`). But this is a very corner-case scenario which is quite unlikely to happen.
Get operations which require a zip file can be executed cuncurrently without conflicts: the zip file is created in a unique `temporary-directory` whose name is generated randomically, so its integrity is always preserved.
In absence of specific requirements it has been decided not to implement a strictly F/S locking mechanism, adopting indeed an optimistic policy. The rational is to maintain the design simple, also considering the remote chance of conflicts and their negligible effects on metadata representation.
The file `id` is a hash code of file name (which is in turn assumed to be unique), so any conflicts would have no impact on its validity, moreover the class `FilenameMap` (which provides the methods to resolve the filename for a given id) is designed to be thread-safe (a r/w locking mechanism is used for the purposes). So the design should avoid that concurrent requests resulted in server crash or asymptotic instability.

## Resiliency
The application is also designed to be recover from intentional or unintentional restart.
For such reason, httpsrv updates the `FilenameMap` object at start-up (reading the repository files list), as the http requests are not accepted yet, getting the status of any files present in the configured repository path. This allows the server to restart from a given repository state. 

Multiple instances of httpsrv could be run concurrently on the same system, binding on separate ports. In case they share the same repository, it is not guaranteed that a file posted from a server can be visible to another server (until such server executes a request for file list or is restarted) because the `FilenameMap` object would be stored in each (isolated) process memory.

GET and POST requests are processed within a HTTPSession loop. 
`HTTPSession` loop consists in:
* reading receiving and parsing an http request (`HttpRequest`)
* validating and classifieng the request
* executing the business logic related to the request (GET/POST)
* creating a response header + a body
* clean-up resourses and ending the session

### POST
When a file is uploaded, the POST business logic consists in:
* (in absence of errors) writing the file in the repository;
* updating a id-filename map;
* generates a JSON file metadata reading the file attribute;
* reply to the client (either sending back a JSON metadata or HTTP/HTML error response). 

### GET
When a get request is processed the business logic, for each request, does the following action:
* /files: format a JSON formatted body containing a list of metadata corrisponding to file attributes read by repository
* /mrufiles: likewise in `/file`, but the metadata list is generated by a timeordered map and limited to max number of mrufiles configured (3 by default)
* /files/<id>: 
   ** resolve the id via `FilenameMap` object, 
   ** update the file timestamp, 
   ** read the file attributes, 
   ** write in the HTTP response body the JSON metadata
* /files/<id>/zip: 
   ** resolve the id via `FilenameMap` object, 
   ** update the file timestamp, 
   ** read the file attributes,
   ** create a zip archive containing the file in a unique temporary directory
   ** write the zip binary in the HTTP response body 
   ** clean up the temporary directory
* /mrufiles/zip:
   ** create a list of mru files
   ** add each file in a new zip archive stored in unique temporary directory
   ** write the zip binary in the HTTP response body 
   ** clean up the temporary directory
 
### HTTP Errors
httpsrv notifies errors by using a standard HTTP error code and a small error description formatted in HTML.
Empty repository or zero file size is not considered an error. In the first scenario just an empty JSON list `[]` will be send back by server on both `GET` `/files` and `GET` `/mrufiles` valid requests.
If the URI does not respect the given syntax an `HTTP 400 Bad Request` error will be sent to the client. 
If the URI does is valid but the id not found an `HTTP 404 Not Found` error will be sent to the client.
The release version of httpsrv strips out the asserts, so in case of bugs or hardware failures or resources (e. g. memory) exhausted might generate an `HTTP 500 Internal Server Error`.

## Todo
Low level communications is provided by socket library (WinSocket on Windows), wrapped around following classes:
- TransportSocket and TcpSocket classes expose basic socket functions including send/recv APIs.
- TcpListener provides the interface for passive TCP functions such a listen and accept.

## 3pp
The server relies on C++ standard library (which is part of language) and other few 3pp part libraries such as:
- PicoSHA2 - https://github.com/okdshin/PicoSHA2 (SHA256 hash generator)
- zip - https://github.com/kuba--/zip (a portable simple zip library written in C)
- Boost Filesystem Library Version 3 - (https://www.boost.org/doc/libs/1_67_0/libs/filesystem/doc/index.htm)

PicoSHA2 is a simple alternative to OpenSSL implementation.
Zip is quite simple and portable as well. 
I have embedded the repositories content of such libraries in httpsrv source tree (in 3pp subdir) and included the related source code as part of project, which is the simplest  way to embed them.
Wrapper function/class for such libraries have been provided:
- ZipArchive (ZipArchive.h) is a wrapper class around zip functions
- function hashCode exported by FileUtils is a wrapper around picosha2::hash256_hex_string function
Boost Filesystem can be replaced by standard version if fully supported by C++ compiler ( defining USE_STD_FS).

## Limitations
HTTP protocol has been supported only for providing the specific API exposed.


## Tests (TODO)
Tested on the following platforms:
CMake support and Visual Studio 2019 project files are provided.
The server has been built and tested on Linux, MacOS and Windows, more precisely it has been tested on:
- Darwin (18.7.0 Darwin Kernel Version 18.7.0) on MacBook Pro, built using Apple clang version 11.0.0 (clang-1100.0.20.17), Target: x86_64-apple-darwin18.7.0, Thread model: posix, cmake version 3.12.2
- Linux 5.0.0-38-generic #41-Ubuntu SMP Tue Dec 3 00:27:35 UTC 2019 x86_64 x86_64 x86_64 GNU/Linux, built using g++ (Ubuntu 8.3.0-6ubuntu1) 8.3.0, cmake version 3.13.4
- Windows 10 built using Visual Studio 2019

