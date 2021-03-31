# Tasks

## High priority

- Refactor how window recreation is handled for Application::run
  - Currently there's no real structure to how certain runtime resources
    (i.e. fields of RenderContext) are handled, and issues with them are
    dealt as they pop up
- Use the "useMultisampling" option in the settings file, or remove it
- Move all the Vulkan (and Vulkan C++ bindings) calls away from application_run.cpp
- Refactor Application::data_t::options to Molise (the joke is that it will
  not exist after refactoring)

## Low priority

- RAIIize some RAIIizeable C-with-classes classes
- Move from `params.cfg` to `scene.cfg` as many things as possible
- Prune unused constants from constants.hpp
- Make the RenderPass <=> Pipeline co-dependency co-nsistent
- Add mouse camera rotation
- Simplify RenderPass::runRenderPass
- Try and remove some redundant main/outline vertex shader computations,
  possibly with some Vulkan pipeline mechanism I don't know yet
- Implement dynamic model LOD
- Add outline color to the push constant
- Port to Visual Studio, if possible at all
- Add R8G8B8 image format support
  - This is tough, because it is apparently not uncommon for devices
    to have more inclusive support for RGBA than RGB - which
    means this task is either going to be discarded, or to create a
    RGB-to-RGBA blit procedure
    - Should this task be discarded, the relevant functions should be
      removed from `graphics.hpp`
- Use an environmental variable to locate `params.cfg` and `scene.cfg`
  - `params.cfg` should be searched for in the current working directory
    by default, lest it doesn't serve its purpose of defining default
    locations for assets and shaders

## Bug fixes

- Fix model loading causing a segfault if a model cache is not provided
- Investigate memory leaks
- Investigate why createSwapchainKHR fails with error "ErrorUnknown"
  - Happens when the swapchain is cached while recreating it, also
    depending on the implementation of Pipeline::waitIdle
