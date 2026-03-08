# Huffman-Coding-CSC-401-Project

Project about exploring the algorithms behind huffman coding, for the CSC 401 class.

## CMake, configuration, Building

Project uses cmake for configuration and building, and uses presets to assist with this greatly.

Some IDE's, like Visual Studio, take the task of configuring and building the project on themselves. 

For VS specifically, this changes the whole process to selecting one of the build presets and using it as desired.

You can read on how to compile the project using command line in [BUILDING.md](./BUILDING.md)



If you want to configure/use a different preset that isn't in the list, compile and run the `preset_generator.cpp` to regenerate the list of presets.

If you want add another sub-project to this CMake project, compile and run the `create_cmake_project.cpp`