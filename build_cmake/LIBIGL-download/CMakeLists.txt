cmake_minimum_required(VERSION 2.8.2)

project(LIBIGL-download NONE)

include(ExternalProject)
ExternalProject_Add(LIBIGL-download
                    GIT_REPOSITORY;https://github.com/libigl/libigl.git;GIT_TAG;master;UPDATE_DISCONNECTED 1
                    SOURCE_DIR          "/Users/isabella/knitogami2024/build_cmake/LIBIGL-src"
                    BINARY_DIR          "/Users/isabella/knitogami2024/build_cmake/LIBIGL-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
)
