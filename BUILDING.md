## Configuration

You can configure the project using the following command:
```
cmake --preset <compiler>-<architecture>-<build type>-<optional compiler-exclusive tools>
```

You can see all available configuration presets using the following command:
```
cmake --list-presets
```

The included presets are all the permutations of supported compiler, architecture, and build types for easier development.

Supported compilers: 

* `msvc` (`cl.exe`) (windows-only)
* `clang` (`clang` and `clang++`)
* `gcc` (`gcc` and `g++`).

Note: the chosen compiler must be accessible form PATH for this to work.

Supported architecture: 
* `x86`: 32-bit
* `x64`: 64-bit
* `native`: no specied architecture, compiler default. NOTE: this may fail on some systems, if default compiler and linker have mismatched architecture.

Supported build type: 

* `debug`
* `release`

Supported compiler-exclusive tools:

* `analyzer` to enable GCC-specific static analyser, only for **debug** builds (`-fanalyzer`).
* `tuned` to enable CLang/GCC-specific native architecture tuning, only for **release** builds (`-march=native -mtune=native`)


If you want to configure/use a different preset that isn't in the list, compile and run the `preset_generator.cpp` to regenerate the list of presets.

If you want add another sub-project to this CMake project, compile and run the `create_cmake_project.cpp`


#### Quick configuration commands:

```
cmake --preset msvc-native-debug
cmake --preset clang-native-debug
cmake --preset gccc-native-debug
```

## Building

Building the project is similar to configuration, as presets add same-named build presets.

To build, you need to configure the project using the presets above.

You can build the project, fully, using the following command:

```
cmake --build --preset <the name of the preset>
```

To build a specific project/target:

```
cmake --build --preset <the name of the preset> --target <name of the target>
```

You can get a list of targets using the following command:
```
cmake --build --preset <the name of the preset> --target help
```

Sub-project targets for this project:
* `Naive`
* `Greedy`
* `Main` (depends on `Naive` and `Greedy`)

Usual cmake utility targets:

* `all` -- default if nothing was specified, builds all sub-projects. If code wasn't changed after last successful build, doesn't do anything.
* `clean` -- deletes the results of building (object files, executables, .lib, etc)

To achieve the re-build functionality (forcefully re-compile and build the project/target from scratch), add `--clean-first` to the cmake build command

#### Quick Build commands (based on quick configuration above):

```
cmake --build --preset msvc-native-debug
cmake --build --preset clang-native-debug
cmake --build --preset gccc-native-debug
```

## Running:

You can find the compiled executable, from the given preset on this path (relative to project root):
```
./out/build/<preset name>/<sub-project name>/
```

#### Quick Run commands (based on quick build above):

```
./out/build/msvc-native-debug/Naive/Naive.exe
./out/build/clang-native-debug/Greedy/Greedy
./out/build/gcc-native-debug/Main/Main
```