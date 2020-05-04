#include <voxen/common/player.hpp>

#include <bicycle/math.hpp>

namespace voxen
{

void Player::updateState(glm::dvec3 new_pos, glm::dquat new_rot) noexcept {
	m_position = new_pos;
	m_orientation = new_rot;
	updateSecondaryFactors();
}

void Player::updateSecondaryFactors() noexcept {
	glm::dmat3 rot_mat = glm::mat3_cast(m_orientation);

	m_look_vector = glm::dvec3(rot_mat[0][2], rot_mat[1][2], rot_mat[2][2]);
	m_right_vector = glm::dvec3(rot_mat[0][0], rot_mat[1][0], rot_mat[2][0]);
	m_up_vector = glm::dvec3(rot_mat[0][1], rot_mat[1][1], rot_mat[2][1]);

	m_proj_matrix = bicycle::perspective(fovx, fovy, znear, zfar);
	m_view_matrix = bicycle::lookAt(m_position, m_look_vector, m_up_vector);
	m_cam_matrix = m_proj_matrix * m_view_matrix;
}

}
