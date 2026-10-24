#ifndef ZELDA3_TYPES_H_
#define ZELDA3_TYPES_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

// Build time config options
enum {
  kEnableLargeScreen = 1,
  // How much extra spacing to add on the sides
  kPpuExtraLeftRight = kEnableLargeScreen ? 96 : 0,
};

typedef uint8_t uint8;
typedef int8_t int8;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint64_t uint64;
typedef int64_t int64;
typedef unsigned int uint;

#define arraysize(x) sizeof(x)/sizeof(x[0])
#define sign8(x) ((x) & 0x80)
#define sign16(x) ((x) & 0x8000)
#define load24(x) (RAM32(&(x))&0xffffff)

#ifdef _MSC_VER
#define countof _countof
#define NORETURN __declspec(noreturn)
#define FORCEINLINE __forceinline
#define NOINLINE __declspec(noinline)
#else
#define countof(a) (sizeof(a)/sizeof(*(a)))
#define NORETURN
#define FORCEINLINE inline
#define NOINLINE
#endif

#ifdef _DEBUG
#define kDebugFlag 1
#else
#define kDebugFlag 0
#endif

static FORCEINLINE uint16 abs16(uint16 t) { return sign16(t) ? -t : t; }
static FORCEINLINE uint8 abs8(uint8 t) { return sign8(t) ? -t : t; }
static FORCEINLINE int IntMin(int a, int b) { return a < b ? a : b; }
static FORCEINLINE int IntMax(int a, int b) { return a > b ? a : b; }
static FORCEINLINE uint UintMin(uint a, uint b) { return a < b ? a : b; }
static FORCEINLINE uint UintMax(uint a, uint b) { return a > b ? a : b; }

// windows.h defines this too
#ifdef HIBYTE
#undef HIBYTE
#endif

// ---- Endianness ---------------------------------------------------------------------------
// The game's RAM, ROM-derived assets and save data are little-endian (SNES). On a big-endian
// host (Xbox 360) multi-byte accesses go through types the compiler byte-swaps for us.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ZELDA_BIG_ENDIAN_HOST 1
#define ZELDA_LE_STRUCT __attribute__((scalar_storage_order("little-endian"), packed))
typedef struct ZELDA_LE_STRUCT { uint16 a[0x10000]; } zle16_array_t;
typedef struct ZELDA_LE_STRUCT { int16 a[0x10000]; } zles16_array_t;
typedef struct ZELDA_LE_STRUCT { uint32 a[0x10000]; } zle32_array_t;
// GCC folds a little-endian read of a constant *byte* object at a known offset in the wrong byte order (seen with
// GCC 13 for powerpc: a static const uint8 table read through a zle16_t pointer). Passing every pointer through an
// empty asm keeps the compiler from knowing where it points, so such reads happen at run time, correctly.
#define ZELDA_OPAQUE(p) __extension__({ __typeof__((p) + 0) zelda_opaque_ = (p); __asm__("" : "+r"(zelda_opaque_)); zelda_opaque_; })
#define RAM16(p) (((zle16_t*)ZELDA_OPAQUE(p))->v)
#define RAM32(p) (((zle32_t*)ZELDA_OPAQUE(p))->v)
#define RAMS16(p) (((zles16_t*)ZELDA_OPAQUE(p))->v)
#define RAM16A(p) (((zle16_array_t*)ZELDA_OPAQUE(p))->a)
#define RAMS16A(p) (((zles16_array_t*)ZELDA_OPAQUE(p))->a)
#define RAM32A(p) (((zle32_array_t*)ZELDA_OPAQUE(p))->a)
#define ZELDA_LOW_IDX(x) (sizeof(x) - 1)
#define ZELDA_HIGH_IDX(x) (sizeof(x) == 1 ? 1 : sizeof(x) - 2)  // a byte's HIBYTE is the next byte, as on the SNES
#else
#define ZELDA_LE_STRUCT
#define ZELDA_OPAQUE(p) (p)
#define RAM16(p) (*(uint16*)(p))
#define RAM32(p) (*(uint32*)(p))
#define RAMS16(p) (*(int16*)(p))
#define RAM16A(p) ((uint16*)(p))
#define RAMS16A(p) ((int16*)(p))
#define RAM32A(p) ((uint32*)(p))
#define ZELDA_LOW_IDX(x) 0
#define ZELDA_HIGH_IDX(x) 1
#endif

// Little-endian scalar wrappers (plain structs on little-endian hosts), usable for pointers on every host.
typedef struct ZELDA_LE_STRUCT { uint16 v; } zle16_t;
typedef struct ZELDA_LE_STRUCT { int16 v; } zles16_t;
typedef struct ZELDA_LE_STRUCT { uint32 v; } zle32_t;
#define RAM16P(p) ((zle16_t*)ZELDA_OPAQUE(p))
#define RAMS16P(p) ((zles16_t*)ZELDA_OPAQUE(p))

#if defined(ZELDA_XENON) && !defined(ZELDA_BIG_ENDIAN_HOST)
#error "Xbox 360 builds must be detected as big-endian (check __BYTE_ORDER__)"
#endif

// Byte/word views of a variable. For byte lvalues WORD/DWORD read the following bytes as little-endian.
#define BYTE(x) (((uint8*)&(x))[ZELDA_LOW_IDX(x)])
#define HIBYTE(x) (((uint8*)&(x))[ZELDA_HIGH_IDX(x)])
#define WORD(x) RAM16(&(x))
#define DWORD(x) RAM32(&(x))
#define XY(x, y) ((y)*64+(x))

#ifndef swap16
static inline uint16 swap16(uint16 v) { return (v << 8) | (v >> 8); }
#endif

typedef struct Point16U {
  uint16 x, y;
} Point16U;

typedef struct PointU8 {
  uint8 x, y;
} PointU8;

typedef struct Pair16U {
  uint16 a, b;
} Pair16U;

typedef struct PairU8 {
  uint8 a, b;
} PairU8;

typedef struct ProjectSpeedRet {
  uint8 x, y;
  uint8 xdiff, ydiff;
} ProjectSpeedRet;

typedef struct OamEnt {
  uint8 x, y, charnum, flags;
} OamEnt;

typedef struct MemBlk {
  const uint8 *ptr;
  size_t size;
} MemBlk;
MemBlk FindIndexInMemblk(MemBlk data, size_t i);

void NORETURN Die(const char *error);

#endif  // ZELDA3_TYPES_H_
