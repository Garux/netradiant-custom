// Shaders of the Auto-Pigeon regression cases (regression_tests/auto-pigeon). Authored for this repository.
// Their images are written by ci/auto-pigeon/regress.py into the temporary game root it compiles with:
// checker 64x64, wide 64x32, AUPCHK1 32x32, everything else 64x64.

textures/apreg/checker
{
	qer_editorimage textures/apreg/checker.tga
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/apreg/checker.tga
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

textures/apreg/wide
{
	qer_editorimage textures/apreg/wide.tga
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/apreg/wide.tga
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

textures/apreg/patch
{
	qer_editorimage textures/apreg/patch.tga
	cull disable
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/apreg/patch.tga
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

textures/apreg/water
{
	qer_editorimage textures/apreg/water.tga
	qer_trans 0.5
	surfaceparm nonsolid
	surfaceparm water
	surfaceparm trans
	surfaceparm nolightmap
	cull disable
	{
		map textures/apreg/water.tga
		rgbGen identity
	}
}

textures/apreg/clip
{
	qer_editorimage textures/apreg/clip.tga
	qer_trans 0.4
	surfaceparm nodraw
	surfaceparm nolightmap
	surfaceparm nonsolid
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm playerclip
}

// the conventional name of the hidden-face material; the pipe maps of R04-R06 use it
textures/common/caulk
{
	qer_editorimage textures/common/caulk.tga
	surfaceparm nodraw
	surfaceparm nomarks
	surfaceparm nolightmap
}

// the checker the pipe maps of R04-R06 name
textures/AUPCHK1
{
	qer_editorimage textures/AUPCHK1.tga
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/AUPCHK1.tga
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}
