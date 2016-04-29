[![Build Status](https://travis-ci.com/matann/fzdb.svg?token=GNPt1QoQ1MHs1MrnxbiE&branch=master)](https://travis-ci.com/matann/fzdb)
[![Gitter](https://img.shields.io/badge/GITTER-JOIN_CHAT_%E2%86%92-1dce73.svg)](https://gitter.im/matann/fuzzy-database)
[![Waffle](https://badge.waffle.io/matann/fuzzy-database.png)](https://waffle.io/matann/fuzzy-database)

# fzdb
A graph-based fuzzy data store.

## System tests
System tests are run using the 'jasmine' BDD framework. Run `npm install` to install required dependencies and then `npm test` to run the tests. Make sure the server is running before running the system tests :P

## Building

It is best to do 'out of source builds'. This means that all files generated by the build will be in the 
'build' directory, which is included in gitignore.

Create a directory called build `mkdir build` and set the working directory to that directory.
Then call `cmake ..`, this will download and build dependencies as well as configuring a build
system for the project. 

The system will attempt to automatically find a valid generator, but often fails, so it is good to manually provide the generator
that it should use. A full list is available at https://cmake.org/cmake/help/v3.0/manual/cmake-generators.7.html.

For Visual Studio 2015 the option is `cmake .. -G "Visual Studio 14"`. 

By default the system will build in debug mode.
If the command complains that it must be set to DEBUG or release add the option `-DCMAKE_BUILD_TYPE=DEBUG`.

Once the configuration is complete you can either build the project using the generated build files or use the
command `cmake --build .` to get cmake to do that for you.

### Release Mode
On Unix, just run the initial configuration with `-DCMAKE_BUILD_TYPE=RELEASE`.

On Windows run the initial configuration with `-DCMAKE_BUILD_TYPE=RELEASE` and then either select 'Release' in 
Visual Studio or run `cmake --build . --config Release` on the command line.

### Caveats 
A minor bug in BOOST_ASIO results in a load of warnings when you build the project on Windows. The warnings are fine, they 
don't actually break anything. This is known to the authors of the library and should be fixed in the next version.

## Running
The program starts a TCP server on port 1407. Use CTRL-C to kill the running server.
