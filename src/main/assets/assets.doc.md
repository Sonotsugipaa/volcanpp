# Using models

Volcanpp only partially supports the Wavefront OBJ format: the file itself
holds all the geometry (vertex positions, UV coordinates and normals),
but materials as specified by the format are not used. Instead, every model
(here used interchangeably with "mesh") has *one* material with *one* set
of properties.

Every model has a name, one model file (equal to the model's name with the
`.obj` extension) and three optional texture files, with the respective
extensions `.dfs.png`, `.nrm.png` and `.spc.png` for the diffuse, normal and
specular textures. All textures **must** be PNG files with RGBA channels, and
have a the same base name.

The directory to search for models can be specified using the `VKA2_ASSET_PATH`
environment variable, which set to `./assets` if it is undefined; this also
affects the `scene.cfg` file described below.

## Example

A valid set of model files may be:

`${VKA2_ASSET_PATH}/boxoblox.obj`  
`${VKA2_ASSET_PATH}/boxoblox_tx.dfs.png` (optional)  
`${VKA2_ASSET_PATH}/boxoblox_tx.nrm.png` (optional)  
`${VKA2_ASSET_PATH}/boxoblox_tx.spc.png` (optional)


# Setting up the scene

The scene is specified in `${VKA2_ASSET_PATH}/scene.cfg`, and consists of
multiple objects that may share the same material.  
The file consists of three root elements: `objects`, `materials` and
`pointLight`; the first two are lists of groups of settings, the last one is an
array of 4 fractional elements.

## Objects

Groups in the `objects` list have the following properties:

- `meshName` (string): references the name (minus extension) of the model file;
- `materialName` (string, optional): references an entry in the `materials`
  list, defaults to the value of `meshName`;
- `position` (float[3]): the XYZ coordinates of the object (in world space
  coordinates);
- `orientation` (float[3]): the yaw, pitch and roll values for the object's
  rotation;
- `scale` (float[3]): the XYZ multipliers to scale the model with (in model
  space coordinates);
- `color` (float[4]): the RGBA color multiplier for the diffuse and specular
  textures.

## Materials

Groups in the `materials` list have the following properties:

- `name` (string): the name of the material, to be referenced by each object;
  every object **must** have a material with a matching name, otherwise how
  it's rendered is unspecified (most likely, the program terminates with a
  segmentation fault).
- `minDiffuse` (float): the minimum multiplier for diffuse reflections;
- `maxDiffuse` (float): the maximum multiplier for diffuse reflections;
- `minSpecular` (float): the minimum multiplier for specular reflections;
- `maxSpecular` (float): the maximum multiplier for specular reflections;
- `shininess` (float): the precision of specular reflections (its effect scales
  logarithmically);
- `celLevels` (unsigned int:16): the number of levels of cel shading;
- `mergeVertices` (boolean, optional): whether the mesh normals should be merged
  together (defaults to `false`).

## Point Light

At the time of writing, Volcanpp uses two light sources: the sun, that affects
all surfaces the same way, and one "point light", that behaves like a light bulb
would.

The former can be configured in `params.cfg`, the latter **must** be configured
in `scene.cfg` as follows:

- the first three components of the `pointLight` array specify the initial
  position of the point light (X, Y, Z components);
- the fourth component specifies the intensity of the light.

## Examples

A valid `scene.cfg` file:

```libconfig
objects = (
	{
		meshName = "wall";
		position = [ 46.912, -5.5909, -12.563 ];
		orientation = [ 0.0, 0.0, 0.0 ];
		scale = [ 1.0, 1.0, 1.0 ];
		color = [ 1.0, 1.0, 1.0, 1.0 ];
	}, {
		meshName = "tower";
		materialName = "wall";
		position = [ 46.155, -4.6209, -10.088 ];
		orientation = [ 45.0, 0.0, 0.0 ];
		scale = [ 1.0, 1.0, 1.0 ];
		color = [ 1.0, 1.0, 1.0, 1.0 ];
	}, {
		meshName = "monument";
		position = [ 35.303, -2.8933, -16.482 ];
		orientation = [ 0.0, 0.0, 0.0 ];
		scale = [ 1.0, 1.0, 1.0 ];
		color = [ 1.0, 1.0, 1.0, 1.0 ];
	}
);

materials = (
	{
		name = "wall";
		minDiffuse = 0.05;
		maxDiffuse = 0.99;
		minSpecular = 0.0;
		maxSpecular = 0.8;
		celLevels = 0;
		shininess = 64.0;
	}, {
		name = "monument";
		minDiffuse = 0.05;
		maxDiffuse = 0.3;
		minSpecular = 0.0;
		maxSpecular = 1.5;
		celLevels = 13;
		shininess = 16.0;
	}
);

pointLight = [ 36.881, -4.5557, 2.0338, 3.0 ];
```


# CMakeLists.txt

The source directory is simple: it attempts to link every asset to the binary
directory, or, if that fails, to copy assets in it. At the time of writing,
this includes all files matching `*.png`, `*.obj` or `*.cfg`.

The directory is excluded from .gitignore, as assets are to be stored
separatedly from the repository. They can be locally copied to the assets
directory to make debugging or testing more convenient, or they can be manually
placed directly in the binary directory after a successful build.
