#ifndef RACER_PARENA_RUNTIME_H
#define RACER_PARENA_RUNTIME_H

/* parena_runtime.h -- every `parena build`-generated .c file in this repo #includes this (see
 * packages/simulation/bike_gear_mod.c's own header). PARENA's real, canonical runtime
 * (PARENA/runtime/parena_runtime.h) is 1600+ lines because it backs the full language --
 * arenas, strings, Vec, SDL2/SDL2_ttf editor rendering, ptys, sockets. ECOWAR already trims its
 * own copy down to what its own mods actually use (472 lines, packages/simulation/
 * parena_runtime.h) rather than carrying the whole thing; this repo's own first mod
 * (bike_gear_mod.prn) is pure I32 scalar arithmetic -- no Arena, no String, no Vec, no host FFI
 * calls at all -- so it needs literally nothing from the runtime beyond what the generated file
 * already #includes directly (string.h/stdint.h/stdlib.h/math.h). Confirmed by actually
 * compiling against this empty stub, not assumed.
 *
 * Grow this file for real, the same way ECOWAR's own copy grew, the moment a future racer mod
 * genuinely needs Arena/String/Vec/host-glue support -- copy the specific real pieces that mod
 * needs from PARENA/runtime/parena_runtime.h, don't pre-emptively import the whole thing (SDL2_ttf
 * in particular is a real, non-trivial extra system dependency this repo has no other reason to
 * require yet).
 */

#endif
