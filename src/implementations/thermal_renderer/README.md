# Tamashii - Thermal Renderer

## CLI Interface

The CLI interface for the Sustainable Agro Ecosystem projekt.

**Options:**
```
  -h,--help | Print this help message and exit
  --gltf_path (string, required) | Path to GLTF
  --lat (flaot, required) | Latitude
  --long (flaot, required) | Longitude
  --rays-per-triangle (unsigned int, default=10000) | Number of rays to cast from each triangle in the mesh
  --time (string, required) Time of day - should be in %Y-%m-%dT%H:%M:%S format
  --timestep (FLOAT, required) | Timestep (h)
  --verbose | Be verbose.
  --gui | Show GUI.
```
**GLTF scene input**

In order to prevent hard seams in the simulation output values surfaces that should be conneceted need to have non duplicate vertices. In Blender this can be acchived by smooth shading since then the verteices are not split and share normals.

Custom properties per object:
```
- kelvin (float) (flaot, required) | the temperature of the object
- temperature-fixed (bool) | fix temperature during simulation
- diffuse-reflectance (float) | amount of diffuse reflected light (range: 0-1)
- specular-reflectance (float) | amount of specular reflected light (range: 0-1)
- diffuse-emission (bool) | enable diffuse emission (otherwise along normal)
- thickness (float) | surface thickness
- density (float) | material density
- heat-capacity (float) | material heat capacity
- heat-conductivity (float) | material conductivity
- traceable (bool) | enable intersection with rays
```

## Implmentation details
The renderer pre-computes a so called transport matrix which encodes how much energy can be potetially exchanged via radiation between the surfaces and stores this factor per vertex. This step is accelerated via hardware raytracing RTX.
It uses light tracing (with russian roulette) to trace photon paths through the scene and record where they get absorbed.

The current material model is a simple mixture between perfect diffuse and specular.
Photon emission is performed either in direction of the normal or uniformly over the hemisphere (diffuse).

Themerature or radiance is computed by solving a system of equations based on the transport matrix and initial values per vertex. The solver currently runs on the CPU.
It is important to point out that, as long as the geometry of the scene does not change, the transport matrix can be reused and effectively caches the expensive light simulation.