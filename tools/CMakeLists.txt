# Host tools: asset compilers, code generators, developer utilities etc.
# They link only `tk` and third-party libraries, never the engine,
# so engine changes don't cause rebuilds of tools and their outputs.

add_subdirectory(assetc)
add_subdirectory(uidgen)
