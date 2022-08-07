#pragma once

#include <glm/vec3.hpp>

namespace voxen::terrain
{

/* An offset in cell-size units from the lowest to a given corner.
 Equal to permuted bit representation of entry index (idx = YXZ).
 Equal to `glm::uvec3((idx & 2) >> 1, (idx & 4) >> 2, idx & 1)`. */
constexpr glm::uvec3 CELL_CORNER_OFFSET_TABLE[8] = {
	{ 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 1 },
	{ 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 0 }, { 1, 1, 1 }
};

/* Quadruples of cell's children sharing an edge along some axis. First dimension
 is axis, second dimension - quadruple number, third dimension - children ID's list.
 NOTE: order of ID's in the list is important for `edgeProc` call, preserve it. */
constexpr uint32_t SUBEDGE_SHARING_TABLE[3][2][4] = {
	{ { 0, 4, 5, 1 }, { 2, 6, 7, 3 } }, // X
	{ { 0, 1, 3, 2 }, { 4, 5, 7, 6 } }, // Y
	{ { 0, 2, 6, 4 }, { 1, 3, 7, 5 } }  // Z
};

/* Pairs of cell's children sharing a face along some axis. First dimension
 is axis, second dimension - pair number, third dimension - children ID's list.
 NOTE: order of ID's in the list is important for `faceProc` call, preserve it. */
constexpr uint32_t SUBFACE_SHARING_TABLE[3][4][2] = {
	{ { 0, 2 }, { 4, 6 }, { 5, 7 }, { 1, 3 } }, // X
	{ { 0, 4 }, { 1, 5 }, { 3, 7 }, { 2, 6 } }, // Y
	{ { 0, 1 }, { 2, 3 }, { 6, 7 }, { 4, 5 } }  // Z
};

/* This table is used to recursively descend during visiting edge-sharing nodes quad.
 Four edge-sharing nodes will have up to 8 children used in recursive calls. First dimension
 is axis of `edgeProc`, second dimension - child number, third dimension - a pair of indices,
 first is the parent node ID in `edgeProc` arguments list, second is it's child ID. If a
 given node is a leaf (not a cell), it should be used itself instead of selecting a child.

 2D example (shared axis goes through X and is orthogonal to the screen):
 *---*---* => *---*---*
 |   |   | => |2|3|2|3|
 | 2 | 3 | => *-+-*-+-*
 |   |   | => |0|1|0|1|
 *---X---* => *-*-X-*-*
 |   |   | => |2|3|2|3|
 | 0 | 1 | => *-+-*-+-*
 |   |   | => |0|1|0|1|
 *---*---* => *-*-*-*-*
 On the left, nodes are numbered as in `edgeProc` arguments list. On the right, their
 children are numbered with conventional children numbering notation. We can see we'll
 need "child 3 of node 0", "child 2 of node 1" etc. during the recursive call. */
constexpr uint32_t EDGE_PROC_RECURSION_TABLE[3][8][2] = {
	{ { 0, 5 }, { 3, 4 }, { 0, 7 }, { 3, 6 },
	  { 1, 1 }, { 2, 0 }, { 1, 3 }, { 2, 2 } }, // X
	{ { 0, 3 }, { 1, 2 }, { 3, 1 }, { 2, 0 },
	  { 0, 7 }, { 1, 6 }, { 3, 5 }, { 2, 4 } }, // Y
	{ { 0, 6 }, { 0, 7 }, { 1, 4 }, { 1, 5 },
	  { 3, 2 }, { 3, 3 }, { 2, 0 }, { 2, 1 } }  // Z
};

/* This table is used to recursively descend during visiting face-sharing nodes pair.
 Two face-sharing nodes will have up to 8 children used in recursive calls. First dimension
 is axis of `faceProc`, second dimension - child number, third dimension - a pair of indices,
 first is the parent node ID in `faceProc` arguments list, second is it's child ID. If a
 given node is a leaf (not a cell), it should be used itself instead of selecting a child.

 2D example:
 *---*---* => *---*---*
 |   |   | => |2|3|2|3|
 | 0 | 1 | => *-+-*-+-*
 |   |   | => |0|1|0|1|
 *---*---* => *-*-*-*-*
 On the left, nodes are numbered as in `faceProc` arguments list. On the right, their
 children are numbered with conventional children numbering notation. We can see we'll
 need "child 1 of node 0", "child 2 of node 1" etc. during the recursive call. */
constexpr uint32_t FACE_PROC_RECURSION_TABLE[3][8][2] = {
	{ { 0, 2 }, { 0, 3 }, { 1, 0 }, { 1, 1 },
	  { 0, 6 }, { 0, 7 }, { 1, 4 }, { 1, 5 } }, // X
	{ { 0, 4 }, { 0, 5 }, { 0, 6 }, { 0, 7 },
	  { 1, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 } }, // Y
	{ { 0, 1 }, { 1, 0 }, { 0, 3 }, { 1, 2 },
	  { 0, 5 }, { 1, 4 }, { 0, 7 }, { 1, 6 } }  // Z
};

}
