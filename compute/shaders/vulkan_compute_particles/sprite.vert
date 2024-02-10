// vertex shader
#version 430


layout (set = 0, binding = 0) uniform camera
{
  mat4 vp_matrix;
} UBO_camera;

layout (set = 0, binding = 1) uniform model
{
  mat4 model_matrix;
} UBO_model;

// input
layout (location = 0) in vec2 in_position;

// instanced input
layout (location = 2) in float instance_x_pos;
layout (location = 3) in float instance_y_pos;

void main ()
{
  gl_Position = UBO_camera.vp_matrix * UBO_model.model_matrix * vec4 (in_position.x + instance_x_pos, in_position.y + instance_y_pos, 0.5, 1.0);
}
