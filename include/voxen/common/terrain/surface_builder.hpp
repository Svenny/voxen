#pragma once

namespace voxen::terrain
{

class Chunk;

class SurfaceBuilder final {
public:
	SurfaceBuilder() = default;
	SurfaceBuilder(SurfaceBuilder &&) = delete;
	SurfaceBuilder(const SurfaceBuilder &) = delete;
	SurfaceBuilder &operator = (SurfaceBuilder &&) = delete;
	SurfaceBuilder &operator = (const SurfaceBuilder &) = delete;
	~SurfaceBuilder() = default;

	static void buildOctree(Chunk &chunk);
	static void buildSurface(Chunk &chunk);
};

}
