﻿/*
 * SDLImage.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "SDLImage.h"

#include "SDLImageLoader.h"
#include "SDL_Extensions.h"

#include "../render/ColorFilter.h"
#include "../render/Colors.h"
#include "../render/CBitmapHandler.h"
#include "../render/CDefFile.h"
#include "../render/Graphics.h"

#include <SDL_surface.h>

class SDLImageLoader;

//First 8 colors in def palette used for transparency
static const SDL_Color sourcePalette[8] = {
	{0,   255, 255, SDL_ALPHA_OPAQUE},
	{255, 150, 255, SDL_ALPHA_OPAQUE},
	{255, 100, 255, SDL_ALPHA_OPAQUE},
	{255, 50,  255, SDL_ALPHA_OPAQUE},
	{255, 0,   255, SDL_ALPHA_OPAQUE},
	{255, 255, 0,   SDL_ALPHA_OPAQUE},
	{180, 0,   255, SDL_ALPHA_OPAQUE},
	{0,   255, 0,   SDL_ALPHA_OPAQUE}
};

static const ColorRGBA targetPalette[8] = {
	{0, 0, 0, 0  }, // 0 - transparency                  ( used in most images )
	{0, 0, 0, 64 }, // 1 - shadow border                 ( used in battle, adventure map def's )
	{0, 0, 0, 64 }, // 2 - shadow border                 ( used in fog-of-war def's )
	{0, 0, 0, 128}, // 3 - shadow body                   ( used in fog-of-war def's )
	{0, 0, 0, 128}, // 4 - shadow body                   ( used in battle, adventure map def's )
	{0, 0, 0, 0  }, // 5 - selection / owner flag        ( used in battle, adventure map def's )
	{0, 0, 0, 128}, // 6 - shadow body   below selection ( used in battle def's )
	{0, 0, 0, 64 }  // 7 - shadow border below selection ( used in battle def's )
};

static ui8 mixChannels(ui8 c1, ui8 c2, ui8 a1, ui8 a2)
{
	return c1*a1 / 256 + c2*a2*(255 - a1) / 256 / 256;
}

static ColorRGBA addColors(const ColorRGBA & base, const ColorRGBA & over)
{
	return ColorRGBA(
		mixChannels(over.r, base.r, over.a, base.a),
		mixChannels(over.g, base.g, over.a, base.a),
		mixChannels(over.b, base.b, over.a, base.a),
		ui8(over.a + base.a * (255 - over.a) / 256)
		);
}

static bool colorsSimilar (const SDL_Color & lhs, const SDL_Color & rhs)
{
	// it seems that H3 does not requires exact match to replace colors -> (255, 103, 255) gets interpreted as shadow
	// exact logic is not clear and requires extensive testing with image editing
	// potential reason is that H3 uses 16-bit color format (565 RGB bits), meaning that 3 least significant bits are lost in red and blue component
	static const int threshold = 8;

	int diffR = static_cast<int>(lhs.r) - rhs.r;
	int diffG = static_cast<int>(lhs.g) - rhs.g;
	int diffB = static_cast<int>(lhs.b) - rhs.b;
	int diffA = static_cast<int>(lhs.a) - rhs.a;

	return std::abs(diffR) < threshold && std::abs(diffG) < threshold && std::abs(diffB) < threshold && std::abs(diffA) < threshold;
}

int IImage::width() const
{
	return dimensions().x;
}

int IImage::height() const
{
	return dimensions().y;
}

SDLImageShared::SDLImageShared(CDefFile * data, size_t frame, size_t group)
	: surf(nullptr),
	margins(0, 0),
	fullSize(0, 0),
	originalPalette(nullptr)
{
	SDLImageLoader loader(this);
	data->loadFrame(frame, group, loader);

	savePalette();
}

SDLImageShared::SDLImageShared(SDL_Surface * from)
	: surf(nullptr),
	margins(0, 0),
	fullSize(0, 0),
	originalPalette(nullptr)
{
	surf = from;
	if (surf == nullptr)
		return;

	savePalette();

	surf->refcount++;
	fullSize.x = surf->w;
	fullSize.y = surf->h;
}

SDLImageShared::SDLImageShared(const ImagePath & filename)
	: surf(nullptr),
	margins(0, 0),
	fullSize(0, 0),
	originalPalette(nullptr)
{
	surf = BitmapHandler::loadBitmap(filename);

	if(surf == nullptr)
	{
		logGlobal->error("Error: failed to load image %s", filename.getOriginalName());
		return;
	}
	else
	{
		savePalette();
		fullSize.x = surf->w;
		fullSize.y = surf->h;
	}
}


void SDLImageShared::draw(SDL_Surface * where, SDL_Palette * palette, const Point & dest, const Rect * src, const ColorRGBA & colorMultiplier, uint8_t alpha, EImageBlitMode mode) const
{
	if (!surf)
		return;

	Rect sourceRect(0, 0, surf->w, surf->h);

	Point destShift(0, 0);

	if(src)
	{
		if(src->x < margins.x)
			destShift.x += margins.x - src->x;

		if(src->y < margins.y)
			destShift.y += margins.y - src->y;

		sourceRect = Rect(*src).intersect(Rect(margins.x, margins.y, surf->w, surf->h));

		sourceRect -= margins;
	}
	else
		destShift = margins;

	destShift += dest;

	SDL_SetSurfaceColorMod(surf, colorMultiplier.r, colorMultiplier.g, colorMultiplier.b);
	SDL_SetSurfaceAlphaMod(surf, alpha);

	if (alpha != SDL_ALPHA_OPAQUE || (mode != EImageBlitMode::OPAQUE && surf->format->Amask != 0))
		SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_BLEND);
	else
		SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);

	if (palette && surf->format->palette)
		SDL_SetSurfacePalette(surf, palette);

	if(surf->format->palette && mode == EImageBlitMode::ALPHA)
	{
		CSDL_Ext::blit8bppAlphaTo24bpp(surf, sourceRect, where, destShift, alpha);
	}
	else
	{
		CSDL_Ext::blitSurface(surf, sourceRect, where, destShift);
	}

	if (surf->format->palette)
		SDL_SetSurfacePalette(surf, originalPalette);
}

std::shared_ptr<ISharedImage> SDLImageShared::scaleFast(const Point & size, SDL_Palette * palette) const
{
	float scaleX = float(size.x) / dimensions().x;
	float scaleY = float(size.y) / dimensions().y;

	if (palette && surf->format->palette)
		SDL_SetSurfacePalette(surf, palette);

	auto scaled = CSDL_Ext::scaleSurface(surf, (int)(surf->w * scaleX), (int)(surf->h * scaleY));

	if (scaled->format && scaled->format->palette) // fix color keying, because SDL loses it at this point
		CSDL_Ext::setColorKey(scaled, scaled->format->palette->colors[0]);
	else if(scaled->format && scaled->format->Amask)
		SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);//just in case
	else
		CSDL_Ext::setDefaultColorKey(scaled);//just in case

	auto ret = std::make_shared<SDLImageShared>(scaled);

	ret->fullSize.x = (int) round((float)fullSize.x * scaleX);
	ret->fullSize.y = (int) round((float)fullSize.y * scaleY);

	ret->margins.x = (int) round((float)margins.x * scaleX);
	ret->margins.y = (int) round((float)margins.y * scaleY);

	// erase our own reference
	SDL_FreeSurface(scaled);

	if (surf->format->palette)
		SDL_SetSurfacePalette(surf, originalPalette);

	return ret;
}

void SDLImageShared::exportBitmap(const boost::filesystem::path& path) const
{
	SDL_SaveBMP(surf, path.string().c_str());
}

void SDLImageIndexed::playerColored(PlayerColor player)
{
	graphics->setPlayerPalette(currentPalette, player);
}

bool SDLImageShared::isTransparent(const Point & coords) const
{
	if (surf)
		return CSDL_Ext::isTransparent(surf, coords.x, coords.y);
	else
		return true;
}

Point SDLImageShared::dimensions() const
{
	return fullSize;
}

std::shared_ptr<IImage> SDLImageShared::createImageReference(EImageBlitMode mode)
{
	if (surf && surf->format->palette)
		return std::make_shared<SDLImageIndexed>(shared_from_this(), originalPalette, mode);
	else
		return std::make_shared<SDLImageRGB>(shared_from_this(), mode);
}

std::shared_ptr<ISharedImage> SDLImageShared::horizontalFlip() const
{
	SDL_Surface * flipped = CSDL_Ext::horizontalFlip(surf);
	auto ret = std::make_shared<SDLImageShared>(flipped);
	ret->fullSize = fullSize;
	ret->margins.x = margins.x;
	ret->margins.y = fullSize.y - surf->h - margins.y;
	ret->fullSize = fullSize;

	return ret;
}

std::shared_ptr<ISharedImage> SDLImageShared::verticalFlip() const
{
	SDL_Surface * flipped = CSDL_Ext::verticalFlip(surf);
	auto ret = std::make_shared<SDLImageShared>(flipped);
	ret->fullSize = fullSize;
	ret->margins.x = fullSize.x - surf->w - margins.x;
	ret->margins.y = margins.y;
	ret->fullSize = fullSize;

	return ret;
}

// Keep the original palette, in order to do color switching operation
void SDLImageShared::savePalette()
{
	// For some images that don't have palette, skip this
	if(surf->format->palette == nullptr)
		return;

	if(originalPalette == nullptr)
		originalPalette = SDL_AllocPalette(surf->format->palette->ncolors);

	SDL_SetPaletteColors(originalPalette, surf->format->palette->colors, 0, surf->format->palette->ncolors);
}

void SDLImageIndexed::shiftPalette(uint32_t firstColorID, uint32_t colorsToMove, uint32_t distanceToMove)
{
	std::vector<SDL_Color> shifterColors(colorsToMove);

	for(uint32_t i=0; i<colorsToMove; ++i)
		shifterColors[(i+distanceToMove)%colorsToMove] = originalPalette->colors[firstColorID + i];

	SDL_SetPaletteColors(currentPalette, shifterColors.data(), firstColorID, colorsToMove);
}

void SDLImageIndexed::adjustPalette(const ColorFilter & shifter, uint32_t colorsToSkipMask)
{
	// Note: here we skip first colors in the palette that are predefined in H3 images
	for(int i = 0; i < currentPalette->ncolors; i++)
	{
		if (i < std::size(sourcePalette) && colorsSimilar(sourcePalette[i], originalPalette->colors[i]))
			continue;

		if(i < std::numeric_limits<uint32_t>::digits && ((colorsToSkipMask >> i) & 1) == 1)
			continue;

		currentPalette->colors[i] = CSDL_Ext::toSDL(shifter.shiftColor(CSDL_Ext::fromSDL(originalPalette->colors[i])));
	}
}

SDLImageIndexed::SDLImageIndexed(const std::shared_ptr<ISharedImage> & image, SDL_Palette * originalPalette, EImageBlitMode mode)
	:SDLImageBase::SDLImageBase(image, mode)
	,originalPalette(originalPalette)
{

	currentPalette = SDL_AllocPalette(originalPalette->ncolors);
	SDL_SetPaletteColors(currentPalette, originalPalette->colors, 0, originalPalette->ncolors);

	setOverlayColor(Colors::TRANSPARENCY);
	setShadowTransparency(0);
}

SDLImageIndexed::~SDLImageIndexed()
{
	SDL_FreePalette(currentPalette);
}

void SDLImageIndexed::setShadowTransparency(float factor)
{
	ColorRGBA shadow50(0, 0, 0, 128 * factor);
	ColorRGBA shadow25(0, 0, 0,  64 * factor);

	// seems to be used unconditionally
	currentPalette->colors[1] = CSDL_Ext::toSDL(shadow25);
	currentPalette->colors[4] = CSDL_Ext::toSDL(shadow50);

	// seems to be used only if color matches
	if (colorsSimilar(originalPalette->colors[2], sourcePalette[2]))
		currentPalette->colors[2] = CSDL_Ext::toSDL(shadow25);

	if (colorsSimilar(originalPalette->colors[3], sourcePalette[3]))
		currentPalette->colors[3] = CSDL_Ext::toSDL(shadow50);
}

void SDLImageIndexed::setOverlayColor(const ColorRGBA & color)
{
	for (int i : {5,6,7})
	{
		if (colorsSimilar(originalPalette->colors[i], sourcePalette[i]))
			currentPalette->colors[i] = CSDL_Ext::toSDL(addColors(targetPalette[i], color));
	}
}

void SDLImageIndexed::setShadowEnabled(bool on)
{
	if (on)
		setShadowTransparency(1.0);
	else
		setShadowTransparency(0);

	shadowEnabled = on;
}

void SDLImageIndexed::setBodyEnabled(bool on)
{
	if (on)
		adjustPalette(ColorFilter::genEmptyShifter(), 0);
	else
		adjustPalette(ColorFilter::genAlphaShifter(0), 0);

	bodyEnabled = on;
}

void SDLImageIndexed::setOverlayEnabled(bool on)
{
	if (on)
		setOverlayColor(Colors::WHITE_TRUE);
	else
		setOverlayColor(Colors::TRANSPARENCY);
	overlayEnabled = on;
}

SDLImageShared::~SDLImageShared()
{
	SDL_FreeSurface(surf);
	SDL_FreePalette(originalPalette);
}

SDLImageBase::SDLImageBase(const std::shared_ptr<ISharedImage> & image, EImageBlitMode mode)
	:image(image)
	, alphaValue(SDL_ALPHA_OPAQUE)
	, blitMode(mode)
{}

std::shared_ptr<ISharedImage> SDLImageBase::getSharedImage() const
{
	return image;
}

void SDLImageRGB::draw(SDL_Surface * where, const Point & pos, const Rect * src) const
{
	image->draw(where, nullptr, pos, src, Colors::WHITE_TRUE, alphaValue, blitMode);
}

void SDLImageIndexed::draw(SDL_Surface * where, const Point & pos, const Rect * src) const
{
	image->draw(where, currentPalette, pos, src, Colors::WHITE_TRUE, alphaValue, blitMode);
}

void SDLImageIndexed::scaleFast(const Point & size)
{
	image = image->scaleFast(size, currentPalette);
}

void SDLImageRGB::scaleFast(const Point & size)
{
	image = image->scaleFast(size, nullptr);
}

void SDLImageBase::exportBitmap(const boost::filesystem::path & path) const
{
	image->exportBitmap(path);
}

bool SDLImageBase::isTransparent(const Point & coords) const
{
	return image->isTransparent(coords);
}

Point SDLImageBase::dimensions() const
{
	return image->dimensions();
}

void SDLImageBase::setAlpha(uint8_t value)
{
	alphaValue = value;
}

void SDLImageBase::setBlitMode(EImageBlitMode mode)
{
	blitMode = mode;
}

void SDLImageRGB::setShadowEnabled(bool on)
{
	// Not supported. Theoretically we can try to extract all pixels of specific colors, but better to use 8-bit images or composite images
}

void SDLImageRGB::setBodyEnabled(bool on)
{
	// Not supported. Theoretically we can try to extract all pixels of specific colors, but better to use 8-bit images or composite images
}

void SDLImageRGB::setOverlayEnabled(bool on)
{
	// Not supported. Theoretically we can try to extract all pixels of specific colors, but better to use 8-bit images or composite images
}

void SDLImageRGB::setOverlayColor(const ColorRGBA & color)
{}

void SDLImageRGB::playerColored(PlayerColor player)
{}

void SDLImageRGB::shiftPalette(uint32_t firstColorID, uint32_t colorsToMove, uint32_t distanceToMove)
{}

void SDLImageRGB::adjustPalette(const ColorFilter & shifter, uint32_t colorsToSkipMask)
{}


