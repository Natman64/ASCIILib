#include "Graphics.h"

#include <algorithm>
#include <string>
#include <sstream>

#include "unicode/uchar.h"
#include "unicode/locid.h"
#include "unicode/ustream.h"
#include "unicode/ustdio.h"

#include <SDL_image.h>

#include "Log.h"
#include "StringTokenizer.h"
#include "FileReader.h"
using namespace ascii;

//static
const unsigned int ascii::Graphics::kBufferWidth = 80;

//static
const unsigned int ascii::Graphics::kBufferHeight = 25;


ascii::Graphics::Graphics(const char* title, int charWidth, int charHeight,
        vector<float> scaleOptions, int currentScaleOption, bool fullscreen,
        int bufferWidth, int bufferHeight)
	: Surface(bufferWidth, bufferHeight),
    mTitle(title),
    mBackgroundColor(ascii::Color::Black),
    mpWindow(NULL), mpRenderer(NULL), mHidingImages(false),
    mFullscreen(fullscreen),
    mCellFonts(bufferWidth, vector<string>(bufferHeight, "")),
    mCharWidth(charWidth), mCharHeight(charHeight)
{
    // Start by creating the window in the correct scale
    mScaleOptions.insert(mScaleOptions.end(), scaleOptions.begin(), scaleOptions.end());
    mCurrentScaleOption = currentScaleOption;
    mScale = mScaleOptions[mCurrentScaleOption];

    int flags = SDL_WINDOW_SHOWN;

    // Only create the window once
	mpWindow = SDL_CreateWindow(mTitle, 
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
		pixelWindowWidth(), pixelWindowHeight(),
		flags);

    if (!mpWindow)
    {
        Log::Error("Failed to create a window for the game.");
        Log::SDLError();
    }

    // oOnly create the renderer once
	mpRenderer = SDL_CreateRenderer(mpWindow, -1, SDL_RENDERER_ACCELERATED);

    if (!mpRenderer)
    {
        Log::Error("Failed to create SDL_Renderer.");
        Log::SDLError();
    }

    // Only create the ImageCache once
	mpCache = new ascii::ImageCache(mpRenderer,
            mCharWidth,
            mCharHeight);

    ApplyOptions();
}

ascii::Graphics::~Graphics(void)
{
    Dispose();
}

void ascii::Graphics::ApplyOptions()
{
    for (auto it = mFonts.begin(); it != mFonts.end(); ++it)
    {
        if (it->second->charHeight() == mCharHeight * mScale)
        {
            it->second->Initialize(mpRenderer);
        }
    }
    
    // Use linear scaling
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    // Save the display index so we can react when it changes
    mLastDisplayIndex = SDL_GetWindowDisplayIndex(mpWindow);

    // Set the window size to the current scale option
    SDL_SetWindowSize(mpWindow, pixelWindowWidth(), pixelWindowHeight());
    SDL_SetWindowPosition(mpWindow,
            SDL_WINDOWPOS_CENTERED_DISPLAY(mLastDisplayIndex),
            SDL_WINDOWPOS_CENTERED_DISPLAY(mLastDisplayIndex));

	checkSize();
    
    // Go fullscreen if fullscreen is needed

    if (mFullscreen)
    {
        if (SDL_SetWindowFullscreen(mpWindow, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
        {
            Log::Error("Failed to set fullscreen.");
            Log::SDLError();
        }
    }
    else
    {
        if (SDL_SetWindowFullscreen(mpWindow, SDL_WINDOW_SHOWN) != 0)
        {
            Log::Error("Failed to disable fullscreen.");
            Log::SDLError();
        }
    }
}

void ascii::Graphics::getCurrentDisplayResolution(int* outWidth, int* outHeight)
{
    // Check which display the window is on right now
    int displayIndex = SDL_GetWindowDisplayIndex(mpWindow);
    
    if (displayIndex == -1)
    {
        Log::Error("Failed to determine which display the window is on!");
        Log::SDLError();
    }

    // Retrieve the native max resolution of the player's display
    SDL_DisplayMode mode;

    if (SDL_GetDesktopDisplayMode(displayIndex, &mode) != 0)
    {
        Log::Error("Failed to get display mode of the current display.");
        Log::SDLError();
    }

    *outWidth = mode.w;
    *outHeight = mode.h;
}


void ascii::Graphics::ApplyClosestScaleOption(int option)
{
    Log::Print("Applying scale option: ", false);
    Log::Print(option);

    // Remember the scale option
    mCurrentScaleOption = option;

    // Find the native resolution of the screen the game window appears on
    int resolutionWidth, resolutionHeight;
    getCurrentDisplayResolution(&resolutionWidth, &resolutionHeight);

    // Start at the index where the desired scale resides in the vector
    for (int idx = option; idx < mScaleOptions.size(); ++idx)
    {
        float scaleToCheck = mScaleOptions[idx];

        if (resolutionWidth > width() * mCharWidth * scaleToCheck
                && resolutionHeight > height() * mCharHeight * scaleToCheck)
        {
            Log::Print("Closest available option was: ", false);
            Log::Print(idx);
            mScale = scaleToCheck;
            ApplyOptions();
            return;
        }
    }
}

void ascii::Graphics::Dispose()
{
    SDL_DestroyRenderer(mpRenderer);
    SDL_DestroyWindow(mpWindow);

    // Dispose of fonts
    for (auto it = mFonts.begin(); it != mFonts.end(); ++it)
    {
        it->second->Dispose();
    }

    delete mpCache;
}

void ascii::Graphics::AddFont(string key, int size, string fontLayoutPath, string fontPath)
{
    stringstream sstream;
    sstream << key << size;
    
    float scale = (float) size / (float) mCharHeight;
    mFonts[sstream.str()] = new PixelFont(mCharWidth * scale, size, fontLayoutPath, fontPath);

    if (size == mCharHeight * mScale)
    {
        mFonts[sstream.str()]->Initialize(mpRenderer);
    }
}

void ascii::Graphics::UnloadFont(string key, int size)
{
    stringstream sstream;
    sstream << key << size;
    PixelFont* pFont = mFonts[sstream.str()];
    delete pFont;
    mFonts.erase(key);
}

void ascii::Graphics::UnloadAllFonts()
{
    for (auto it = mFonts.begin(); it != mFonts.end(); ++it)
    {
        delete it->second;
    }
    mFonts.clear();
}

void ascii::Graphics::SetDefaultFont(string key)
{
    mDefaultFont = key;
}


void ascii::Graphics::SetFullscreen(bool fullscreen)
{
    mFullscreen = fullscreen;

    ApplyOptions();

}

void ascii::Graphics::ToggleFullscreen()
{
    SetFullscreen(!mFullscreen);
}

int ascii::Graphics::pixelToCellX(int pixelX)
{
    if (drawOrigin().x > 0)
    {
        pixelX -= drawOrigin().x;
    }
    return pixelX / (mCharWidth * mScale);
}

int ascii::Graphics::pixelToCellY(int pixelY)
{
    if (drawOrigin().y > 0)
    {
        pixelY -= drawOrigin().y;
    }
    return pixelY / (mCharHeight * mScale);
}

int ascii::Graphics::cellToPixelX(int cellX)
{
    return drawOrigin().x + (cellX * mCharWidth * mScale);
}

int ascii::Graphics::cellToPixelY(int cellY)
{
    return drawOrigin().y + (cellY * mCharHeight * mScale);
}

ascii::Point ascii::Graphics::drawOrigin()
{
    // Calculate the top-left corner of the rendering region
    int drawX = 0;
    int drawY = 0;

    Point actualRes = actualResolution();
    Point drawRes = drawResolution();

    if (actualRes.x > drawRes.x || actualRes.y > drawRes.y)
    {
        // Dealing with a fullscreen window
        drawX = actualRes.x / 2 - drawRes.x / 2;
        drawY = actualRes.y / 2 - drawRes.y / 2;
    }

    return Point(drawX, drawY);
}

void ascii::Graphics::clearScreen()
{
	// Draw background color
	SDL_SetRenderDrawColor(mpRenderer, mBackgroundColor.r, mBackgroundColor.g, mBackgroundColor.b, ascii::Color::kAlpha);
	SDL_RenderFillRect(mpRenderer, NULL);
}

void ascii::Graphics::drawImages(std::map<std::string, Image>* images)
{
    // Don't draw any images if they're currently being hidden
    if (!mHidingImages)
    {
        // Draw every image in the given map otherwise, using their specified
        // positions
        for (auto it = images->begin(); it != images->end(); ++it)
        {
            SDL_Rect dest;
            
            dest.x = cellToPixelX(it->second.second.x);
            dest.y = cellToPixelY(it->second.second.y);

            SDL_QueryTexture(it->second.first, NULL, NULL, &dest.w, &dest.h);
            dest.w *= mScale;
            dest.h *= mScale;

            SDL_RenderCopy(mpRenderer, it->second.first, NULL, &dest);
        }
    }
}

void ascii::Graphics::drawBackgroundColors(ascii::Surface* surface, int x, int y)
{
    for (int ySrc = 0; ySrc < surface->height(); ++ySrc)
    {
		int xSrc = 0;

		while (xSrc < surface->width())
		{
			//chain all adjacent background colors in a row for more efficient rendering
			SDL_Rect colorRect;

			colorRect.x = cellToPixelX(x + xSrc);
			colorRect.y = cellToPixelY(y + ySrc);
			colorRect.w = 0;
			colorRect.h = mCharHeight * mScale;

			Color backgroundColor = surface->getBackgroundColor(xSrc, ySrc);

			do
			{
				if (!surface->isCellOpaque(xSrc, ySrc))
                {
                    ++xSrc;
                    break;
                }

				colorRect.w += mCharWidth * mScale;
				++xSrc;
			} while (xSrc < surface->width() && surface->getBackgroundColor(xSrc, ySrc) == backgroundColor);

			SDL_SetRenderDrawColor(mpRenderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, Color::kAlpha);
			SDL_RenderFillRect(mpRenderer, &colorRect);
		}
	}
}

void ascii::Graphics::drawCharacters(ascii::Surface* surface, int x, int y)
{
	//draw all characters
	for (int ySrc = 0; ySrc < surface->height(); ++ySrc)
	{
        for (int xSrc = 0; xSrc < surface->width(); ++xSrc)
        {
            UChar character = surface->getCharacter(xSrc, ySrc);

            if (!IsWhiteSpace(character) && surface->isCellOpaque(xSrc, ySrc))
            {
                Color color = surface->getCharacterColor(xSrc, ySrc);

                int destCellX = x + xSrc;
                int destCellY = y + ySrc;
                int destPixelX = cellToPixelX(destCellX);
                int destPixelY = cellToPixelY(destCellY);

                //Log::Print("Rendering character: " + UnicodeString(character));
                //Log::Print("x: ", false);
                //Log::Print(pixelX);
                //Log::Print("y: ", false);
                //Log::Print(pixelY);

                string cellFont = mCellFonts[destCellX][destCellY];

                PixelFont* font = GetFont(cellFont);

                if (font)
                    font->RenderCharacter(character, destPixelX, destPixelY, color);
            }
        }
	}
}

void ascii::Graphics::drawSurface(ascii::Surface* surface, int x, int y)
{
	// Draw all background colors
    drawBackgroundColors(surface, x, y);

    // Draw all characters
    drawCharacters(surface, x, y);
}

void ascii::Graphics::refresh()
{
    SDL_RenderPresent(mpRenderer);
}

void ascii::Graphics::update()
{
    // If the window has moved to a different display, refresh the scaling
    // options to fit bigger/smaller screen space
    if (mLastDisplayIndex != SDL_GetWindowDisplayIndex(mpWindow))
    {
        ApplyClosestScaleOption(mCurrentScaleOption);
    }

    // Clear the screen for drawing
    clearScreen();

	// Draw background images
    drawImages(&mBackgroundImages);

    // Draw the buffer surface in between
    drawSurface(this, 0, 0);

	// Draw foreground images
    drawImages(&mForegroundImages);

    // Draw foreground surfaces
    for (int i = 0; i < mForegroundSurfaces.size(); ++i)
    {
        Surface* surface = mForegroundSurfaces[i].first;
        Point position = mForegroundSurfaces[i].second;

        drawSurface(surface, position.x, position.y);
    }

    // Clear any surfaces from the foreground
    mForegroundSurfaces.clear();

    // Refresh the window to show all changes
    refresh();
}

void ascii::Graphics::drawForegroundSurface(ascii::Surface* surface, int x, int y)
{
    mForegroundSurfaces.push_back(make_pair(surface, Point(x, y)));
}

void ascii::Graphics::addBackgroundImage(std::string key, std::string textureKey, int x, int y)
{
	mBackgroundImages[key] = std::make_pair(mpCache->getTexture(textureKey), ascii::Point(x, y));
}

void ascii::Graphics::removeBackgroundImage(std::string key)
{
	mBackgroundImages.erase(key);
}

void ascii::Graphics::addForegroundImage(std::string key, std::string textureKey, int x, int y)
{
	mForegroundImages[key] = std::make_pair(mpCache->getTexture(textureKey), ascii::Point(x, y));
}

void ascii::Graphics::removeForegroundImage(std::string key)
{
	mForegroundImages.erase(key);
}

void ascii::Graphics::clearImages()
{
	mBackgroundImages.clear();
	mForegroundImages.clear();
}

void ascii::Graphics::hideImages()
{
    mHidingImages = true;
}

void ascii::Graphics::showImages()
{
    mHidingImages = false;
}

ascii::Point ascii::Graphics::drawResolution()
{
    return ascii::Point(mCharWidth * width() * mScale, mCharHeight * height() * mScale);
}

ascii::Point ascii::Graphics::actualResolution()
{
    int w, h;
    SDL_GetWindowSize(mpWindow, &w, &h);
    return ascii::Point(w, h);
}

void ascii::Graphics::checkSize()
{
    if (!mFullscreen)
    {
        int w, h;
        SDL_GetWindowSize(mpWindow, &w, &h);

        int expectedWidth = pixelWindowWidth();
        int expectedHeight = pixelWindowHeight();
        bool check = expectedWidth == w && expectedHeight == h;

        if (!check)
        {
            Log::Error("The size of the created window does not match expected dimensions! This could be because the width and height of the graphics buffer are too small.");
            Log::Print("Expected: (", false);
            Log::Print(expectedWidth, false);
            Log::Print(", ", false);
            Log::Print(expectedHeight, false);
            Log::Print(")");
            Log::Print("Got: (", false);
            Log::Print(w, false);
            Log::Print(", ", false);
            Log::Print(h, false);
            Log::Print(")");
        }
    }
}

void ascii::Graphics::clearCellFonts()
{
    for (int x = 0; x < width(); ++x)
    {
        for (int y = 0; y < height(); ++y)
        {
            mCellFonts[x][y] = "";
        }
    }
}

void ascii::Graphics::setCellFont(Rectangle cells, string font)
{
    for (int x = cells.x; x < cells.right(); ++x)
    {
        for (int y = cells.y; y < cells.bottom(); ++y)
        {
            mCellFonts[x][y] = font;
        }
    }
}

PixelFont* ascii::Graphics::GetFont(string key)
{
    PixelFont* font = NULL;

    if (key.empty())
    {
        key = mDefaultFont;
    }

    stringstream sstream;
    sstream << key << (mCharHeight * mScale);
    key = sstream.str();

    if (mFonts.find(key) != mFonts.end())
    {
        font = mFonts[key];
    }
    else
    {
        string error = "Graphics tried to render a character in a nonexistent font: ";
        if (key.empty())
        {
            error += "[DEFAULT FONT]";
        }
        else
        {
            error += key;
        }

        Log::Error(error);
    }

    return font;
}
