
# Algorithm Plan Overview

- Rough Terrain
  - based on configuration, the terrain may be very earth-like or extremely unnatural with eg. floating patches of land and tunnels through the core
  - the overall shape of the planet is defined as a sum of implicitly defined shape function and a 3d noise functions
    - use sphere and no noise to produce natural planets
	- use different base shape: cube, cone, torus, tetrahedron etc
  - convert the implicit function into surface representation (marching cubes/tetrahedrons)
  - continents, islands and mountains are computed by tectonic plates simulation

- Climate Simulation
  - finer terrain details are added based on climate simulation
  - temperature levels are computed based on average sunlight impact
  - whole water cycle is simulated: precipitation, evaporation, rivers, etc.
  - wind simulation
  - land deposition

- Finer Details
  - add lakes, rivers
  - caves

- Vegetation Simulation
  - generate several plant and tree species with predefined conditions
  - simulate vegetation growth, spread and dying - this will ensure that species look unique
  - based on species properties generate base meshes and than slightly randomize it for every instance
  - very small plants (eg. grass) will only impact planet textures and materials with no actual changes to meshes

- Artificial Features
  - cities, villages, buildings
  - roads
  - farmland

- LODs Simplification
