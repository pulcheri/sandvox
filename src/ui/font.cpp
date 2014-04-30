#include "font.hpp"

#include "gfx/texture.hpp"

#include <fstream>

#define STBTT_malloc(x,u)  malloc(x)
#define STBTT_free(x,u)    free(x)

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <ft2build.h>
#include FT_FREETYPE_H

class FontFT: public Font
{
public:
    FontFT(FontAtlas* atlas, const string& path)
    : atlas(atlas)
    , face(nullptr)
    {
        static FT_Library library = initializeLibrary();
        
        if (FT_New_Face(library, path.c_str(), 0, &face))
            throw runtime_error("Error loading font");
    }
    
    ~FontFT()
    {
        FT_Done_Face(face);
    }
    
    FontMetrics getMetrics(float size) override
    {
        float scale = getScale(size);
        
        return { static_cast<short>(face->ascender * scale), static_cast<short>(face->descender * scale), static_cast<short>(face->height * scale) };
    }
    
    optional<GlyphBitmap> getGlyphBitmap(float size, unsigned int cp) override
    {
        if (optional<GlyphBitmap> cached = atlas->getBitmap(this, size, cp))
            return cached;
        
        unsigned int index = FT_Get_Char_Index(face, cp);
        
        if (index)
        {
            FT_Size_RequestRec req =
            {
                FT_SIZE_REQUEST_TYPE_REAL_DIM,
                0,
                int(size * (1 << 6)),
                0, 0
            };
            
            FT_Request_Size(face, &req);
            FT_Load_Glyph(face, index, FT_LOAD_RENDER);
            
            FT_GlyphSlot glyph = face->glyph;
            
            GlyphMetrics gm = { static_cast<short>(glyph->bitmap_left), static_cast<short>(glyph->bitmap_top), static_cast<short>(glyph->metrics.horiAdvance >> 6) };
            
            unsigned int gw = glyph->bitmap.width;
            unsigned int gh = glyph->bitmap.rows;
            
            vector<unsigned char> pixels((gw + 1) * (gh + 1));
            
            for (unsigned int y = 0; y < gh; ++y)
                memcpy(&pixels[y * (gw + 1)], &glyph->bitmap.buffer[y * glyph->bitmap.pitch], gw);
            
            return atlas->addBitmap(this, size, cp, gm, gw + 1, gh + 1, pixels.data());
        }
        
        return {};
    }
    
    short getKerning(float size, unsigned int cp1, unsigned int cp2) override
    {
        unsigned int index1 = FT_Get_Char_Index(face, cp1);
        unsigned int index2 = FT_Get_Char_Index(face, cp2);
        
        if (index1 && index2)
        {
            FT_Vector result;
            if (FT_Get_Kerning(face, index1, index2, FT_KERNING_UNSCALED, &result) == 0)
            {
                return result.x * getScale(size);
            }
        }
        
        return 0;
    }
    
private:
    static FT_Library initializeLibrary()
    {
        FT_Library result;
        if (FT_Init_FreeType(&result))
            throw runtime_error("Error initializing FreeType");
 
        return result;
    }
    
    float getScale(float size) const
    {
        return size / (face->ascender - face->descender);
    }
    
    FontAtlas* atlas;
    
    FT_Face face;
};

class FontSTB: public Font
{
public:
    FontSTB(FontAtlas* atlas, const string& path)
    : atlas(atlas)
    {
        ifstream in(path, ios::in | ios::binary);
        if (!in)
            throw runtime_error("Error loading font");
        
        in.seekg(0, ios::end);
        istream::pos_type length = in.tellg();
        in.seekg(0, ios::beg);
        
        if (length <= 0)
            throw runtime_error("Error loading font");
        
        data.reset(new char[length]);
        
        in.read(data.get(), length);
        
        if (in.gcount() != length)
            throw runtime_error("Error loading font");
        
        if (!stbtt_InitFont(&font, reinterpret_cast<unsigned char*>(data.get()), 0))
            throw runtime_error("Error loading font");
    }
    
    ~FontSTB()
    {
    }
    
    FontMetrics getMetrics(float size) override
    {
        float scale = getScale(size);
        
        int ascent, descent, linegap;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &linegap);
        
        int height = ascent - descent + linegap;
        
        return { static_cast<short>(ascent * scale), static_cast<short>(descent * scale), static_cast<short>(height * scale) };
    }
    
    optional<GlyphBitmap> getGlyphBitmap(float size, unsigned int cp) override
    {
        if (optional<GlyphBitmap> cached = atlas->getBitmap(this, size, cp))
            return cached;
        
        unsigned int index = stbtt_FindGlyphIndex(&font, cp);
        
        if (index)
        {
            float scale = getScale(size);
            
            int x0, y0, x1, y1;
            stbtt_GetGlyphBitmapBox(&font, index, scale, scale, &x0, &y0, &x1, &y1);
            
            int advance, leftSideBearing;
            stbtt_GetGlyphHMetrics(&font, index, &advance, &leftSideBearing);
            
            GlyphMetrics gm = {static_cast<short>(x0), static_cast<short>(-y0), static_cast<short>(advance * scale)};
            unsigned int gw = x1 - x0;
            unsigned int gh = y1 - y0;
            
            vector<unsigned char> pixels((gw + 1) * (gh + 1));
            
            stbtt_MakeGlyphBitmap(&font, pixels.data(), gw, gh, gw + 1, scale, scale, index);
            
            return atlas->addBitmap(this, size, cp, gm, gw + 1, gh + 1, pixels.data());
        }
        
        return {};
    }
    
    short getKerning(float size, unsigned int cp1, unsigned int cp2) override
    {
        if (!font.kern) return 0;
        
        unsigned int index1 = stbtt_FindGlyphIndex(&font, cp1);
        unsigned int index2 = stbtt_FindGlyphIndex(&font, cp2);
        
        if (index1 && index2)
        {
            return stbtt_GetGlyphKernAdvance(&font, index1, index2) * getScale(size);
        }
        
        return 0;
    }
    
private:
    float getScale(float size)
    {
        return stbtt_ScaleForPixelHeight(&font, size);
    }
    
    FontAtlas* atlas;
    
    stbtt_fontinfo font;
    unique_ptr<char[]> data;
};

Font::~Font()
{
}

bool FontAtlas::GlyphKey::operator==(const GlyphKey& other) const
{
    return font == other.font && size == other.size && cp == other.cp;
}

size_t FontAtlas::GlyphKeyHash::operator()(const GlyphKey& key) const
{
    return hash_combine(hash_value(key.font), hash_combine(hash_value(key.size), hash_value(key.cp)));
}

FontAtlas::FontAtlas(unsigned int atlasWidth, unsigned int atlasHeight)
: layoutBegin(0)
, layoutEnd(atlasHeight)
, layoutLineBegin(0)
, layoutLineEnd(0)
, layoutPosition(0)
{
    texture = make_unique<Texture>(Texture::Type_2D, Texture::Format_R8, atlasWidth, atlasHeight, 1, 1);
}

FontAtlas::~FontAtlas()
{
}
    
optional<Font::GlyphBitmap> FontAtlas::getBitmap(Font* font, float size, unsigned int cp)
{
    GlyphKey key = { font, size, cp };
    auto it = glyphs.find(key);
    
    return (it == glyphs.end()) ? optional<Font::GlyphBitmap>() : make_optional(it->second);
}

optional<Font::GlyphBitmap> FontAtlas::addBitmap(Font* font, float size, unsigned int cp, const Font::GlyphMetrics& metrics, unsigned int width, unsigned int height, const unsigned char* pixels)
{
    GlyphKey key = { font, size, cp };
        
    assert(glyphs.count(key) == 0);
    
    auto result = layoutBitmap(width, height);
    
    if (result)
    {
        unsigned int x = result->first;
        unsigned int y = result->second % texture->getHeight();
        
        texture->upload(0, 0, 0, TextureRegion { x, y, 0, width, height, 1 }, pixels, width * height);
        
        Font::GlyphBitmap bitmap = { metrics, static_cast<short>(x), static_cast<short>(y), static_cast<short>(width), static_cast<short>(height) };
        
        glyphs[key] = bitmap;
        glyphsY.insert(make_pair(result->second, key));
        
        return make_optional(bitmap);
    }
    
    return {};
}

void FontAtlas::flush()
{
    assert(layoutBegin < layoutEnd);
    assert(layoutLineBegin <= layoutLineEnd);
    assert(layoutLineBegin >= layoutBegin && layoutLineEnd <= layoutEnd);
    assert(layoutEnd - layoutBegin <= texture->getHeight());
    
    // Let's figure out how much space we have left and make sure we have at least 1/3 of the texture
    unsigned int layoutHeight = layoutEnd - layoutLineEnd;
    unsigned int layoutDesiredHeight = texture->getHeight() / 3;
    
    if (layoutHeight < layoutDesiredHeight)
    {
        unsigned int difference = layoutDesiredHeight - layoutHeight;
        
        auto begin = glyphsY.lower_bound(layoutBegin);
        auto end = glyphsY.upper_bound(layoutBegin + difference);
    
        for (auto it = begin; it != end; ++it)
            glyphs.erase(it->second);
        
        glyphsY.erase(begin, end);
        
        vector<unsigned char> empty(texture->getWidth());
        
        for (unsigned int i = 0; i < difference; ++i)
        {
            unsigned int y = (layoutBegin + i) % texture->getHeight();
            
            texture->upload(0, 0, 0, TextureRegion { 0, y, 0, texture->getWidth(), 1, 1 }, empty.data(), empty.size());
        }
        
        layoutBegin += difference;
        layoutEnd += difference;
    }
}

static bool isRangeValid(unsigned long long start, unsigned int size, unsigned int wrap)
{
    return start / wrap == (start + size - 1) / wrap;
}

optional<pair<unsigned int, unsigned long long>> FontAtlas::layoutBitmap(unsigned int width, unsigned int height)
{
    // Try to fit in the same line
    if (layoutPosition + width <= texture->getWidth() && layoutLineBegin + height <= layoutEnd && isRangeValid(layoutLineBegin, height, texture->getHeight()))
    {
        auto result = make_pair(layoutPosition, layoutLineBegin);
        
        layoutPosition += width;
        layoutLineEnd = max(layoutLineEnd, layoutLineBegin + height);
        
        return make_optional(result);
    }
    
    // Try to fit in the next line
    if (width <= texture->getWidth() && layoutLineEnd + height <= layoutEnd)
    {
        if (isRangeValid(layoutLineEnd, height, texture->getHeight()))
        {
            auto result = make_pair(0u, layoutLineEnd);
            
            layoutPosition = width;
            layoutLineBegin = layoutLineEnd;
            layoutLineEnd = layoutLineEnd + height;
            
            return make_optional(result);
        }
        else
        {
            // Try to fit with a wraparound
            unsigned long long lineWrap = (layoutLineEnd + height) / texture->getHeight() * texture->getHeight();
            
            if (lineWrap + height <= layoutEnd)
            {
                auto result = make_pair(0u, lineWrap);
                
                layoutPosition = width;
                layoutLineBegin = lineWrap;
                layoutLineEnd = lineWrap + height;
                
                return make_optional(result);
            }
        }
    }
    
    // Fail
    return {};
}

FontLibrary::FontLibrary(unsigned int atlasWidth, unsigned int atlasHeight)
: atlas(make_unique<FontAtlas>(atlasWidth, atlasHeight))
{
}

FontLibrary::~FontLibrary()
{
}

void FontLibrary::addFont(const string& name, const string& path, bool freetype)
{
    assert(fonts.count(name) == 0);
    
    if (freetype)
        fonts[name] = make_unique<FontFT>(atlas.get(), path);
    else
        fonts[name] = make_unique<FontSTB>(atlas.get(), path);
}

Font* FontLibrary::getFont(const string& name)
{
    auto it = fonts.find(name);
    
    return (it == fonts.end()) ? nullptr : it->second.get();
}

void FontLibrary::flush()
{
    atlas->flush();
}