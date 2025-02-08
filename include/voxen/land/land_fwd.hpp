#pragma once

namespace voxen::land
{

class Chunk;
struct ChunkAdjacencyRef;
class LandService;
struct LandState;
class PrivateStorageTree;
class PseudoChunkData;
class StorageTree;
struct StorageTreeControl;

namespace detail
{

struct ChunkNode;
struct DuoctreeX4Node;
struct DuoctreeX16Node;
struct DuoctreeX64Node;
struct DuoctreeX256Node;
class LandServiceImpl;
struct TriquadtreeBridgeNode;
struct TriquadtreeRootNode;

} // namespace detail

} // namespace voxen::land
