# Tasks

## High priority

- Use the "useMultisampling" option in the settings file, or remove it
- Move all the Vulkan (and Vulkan C++ bindings) calls away from application_run.cpp

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

## Bug fixes

- Fix model loading causing a segfault if a model cache is not provided
- Investigate memory leaks
- Investigate why createSwapchainKHR fails with error "ErrorUnknown"
  - Happens when the swapchain is cached while recreating it, also
    depending on the implementation of Pipeline::waitIdle
