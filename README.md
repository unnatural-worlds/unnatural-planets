
Unnatural Planets is a tool to synthesize planet models.

Outputs are meshes (obj format) with albedo, bump and roughness textures and additional simplified collision mesh.

The generation process is designed for offline use and will take some time.

# Running

```bash
./unnatural-planets --optimize false --preview --shape sphere
```

- `--optimize false` disables navigation mesh optimizations, which is only needed when generating maps for Unnatural Worlds.
- `--preview` opens Blender and imports generated render meshes with proper materials and textures. Blender 2.90 or newer must be in the PATH environment variable.
- `--shape sphere` forces generating a planet with spherical basic shape. See source code for other options available or omit the parameter entirely to use randomly chosen base shape.

# Building

See [BUILDING](https://github.com/ucpu/cage/blob/master/BUILDING.md) instructions for the Cage. They are the same here.
