MAMBA-NIKE-256 Reference Implementation
============================================

Portable C99 NGCC KEX implementation.  Polynomial multiplication is performed
only by coefficient-domain Toom-Cook-4 followed by negacyclic folding and the
q-1 mask.  This directory contains no AVX2 intrinsics, AVX2 assembly, or AVX2
compiler flags.

Build and run
-------------
  make clean && make
  ./KAT_KEX

Profile constants (params.h)
----------------------------
  PARAM_N     1024
  PARAM_K     7
  PARAM_Q     8192
  LOG2Q       13
  PARAM_T_PK  10
  PARAM_T_U   10
  PARAM_T_V   6

API byte sizes
--------------
  public key / send-a   1312
  secret key            3360
  pass message / send-b 1568
  shared secret         32

The profile-specific params.h is validated against the immutable repository
parameters.json manifest.  Parameters are not overridden through compiler flags.
