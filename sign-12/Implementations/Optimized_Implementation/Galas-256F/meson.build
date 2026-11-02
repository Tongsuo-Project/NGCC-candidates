project('galas_opt', ['c','cpp'],
  version : '0.1.0',
  default_options : [
    'buildtype=release',
    'optimization=3',
    'c_std=c99',
    'cpp_std=c++23',
    'warning_level=1',
    'b_lto=true',
  ])

add_project_arguments(
  ['-march=x86-64', '-mavx2', '-maes', '-mpclmul', '-mtune=native',
   '-fomit-frame-pointer'],
  language : 'c')
add_project_arguments(
  ['-march=x86-64', '-mavx2', '-maes', '-mpclmul', '-mtune=native',
   '-fomit-frame-pointer'],
  language : 'cpp')

ngcc_lib = static_library('ngcc_aux', ['auxfunc.c','drng.c'])
ngcc_dep = declare_dependency(link_with : ngcc_lib, include_directories : '.')

galas_sources = ['avx2/aes_impl.cpp','faest.cpp','owf_proof.cpp','polynomials_constants.cpp','small_vole.cpp','vector_com.cpp','vole_commit.cpp','randomness_randombytes.c','randomness_os.c']

galas_lib = static_library('galas_opt', galas_sources, dependencies: [ngcc_dep], include_directories : '.')
galas_dep = declare_dependency(link_with : galas_lib, dependencies: [ngcc_dep], include_directories : '.')

executable('galas_test', ['test_main.cpp'], dependencies: [galas_dep], include_directories : '.')
executable('galas_sign_test', ['test_sign.cpp'], dependencies: [galas_dep], include_directories : '.')
executable('galas_bench', ['bench.cpp'], dependencies: [galas_dep], include_directories : '.')
executable('galas_verify_kat', ['verify_kat.cpp'], dependencies: [galas_dep], include_directories : '.')
