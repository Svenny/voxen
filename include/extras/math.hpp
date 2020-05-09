#pragma once

#include <glm/glm.hpp>

namespace extras
{

inline glm::mat4 translate(float x, float y, float z) noexcept {
	glm::mat4 mat;
	mat[0] = { 1, 0, 0, 0 };
	mat[1] = { 0, 1, 0, 0 };
	mat[2] = { 0, 0, 1, 0 };
	mat[3] = { x, y, z, 1 };
	return mat;
}

inline glm::mat4 scale(float s) noexcept {
	glm::mat4 mat;
	mat[0] = { 1, 0, 0, 0 };
	mat[1] = { 0, 1, 0, 0 };
	mat[2] = { 0, 0, 1, 0 };
	mat[3] = { 0, 0, 0, 1 / s };
	return mat;
}

inline glm::mat4 scale_translate(float x, float y, float z, float s) noexcept {
	glm::mat4 mat;
	mat[0] = { 1, 0, 0, 0 };
	mat[1] = { 0, 1, 0, 0 };
	mat[2] = { 0, 0, 1, 0 };
	float div = 1.0f / s;
	mat[3] = { x * div, y * div, z * div, div };
	return mat;
}

inline glm::mat4 perspective(double fovx, double fovy, double znear, double zfar) noexcept {
	glm::mat4 mat;
	float x = float(1.0 / tan(fovx * 0.5));
	float y = float(1.0 / tan(fovy * 0.5));
	float z = float(znear / (zfar - znear));
	float w = float((znear * zfar) / (zfar - znear));
	// Actual matrix looks like this transposed
	mat[0] = { x, 0, 0, 0 };
	mat[1] = { 0, y, 0, 0 };
	mat[2] = { 0, 0, z, 1 };
	mat[3] = { 0, 0, w, 0 };
	return mat;
}

inline glm::mat4 lookAt(const glm::dvec3 &pos, const glm::dvec3 &forward, const glm::dvec3 &up) noexcept {
	glm::vec3 p { pos };
	glm::vec3 f { forward };
	glm::vec3 u { up };
	glm::vec3 s = glm::cross(u, f);
	float ox = glm::dot(p, s);
	float oy = glm::dot(p, u);
	float oz = glm::dot(p, f);

	glm::mat4 mat;
	// Actual matrix looks like this transposed
	mat[0] = { s.x, -u.x, f.x, 0 };
	mat[1] = { s.y, -u.y, f.y, 0 };
	mat[2] = { s.z, -u.z, f.z, 0 };
	mat[3] = { -ox,  oy, -oz, 1 };
	return mat;
}

inline glm::dvec3 dirFromOrientation(glm::dmat3 rot_mat) noexcept {
	return glm::dvec3(rot_mat[0][2], rot_mat[1][2], rot_mat[2][2]);
}

inline glm::dvec3 upFromOrientation(glm::dmat3 rot_mat) noexcept {
	return glm::dvec3(rot_mat[0][1], rot_mat[1][1], rot_mat[2][1]);
}

inline glm::dvec3 rightFromOrientation(glm::dmat3 rot_mat) noexcept {
	return glm::dvec3(rot_mat[0][0], rot_mat[1][0], rot_mat[2][0]);
}

}
