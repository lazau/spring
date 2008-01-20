#ifndef __FARTEXTURE_HANDLER_H__
#define __FARTEXTURE_HANDLER_H__

#include <vector>
#include "Rendering/GL/myGL.h"

struct S3DOModel;

/**
 * @brief Cheap unit lodding using imposters.
 */
class CFartextureHandler
{
public:
	GLuint farTexture;

	CFartextureHandler(void);
	~CFartextureHandler(void);
	void CreateFarTexture(S3DOModel* model);
	void CreateFarTextures();

private:
	void ReallyCreateFarTexture(S3DOModel* model);

	unsigned char* farTextureMem;
	int usedFarTextures;
	std::vector<S3DOModel*> pending;
};

extern CFartextureHandler* fartextureHandler;

#endif // __FARTEXTURE_HANDLER_H__
