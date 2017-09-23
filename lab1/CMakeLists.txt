cmake_minimum_required(VERSION 3.6)
project(lab1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    lang/algorithm.h
    lang/verify.h
    rpc/connection.h
    rpc/fifo.h
    rpc/jsl_log.h
    rpc/marshall.h
    rpc/method_thread.h
    rpc/pollmgr.h
    rpc/rpc.h
    rpc/slock.h
    rpc/thr_pool.h
    extent_client.cc
    extent_client.h
    extent_protocol.h
    extent_server.cc
    extent_server.h
    GNUmakefile
    inode_manager.cc
    inode_manager.h
    lab1_tester.cc
    readme)

add_executable(lab1 ${SOURCE_FILES})