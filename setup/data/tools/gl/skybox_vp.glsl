
uniform vec3 u_view_origin;

void main()
{
    gl_TexCoord[0] = vec4( ( gl_Vertex.xyz - u_view_origin ), 1 );
    gl_Position = ftransform();
} 