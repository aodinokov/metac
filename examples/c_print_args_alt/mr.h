// taken from https://github.com/alexanderchuranov/Metaresc/blob/master/src/metaresc.h

#ifndef MR_H_
#define MR_H_

#include "mr_pp.generated.h"

/* internal macros for arguments evaluation and concatination */
#define MR_PASTE2(...) MR_PASTE2_ (__VA_ARGS__)
#define MR_PASTE2_(_0, _1) _0 ## _1
#define MR_PASTE3(...) MR_PASTE3_ (__VA_ARGS__)
#define MR_PASTE3_(_0, _1, _2) _0 ## _1 ## _2
#define MR_PASTE4(...) MR_PASTE4_ (__VA_ARGS__)
#define MR_PASTE4_(_0, _1, _2, _3) _0 ## _1 ## _2 ## _3
#define MR_PASTE5(...) MR_PASTE5_ (__VA_ARGS__)
#define MR_PASTE5_(_0, _1, _2, _3, _4) _0 ## _1 ## _2 ## _3 ## _4

/* Interface macros for unrolled loops from mr_pp.h */
#define MR_FOREACH(X, ...) MR_PASTE2 (MR_FOREACH, MR_NARG (__VA_ARGS__)) (X, __VA_ARGS__)
#define MR_FOREACH_EX(X, ...) MR_PASTE2 (MR_FOREACH_EX_, MR_NARG (__VA_ARGS__)) (X, __VA_ARGS__)
#define MR_FOR(NAME, N, OP, FUNC, ...) MR_PASTE2 (MR_FOR, N) (NAME, OP, FUNC, __VA_ARGS__)

#endif