## Very Simple Langauge Compiler


 <img src="https://github.com/tobiasfremming/CvslCompiler/blob/main/cvlsCompiler.png" alt="logo" width="200" height="200" />
 


Compiles VSL code (Very Simple Language) into runnable x86_64 assembly code.
- LL parser

#### Dependencies
To build, you will need
 - CMake
 - flex v. 2.6
 - bison v. 3.5
 - GNU make or Ninja
 
#### Building

Creating a cmake build inside a new folder `build/`, with the Ninja generator. You only need to do this once.
``` sh
cmake -B build -GNinja
```

Compiling the project
``` sh
cmake --build build
```

#### Running
The final binary can be found in `build/vslc`. See `--help` for help.
Input is passed to stdin, output is printed to stdout.

Example usage:
``` sh
build/vslc -s < vsl_programs/ps2-parser/variables.vsl
```

