# Source subdirectories

There are a few subdirectores:

- `vkapp2/` contains most of the application and engine logic, along with
  Vulkan-related functions and types;
- `assets/` contains large files and runtime specific data - it doesn't *need*
  to have anything in it for the build to succeed, but during development
  it can be used to avoid copying or linking assets to the build directory
  (see the documentation written in the directory itself);
- `shaders/` contains... shaders. Slash.

The `util/` directory is not an actual subdirectory, not in the context of
the CMakeLists.txt setup: it contains two files, `util.hpp` and `util.cpp`,
containing some trivial utilities that could be effortlessly copied and
included in other projects for whatever reason (with a few touches,
obviously).  
The target for this small utility library is defined in the main source
directory.


# Launching the application

Launching the application can be done by simply running `launch.sh`:
it sets the environment variables, and it ensures that the CWD is
correct.


# params.cfg

There's a file, `params.cfg`, that can be used to configure at runtime how the
application behaves. It is generated at runtime with default values if they're
missing, but, similarly to files in `assets/`, there can be a template in the
main directory to be copied.

The set of parameters is documented in `vkapp2/settings/options.hpp`, but,
since this is not an application that has any real purpose other than being
built, there's no user-oriented documentation (since I don't feel like
transcribing comments for non-existent end users).
