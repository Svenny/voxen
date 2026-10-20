# Host tools: asset compilers, code generators and developer utilities.
# They link only `tk` and third-party libraries, never the engine,
# so engine changes don't cause rebuilds of tools and their outputs.

# Make all targets created here appear in a folder in VS
set(CMAKE_FOLDER tools)

add_subdirectory(assetc)
add_subdirectory(uidgen)
