#include <voxen/land/cube_array.hpp>

#include "../../voxen_test_common.hpp"

namespace voxen::land
{

TEST_CASE("'CubeArray' sanity check", "[voxen::land::cube_array]")
{
	CubeArray<uint16_t, 16> arr;
	CHECK(sizeof(arr) == sizeof(uint16_t) * 16 * 16 * 16);
	CHECK(arr.begin() == reinterpret_cast<uint16_t *>(&arr));
	CHECK(arr.end() == reinterpret_cast<uint16_t *>(&arr + 1));

	constexpr uint16_t A = 0x1234;
	arr.fill(A);
	CHECK(arr[glm::uvec3(15)] == A);
	CHECK(arr.data[15][0][7] == A);

	constexpr uint16_t B = 0x4321;
	arr.fill(glm::uvec3(1, 2, 3), glm::uvec3(3), B);

	// "Lower corner" of updated region
	CHECK(arr.data[2][1][3] == B);
	CHECK(arr.data[3][1][3] == B);
	CHECK(arr.data[2][2][3] == B);
	CHECK(arr.data[2][1][4] == B);

	// "Upper corner" of updated region
	CHECK(arr.data[4][3][5] == B);
	CHECK(arr.data[3][3][5] == B);
	CHECK(arr.data[4][2][5] == B);
	CHECK(arr.data[4][3][4] == B);

	// "Below" updated region
	CHECK(arr.data[1][1][3] == A);
	CHECK(arr.data[2][0][3] == A);
	CHECK(arr.data[2][1][2] == A);
	CHECK(arr.data[1][0][2] == A);

	// "Above" updated region
	CHECK(arr.data[5][3][5] == A);
	CHECK(arr.data[4][4][5] == A);
	CHECK(arr.data[4][3][6] == A);
	CHECK(arr.data[5][4][6] == A);
}

TEST_CASE("'CubeArray' extract/insert check", "[voxen::land::cube_array]")
{
	constexpr uint32_t A = 0x1234;
	constexpr uint32_t B = 0x4321;
	constexpr uint32_t C = 0x2143;
	constexpr uint32_t D = 0x3412;

	CubeArray<uint32_t, 6> arr1;
	CubeArray<uint32_t, 3> arr2;

	arr2.fill(A);
	arr1.insertFrom(glm::uvec3(0), arr2);
	arr1.insertFrom(glm::uvec3(3), arr2);
	CHECK(arr1.data[1][1][1] == A);
	CHECK(arr1.data[4][4][4] == A);

	arr2.fill(B);
	arr1.insertFrom(glm::uvec3(3, 0, 0), arr2);
	arr1.insertFrom(glm::uvec3(0, 3, 0), arr2);
	CHECK(arr1.data[1][4][1] == B);
	CHECK(arr1.data[4][1][1] == B);

	arr2.fill(C);
	arr1.insertFrom(glm::uvec3(0, 0, 3), arr2);
	arr1.insertFrom(glm::uvec3(3, 0, 3), arr2);
	CHECK(arr1.data[1][1][4] == C);
	CHECK(arr1.data[1][4][4] == C);

	arr2.fill(D);
	arr1.insertFrom(glm::uvec3(3, 3, 0), arr2);
	arr1.insertFrom(glm::uvec3(0, 3, 3), arr2);
	CHECK(arr1.data[4][4][1] == D);
	CHECK(arr1.data[4][1][4] == D);

	CubeArray<uint32_t, 2> arr3;
	arr1.extractTo(glm::uvec3(2), arr3);
	CHECK(arr3.data[0][0][0] == A);
	CHECK(arr3.data[1][1][1] == A);
	CHECK(arr3.data[0][1][0] == B);
	CHECK(arr3.data[1][0][0] == B);
	CHECK(arr3.data[0][0][1] == C);
	CHECK(arr3.data[0][1][1] == C);
	CHECK(arr3.data[1][1][0] == D);
	CHECK(arr3.data[1][0][1] == D);
}

} // namespace voxen::land
