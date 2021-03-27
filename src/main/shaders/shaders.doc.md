# Pipeline structure

Rendering an image currently consists of two subpasses: one to draw the main
geometry (with textures, lighting etc.), one to draw outlines. Every shader
shares the same uniforms (for now), and only has a switch statement in the
`main` function used to determine the procedure for drawing objects.

Through the "static" uniform, the application can select one of a specific
range of values (named "shader selectors"), that is consistent for all shaders.  
For example, at the time of writing, selector=0 means that only diffuse
lighting is used, while selector=3 means that only specular lighting, cel
shading and the outline are used; the range of values and their meanings may
change frequently, and to save some time on bureaucracy, the range shall be
kept small and be documented in each shader.

## Outline

Usually outlines are drawn using edge detection algorithms (I think), but
this implementation is a bit under-engineered.

After (or before (but preferably after)) drawing the main geometry, a second
Vulkan pipeline is used to draw the same vertices with the outline shaders.  
The latter are not very complex in their design: the vertex shader translates
each vertex along the normal with an arbitrary distance, and increases the
Z position (in screen space) by a constant value, possibly 0 for reasons
described below; the fragment shader simply outputs an arbitrary color.

Naturally, neither shader is supposed to run if the shader selector doesn't
imply that outlines should be used. This is enforced by an "unconditional"
`discard` in the fragment shader, but to optimize this the vertex shader shall
cause the rasterization stage to produce zero or one fragment for each draw.

There are a bunch of drawbacks for this implementation.

- It implies two draws are performed for every object with the same uniforms.
- Z-offsets are in screen space, which means that, with perspective projection
  matrices, the offset depends on the near plane (and the far plane, perhaps?);
  this needs to be accounted for by the application, as doing so in the vertex
  shader would need a few questionable design decisions regarding uniforms and
  redundant calculations.
- Concave geometry causes weird little artifacts.
- Normals always have to be smoothed for all vertices with the same position,
  even if their faces should not be smoothed. This means *two* normal
  attributes may be necessary, and due to how Volcanpp is structured, this is
  always the case.
