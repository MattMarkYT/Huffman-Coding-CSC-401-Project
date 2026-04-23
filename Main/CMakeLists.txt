add_library(Main_incLib INTERFACE)
target_include_directories(Main_incLib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
file(GLOB_RECURSE Main_incLib_SOURCES CONFIGURE_DEPENDS
"${CMAKE_CURRENT_LIST_DIR}/src/huffman/*.cpp"
)

target_compile_features(Main_incLib INTERFACE cxx_std_20)
file(GLOB_RECURSE Main_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)

target_sources(Main_incLib INTERFACE ${Main_HEADERS})

target_sources(Main_incLib INTERFACE ${Main_incLib_SOURCES})
add_executable(Main main.cpp)
target_link_libraries(Main PUBLIC Main_incLib)

target_link_libraries(Main PUBLIC Naive_incLib)
target_link_libraries(Main PUBLIC Greedy_incLib)
target_link_libraries(Main PUBLIC TimeKeeper_incLib)

target_link_libraries(Main PUBLIC FileManager_incLib)