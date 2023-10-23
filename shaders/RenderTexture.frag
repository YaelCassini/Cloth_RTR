#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D ourTexture;

void main()
{
	float depth = texture(ourTexture, TexCoord).r;
	//FragColor = texture(ourTexture, TexCoord);

	// depth = TexCoord.y;
	FragColor = vec4(TexCoord.x,TexCoord.y, 0.0, 1.0);
	FragColor = vec4(depth,depth,depth, 1.0);
	// FragColor = vec4(TexCoord, 0.0, 1.0);
	// FragColor = vec4(TexCoord.x,TexCoord.y,1.0,1.0);
}