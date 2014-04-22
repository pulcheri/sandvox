#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#include <stdlib.h>
#include <stdio.h>

#include "gfx/program.hpp"
#include "gfx/geometry.hpp"

#include "fs/folderwatcher.hpp"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "eigen.hpp"

#define DUAL 0

struct dual
{
    float v, dx, dy, dz;
    
    dual(float v, float dx, float dy, float dz): v(v), dx(dx), dy(dy), dz(dz) {}
    
    static dual c(float v) { return dual(v, 0, 0, 0); }
    static dual vx(float v) { return dual(v, 1, 0, 0); }
    static dual vy(float v) { return dual(v, 0, 1, 0); }
    static dual vz(float v) { return dual(v, 0, 0, 1); }
};

dual operator-(const dual& v) { return dual(-v.v, -v.dx, -v.dy, -v.dz); }

dual operator+(const dual& l, const dual& r) { return dual(l.v + r.v, l.dx + r.dx, l.dy + r.dy, l.dz + r.dz); }
dual operator-(const dual& l, const dual& r) { return dual(l.v - r.v, l.dx - r.dx, l.dy - r.dy, l.dz - r.dz); }

dual operator*(const dual& l, const dual& r)
{
    return dual(l.v * r.v, l.dx * r.v + l.v * r.dx, l.dy * r.v + l.v * r.dy, l.dz * r.v + l.v * r.dz);
}

dual operator*(const dual& l, float r) { return dual(l.v * r, l.dx * r, l.dy * r, l.dz * r); }
dual operator*(float l, const dual& r) { return dual(l * r.v, l * r.dx, l * r.dy, l * r.dz); }
dual operator/(const dual& l, float r) { return dual(l.v / r, l.dx / r, l.dy / r, l.dz / r); }

dual abs(const dual& v) { return v.v >= 0 ? v : -v; }

dual max(const dual& l, const dual& r) { return l.v >= r.v ? l : r; }
dual min(const dual& l, const dual& r) { return l.v <= r.v ? l : r; }

dual sin(const dual& v) { return dual(sinf(v.v), cosf(v.v)*v.dx, cosf(v.v)*v.dy, cosf(v.v)*v.dz); }
dual cos(const dual& v) { return dual(cosf(v.v), -sinf(v.v)*v.dx, -sinf(v.v)*v.dy, -sinf(v.v)*v.dz); }

dual sqrt(const dual& v) { return v.v > 0 ? dual(sqrtf(v.v), 0.5f/sqrtf(v.v)*v.dx, 0.5f/sqrtf(v.v)*v.dy, 0.5f/sqrtf(v.v)*v.dz) : dual::c(0); }

dual length(const dual& x, const dual& y) { return sqrt(x * x + y * y); }
dual length(const dual& x, const dual& y, const dual& z) { return sqrt(x * x + y * y + z * z); }

static const unsigned short MC_EDGETABLE[] =
{
    0x000, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x099, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x033, 0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0x0aa, 0x7a6, 0x6af, 0x5a5, 0x4ac,
    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x066, 0x16f, 0x265, 0x36c,
    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0x0ff, 0x3f5, 0x2fc,
    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x055, 0x15c,
    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0x0cc,
    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
    0x0cc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
    0x15c, 0x055, 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
    0x2fc, 0x3f5, 0x0ff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
    0x36c, 0x265, 0x16f, 0x066, 0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
    0x4ac, 0x5a5, 0x6af, 0x7a6, 0x0aa, 0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x033, 0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x099, 0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x000
};

static const signed char MC_TRITABLE[][16] =
{
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
    {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
    {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
    {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
    {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
    {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
    {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
    {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
    {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
    {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
    {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
    {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
    {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
    {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
    {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
    {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
    {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
    {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
    {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
    {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
    {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
    {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
    {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
    {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
    {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
    {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
    {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
    {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
    {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
    {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
    {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
    {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
    {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
    {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
    {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
    {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
    {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
    {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
    {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
    {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
    {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
    {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
    {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
    {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
    {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
    {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
    {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
    {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
    {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
    {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
    {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
    {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
    {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
    {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
    {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
    {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
    {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
    {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
    {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
    {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
    {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
    {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
    {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
    {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
    {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
    {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
    {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
    {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
    {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
    {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
    {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
    {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
    {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
    {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
    {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
    {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
    {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
    {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
    {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
    {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
    {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
    {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
    {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
    {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
    {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
    {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
    {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
    {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
    {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
    {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
    {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
    {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
    {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
    {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
    {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
    {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
    {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
    {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
    {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
    {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
    {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
    {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
    {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
    {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
    {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
    {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
    {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
    {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
    {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
    {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
    {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
    {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
    {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
    {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
    {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
    {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
    {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
    {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
    {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
    {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
    {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
    {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
    {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
    {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
    {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
    {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
    {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
    {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
    {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
    {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
    {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
    {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
    {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
    {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
    {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
    {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
    {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
    {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
    {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
    {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
    {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
    {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
    {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
    {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
    {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
    {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
    {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
    {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
    {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
    {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
    {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
    {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};

static const unsigned char MC_VERTINDEX[][3] =
{
    {0, 0, 0},
    {1, 0, 0},
    {1, 1, 0},
    {0, 1, 0},
    {0, 0, 1},
    {1, 0, 1},
    {1, 1, 1},
    {0, 1, 1},
};

static const unsigned char MC_EDGEINDEX[][2] =
{
    {0, 1},
    {1, 2},
    {2, 3},
    {3, 0},
    {4, 5},
    {5, 6},
    {6, 7},
    {7, 4},
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7}
};

struct GridVertex
{
    float iso;
    float nx, ny, nz;
};

struct TerrainVertex
{
    vec3 position;
    vec3 normal;
};

#if DUAL
template <typename F>
GridVertex evaluateSDF(const F& f, const vec3& p)
{
    dual d = f(dual::vx(p.x), dual::vy(p.y), dual::vz(p.z));
    
    return { d.v, d.dx, d.dy, d.dz };
}
#else
template <typename F>
GridVertex evaluateSDF(const F& f, const vec3& p)
{
    float grad = 0.01f;
    
    float iso = f(p);
    float nx = f(p + vec3(grad, 0, 0)) - iso;
    float ny = f(p + vec3(0, grad, 0)) - iso;
    float nz = f(p + vec3(0, 0, grad)) - iso;
    
    return { iso, nx, ny, nz };
}
#endif

namespace MarchingCubes
{
    template <int Lod>
    struct CubeGenerator
    {
        static void generate(
            vector<TerrainVertex>& vb, vector<unsigned int>& ib,
            const GridVertex& v000, const GridVertex& v100, const GridVertex& v110, const GridVertex& v010,
            const GridVertex& v001, const GridVertex& v101, const GridVertex& v111, const GridVertex& v011,
            float isolevel, const vec3& offset, float scale)
        {
            // get cube index from grid values
            int cubeindex = 0;
            
            if (v000.iso < isolevel) cubeindex |= 1 << 0;
            if (v100.iso < isolevel) cubeindex |= 1 << 1;
            if (v110.iso < isolevel) cubeindex |= 1 << 2;
            if (v010.iso < isolevel) cubeindex |= 1 << 3;
            if (v001.iso < isolevel) cubeindex |= 1 << 4;
            if (v101.iso < isolevel) cubeindex |= 1 << 5;
            if (v111.iso < isolevel) cubeindex |= 1 << 6;
            if (v011.iso < isolevel) cubeindex |= 1 << 7;
            
            int edgemask = MC_EDGETABLE[cubeindex];
            
            if (edgemask != 0)
            {
                const GridVertex* grid[2][2][2] =
                {
                    {
                        {&v000, &v100},
                        {&v010, &v110},
                    },
                    {
                        {&v001, &v101},
                        {&v011, &v111},
                    }
                };
                
                // tesselate 2x2x2 grid into 3x3x3 grid & recurse
                GridVertex tgrid[3][3][3];
                
                for (int z = 0; z < 3; ++z)
                    for (int y = 0; y < 3; ++y)
                        for (int x = 0; x < 3; ++x)
                        {
                            int x0 = x >> 1;
                            int x1 = x0 + (x & 1);
                            int y0 = y >> 1;
                            int y1 = y0 + (y & 1);
                            int z0 = z >> 1;
                            int z1 = z0 + (z & 1);
                            
                            tgrid[z][y][x].iso = (
                                                  (*grid[z0][y0][x0]).iso + (*grid[z0][y0][x1]).iso + (*grid[z0][y1][x0]).iso + (*grid[z0][y1][x1]).iso +
                                                  (*grid[z1][y0][x0]).iso + (*grid[z1][y0][x1]).iso + (*grid[z1][y1][x0]).iso + (*grid[z1][y1][x1]).iso) / 8;
                            
                            tgrid[z][y][x].nx = (*grid[z0][y0][x0]).nx;
                            tgrid[z][y][x].ny = (*grid[z0][y0][x0]).ny;
                            tgrid[z][y][x].nz = (*grid[z0][y0][x0]).nz;
                        }
                
                for (int z = 0; z < 2; ++z)
                    for (int y = 0; y < 2; ++y)
                        for (int x = 0; x < 2; ++x)
                        {
                            CubeGenerator<Lod-1>::generate(vb, ib,
                                                           tgrid[z+0][y+0][x+0],
                                                           tgrid[z+0][y+0][x+1],
                                                           tgrid[z+0][y+1][x+1],
                                                           tgrid[z+0][y+1][x+0],
                                                           tgrid[z+1][y+0][x+0],
                                                           tgrid[z+1][y+0][x+1],
                                                           tgrid[z+1][y+1][x+1],
                                                           tgrid[z+1][y+1][x+0],
                                                           isolevel,
                                                           offset + vec3(x, y, z) * (scale / 2),
                                                           scale / 2);
                        }
            }
        }
    };
    
    template <>
    struct CubeGenerator<0>
    {
        static void generate(
            vector<TerrainVertex>& vb, vector<unsigned int>& ib,
            const GridVertex& v000, const GridVertex& v100, const GridVertex& v110, const GridVertex& v010,
            const GridVertex& v001, const GridVertex& v101, const GridVertex& v111, const GridVertex& v011,
            float isolevel, const vec3& offset, float scale)
        {
            // get cube index from grid values
            int cubeindex = 0;
            
            if (v000.iso < isolevel) cubeindex |= 1 << 0;
            if (v100.iso < isolevel) cubeindex |= 1 << 1;
            if (v110.iso < isolevel) cubeindex |= 1 << 2;
            if (v010.iso < isolevel) cubeindex |= 1 << 3;
            if (v001.iso < isolevel) cubeindex |= 1 << 4;
            if (v101.iso < isolevel) cubeindex |= 1 << 5;
            if (v111.iso < isolevel) cubeindex |= 1 << 6;
            if (v011.iso < isolevel) cubeindex |= 1 << 7;
            
            int edgemask = MC_EDGETABLE[cubeindex];
            
            if (edgemask != 0)
            {
                const GridVertex* grid[2][2][2] =
                {
                    {
                        {&v000, &v100},
                        {&v010, &v110},
                    },
                    {
                        {&v001, &v101},
                        {&v011, &v111},
                    }
                };
                
                int edges[12];
                
                // add vertices
                for (int i = 0; i < 12; ++i)
                {
                    if (edgemask & (1 << i))
                    {
                        edges[i] = vb.size();
                        
                        int e0 = MC_EDGEINDEX[i][0];
                        int e1 = MC_EDGEINDEX[i][1];
                        int p0x = MC_VERTINDEX[e0][0];
                        int p0y = MC_VERTINDEX[e0][1];
                        int p0z = MC_VERTINDEX[e0][2];
                        int p1x = MC_VERTINDEX[e1][0];
                        int p1y = MC_VERTINDEX[e1][1];
                        int p1z = MC_VERTINDEX[e1][2];
                        
                        const GridVertex& g0 = *grid[p0z][p0y][p0x];
                        const GridVertex& g1 = *grid[p1z][p1y][p1x];
                        
                        float t =
                        (fabsf(g0.iso - g1.iso) > 0.0001)
                        ? (isolevel - g0.iso) / (g1.iso - g0.iso)
                        : 0;
                        
                        float px = p0x + (p1x - p0x) * t;
                        float py = p0y + (p1y - p0y) * t;
                        float pz = p0z + (p1z - p0z) * t;
                        
                        float nx = g0.nx + (g1.nx - g0.nx) * t;
                        float ny = g0.ny + (g1.ny - g0.ny) * t;
                        float nz = g0.nz + (g1.nz - g0.nz) * t;
                        
                        vb.push_back({ vec3(px, py, pz) * scale + offset, glm::normalize(vec3(nx, ny, nz)) });
                    }
                }
                
                // add indices
                for (int i = 0; i < 15; i += 3)
                {
                    if (MC_TRITABLE[cubeindex][i] < 0)
                        break;
                    
                    ib.push_back(edges[MC_TRITABLE[cubeindex][i+0]]);
                    ib.push_back(edges[MC_TRITABLE[cubeindex][i+2]]);
                    ib.push_back(edges[MC_TRITABLE[cubeindex][i+1]]);
                }
            }
        }
    };
    
    template <int Lod, typename F>
    pair<unique_ptr<Geometry>, unsigned int> generateSDF(
        const F& f, float isolevel, const vec3& min, const vec3& max, float cubesize)
    {
        vector<TerrainVertex> vb;
        vector<unsigned int> ib;
        
        int sizeX = ceil((max.x - min.x) / cubesize);
        int sizeY = ceil((max.y - min.y) / cubesize);
        int sizeZ = ceil((max.z - min.z) / cubesize);
        
        assert(sizeX > 0 && sizeY > 0 && sizeZ > 0);
        
        unique_ptr<GridVertex[]> grid(new GridVertex[sizeX * sizeY * sizeZ]);
        
        for (int z = 0; z < sizeZ; ++z)
            for (int y = 0; y < sizeY; ++y)
                for (int x = 0; x < sizeX; ++x)
                    grid[x + sizeX * (y + sizeY * z)] = evaluateSDF(f, min + vec3(x, y, z) * cubesize);
        
        for (int z = 0; z + 1 < sizeZ; ++z)
            for (int y = 0; y + 1 < sizeY; ++y)
                for (int x = 0; x + 1 < sizeX; ++x)
                {
                    const GridVertex& v000 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v100 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v110 = grid[(x + 1) + sizeX * ((y + 1) + sizeY * (z + 0))];
                    const GridVertex& v010 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 0))];
                    const GridVertex& v001 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 1))];
                    const GridVertex& v101 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 1))];
                    const GridVertex& v111 = grid[(x + 1) + sizeX * ((y + 1) + sizeY * (z + 1))];
                    const GridVertex& v011 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 1))];
                    
                    CubeGenerator<Lod>::generate(vb, ib, v000, v100, v110, v010, v001, v101, v111, v011, isolevel, min + vec3(x, y, z) * cubesize, cubesize);
                }
        
        if (ib.empty())
            return make_pair(unique_ptr<Geometry>(), 0);
        
        shared_ptr<Buffer> gvb = make_shared<Buffer>(Buffer::Type_Vertex, sizeof(TerrainVertex), vb.size(), Buffer::Usage_Static);
        shared_ptr<Buffer> gib = make_shared<Buffer>(Buffer::Type_Index, sizeof(unsigned int), ib.size(), Buffer::Usage_Static);
    
        gvb->upload(0, vb.data(), vb.size() * sizeof(TerrainVertex));
        gib->upload(0, ib.data(), ib.size() * sizeof(unsigned int));
        
        vector<Geometry::Element> layout =
        {
            Geometry::Element(offsetof(TerrainVertex, position), Geometry::Format_Float3),
            Geometry::Element(offsetof(TerrainVertex, normal), Geometry::Format_Float3),
        };
        
        return make_pair(unique_ptr<Geometry>(new Geometry(layout, gvb, gib)), ib.size());
    }
}

namespace SurfaceNets
{
    template <typename F> struct NaiveTraits
    {
        static pair<vec3, vec3> intersect(const GridVertex& g0, const GridVertex& g1, const F& f, float isolevel, const vec3& corner, const vec3& v0, const vec3& v1)
        {
            float t =
                (fabsf(g0.iso - g1.iso) > 0.0001)
                ? (isolevel - g0.iso) / (g1.iso - g0.iso)
                : 0;
            
            return make_pair(glm::mix(v0, v1, t), glm::mix(vec3(g0.nx, g0.ny, g0.nz), vec3(g1.nx, g1.ny, g1.nz), t));
        }
        
        static pair<vec3, vec3> average(const pair<vec3, vec3>* points, size_t count, const vec3& v0, const vec3& v1, const vec3& corner)
        {
            vec3 position;
            vec3 normal;
            
            for (size_t i = 0; i < count; ++i)
            {
                position += points[i].first;
                normal += points[i].second;
            }
            
            float n = 1.f / count;
            
            return make_pair(position * n, normal * n);
        }
    };
    
    template <typename LerpK> struct AdjustableNaiveTraits
    {
        template <typename F> struct Value
        {
            static pair<vec3, vec3> intersect(const GridVertex& g0, const GridVertex& g1, const F& f, float isolevel, const vec3& corner, const vec3& v0, const vec3& v1)
            {
                float t =
                    (fabsf(g0.iso - g1.iso) > 0.0001)
                    ? (isolevel - g0.iso) / (g1.iso - g0.iso)
                    : 0;
                
                return make_pair(glm::mix(v0, v1, t), glm::normalize(glm::mix(vec3(g0.nx, g0.ny, g0.nz), vec3(g1.nx, g1.ny, g1.nz), t)));
            }
            
            static pair<vec3, vec3> average(const pair<vec3, vec3>* points, size_t count, const vec3& v0, const vec3& v1, const vec3& corner)
            {
                vec3 position;
                vec3 normal;
                
                for (size_t i = 0; i < count; ++i)
                {
                    position += points[i].first;
                    normal += points[i].second;
                }
                
                float n = 1.f / count;
                
                return make_pair(LerpK()(corner, position * n, (v0 + v1) / 2.f), normal * n);
            }
        };
    };
    
    template <typename F> struct DualContouringTraits
    {
        static pair<vec3, vec3> intersect(const GridVertex& g0, const GridVertex& g1, const F& f, float isolevel, const vec3& corner, const vec3& v0, const vec3& v1)
        {
            if (g0.iso > g1.iso)
                return intersect(g1, g0, f, isolevel, corner, v1, v0);
            
            float mint = 0;
            float miniso = g0.iso;
            float maxt = 1;
            float maxiso = g1.iso;
            
            for (int i = 0; i < 10; ++i)
            {
                float t = (isolevel - miniso) / (maxiso - miniso) * (maxt - mint) + mint;
                float iso = evaluateSDF(f, glm::mix(v0, v1, t) + corner).iso;
                
                if (iso < isolevel)
                    mint = t, miniso = iso;
                else
                    maxt = t, maxiso = iso;
            }
            
            float t = (mint + maxt) / 2;
            
            GridVertex g = evaluateSDF(f, glm::mix(v0, v1, t) + corner);
            
            return make_pair(glm::mix(v0, v1, t), glm::normalize(vec3(g.nx, g.ny, g.nz)));
        }
        
        static pair<vec3, vec3> average(const pair<vec3, vec3>* points, size_t count, const vec3& v0, const vec3& v1, const vec3& corner)
        {
            float mp[3] = {}; // this is the minimizer point of the QEF
            float ata[6] = {}, atb[3] = {}, btb = 0; // QEF data
            float pt[3] = {};

            for (size_t i = 0; i < count; ++i)
            {
                const vec3& p = points[i].first;
                const vec3& n = points[i].second;

                // QEF
                ata[ 0 ] += (float) ( n[ 0 ] * n[ 0 ] );
                ata[ 1 ] += (float) ( n[ 0 ] * n[ 1 ] );
                ata[ 2 ] += (float) ( n[ 0 ] * n[ 2 ] );
                ata[ 3 ] += (float) ( n[ 1 ] * n[ 1 ] );
                ata[ 4 ] += (float) ( n[ 1 ] * n[ 2 ] );
                ata[ 5 ] += (float) ( n[ 2 ] * n[ 2 ] );
                double pn = p[0] * n[0] + p[1] * n[1] + p[2] * n[2] ;
                atb[ 0 ] += (float) ( n[ 0 ] * pn ) ;
                atb[ 1 ] += (float) ( n[ 1 ] * pn ) ;
                atb[ 2 ] += (float) ( n[ 2 ] * pn ) ;
                btb += (float) pn * (float) pn ;
                // Minimizer
                pt[0] += p[0] ;
                pt[1] += p[1] ;
                pt[2] += p[2] ;
            }
            // we minimize towards the average of all intersection points
            pt[0] /= count ;
            pt[1] /= count ;
            pt[2] /= count ;
            // Solve
            float mat[10] ;
            BoundingBoxf box;
            box.begin.x = (float) v0[0] ;
            box.begin.y = (float) v0[1] ;
            box.begin.z = (float) v0[2] ;
            box.end.x = (float) v1[0];
            box.end.y = (float) v1[1];
            box.end.z = (float) v1[2];

            // eigen.hpp
            // calculate minimizer point, and return error
            // QEF: ata, atb, btb
            // pt is the average of the intersection points
            // mp is the result
            // box is a bounding-box for this node
            // mat is storage for calcPoint() ?
            double err = calcPoint( ata, atb, btb, pt, mp, &box, mat ) ;
            
            return make_pair(glm::clamp(vec3(mp[0], mp[1], mp[2]), v0, v1), NaiveTraits<F>::average(points, count, v0, v1, corner).second);
        }
    };
    
    TerrainVertex normalLerp(const TerrainVertex& v, const vec3& avgp, const vec3& qn)
    {
        float k = glm::dot(v.position - avgp, v.position - avgp);
        
        k = 1 - (1 - k) * (1 - k);
        
        return TerrainVertex {v.position, glm::normalize(glm::mix(v.normal, qn, glm::clamp(k, 0.f, 1.f)))};
    }
    
    void pushQuad(vector<TerrainVertex>& vb, vector<unsigned int>& ib,
        const pair<vec3, TerrainVertex>& v0, const pair<vec3, TerrainVertex>& v1, const pair<vec3, TerrainVertex>& v2, const pair<vec3, TerrainVertex>& v3,
        bool flip)
    {
        size_t offset = vb.size();
        
        vec3 qn = (flip ? -1.f : 1.f) * glm::normalize(glm::cross(v1.second.position - v0.second.position, v2.second.position - v0.second.position));
        
        vb.push_back(normalLerp(v0.second, v0.first, qn));
        vb.push_back(normalLerp(v1.second, v1.first, qn));
        vb.push_back(normalLerp(v2.second, v2.first, qn));
        vb.push_back(normalLerp(v3.second, v3.first, qn));
        
        if (flip)
        {
            ib.push_back(offset + 0);
            ib.push_back(offset + 2);
            ib.push_back(offset + 1);
            ib.push_back(offset + 0);
            ib.push_back(offset + 3);
            ib.push_back(offset + 2);
        }
        else
        {
            ib.push_back(offset + 0);
            ib.push_back(offset + 1);
            ib.push_back(offset + 2);
            ib.push_back(offset + 0);
            ib.push_back(offset + 2);
            ib.push_back(offset + 3);
        }
    }
    
    template <template <typename> class Traits, typename F>
    pair<unique_ptr<Geometry>, unsigned int> generateSDF(
        const F& f, float isolevel, const vec3& min, const vec3& max, float cubesize)
    {
        vector<TerrainVertex> vb;
        vector<unsigned int> ib;
        
        int sizeX = ceil((max.x - min.x) / cubesize);
        int sizeY = ceil((max.y - min.y) / cubesize);
        int sizeZ = ceil((max.z - min.z) / cubesize);
        
        assert(sizeX > 0 && sizeY > 0 && sizeZ > 0);
        
        unique_ptr<GridVertex[]> grid(new GridVertex[sizeX * sizeY * sizeZ]);
        
        for (int z = 0; z < sizeZ; ++z)
            for (int y = 0; y < sizeY; ++y)
                for (int x = 0; x < sizeX; ++x)
                    grid[x + sizeX * (y + sizeY * z)] = evaluateSDF(f, min + vec3(x, y, z) * cubesize);
        
        unique_ptr<pair<vec3, TerrainVertex>[]> gv(new pair<vec3, TerrainVertex>[sizeX * sizeY * sizeZ]);
        
        for (int z = 0; z + 1 < sizeZ; ++z)
            for (int y = 0; y + 1 < sizeY; ++y)
                for (int x = 0; x + 1 < sizeX; ++x)
                {
                    const GridVertex& v000 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v100 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v110 = grid[(x + 1) + sizeX * ((y + 1) + sizeY * (z + 0))];
                    const GridVertex& v010 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 0))];
                    const GridVertex& v001 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 1))];
                    const GridVertex& v101 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 1))];
                    const GridVertex& v111 = grid[(x + 1) + sizeX * ((y + 1) + sizeY * (z + 1))];
                    const GridVertex& v011 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 1))];
                    
                    // get cube index from grid values
                    int cubeindex = 0;
                    
                    if (v000.iso < isolevel) cubeindex |= 1 << 0;
                    if (v100.iso < isolevel) cubeindex |= 1 << 1;
                    if (v110.iso < isolevel) cubeindex |= 1 << 2;
                    if (v010.iso < isolevel) cubeindex |= 1 << 3;
                    if (v001.iso < isolevel) cubeindex |= 1 << 4;
                    if (v101.iso < isolevel) cubeindex |= 1 << 5;
                    if (v111.iso < isolevel) cubeindex |= 1 << 6;
                    if (v011.iso < isolevel) cubeindex |= 1 << 7;
                    
                    int edgemask = MC_EDGETABLE[cubeindex];
                    
                    if (edgemask != 0)
                    {
                        vec3 corner = vec3(x, y, z) * cubesize + min;
                        
                        pair<vec3, vec3> ev[12];
                        size_t ecount = 0;
                        vec3 evavg;
                        
                        // add vertices
                        for (int i = 0; i < 12; ++i)
                        {
                            if (edgemask & (1 << i))
                            {
                                int e0 = MC_EDGEINDEX[i][0];
                                int e1 = MC_EDGEINDEX[i][1];
                                int p0x = MC_VERTINDEX[e0][0];
                                int p0y = MC_VERTINDEX[e0][1];
                                int p0z = MC_VERTINDEX[e0][2];
                                int p1x = MC_VERTINDEX[e1][0];
                                int p1y = MC_VERTINDEX[e1][1];
                                int p1z = MC_VERTINDEX[e1][2];
                                
                                const GridVertex& g0 = grid[(x + p0x) + sizeX * ((y + p0y) + sizeY * (z + p0z))];
                                const GridVertex& g1 = grid[(x + p1x) + sizeX * ((y + p1y) + sizeY * (z + p1z))];
                                
                                pair<vec3, vec3> gt = Traits<F>::intersect(g0, g1, f, isolevel, corner, vec3(p0x, p0y, p0z) * cubesize, vec3(p1x, p1y, p1z) * cubesize);
                                
                                ev[ecount++] = gt;
                                evavg += gt.first;
                            }
                        }
                        
                        pair<vec3, vec3> ga = Traits<F>::average(ev, ecount, vec3(), vec3(cubesize), corner);
                        
                        gv[x + sizeX * (y + sizeY * z)] = make_pair(corner + evavg / float(ecount), TerrainVertex { corner + ga.first, glm::normalize(ga.second) });
                    }
                }
        
        for (int z = 1; z + 1 < sizeZ; ++z)
            for (int y = 1; y + 1 < sizeY; ++y)
                for (int x = 1; x + 1 < sizeX; ++x)
                {
                    const GridVertex& v000 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v100 = grid[(x + 1) + sizeX * ((y + 0) + sizeY * (z + 0))];
                    const GridVertex& v010 = grid[(x + 0) + sizeX * ((y + 1) + sizeY * (z + 0))];
                    const GridVertex& v001 = grid[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 1))];
                    
                    // add quads
                    if ((v000.iso < isolevel) != (v100.iso < isolevel))
                    {
                        pushQuad(vb, ib,
                            gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))],
                            gv[(x + 0) + sizeX * ((y - 1) + sizeY * (z + 0))],
                            gv[(x + 0) + sizeX * ((y - 1) + sizeY * (z - 1))],
                            gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z - 1))],
                            !(v000.iso < isolevel));
                    }
                    
                    if ((v000.iso < isolevel) != (v010.iso < isolevel))
                    {
                        pushQuad(vb, ib,
                            gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))],
                            gv[(x - 1) + sizeX * ((y + 0) + sizeY * (z + 0))],
                            gv[(x - 1) + sizeX * ((y + 0) + sizeY * (z - 1))],
                            gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z - 1))],
                            v000.iso < isolevel);
                    }
                    
                    if ((v000.iso < isolevel) != (v001.iso < isolevel))
                    {
                        pushQuad(vb, ib,
                            gv[(x + 0) + sizeX * ((y + 0) + sizeY * (z + 0))],
                            gv[(x - 1) + sizeX * ((y + 0) + sizeY * (z + 0))],
                            gv[(x - 1) + sizeX * ((y - 1) + sizeY * (z + 0))],
                            gv[(x + 0) + sizeX * ((y - 1) + sizeY * (z + 0))],
                            !(v000.iso < isolevel));
                    }
                }
        
        if (ib.empty())
            return make_pair(unique_ptr<Geometry>(), 0);
        
        shared_ptr<Buffer> gvb = make_shared<Buffer>(Buffer::Type_Vertex, sizeof(TerrainVertex), vb.size(), Buffer::Usage_Static);
        shared_ptr<Buffer> gib = make_shared<Buffer>(Buffer::Type_Index, sizeof(unsigned int), ib.size(), Buffer::Usage_Static);
    
        gvb->upload(0, vb.data(), vb.size() * sizeof(TerrainVertex));
        gib->upload(0, ib.data(), ib.size() * sizeof(unsigned int));
        
        vector<Geometry::Element> layout =
        {
            Geometry::Element(offsetof(TerrainVertex, position), Geometry::Format_Float3),
            Geometry::Element(offsetof(TerrainVertex, normal), Geometry::Format_Float3),
        };
        
        return make_pair(make_unique<Geometry>(layout, gvb, gib), ib.size());
    }
}

namespace World
{
#if DUAL
    template <typename T>
    auto mktransform(T t, const mat4& xf)
    {
        mat4 xfi = glm::inverse(xf);
        
        return [=](const dual& px, const dual& py, const dual& pz)
        {
            dual ptx = px * xfi[0][0] + py * xfi[1][0] + pz * xfi[2][0] + dual::c(xfi[3][0]);
            dual pty = px * xfi[0][1] + py * xfi[1][1] + pz * xfi[2][1] + dual::c(xfi[3][1]);
            dual ptz = px * xfi[0][2] + py * xfi[1][2] + pz * xfi[2][2] + dual::c(xfi[3][2]);
            
            return t(ptx, pty, ptz);
        };
    }
    
    template <typename T>
    auto mktranslate(T t, const vec3& v)
    {
        return mktransform(t, glm::translate(mat4(), v));
    }
    
    template <typename T>
    auto mkrotate(T t, float angle, const vec3& axis)
    {
        return mktransform(t, glm::rotate(mat4(), angle, axis));
    }
    
    auto mksphere(float radius)
    {
        return [=](const dual& px, const dual& py, const dual& pz)
        {
            return length(px, py, pz) - dual::c(radius);
        };
    }
    
    auto mkbox(float ex, float ey, float ez)
    {
        return [=](const dual& px, const dual& py, const dual& pz)
        {
            dual dx = abs(px) - dual::c(ex), dy = abs(py) - dual::c(ey), dz = abs(pz) - dual::c(ez);
            
            dual face = min(max(dx, max(dy, dz)), dual::c(0));
            dual edge = length(max(dx, dual::c(0)), max(dy, dual::c(0)), max(dz, dual::c(0)));
            
            return face + edge;
        };
    }
    
    auto mkcone(float radius, float height)
    {
        return [=](const dual& px, const dual& py, const dual& pz)
        {
            dual q = length(px, py);
            return pz.v <= 0
                ? length(pz, max(q - dual::c(radius), dual::c(0)))
                : pz.v > height
                    ? length(px, py, pz - dual::c(height))
                    : q - (dual::c(1) - pz / height) * radius;
        };
    }
    
    template <typename T, typename U>
    auto mkunion(T t, U u)
    {
        return [=](const dual& px, const dual& py, const dual& pz) { return min(t(px, py, pz), u(px, py, pz)); };
    }
    
    template <typename T, typename U>
    auto mksubtract(T t, U u)
    {
        return [=](const dual& px, const dual& py, const dual& pz) { return max(t(px, py, pz), -u(px, py, pz)); };
    }
    
    template <typename T, typename U>
    auto mkintersect(T t, U u)
    {
        return [=](const dual& px, const dual& py, const dual& pz) { return max(t(px, py, pz), u(px, py, pz)); };
    }
    
    template <typename T>
    auto mktwist(T t, float scale)
    {
        return [=](const dual& px, const dual& py, const dual& pz)
        {
            dual angle = -pz * scale;
            dual sina = sin(angle), cosa = cos(angle);
            
            dual prx = px * cosa + py * sina;
            dual pry = px * -sina + py * cosa;
            
            return t(prx, pry, pz);
        };
    }
#else
    template <typename T>
    auto mktransform(T t, const mat4& xf)
    {
        mat4 xfi = glm::inverse(xf);
        
        return [=](const vec3& p) { return t(vec3(xfi * vec4(p, 1))); };
    }
    
    template <typename T>
    auto mktranslate(T t, const vec3& v)
    {
        return mktransform(t, glm::translate(mat4(), v));
    }
    
    template <typename T>
    auto mkrotate(T t, float angle, const vec3& axis)
    {
        return mktransform(t, glm::rotate(mat4(), angle, axis));
    }
    
    auto mksphere(float radius)
    {
        return [=](const vec3& p) { return glm::length(p) - radius; };
    }
    
    auto mkbox(float ex, float ey, float ez)
    {
        return [=](const vec3& p) { vec3 d = glm::abs(p) - vec3(ex, ey, ez); return min(max(d.x, max(d.y, d.z)), 0.f) + glm::length(glm::max(d, 0.f)); };
    }
    
    auto mkcone(float radius, float height)
    {
        return [=](const vec3& p) { float q = glm::length(vec2(p.x, p.y)); return p.z <= 0 ? glm::length(vec2(p.z, max(q - radius, 0.f))) : p.z > height ? glm::distance(p, vec3(0, 0, height)) : q - (1 - p.z / height) * radius; };
    }
    
    template <typename T, typename U>
    auto mkunion(T t, U u)
    {
        return [=](const vec3& p) { return min(t(p), u(p)); };
    }
    
    template <typename T, typename U>
    auto mksubtract(T t, U u)
    {
        return [=](const vec3& p) { return max(t(p), -u(p)); };
    }
    
    template <typename T, typename U>
    auto mkintersect(T t, U u)
    {
        return [=](const vec3& p) { return max(t(p), u(p)); };
    }
    
    template <typename T>
    auto mktwist(T t, float scale)
    {
        return [=](const vec3& p) { return t(vec3(glm::rotate(mat4(), p.z * scale, vec3(0.f, 0.f, 1.f)) * vec4(p, 0))); };
    }
#endif
    
    auto mkworld()
    {
    #if 0
        return mkrotate(mkcone(2, 6), glm::radians(90.f), vec3(0, 1, 0));
    #else
        return
            mkunion(
                mkunion(
                    mkunion(
                        mkunion(
                            mksubtract(
                                mksubtract(
                                    mkunion(
                                        mkunion(
                                            mktranslate(mksphere(5), vec3(7, 0, 0)),
                                            mktranslate(mksphere(7), vec3(-7, 0, 0))),
                                        mkbox(10, 8, 1)),
                                    mktranslate(mkbox(2, 2, 2), vec3(0, 7.5f, 0))),
                                mktranslate(mkbox(2, 2, 2), vec3(0, -7, 0))),
                            mktranslate(mkrotate(mkcone(2, 6), glm::radians(90.f), vec3(0, 1, 0)), vec3(11, 0, 0))),
                        mktranslate(mkrotate(mkbox(3, 3, 1), glm::radians(45.f), vec3(0, 0, 1)), vec3(5, 8, -2))),
                    mktranslate(
                        mkintersect(
                            mkrotate(mkbox(6, 6, 6), glm::radians(45.f), glm::normalize(vec3(1, 1, 0))),
                            mkbox(6, 6, 6)), vec3(-30, 0, 0))),
            mktranslate(mktwist(mkbox(4, 4, 10), 1.f / 10.f), vec3(30, 0, 0)));
    #endif
    }
}

static void error_callback(int error, const char* description)
{
    fputs(description, stderr);
}

struct AdjustableLerpKConstant
{
    vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
    {
        return glm::mix(smoothpt, centerpt, 0.5f);
    }
};

struct AdjustableLerpKHeight
{
    vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
    {
        return glm::mix(smoothpt, centerpt, glm::smoothstep(2.f, 2.5f, corner.z));
    }
};

struct AdjustableLerpKRandom
{
    vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
    {
        return glm::clamp(glm::mix(smoothpt, centerpt + (vec3(rand() / float(RAND_MAX), rand() / float(RAND_MAX), rand() / float(RAND_MAX)) * 2.f - vec3(1.f)) * 0.3f, 0.5f), vec3(0.f), vec3(1.f));
    }
};

struct AdjustableLerpKRandomRight
{
    vec3 operator()(const vec3& corner, const vec3& smoothpt, const vec3& centerpt) const
    {
        return glm::mix(smoothpt, AdjustableLerpKRandom()(corner, smoothpt, centerpt), glm::smoothstep(3.f, 4.f, corner.x));
    }
};

pair<unique_ptr<Geometry>, unsigned int> generateWorld(int index)
{
    float isolevel = 0.01f;
    vec3 min = vec3(-40, -16, -16);
    vec3 max = vec3(40, 16, 16);

    switch (index + 1)
    {
    case 1:
        return MarchingCubes::generateSDF<0>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 2:
        return SurfaceNets::generateSDF<SurfaceNets::NaiveTraits>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 3:
        return SurfaceNets::generateSDF<SurfaceNets::AdjustableNaiveTraits<AdjustableLerpKConstant>::Value>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 4:
        return SurfaceNets::generateSDF<SurfaceNets::AdjustableNaiveTraits<AdjustableLerpKHeight>::Value>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 5:
        return SurfaceNets::generateSDF<SurfaceNets::AdjustableNaiveTraits<AdjustableLerpKRandom>::Value>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 6:
        return SurfaceNets::generateSDF<SurfaceNets::AdjustableNaiveTraits<AdjustableLerpKRandomRight>::Value>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 7:
        return SurfaceNets::generateSDF<SurfaceNets::DualContouringTraits>(World::mkworld(), isolevel, min, max, 1);
        break;
        
    case 8:
        return SurfaceNets::generateSDF<SurfaceNets::DualContouringTraits>(World::mkworld(), isolevel, min, max, 1.f / 2.f);
        break;
        
    default:
        return make_pair(unique_ptr<Geometry>(), 0);
    }
}

bool wireframe = false;
float camerau = 1.5;
float camerav = 0.8;
float cameradist = 49;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        wireframe = !wireframe;
    
    float camerauspeed = 0.1f;
    float cameravspeed = 0.1f;
    float cameradistspeed = 1;
    
    if (key == GLFW_KEY_LEFT)
        camerau -= camerauspeed;
    
    if (key == GLFW_KEY_RIGHT)
        camerau += camerauspeed;
    
    if (key == GLFW_KEY_DOWN)
        camerav -= cameravspeed;
    
    if (key == GLFW_KEY_UP)
        camerav += cameravspeed;
    
    if (key == GLFW_KEY_PAGE_UP)
        cameradist -= cameradistspeed;
    
    if (key == GLFW_KEY_PAGE_DOWN)
        cameradist += cameradistspeed;
    
    if (action == GLFW_PRESS && (key >= GLFW_KEY_1 && key <= GLFW_KEY_9))
    {
        auto p = static_cast<pair<unique_ptr<Geometry>, unsigned int>*>(glfwGetWindowUserPointer(window));
        
        clock_t start = clock();
        *p = generateWorld(key - GLFW_KEY_1);
        clock_t end = clock();
        
        printf("Generated world %d (%d tri) in %.1f msec\n", key - GLFW_KEY_1, p->second / 3, (end - start) * 1000.0 / CLOCKS_PER_SEC);
    }
}

int main()
{
    glfwSetErrorCallback(error_callback);
    
    if (!glfwInit())
        exit(EXIT_FAILURE);
    
    glfwWindowHint(GLFW_VISIBLE, false);
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
 
    GLFWwindow* window = glfwCreateWindow(1280, 720, "sandvox", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    
    glfwMakeContextCurrent(window);
    
    glfwShowWindow(window);
    glfwSetKeyCallback(window, key_callback);
    
    printf("Version: %s\n", glGetString(GL_VERSION));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    
    FolderWatcher fw("../..");
    ProgramManager pm("../../src/shaders", &fw);
    
    auto geom = generateWorld(0);
    
    glfwSetWindowUserPointer(window, &geom);
    
    while (!glfwWindowShouldClose(window))
    {
        fw.processChanges();
        
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        
        float su = sinf(camerau), cu = cosf(camerau);
        float sv = sinf(camerav), cv = cosf(camerav);
        
        mat4 view = glm::lookAt(cameradist * vec3(cu * sv, su * sv, cv), vec3(0, 0, 0), vec3(0, 0, 1));
        mat4 proj = glm::perspectiveFov(glm::radians(60.f), float(width), float(height), 0.1f, 1000.f);
        mat4 viewproj = proj * view;
        
        glViewport(0, 0, width, height);
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClearDepth(1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        
        if (Program* prog = pm.get("terrain-vs", "terrain-fs"))
        {
            prog->bind();
            
            int location = glGetUniformLocation(prog->getId(), "ViewProjection");
            assert(location >= 0);
            
            glUniformMatrix4fv(location, 1, false, glm::value_ptr(viewproj));
            
            if (geom.first)
                geom.first->draw(Geometry::Primitive_Triangles, 0, geom.second);
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwDestroyWindow(window);
    
    glfwTerminate();
}