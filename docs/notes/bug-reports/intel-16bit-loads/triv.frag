#version 460
layout(location = 0) in vec3 n;
layout(location = 1) in vec4 c;
layout(location = 0) out vec4 o;
void main() { o = c + vec4(n, 0); }
