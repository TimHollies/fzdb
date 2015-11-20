[![Build Status](https://magnum.travis-ci.com/matann/fuzzy-database.svg?token=9y2FhEje8Gso8srsgnQj&branch=develop)](https://magnum.travis-ci.com/matann/fuzzy-database)

# fuzzy-database
A graph-based fuzzy data store.

## Changes to directory structure
All code for the database now goes in the `src` directory. Using sub-directories works fine and is a good idea to help keep the codebase
more managable. The build system will search the `src` directory recursively to find all *.c and *.cpp files.

The `build_modules` directory is for cmake files. Each third party library that we use should have a file in here which downloads/configures/builds
the depedency.

The `frontend` and `tools` directories have not changed usage.

All files generated by the build will be in a directory called `build` if you follow the instructions below.

## Building

It is best to do 'out of source builds'.

Create a directory called build `mkdir build` and set the working directory to that directory.
Then call `cmake ..`, this will download and build dependencies as well as configuring a build
system for the project. 

The system will attempt to automatically find a valid generator, but often fails, so it is good to manually provide the generator
that it should use. A full list is available at https://cmake.org/cmake/help/v3.0/manual/cmake-generators.7.html.

For Visual Studio 2015 the option is `cmake .. -G "Visual Studio 14"`. 

If the command complains that it must be set to DEBUG or release add the option `-DCMAKE_BUILD_TYPE=DEBUG`.

Once the configuration is complete you can either build the project using the generated build files or use the
command `cmake --build .` to get cmake to do that for you.

### Caveats 
RELEASE build mode is still pretty experimental but DEBUG builds should always work. 

Also note that a minor bug in BOOST_ASIO results in a load of warnings when you build the project on Windows. The warnings are fine, they 
don't actually break anything. This is known to the authors of the library and should be fixed in the next version.

## Running
The program starts a TCP server on port 1407. Use CTRL-C to kill the running server.