#ifndef PHYSICS_RT_DLL_BOUNDARY_H
#define PHYSICS_RT_DLL_BOUNDARY_H

/*
 * VxMathCompiler.h retains the original Virtools convention that marks every
 * CK class as dllexport in a plug-in translation unit.  Force-including this
 * header for the Windows physics_RT DLL changes CK declarations back to their
 * consumer form before any CK header is parsed.  CK loader exports use the
 * separate PLUGIN_EXPORT macro, and PhysicsRT_GetApi uses PHYSICSRT_PUBLIC.
 */
#include <VxMathCompiler.h>

#ifdef DLL_EXPORT
#undef DLL_EXPORT
#endif
#define DLL_EXPORT __declspec(dllimport)

#endif /* PHYSICS_RT_DLL_BOUNDARY_H */
