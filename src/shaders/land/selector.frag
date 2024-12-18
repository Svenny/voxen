#version 460 core

layout(location = 0) in smooth vec3 in_cube_vector;

layout(location = 0) out vec4 out_color;

const vec3 COLOR = vec3(0.63, 0.67, 0.85);

void main()
{
	vec3 cv = abs(in_cube_vector);
	cv = cv * cv * cv * cv;
	cv *= cv;

	float alpha = 0.4 * (cv.x + cv.y + cv.z);
	out_color = vec4(COLOR, alpha);
}
