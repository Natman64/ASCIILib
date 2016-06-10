#include "Graphics.h"

#include <sstream>
#include <iostream>
#include <fstream>

#include "unicode/uchar.h"
#include "unicode/locid.h"
#include "unicode/ustream.h"
#include "unicode/ustdio.h"

#include <SDL_image.h>

const int kFontSize = 12;

//static
const unsigned int ascii::Graphics::kBufferWidth = 80;

//static
const unsigned int ascii::Graphics::kBufferHeight = 25;

namespace
{
    // Needs to be big enough for the file path of the flair sheet
    const int32_t MAX_FLAIR_TABLE_LINE_SIZE = 30;
    const string FLAIR_SHEET_KEY("FLAIR_SHEET");
}


ascii::Graphics::Graphics(const char* title, const char* fontpath)
	: Surface(kBufferWidth, kBufferHeight),
    mTitle(title), mFullscreen(false),
    mBackgroundColor(ascii::Color::Black), mWindow(NULL), mRenderer(NULL),
    mHidingImages(false), mHasSpecialCharTable(false)
{
	TTF_Init();

	mFont = TTF_OpenFont(fontpath, kFontSize);
    
    Initialize();

    UErrorCode error = U_ZERO_ERROR;
    mpLineBreakIt = BreakIterator::createLineInstance(Locale::getDefault(), error);
}

ascii::Graphics::Graphics(const char* title, const char* fontpath,
        int bufferWidth, int bufferHeight)
	: Surface(bufferWidth, bufferHeight), mTitle(title),
    mFullscreen(false), mBackgroundColor(ascii::Color::Black),
    mWindow(NULL), mRenderer(NULL), mHidingImages(false),
    mHasSpecialCharTable(false)
{
	TTF_Init();

	mFont = TTF_OpenFont(fontpath, kFontSize);

    Initialize();

    UErrorCode error = U_ZERO_ERROR;
    mpLineBreakIt = BreakIterator::createLineInstance(Locale::getDefault(), error);
}

ascii::Graphics::~Graphics(void)
{
    delete mpLineBreakIt;
    Dispose();

	TTF_CloseFont(mFont);

	TTF_Quit();
}

void ascii::Graphics::Initialize()
{
    mFullscreen = false;

    UpdateCharSize();

    int flags = SDL_WINDOW_SHOWN;

	mWindow = SDL_CreateWindow(mTitle, 
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
		width() * mCharWidth, height() * mCharHeight, 
		flags);

	checkSize();

	mRenderer = SDL_CreateRenderer(mWindow, -1, SDL_RENDERER_ACCELERATED);

	mCache = new ascii::ImageCache(mRenderer,
            mCharWidth,
            mCharHeight);

    if (mHasSpecialCharTable)
    {
        LoadSpecialCharTable(mFlairTablePath.c_str());
    }
}

void ascii::Graphics::Dispose()
{
    clearGlyphs();
    SDL_DestroyRenderer(mRenderer);
    SDL_DestroyWindow(mWindow);
    delete mCache;
}

void ascii::Graphics::LoadSpecialCharTable(const char* path)
{
    if (mHasSpecialCharTable)
    {
        DisposeSpecialCharTable();
    }

    mFlairTablePath = path;

    // Open the UTF-8 file
    ifstream file(path);

    // Make sure it opened properly
    if (file.is_open())
    {
        // Skip the UTF-8 BOM if one is present
        char a,b,c;
        a = file.get();
        b = file.get();
        c = file.get();
        if(a!=(char)0xEF || b!=(char)0xBB || c!=(char)0xBF)
        {
            file.seekg(0);
        }
        else
        {
            cout << "Warning: file " << path << " contains UTF-8 bit order mark" << endl;
        }

        // Read the first line, which holds the path to the flair sheet
        string line;
        getline(file, line);
        // Strip trailing carriage returns
        line.erase(line.find_last_not_of(" \n\r\t") + 1);

        // Load the sheet as a texture
        // Strip the trailing newline
        string temp;
        string sheetPath = line;
        cout << "Loading texture " << sheetPath << endl;
        mCache->loadTexture(FLAIR_SHEET_KEY, sheetPath.c_str());

        // Parse each line of the special char table
        while(getline(file, line))
        {
            // Strip trailing carriage return
            line.erase(line.find_last_not_of(" \n\r\t") + 1);

            // Parse in Unicode for special characters
            UnicodeString lineUnicode = UnicodeString::fromUTF8(StringPiece(line.c_str()));

            //string lineOutput = lineUnicode.toUTF8String(temp);
            u_printf("%s\n", lineUnicode.getBuffer());
            cout << "Line size: " << lineUnicode.length() << endl;

            // Split the line into tokens
            mpLineBreakIt->setText(lineUnicode);
            vector<UnicodeString> tokens;

            int32_t start = mpLineBreakIt->first();
            int32_t end = start;
            while (true)
            {
                end = mpLineBreakIt->next();

                if (end == BreakIterator::DONE)
                    break;

                UnicodeString token = lineUnicode.tempSubStringBetween(start, end);
                token.trim();

                start = end;

                tokens.push_back(token);
            }

            // The line will be structured as follows:
            // [Unicode char] [ASCII char] [flair index] [(optional) y offset]
            UChar specialChar = tokens[0][0];

            // Don't read a normal char if that token has more than one
            // character i.e. "NONE"
            UChar normalChar = ' ';
            if (tokens[1].length() == 1)
                normalChar = tokens[1][0];

            UnicodeString indexString = tokens[2];
            string temp;
            int flairIndex = atoi(indexString.toUTF8String(temp).c_str());

            int flairOffset = 0;
            if (tokens.size() > 3)
            {
                UnicodeString offsetString = tokens[3];
                temp = "";
                flairOffset = atoi(offsetString.toUTF8String(temp).c_str());
                //cout << "Changing flair offset: " << flairOffset << endl;
            }

            ComboChar comboChar;
            comboChar.base = normalChar;
            //cout << "Flair index: " << flairIndex << endl;
            comboChar.flairIndex = flairIndex;
            comboChar.flairOffset = flairOffset;
            mSpecialCharTable[specialChar] = comboChar;
        }

        mHasSpecialCharTable = true;
    }
    else
    {
        cout << "Error loading special character table " << path << endl;
    }
}

void ascii::Graphics::DisposeSpecialCharTable()
{
    mCache->freeTexture(FLAIR_SHEET_KEY);
    mSpecialCharTable.clear();
    mHasSpecialCharTable = false;
}

void ascii::Graphics::SetFullscreen(bool fullscreen)
{
    // Don't go into any of these operations if unnecessary
    if (fullscreen == mFullscreen) return;

    mFullscreen = fullscreen;

    Uint32 flags = 0; 
    if (fullscreen)
    {
        flags = flags | SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    else
    {
        Dispose();
        Initialize();
    }

    if (SDL_SetWindowFullscreen(mWindow, flags) != 0)
    {
        cout << SDL_GetError() << endl;
    }
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
    return pixelX / mCharWidth;
}

int ascii::Graphics::pixelToCellY(int pixelY)
{
    if (drawOrigin().y > 0)
    {
        pixelY -= drawOrigin().y;
    }
    return pixelY / mCharHeight;
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
	SDL_SetRenderDrawColor(mRenderer, mBackgroundColor.r, mBackgroundColor.g, mBackgroundColor.b, ascii::Color::kAlpha);
	SDL_RenderFillRect(mRenderer, NULL);
}

void ascii::Graphics::drawImages(std::map<std::string, Image>* images)
{
    // Don't draw any images if they're currently being hidden
    if (!mHidingImages)
    {
        int drawX = drawOrigin().x;
        int drawY = drawOrigin().y;

        // Draw every image in the given map otherwise, using their specified
        // positions
        for (auto it = images->begin(); it != images->end(); ++it)
        {
            SDL_Rect dest;
            
            dest.x = drawX + it->second.second.x * mCharWidth;
            dest.y = drawY + it->second.second.y * mCharHeight;

            SDL_QueryTexture(it->second.first, NULL, NULL, &dest.w, &dest.h);

            SDL_RenderCopy(mRenderer, it->second.first, NULL, &dest);
        }
    }
}

void ascii::Graphics::drawBackgroundColors(ascii::Surface* surface, int x, int y)
{
    int drawX = drawOrigin().x;
    int drawY = drawOrigin().y;

	for (int ySrc = 0; ySrc < surface->height(); ++ySrc) 
    {
		int xSrc = 0;

		while (xSrc < surface->width())
		{
			//chain all adjacent background colors in a row for more efficient rendering
			SDL_Rect colorRect;

			colorRect.x = drawX + (x + xSrc) * mCharWidth;
			colorRect.y = drawY + (y + ySrc) * mCharHeight;
			colorRect.w = 0;
			colorRect.h = mCharHeight;

			Color backgroundColor = surface->getBackgroundColor(xSrc, ySrc);

			do
			{
				if (!surface->isCellOpaque(xSrc, ySrc))
                {
                    ++xSrc;
                    break;
                }

				colorRect.w += mCharWidth;
				++xSrc;
			} while (xSrc < surface->width() && surface->getBackgroundColor(xSrc, ySrc) == backgroundColor);

			SDL_SetRenderDrawColor(mRenderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, Color::kAlpha);
			SDL_RenderFillRect(mRenderer, &colorRect);
		}
	}
}

void ascii::Graphics::drawCharacters(ascii::Surface* surface, int x, int y)
{
    int drawX = drawOrigin().x;
    int drawY = drawOrigin().y;

	//draw all characters
	for (int ySrc = 0; ySrc < surface->height(); ++ySrc)
	{
		int xSrc = 0;

		while (xSrc < surface->width())
		{
			// Chain all adjacent characters with the same color into strings
            // for more efficient rendering

            // Don't bother chaining spaces together
			UChar uch = surface->getCharacter(xSrc, ySrc);

			if (IsWhiteSpace(uch))
			{
				++xSrc;
				continue;
			}

            // If the character is not a space, start chaining with its
            // neighbors
            UnicodeString charChain;
			SDL_Rect textRect;

			textRect.x = drawX + (x + xSrc) * mCharWidth;
			textRect.y = drawY + (y + ySrc) * mCharHeight;
			textRect.w = 0;
			textRect.h = mCharHeight;
			Color characterColor = surface->getCharacterColor(xSrc, ySrc);

			do
			{
				if (!surface->isCellOpaque(xSrc, ySrc))
				{
					++xSrc;
					break;
				}

                // First process each character as unicode to see if it must be
                // rendered as a combo of a normal character and a flair
				UChar uch = surface->getCharacter(xSrc, ySrc);

                if (mHasSpecialCharTable && mSpecialCharTable.find(uch)
                        != mSpecialCharTable.end())
                {
                    //cout << "Processing special character" << endl;
                    // Must process as a special character
                    ComboChar combo = mSpecialCharTable[uch];
                    // Adopt a normal character as base
                    uch = combo.base;
                    // Retrieve the index of the flair to draw in conjunction
                    int flairIndex = combo.flairIndex;
                    // Retrieve the y offset for drawing the flair
                    int flairOffset = combo.flairOffset;

                    //cout << "Flair index: " << flairIndex << endl;
                    //cout << "Flair offset: " << flairOffset << endl;

                    // Draw the flair
                    SDL_Rect src = Rectangle(
                            flairIndex * mCharWidth,
                            0,
                            mCharWidth,
                            mCharHeight);
                    SDL_Rect dest = Rectangle(
                            drawX + (x + xSrc) * mCharWidth,
                            drawY + (y + ySrc) * mCharHeight + flairOffset,
                            mCharWidth,
                            mCharHeight);

                    SDL_Texture* flairSheet = mCache->getTexture(FLAIR_SHEET_KEY);
                    // Using the proper color
                    SDL_SetTextureColorMod(flairSheet,
                            characterColor.r,
                            characterColor.g,
                            characterColor.b);

                    SDL_RenderCopy(mRenderer, flairSheet, &src, &dest);
                }

                // Don't chain empty space in a word. Empty space can exist
                // here if it is used as the base for a non-space character
                // combo
                if (!IsWhiteSpace(uch))
                {
                    charChain += uch;
                    textRect.w += mCharWidth;
                }
                else
                {
                    ++xSrc;
                    break;
                }

				++xSrc;
			} while (
                // Stop when we reach the end of the row
                xSrc < surface->width()
                // Stop if the next character has a different color
                && surface->getCharacterColor(xSrc, ySrc) == characterColor
                // Stop if the next character is another space
                && !IsWhiteSpace(surface->getCharacter(xSrc, ySrc)));

            // Convert the unicode into an appropriate string encoding for
            // TTF_RenderText()
            string temp;

            // TODO this line creates strings that are incompatible with
            // rendering certain symbols on Windows, i.e. "-" and "+". The
            // likely solution is not to convert to UTF-8, but to the system's
            // default codepage
            string str = charChain.toUTF8String(temp);

			Glyph glyph = std::make_pair(str, characterColor);

			SDL_Texture* texture = NULL;

			if (mGlyphTextures[glyph])
			{
				texture = mGlyphTextures[glyph];
			}
			else
			{
				SDL_Surface* surface = TTF_RenderUTF8_Solid(mFont, str.c_str(), characterColor);
				texture = SDL_CreateTextureFromSurface(mRenderer, surface);

				mGlyphTextures[glyph] = texture;
				
				SDL_FreeSurface(surface);
			}

            SDL_RenderCopy(mRenderer, texture, NULL, &textRect);
		}
	}

    //SDL_RenderCopy(mRenderer, mpFlairSheet, NULL, NULL);
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
    SDL_RenderPresent(mRenderer);
}

void ascii::Graphics::update()
{
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
    //cout << "Drawing flair sheet everywhere" << endl;
    //SDL_RenderCopy(mRenderer, mpFlairSheet, NULL, NULL);
    refresh();
}

void ascii::Graphics::drawForegroundSurface(ascii::Surface* surface, int x, int y)
{
    mForegroundSurfaces.push_back(make_pair(surface, Point(x, y)));
}

void ascii::Graphics::addBackgroundImage(std::string key, std::string textureKey, int x, int y)
{
	mBackgroundImages[key] = std::make_pair(mCache->getTexture(textureKey), ascii::Point(x, y));
}

void ascii::Graphics::removeBackgroundImage(std::string key)
{
	mBackgroundImages.erase(key);
}

void ascii::Graphics::addForegroundImage(std::string key, std::string textureKey, int x, int y)
{
	mForegroundImages[key] = std::make_pair(mCache->getTexture(textureKey), ascii::Point(x, y));
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

void ascii::Graphics::clearGlyphs()
{
	for (std::map<Glyph, SDL_Texture*>::iterator it = mGlyphTextures.begin(); it != mGlyphTextures.end(); ++it)
	{
		SDL_DestroyTexture(it->second); //destroy all stored string textures
	}

	mGlyphTextures.clear();
}

ascii::Point ascii::Graphics::drawResolution()
{
    return ascii::Point(mCharWidth * width(), mCharHeight * height());
}

ascii::Point ascii::Graphics::actualResolution()
{
    int w, h;
    SDL_GetWindowSize(mWindow, &w, &h);
    return ascii::Point(w, h);
}

void ascii::Graphics::checkSize()
{
    if (!mFullscreen)
    {
        int w, h;
        SDL_GetWindowSize(mWindow, &w, &h);

        SDL_assert(width() * mCharWidth == w && height() * mCharHeight == h);
    }
}

void ascii::Graphics::UpdateCharSize()
{
	TTF_SizeText(mFont, " ", &mCharWidth, &mCharHeight);
}

bool ascii::Graphics::IsWhiteSpace(UChar uch)
{
	return uch == UnicodeString(" ")[0];
}
