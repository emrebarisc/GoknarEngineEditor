# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/baris/Desktop/Projects/GoknarEngineEditor

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug

# Include any dependencies generated for this target.
include Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/compiler_depend.make

# Include the progress variables for this target.
include Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/progress.make

# Include the compile flags for this target's objects.
include Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/flags.make

Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.o: Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/flags.make
Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.o: ../GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3AlignedAllocator.cpp
Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.o: Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.o"
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.o -MF CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.o.d -o CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.o -c /home/baris/Desktop/Projects/GoknarEngineEditor/GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3AlignedAllocator.cpp

Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.i"
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/baris/Desktop/Projects/GoknarEngineEditor/GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3AlignedAllocator.cpp > CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.i

Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.s"
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/baris/Desktop/Projects/GoknarEngineEditor/GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3AlignedAllocator.cpp -o CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.s

Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Vector3.o: Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/flags.make
Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Vector3.o: ../GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3Vector3.cpp
Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Vector3.o: Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Vector3.o"
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Vector3.o -MF CMakeFiles/Bullet3Common.dir/b3Vector3.o.d -o CMakeFiles/Bullet3Common.dir/b3Vector3.o -c /home/baris/Desktop/Projects/GoknarEngineEditor/GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3Vector3.cpp

Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Vector3.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Bullet3Common.dir/b3Vector3.i"
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/baris/Desktop/Projects/GoknarEngineEditor/GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3Vector3.cpp > CMakeFiles/Bullet3Common.dir/b3Vector3.i

Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Vector3.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Bullet3Common.dir/b3Vector3.s"
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/baris/Desktop/Projects/GoknarEngineEditor/GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3Vector3.cpp -o CMakeFiles/Bullet3Common.dir/b3Vector3.s

Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Logging.o: Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/flags.make
Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Logging.o: ../GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3Logging.cpp
Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Logging.o: Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Logging.o"
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Logging.o -MF CMakeFiles/Bullet3Common.dir/b3Logging.o.d -o CMakeFiles/Bullet3Common.dir/b3Logging.o -c /home/baris/Desktop/Projects/GoknarEngineEditor/GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3Logging.cpp

Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Logging.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Bullet3Common.dir/b3Logging.i"
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/baris/Desktop/Projects/GoknarEngineEditor/GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3Logging.cpp > CMakeFiles/Bullet3Common.dir/b3Logging.i

Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Logging.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Bullet3Common.dir/b3Logging.s"
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/baris/Desktop/Projects/GoknarEngineEditor/GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common/b3Logging.cpp -o CMakeFiles/Bullet3Common.dir/b3Logging.s

# Object files for target Bullet3Common
Bullet3Common_OBJECTS = \
"CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.o" \
"CMakeFiles/Bullet3Common.dir/b3Vector3.o" \
"CMakeFiles/Bullet3Common.dir/b3Logging.o"

# External object files for target Bullet3Common
Bullet3Common_EXTERNAL_OBJECTS =

Goknar/thirdparty/bullet3/src/Bullet3Common/libBullet3Commond.a: Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3AlignedAllocator.o
Goknar/thirdparty/bullet3/src/Bullet3Common/libBullet3Commond.a: Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Vector3.o
Goknar/thirdparty/bullet3/src/Bullet3Common/libBullet3Commond.a: Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/b3Logging.o
Goknar/thirdparty/bullet3/src/Bullet3Common/libBullet3Commond.a: Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/build.make
Goknar/thirdparty/bullet3/src/Bullet3Common/libBullet3Commond.a: Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking CXX static library libBullet3Commond.a"
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && $(CMAKE_COMMAND) -P CMakeFiles/Bullet3Common.dir/cmake_clean_target.cmake
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/Bullet3Common.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/build: Goknar/thirdparty/bullet3/src/Bullet3Common/libBullet3Commond.a
.PHONY : Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/build

Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/clean:
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common && $(CMAKE_COMMAND) -P CMakeFiles/Bullet3Common.dir/cmake_clean.cmake
.PHONY : Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/clean

Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/depend:
	cd /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/baris/Desktop/Projects/GoknarEngineEditor /home/baris/Desktop/Projects/GoknarEngineEditor/GoknarEngine/Goknar/thirdparty/bullet3/src/Bullet3Common /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common /home/baris/Desktop/Projects/GoknarEngineEditor/Build_Debug/Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : Goknar/thirdparty/bullet3/src/Bullet3Common/CMakeFiles/Bullet3Common.dir/depend

