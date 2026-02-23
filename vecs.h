/*
    -- Vecs --

    Copyright 2026 UAA Software

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
    associated documentation files (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge, publish, distribute,
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial
    portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
    NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
    OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <cstring>
#include <cassert>
#include <new>

#ifndef VECS_MAX_ENTITIES
#define VECS_MAX_ENTITIES 65536
#endif

#ifndef VECS_MAX_COMPONENTS
#define VECS_MAX_COMPONENTS 256
#endif

#define VECS_L2_COUNT  ( VECS_MAX_ENTITIES / 64 )
#define VECS_TOP_COUNT ( VECS_L2_COUNT / 64 )

constexpr uint64_t VECS_INVALID_ENTITY = UINT64_MAX;
constexpr uint32_t VECS_INVALID_INDEX  = UINT32_MAX;

// --------------------------------------------------------------------------
// Bit Intrinsics
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Entity
// --------------------------------------------------------------------------

typedef uint64_t veEntity;

// --------------------------------------------------------------------------
// Bitfield
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Component Pool
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// World
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Query
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Singleton
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Command Buffer
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// SIMD
// --------------------------------------------------------------------------
