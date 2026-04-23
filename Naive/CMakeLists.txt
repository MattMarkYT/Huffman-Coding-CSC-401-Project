add_library(Naive_incLib INTERFACE)
target_include_directories(Naive_incLib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
file(GLOB_RECURSE Naive_incLib_SOURCES CONFIGURE_DEPENDS
"${CMAKE_CURRENT_LIST_DIR}/src/huffman/*.cpp"
)

target_compile_features(Naive_incLib INTERFACE cxx_std_20)
file(GLOB_RECURSE Naive_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)

target_sources(Naive_incLib INTERFACE ${Naive_HEADERS})

target_sources(Naive_incLib INTERFACE ${Naive_incLib_SOURCES})
add_executable(Naive main.cpp)
target_link_libraries(Naive PUBLIC Naive_incLib)
