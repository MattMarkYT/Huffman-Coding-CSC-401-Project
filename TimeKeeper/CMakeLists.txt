add_library(TimeKeeper_incLib INTERFACE)
target_include_directories(TimeKeeper_incLib INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
file(GLOB_RECURSE TimeKeeper_incLib_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/src/huffman/*.cpp"
)
target_sources(TimeKeeper_incLib INTERFACE ${TimeKeeper_incLib_SOURCES})
add_executable(TimeKeeper main.cpp)
target_link_libraries(TimeKeeper PUBLIC TimeKeeper_incLib)
