# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/isabella/knitogami2024/build_cmake/LIBIGL-src"
  "/Users/isabella/knitogami2024/build_cmake/LIBIGL-build"
  "/Users/isabella/knitogami2024/build_cmake/LIBIGL-download/LIBIGL-download-prefix"
  "/Users/isabella/knitogami2024/build_cmake/LIBIGL-download/LIBIGL-download-prefix/tmp"
  "/Users/isabella/knitogami2024/build_cmake/LIBIGL-download/LIBIGL-download-prefix/src/LIBIGL-download-stamp"
  "/Users/isabella/knitogami2024/build_cmake/LIBIGL-download/LIBIGL-download-prefix/src"
  "/Users/isabella/knitogami2024/build_cmake/LIBIGL-download/LIBIGL-download-prefix/src/LIBIGL-download-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/isabella/knitogami2024/build_cmake/LIBIGL-download/LIBIGL-download-prefix/src/LIBIGL-download-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/isabella/knitogami2024/build_cmake/LIBIGL-download/LIBIGL-download-prefix/src/LIBIGL-download-stamp${cfgdir}") # cfgdir has leading slash
endif()
