#include <voxen/common/player.hpp>

#include <bicycle/math.hpp>

namespace voxen
{

glm::mat4 Player::cameraMatrix() const noexcept {
	glm::mat4 proj = bicycle::perspective(fovx, fovy, znear, zfar);
	glm::mat4 view { 0.0f };
	view = bicycle::lookAt(pos, forward, up);
	return proj * view;
}

}
