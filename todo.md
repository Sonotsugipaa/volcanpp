# Tasks

## High priority

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

## Bug fixes

- Fix model loading causing a segfault if a model cache is not provided
- Investigate why createSwapchainKHR fails with error "ErrorUnknown"
  - Happens when the swapchain is cached while recreating it, also
    depending on the implementation of Pipeline::waitIdle
