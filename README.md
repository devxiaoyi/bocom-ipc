# Bocom
Inter Process Communication (IPC) library, based on boost shared memory and mutex implementation

## Directory Introduction

* boost   --  the Boost library's header
* bocom   --  encapsulated interface
* test    --  the main function for test

## quick start

### clone

git clone --recursive git@github.com:devxiaoyi/bocom-ipc.git

### prepare boost

1. cd boost && ./bootstrap.sh 

2. ./b2 headers && cd ..

For your reference: https://github.com/boostorg/wiki/wiki/Getting-Started%3A-Overview
"""
NOTE: Following Boost modularization and move to Git, header files were moved to their corresponding libraries, into an include subdirectory.
For example, the header files of Boost.Filesystem are now in /libs/filesystem/include.
For compatibility, /boost sub-directory is now a "virtual" directory, containing links to the headers.
The command b2 headers creates or recreates the contents of this /boost directory.
"""

### build

1. if compile for arm platform, otherwise ignore this step
   export CC=arm-linux-gnu-gcc
   export CXX=arm-linux-gnu-g++

2. mkdir build && cd build

3. cmake .. && make

4. run

    'main_pub' is a published program

    'main_rec' is a program to fetch data
