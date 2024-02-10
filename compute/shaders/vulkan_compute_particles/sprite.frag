// frag shader
#version 430

layout (set = 1, binding = 0) uniform particle_colour
{
	vec4 Colour;
}UBO_Colour;

// output
layout (location = 0) out vec4 frag_colour;


void main ()
{
  frag_colour = UBO_Colour.Colour;
}
