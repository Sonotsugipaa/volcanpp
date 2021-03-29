# Tasks

## High priority

- Subdivide procedures currently globbed in Application::Run

## Low priority

- Move from `params.cfg` to `scene.cfg` as many things as possible
- Implement outline shape randomization
- Prune unused constants from constants.hpp
- Use the "useMultisampling" option in the settings file, or remove it
- Make the RenderPass <=> Pipeline co-dependency co-nsistent
- Fix model loading causing a segfault if a model cache is not provided
- Move all the Vulkan (and Vulkan C++ bindings) calls away from application_run.cpp
- Ensure that model normals, tangents and bitangents are orthogonal
  - the current code kinda throws that out of the window
- Add fullscreenability
- Add mouse camera rotation
- Investigate why createSwapchainKHR fails with error "ErrorUnknown"
  - Happens when the swapchain is cached while recreating it, also
    depending on the implementation of Pipeline::waitIdle
- Investigate memory leaks
- Simplify RenderPass::runRenderPass
- Try and remove some redundant main/outline vertex shader computations,
  possibly with some Vulkan pipeline mechanism I don't know yet
