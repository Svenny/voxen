#version 460
layout(location = 0) out vec3 n;
layout(location = 1) out vec4 c;
void main() { n = vec3(0); c = vec4(1); gl_Position = vec4(0, 0, 0, 1); }
