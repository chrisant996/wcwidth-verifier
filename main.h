#pragma once

#include <windows.h>
#include <stdio.h>
#include <vector>
#include <assert.h>

typedef __int8 int8;
typedef unsigned __int8 uint8;
typedef __int32 int32;
typedef unsigned __int32 uint32;

#undef min
#undef max
template<class T> T min(T a, T b) { return (a <= b) ? a : b; }
template<class T> T max(T a, T b) { return (a >= b) ? a : b; }

#include "wcwidth.h"
