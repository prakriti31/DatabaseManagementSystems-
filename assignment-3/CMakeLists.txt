cmake_minimum_required(VERSION 3.29)
project(assignment_3 C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(assignment_3
        buffer_mgr.c
        buffer_mgr.h
        buffer_mgr_stat.c
        buffer_mgr_stat.h
        dberror.c
        dberror.h
        dt.h
        expr.c
        expr.h
        record_mgr.h
        rm_serializer.c
        storage_mgr.c
        storage_mgr.h
        tables.h
        test_assign3_1.c
        test_helper.h
        record_mgr.c
)

add_executable(cli
        buffer_mgr.c
        buffer_mgr.h
        buffer_mgr_stat.c
        buffer_mgr_stat.h
        dberror.c
        dberror.h
        dt.h
        expr.c
        expr.h
        record_mgr.h
        rm_serializer.c
        storage_mgr.c
        storage_mgr.h
        tables.h
        record_mgr.c
        cli.c
)
