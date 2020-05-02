#pragma once

#include <glm/glm.hpp>

namespace voxen
{

class Player {
public:

	glm::dvec3 position() const noexcept { return pos; }
	glm::mat4 cameraMatrix() const noexcept;
private:
	glm::dvec3 pos { 100, 100, -1100 };
	glm::dvec3 forward { 0, 0, 1 };
	glm::dvec3 up { 0, 1, 0 };
	double fovx = 1.5, fovy = 1.5 * 9.0 / 16.0;
	double znear = 0.1, zfar = 1000000.0;
};

}
