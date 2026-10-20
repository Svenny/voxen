module mesh_layouts;

// Pointees of buffer device addresses, accessed with natural (scalar-like) layout

// TODO: unify with declaration of `voxen::land::PseudoSurfaceVertexPosition`
public struct PseudoChunkSurfacePosition {
	uint16_t3 position_unorm;
};

// TODO: unify with declaration of `voxen::land::PseudoSurfaceVertexAttributes`
public struct PseudoChunkSurfaceAttributes {
	int16_t2 normal_oct_snorm;
	uint16_t4 mat_hist_entries;
	uint8_t4 mat_hist_weights;
};

// TODO: unify with declaration of `PseudoSurfaceDrawCommand` in C++
public struct DrawCommand {
	PseudoChunkSurfacePosition *pos_data;
	PseudoChunkSurfaceAttributes *attrib_data;

	float3 chunk_base_camworld;
	float chunk_size_metres;
};

// Draw command list buffer: `uint` count of valid commands padded to 16 bytes, then `DrawCommand` array.
// Slang has no structs with a trailing unsized array (GLSL buffer blocks), so the list
// is bound as a byte address buffer and accessed with typed `Load`/`Store` at these offsets.
public static const uint DRAW_CMD_LIST_COUNT_OFFSET = 0;

public uint drawCmdListItemOffset(uint index)
{
	return 16 + index * sizeof(DrawCommand);
}
