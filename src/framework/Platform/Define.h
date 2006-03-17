/* Define.h
 *
 * Copyright (C) 2005 MaNGOS <https://opensvn.csie.org/traccgi/MaNGOS/trac.cgi/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MANGOS_DEFINE_H
#define MANGOS_DEFINE_H

#include "Platform/CompilerDefs.h"

#ifdef WIN32

#ifdef MANGOS_WIN32_DLL_IMPORT

#define MANGOS_DLL_DECL __declspec(dllimport)
#else
#ifdef MANGOS_WIND_DLL_EXPORT
#define MANGOS_DLL_DECL __declspec(dllexport)
#else
#define MANGOS_DLL_DECL
#endif

#endif

#else
#define MANGOS_DLL_DECL
#endif

#ifndef DEBUG
#define MANGOS_INLINE inline
#else
#define MANGOS_INLINE
#endif

#if COMPILER == COMPILER_MICROSOFT
typedef __int64   int64;
#else
typedef long long int64;
#endif
typedef long        int32;
typedef short       int16;
typedef char        int8;

#if COMPILER == COMPILER_MICROSOFT
typedef unsigned __int64   uint64;
#else
typedef unsigned long long  uint64;
typedef unsigned long      DWORD;
#endif
typedef unsigned long        uint32;
typedef unsigned short       uint16;
typedef unsigned char        uint8;
typedef uint64 OBJECT_HANDLE;

#endif 
