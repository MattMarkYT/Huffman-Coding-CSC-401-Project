add_library(FileManager_incLib INTERFACE)



target_include_directories(FileManager_incLib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include/
)

target_compile_features(FileManager_incLib INTERFACE cxx_std_20)
file(GLOB_RECURSE FileManager_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)

target_sources(FileManager_incLib INTERFACE ${FileManager_HEADERS})



file(GLOB_RECURSE FileManager_incLib_SOURCES CONFIGURE_DEPENDS
"${CMAKE_CURRENT_LIST_DIR}/src/huffman/*.cpp"
)


target_sources(FileManager_incLib INTERFACE ${FileManager_incLib_SOURCES})

add_executable(FileManager main.cpp "fm_tests.hpp")
target_compile_features(FileManager PRIVATE cxx_std_20)
target_link_libraries(FileManager PUBLIC Naive_incLib)
target_link_libraries(FileManager PUBLIC Greedy_incLib)
target_link_libraries(FileManager PUBLIC FileManager_incLib)
