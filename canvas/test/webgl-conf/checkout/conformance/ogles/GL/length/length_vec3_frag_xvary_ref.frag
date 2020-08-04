
/*
Copyright (c) 2019 The Khronos Group Inc.
Use of this source code is governed by an MIT-style license that can be
found in the LICENSE.txt file.
*/


#ifdef GL_ES
precision mediump float;
#endif
varying vec4 color;

void main (void)
{
	gl_FragColor = vec4(vec3(sqrt(color.r*color.r + color.g*color.g + color.b*color.b) / 3.0), 1.0);
}
