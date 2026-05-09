# Huffman-Coding-CSC-401-Project

Project about exploring the algorithms behind huffman coding, for the CSC 401 class.

## CMake, configuration, Building

Project uses cmake for configuration and building, and uses presets to assist with this greatly.

Some IDE's, like Visual Studio, take the task of configuring and building the project on themselves. 

For VS specifically, this changes the whole process to selecting one of the build presets and using it as desired.

You can read on how to compile the project using command line in [BUILDING.md](./BUILDING.md)



If you want to configure/use a different preset that isn't in the list, compile and run the `preset_generator.cpp` to regenerate the list of presets.

If you want add another sub-project to this CMake project, compile and run the `create_cmake_project.cpp`

## Running the Test Suite

Our test suite is a basic command-line interface with a menu-driven console UI.  
After building the project, simply run `Main`.

```
Welcome to Huffman Test Suite

1. Run Manual Input Test
2. Run File Test (Refer to Input Folder)
3. Switch Mode (Currently using Naive)
4. Exit
```

In the build directory of the compiled project, create an `input` folder in the same directory as `Main` and place any files you would like to test inside it.

Option `3` acts as a toggle between our naive fixed-length encoding method and our greedy Huffman encoding method.

Option `2` runs batch tests on all files located in the `input` folder.

Option `1` allows you to manually test string input directly through the console.

The file test mode outputs compression statistics for the currently selected method across all files located in the `input` folder.

## Reproducing our experimental results

In the build directory of the compiled project, create an `input` folder in the same directory as `Main` and place our test files from the google drive into that folder.

Proceed with Option `2` to run the batch test on all the files.

Following the tests, refer to `output_greedy.csv` and `output_naive.csv` respectively.

