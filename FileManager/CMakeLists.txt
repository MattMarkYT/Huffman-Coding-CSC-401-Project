add_library(FileManager_incLib INTERFACE)
target_include_directories(FileManager_incLib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
file(GLOB_RECURSE FileManager_incLib_SOURCES CONFIGURE_DEPENDS
"${CMAKE_CURRENT_LIST_DIR}/src/huffman/*.cpp"
)

target_sources(FileManager_incLib INTERFACE ${FileManager_incLib_SOURCES})
add_executable(FileManager main.cpp)
target_link_libraries(FileManager PUBLIC FileManager_incLib)
