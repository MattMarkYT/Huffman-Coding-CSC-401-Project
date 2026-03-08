add_library(Greedy_incLib INTERFACE)
target_include_directories(Greedy_incLib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
file(GLOB_RECURSE Greedy_incLib_SOURCES CONFIGURE_DEPENDS
"${CMAKE_CURRENT_LIST_DIR}/src/huffman/*.cpp"
)
target_sources(Greedy_incLib INTERFACE ${Greedy_incLib_SOURCES})
add_executable(Greedy main.cpp)
target_link_libraries(Greedy PUBLIC Greedy_incLib)
