#include "random.h"

void
srand(struct rng* rng, uint seed)
{
  rng->z1 = seed;
  rng->z2 = seed;
  rng->z3 = seed;
  rng->z4 = seed;
}

uint
rand(struct rng* rng)
{
   uint b;
   b  = ((rng->z1 << 6) ^ rng->z1) >> 13;
   rng->z1 = ((rng->z1 & 4294967294U) << 18) ^ b;
   b  = ((rng->z2 << 2) ^ rng->z2) >> 27; 
   rng->z2 = ((rng->z2 & 4294967288U) << 2) ^ b;
   b  = ((rng->z3 << 13) ^ rng->z3) >> 21;
   rng->z3 = ((rng->z3 & 4294967280U) << 7) ^ b;
   b  = ((rng->z4 << 3) ^ rng->z4) >> 12;
   rng->z4 = ((rng->z4 & 4294967168U) << 13) ^ b;
   return (rng->z1 ^ rng->z2 ^ rng->z3 ^ rng->z4);
}
