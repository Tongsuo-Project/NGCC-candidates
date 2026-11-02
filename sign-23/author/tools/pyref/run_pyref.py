#!/usr/bin/env python3
"""run_pyref.py -- top-level self-test + cross-check runner for the SHUTTLE
Python reference (P12).

  python3 run_pyref.py --selftest      # run every module's own self-test
  python3 run_pyref.py --xcheck        # build+run the C-oracle byte-exact
                                       # cross-checks (DRNG/reduce/NTT/pack,
                                       # rANS, sampler)
  python3 run_pyref.py --sign          # end-to-end KeyGen/Sign/Verify byte
                                       # cross-check vs the C ref (SHA3+NGCC,
                                       # all 3 sets, rANS sig path)
  python3 run_pyref.py --m4 [N]        # M4 empirical rANS validation (N sigs)
  python3 run_pyref.py --all           # selftest + xcheck + sign (no M4)

The Python reference is the integration glue + ground-truth oracle for the
deterministic, KAT-load-bearing codec + math + sampler-primitive layers.  It
is cross-checked BYTE-FOR-BYTE against the C ref (the C is the oracle); a
divergence is a bug.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

SELFTESTS = ["params.py", "reduce_ref.py", "ntt_ref.py", "packing_ref.py",
             "drng_ref.py", "rans_ref.py", "sampler_ref.py", "gauss_ref.py",
             "rounding_ref.py", "irs_ref.py", "sign_ref.py"]
XCHECKS = ["xcheck.py", "xcheck_rans.py", "xcheck_sampler.py"]


def run(name, args=()):
    print("---- %s %s ----" % (name, " ".join(args)))
    r = subprocess.run([sys.executable, os.path.join(HERE, name), *args])
    return r.returncode


def main():
    args = sys.argv[1:]
    if not args:
        args = ["--all"]
    rc = 0
    if "--selftest" in args or "--all" in args:
        for m in SELFTESTS:
            rc |= run(m)
    if "--xcheck" in args or "--all" in args:
        for m in XCHECKS:
            rc |= run(m)
    if "--sign" in args or "--all" in args:
        # end-to-end KeyGen/Sign/Verify byte cross-check, both XOF modes.
        rc |= run("xcheck_sign.py", ["--mode", "all"])
        rc |= run("xcheck_sign.py", ["--mode", "all", "--ngcc"])
    if "--m4" in args:
        i = args.index("--m4")
        n = args[i + 1] if i + 1 < len(args) and args[i + 1].isdigit() else "10000"
        rc |= run("m4_validate.py", [n])
    print()
    print("run_pyref: %s" % ("ALL PASS" if rc == 0 else "FAILURES (see above)"))
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
