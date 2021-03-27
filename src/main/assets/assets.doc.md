# Using models

Volcanpp only partially supports the Wavefront OBJ format: the file itself
holds all the geometry (vertex positions, UV coordinates and normals),
but materials as specified by the format are not used. Instead, every model
has *one* material with *one* set of properties.

Every model has a name, one model file (equal to the model's name with the
`.obj` extension) and three optional texture files (equal to the model's name
with the respective extensions `.dfs.png`, `.nrm.png` and `.spc.png` for
the diffuse, normal and specular textures). All textures must be PNG files
with RGBA channels.

The directory to search for models can be specified using the `VKA2_ASSET_PATH`
environment variable, which also affects the `scene.cfg` file described below.

## Example

A valid set of model files may be:

`./assets/boxoblox.obj`  
`./assets/boxoblox.dfs.png` (optional)  
`./assets/boxoblox.nrm.png` (optional)  
`./assets/boxoblox.spc.png` (optional)


# Setting up the scene

The scene is specified in `assets/scene.cfg`. A scene consists of multiple
objects that may share the same model, and the properties of the models
themselves. The file consists of two root elements, `objects` and `models`;
both are lists of groups of settings.

## Objects

Groups in the `objects` list have the following properties:

- `modelName`: references a model in the `models` list;
- `position`: the XYZ coordinates of the object (in world space coordinates);
- `orientation`: the yaw, pitch and roll values for the object's rotation;
- `scale`: the XYZ multipliers to scale the model with (in model space coordinates);
- `color`: the RGBA color multiplier for the diffuse and specular textures.

## Models

Groups in the `models` list have the following properties:

- `name`: the name of the model, to be referenced by each object; every object
  **must** have a model with a matching name, otherwise how it's rendered
  is unspecified.
- `minDiffuse`: the minimum strength for diffuse lighting;
- `maxDiffuse`: the maximum strength for diffuse lighting;
- `minSpecular`: the minimum strength for specular lighting;
- `maxSpecular`: the maximum strength for specular lighting.

## Examples

A valid `scene.cfg` file:

```
objects = (
	{
		modelName = "model0";
		position = [ 0.0, 0.0, 0.0 ];
		orientation = [ -5.0, -5.0, 5.0 ];
		scale = [ 1.0, 1.0, 1.25 ];
		color = [ 1.0, 1.0, 1.0, 1.0 ];
	},
	{
		modelName = "model1";
		position = [ 0.0, 0.0, 0.0 ];
		orientation = [ 5.0, 0.0, 0.0 ];
		scale = [ 1.0, 1.0, 1.0 ];
		color = [ 1.0, 1.0, 1.0, 1.0 ];
	}
);

models = (
	{
		name = "model0";
		minDiffuse = 0.0;
		maxDiffuse = 0.3;
		minSpecular = -8.0;
		maxSpecular = 4.0;
	},
	{
		name = "model1";
		minDiffuse = 0.0;
		maxDiffuse = 0.6;
		minSpecular = -0.3;
		maxSpecular = 1.0;
	}
);
```


# CMakeLists.txt

The source directory is simple: it attempts to link every asset to the binary
directory, or, if that fails, to copy assets in it. At the time of writing,
this includes all files matching `*.png`, `*.obj` or `*.cfg`.

The directory is excluded from .gitignore, as assets are to be stored separatedly
from the repository. They can be locally copied to the assets directory to make
debugging or testing more convenient, or they can be manually placed directly in
the binary directory after a successful build.
