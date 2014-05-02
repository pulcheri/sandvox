#include "renderer.hpp"

#include "gfx/geometry.hpp"
#include "gfx/program.hpp"
#include "gfx/texture.hpp"

#include "ui/font.hpp"

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

static const uint8_t utf8d[] =
{
    // The first part of the table maps bytes to character classes that
    // to reduce the size of the transition table and create bitmasks.
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,
    
    // The second part is a transition table that maps a combination
    // of a state of the automaton and a character class to a state.
    0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
    12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
    12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
    12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
    12,36,12,12,12,12,12,12,12,12,12,12, 
};

inline uint32_t utf8decode(uint32_t* state, uint32_t* codep, uint32_t byte)
{
    uint32_t type = utf8d[byte];
    *codep = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codep << 6) : (0xff >> type) & (byte);
    *state = utf8d[256 + *state + type];
    return *state;
}

namespace ui
{
	Renderer::Renderer(FontLibrary& fonts, Program* program)
    : fonts(fonts)
    , program(program)
    , canvasDensity(1)
	{
	}

	Renderer::~Renderer()
	{
	}

    void Renderer::begin(unsigned int width, unsigned int height, float density)
    {
        canvasScale = vec2(2.f / width, -2.f / height);
        canvasOffset = vec2(-1, 1);
        canvasDensity = density;
    }
    
    void Renderer::rect(const vec2& x0y0, const vec2& x1y1, float r, const vec4& color)
    {
    }
    
    void Renderer::text(const vec2& pos, const string& font, const string& text, float size, const vec4& color)
    {
        ui::Font* f = fonts.getFont(font);
        if (!f) return;
        
        float su = 1.f / fonts.getTexture()->getWidth();
        float sv = 1.f / fonts.getTexture()->getHeight();
        
        Font::FontMetrics metrics = f->getMetrics(size);
        
        vec2 pen = pos - vec2(0, metrics.ascender);
        
        unsigned int lastch = 0;
        
        uint32_t utfstate = 0;
        uint32_t utfcode = 0;
        
        for (char ch: text)
        {
            if (utf8decode(&utfstate, &utfcode, static_cast<unsigned char>(ch)) != UTF8_ACCEPT)
                continue;
            
            if (auto bitmap = f->getGlyphBitmap(size, utfcode))
            {
                pen += f->getKerning(size, lastch, utfcode);
                
                float x0 = pen.x + bitmap->metrics.bearingX;
                float y0 = pen.y - bitmap->metrics.bearingY;
                float x1 = x0 + bitmap->w;
                float y1 = y0 + bitmap->h;
                
                float u0 = su * bitmap->x;
                float u1 = su * (bitmap->x + bitmap->w);
                float v0 = sv * bitmap->y;
                float v1 = sv * (bitmap->y + bitmap->h);
                
                push(vec2(x0, y0), vec2(u0, v0), color);
                push(vec2(x1, y0), vec2(u1, v0), color);
                push(vec2(x1, y1), vec2(u1, v1), color);
                
                push(vec2(x0, y0), vec2(u0, v0), color);
                push(vec2(x1, y1), vec2(u1, v1), color);
                push(vec2(x0, y1), vec2(u0, v1), color);
                
                pen.x += bitmap->metrics.advance;
                
                lastch = utfcode;
            }
            else
            {
                lastch = 0;
            }
        }
    }
    
    void Renderer::end()
    {
        fonts.flush();
    
        if (vertices.empty())
            return;
        
        if (!vb || vb->getElementCount() < vertices.size())
        {
            size_t size = 256;
            while (size < vertices.size()) size += size * 3 / 2;
            
            vector<Geometry::Element> layout =
            {
                { 0, offsetof(Vertex, pos), Geometry::Format_Float2 },
                { 0, offsetof(Vertex, uv), Geometry::Format_Short2 },
                { 0, offsetof(Vertex, color), Geometry::Format_Color }
            };
            
            vb = make_shared<Buffer>(Buffer::Type_Vertex, sizeof(Vertex), size, Buffer::Usage_Dynamic);
            geometry = make_unique<Geometry>(layout, vb);
        }
 
        vb->upload(0, vertices.data(), vertices.size() * sizeof(Vertex));
        
        if (program)
        {
            program->bind();
            fonts.getTexture()->bind(0);
            geometry->draw(Geometry::Primitive_Triangles, 0, vertices.size());
        }
        
        vertices.clear();
	}
    
    void Renderer::push(const vec2& pos, const vec2& uv, const vec4& color)
    {
        vertices.push_back({ pos * canvasScale + canvasOffset, glm::i16vec2(uv * 8192.f), glm::u8vec4(color * 255.f + 0.5f) });
    }
        
}