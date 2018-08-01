
Unnatural Planets is a tool to synthesize planet models.

The planets consist of meshes (obj format), albedo, normal and material textures, simplified collision meshes, temperature, precipitation and wind maps, and possibly more.

Generated planets are inhabited by completely generated vegetation where every single tree is unique.

The generation process is designed for offline use and will take some time, however, the generation can be stopped at any stage to make previews.

The generation process is fully deterministic, based on extremely flexible configuration and several seeds for particular features.

Whole planet is split into chunks with several levels of details for faster rendering while maintaining good visual quality.
The algorithm works mostly in global scale and is therefore unsuitable for procedural (on-demand) generation.

# Algorithm Overview

- Rough Terrain
  - continents, islands and mountains are computed by tectonic plates simulation
  - 3D noise functions are used to change terrain density, this adds unnatural features
  - based on configuration, the terrain may be very earth-like or extremely unnatural with eg. floating patches of land and tunnels throught the core

- Climate Simulation
  - finer terrain details are added based on climate simulation
  - temperature levels are computed based on average sunlight impact
  - whole water cycle is simulated: precipitation, evaporation, sea levels, rivers, etc.
  - wind simulation
  - land deposition

- Finer Details
  - add lakes, rivers
  - caves

- Vegetation Simulation
  - generate several plant and tree species with predefined conditions
  - simulate vegetation growth, spread and dying - this will ensure that speacies look unique
  - based on species properties generate base meshes and than slightly randomize it for every instance
  - very small plants (eg. grass) will only impact planet textures and materials with no actual changes to meshes

- Artificial Features
  - cities, villages, buildings
  - roads
  - farmland

- LODs Simplification

# Building

See [BUILDING](https://github.com/ucpu/cage/blob/master/BUILDING.md) instructions for the Cage. They are the same here.
