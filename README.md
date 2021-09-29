# Volcanpp

Volcanpp is my Nth personal playground around a graphics API, only this time
it's Vulkan. Like platypuses, it doesn't do much: (unlike platypuses) it draws
arbitrary models with arbitrary textures and material properties in arbitrary
ways. These models and textures, contextually refered to as assets, have to
be provided separatedly.

The purpose of this project is simply getting familiar with the API and its
usage.


# Dependencies

External dependencies have to be managed manually.

## Single header libraries

These libraries can be available through package managers, but when they're not,
they can be retrieved and placed into the `include/` directory (like missing
headers from other types of libraries).

### `stb_image.h` (From the STB library)

STB uses the MIT license (and claims to be public domain in the FAQ),
ant it can be obtained from [its Github repository](https://github.com/nothings/stb).
Only the image library is needed, and it should be located in `include/stb/`.

### `Vulkan Memory Allocator`

VMA is licensed under the MIT license, and can be obtained from
[its Github repository](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator).
The header should be located in `include/vma/`.

### `tinyobjloader`

tinyobjloader is licensed under the MIT license, and, you probably guessed it,
it can be obtained from [its Github repository](https://github.com/tinyobjloader/tinyobjloader).
The header should be located in `include/`.

---

## Header only library (GLM)

GLM can be found [here](https://github.com/g-truc/glm).  
As a reference, if the library is set correctly, the file `include/glm/glm.hpp`
(among others) exists.

---

## Other libraries

These libraries cannot simply be copied in `include/`, so they must be included
in the project directory (or installed on the system) according to how each one
expects to be.  
In times of doubt, compiler errors will help; missing headers will most
likely be mentioned by the compiler, static and dynamic libraries are all
to be placed in `lib/`, and executables belong in `bin/`.

- GLFW3
- Vulkan SDK (Vulkan headers and loader)
- shaderc (for `glslc`, the GLSL-to-SPV compiler)
- libconfig++


# Building the application

The only supported platform is GNU/Linux. The root directory contains a shell
script, `alias.src.sh`, that provides some functions (practically equivalent
to aliases) to quickly build and optionally run the application. in order to
do so, the following commands can be used:

```sh
source alias.src.sh
prof=release rm-mk-run # The "rm-" prefix can be omitted for unclean builds
```

Alternatively, `build.sh` can be run with "`release`" or "`debug`" as the first
argument, in order to build the application; it can then be run by using
`release/launch.sh` (or `debug/...`), which will automatically set the
required environment variables and its current working directory.

The application will fail to run if its assets are missing; they have to be
manually provided, as documented in `src/assets/`.
