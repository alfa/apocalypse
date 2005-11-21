#ifndef MANGOS_HASHMAP_H
#define MANGOS_HASHMAP_H

#include "Platform/CompilerDefs.h"
#include "Platform/Define.h"

#if COMPILER == COMPILER_GNU && __GNUC__ >= 3
#include <ext/hash_map>
#else
#include <hash_map>
#endif

#ifdef _STLPORT_VERSION
#define HM_NAMESPACE std
using std::hash_map;
#elif COMPILER == COMPILER_MICROSOFT && _MSC_VER >= 1300
#define HM_NAMESPACE stdext
using stdext::hash_map;
#elif COMPILER == COMPILER_GNU && __GNUC__ >= 3
#define HM_NAMESPACE __gnu_cxx
using __gnu_cxx::hash_map;

namespace __gnu_cxx
{
    template<> struct hash<unsigned long long>
    {
        size_t operator()(const unsigned long long &__x) const { return (size_t)__x; }
    };
    template<typename T> struct hash<T *>
    {
        size_t operator()(T * const &__x) const { return (size_t)__x; }
    };

};

#else
#define HM_NAMESPACE std
using std::hash_map;
#endif


#endif
