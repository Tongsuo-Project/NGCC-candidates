"""Security: Divergence / approximation-error budgets for each SHUTTLE-NGCC-SUF level.

Converted from Security.ipynb. All output is redirected to log/Security.txt.
Run: python Security.py

Background
----------
SHUTTLE-NGCC implements the discrete Gaussian via the isochronous rejection
sampler of HPRR20 (BLISS-like sampling, Alg. 12), whose Bernoulli correction
evaluates exp(.) with a fixed-point routine ApproxExp. We need a bound on the
ApproxExp error that still preserves the search-problem security up to a small
bit loss. There are two ways to state that bound.

Condition (1), HPRR20 (the version we DROP)
-------------------------------------------
HPRR20 [Sec. 5, Thm 7] requires, for every x < 0,

    max( |eps(x)/exp(x)| ,  |eps(x)/(1-exp(x))| )  <=  delta,
    eps(x) := ApproxExp(x) - exp(x),
    delta  := 1 / sqrt( 2*(2*lambda-1)*Q_exp ).

The SECOND term is the reject-branch relative error. Near x -> 0^- we have
1-exp(x) ~ |x| -> 0, so the reject-branch term blows up and forces an
absurdly small absolute error: this is the requirement we cannot meet cheaply.
With our parameters delta ~ 2^-50, yet the reject branch alone would push the
needed absolute precision far beyond that.

Condition (1'), improved (Lithium Thm 1, the version we USE)
------------------------------------------------------------
SampleZ is isochronous: the only ApproxExp-dependent object the attacker ever
observes is the EMITTED-sample distribution; the internal accept/reject bit is
masked by constant time, is not part of the signature, and is not a function of
the long-term secret. So we only need to control the emitted distribution.

Let eta := max_{x<0} |eps(x)/exp(x)| be the ACCEPTANCE-probability relative
error (the reject branch is irrelevant). The emitted distribution is the
single-iteration accept law renormalized; a +/- eta perturbation on each weight
and on the normalizer gives a per-output relative error <= 2*eta/(1-eta). The
gate that still loses only one bit on ApproxExp is therefore

    2*eta/(1-eta)  <=  delta,      delta := 1 / sqrt( 2*(2*lambda-1)*Q_exp ).

Solving for eta (the quantity we actually have to engineer):

    2*eta <= delta*(1 - eta)  =>  eta*(2 + delta) <= delta
    =>  eta <= delta / (2 + delta)  =:  eta_max.

Since delta is tiny, eta_max ~ delta/2, i.e. the acceptance branch must be ~1
bit tighter than the old delta -- a trivial price -- while the impossible
reject-branch requirement disappears entirely.

Condition (2), BaseSampler (unchanged)
--------------------------------------
    R_{2*lambda-1}( BaseSampler , D_{Z+, sigma_max} )  <=  1 + 1/(4*Q_bs),
so the BaseSampler Renyi divergence budget is reported as 1/(4*Q_bs).
"""

import math
import os
import sys
from contextlib import redirect_stdout


def calculate_budgets(lam, q_exp, q_bs):
    """Return log2 of the three budgets for one security level.

    delta    : emitted-sample distortion bound = 1/sqrt(2*(2*lam-1)*Q_exp)
               (RHS of the improved gate; equals old Condition (1) RHS).
    eta_max  : required ACCEPTANCE-probability relative error = delta/(2+delta).
    cond2    : BaseSampler Renyi budget = 1/(4*Q_bs).

    All three are returned as base-2 logarithms (negative numbers).
    """
    # delta = 1 / sqrt( 2 * (2*lambda - 1) * Q_exp )
    delta = 1.0 / math.sqrt(2 * (2 * lam - 1) * q_exp)
    log2_delta = math.log2(delta)

    # eta_max = delta / (2 + delta): the acceptance-probability relative error
    # we must actually achieve. ~ delta/2, i.e. ~1 bit below delta.
    eta_max = delta / (2.0 + delta)
    log2_eta = math.log2(eta_max)

    # Condition (2): BaseSampler Renyi budget 1/(4*Q_bs).
    log2_cond2 = -math.log2(4 * q_bs)

    return log2_delta, log2_eta, log2_cond2


def report(name, lam, n, l, m):
    # Number of exponential calls (== number of base-sampling calls here).
    # 2^80 signatures, n coefficients per polynomial, (L+M+1) Gaussian-sampled
    # polynomials per signature, divided by the ~0.8 average acceptance rate.
    q_exp = (2**80 * n * (l + m + 1)) / 0.8
    q_bs = q_exp
    log2_delta, log2_eta, log2_cond2 = calculate_budgets(lam, q_exp, q_bs)
    print(f"\n{name}:")
    print(f"  lambda = {lam}, n = {n}, L = {l}, M = {m}")
    print(f"  Q_exp = Q_bs = 2^{math.log2(q_exp):.2f}")
    print(f"  emitted-sample distortion bound  delta   = 2^{{{log2_delta:.2f}}}")
    print(f"  required acceptance rel. error   eta_max = 2^{{{log2_eta:.2f}}}")
    print(f"  BaseSampler Renyi budget         1/(4Qbs)= 2^{{{log2_cond2:.2f}}}")


def main():
    # (name, lambda, n, L, M)
    report("SHUTTLE-NGCC-SUF 128", 166, 256, 3, 3)
    report("SHUTTLE-NGCC-SUF 256", 267, 512, 3, 2)
    report("SHUTTLE-NGCC-SUF 512", 523, 1024, 3, 2)


if __name__ == "__main__":
    log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "log")
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, "Security.txt")
    with open(log_path, "w", encoding="utf-8") as f:
        with redirect_stdout(f):
            main()
    print(f"Output written to {log_path}", file=sys.stderr)
