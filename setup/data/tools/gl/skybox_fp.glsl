
uniform samplerCube skybox;

void main()
{
	//doing rotation/flip to fix skybox orientation
	gl_FragColor = textureCube( skybox, vec3( -gl_TexCoord[0].y, gl_TexCoord[0].z, gl_TexCoord[0].x ) );
}