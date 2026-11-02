"""BaseSampler: RCDT base sampler table generation and Renyi divergence analysis.

Converted from BaseSampler.ipynb. All output is redirected to log/BaseSampler.txt.
Run: python BaseSampler.py
"""

import math
import os
import sys
from contextlib import redirect_stdout
from decimal import Decimal, getcontext

# Use high global precision to avoid underflow or precision loss for powers such as a=509.
getcontext().prec = 300


def compute_ideal_gaussian(sigma, max_val):
    """
    Compute the exact probabilities of the ideal discrete half-Gaussian distribution D_{Z+, sigma}.
    max_val should be large enough (for example, 100 * sigma) to approximate the infinite series sum.
    """
    sigma_d = Decimal(str(sigma))
    weights = [Decimal(0)] * max_val
    for x in range(max_val):
        x_d = Decimal(x)
        # exp(-x^2 / (2 * sigma^2))
        weights[x] = Decimal.exp(-(x_d**2) / (Decimal(2) * sigma_d**2))

    total_S = sum(weights)
    return [w / total_S for w in weights]


def compute_base_sampler_tables(sigma, w, theta):
    """
    Generate PDT, CDT, and RCDT according to the paper algorithm.
    :param sigma: standard deviation
    :param w: truncation bound (tailcut)
    :param theta: absolute precision bit width
    :return: pdt_int, cdt_int, rcdt_int (integer arrays scaled by 2^theta)
    """
    sigma_d = Decimal(str(sigma))
    two_theta = Decimal(2) ** theta

    # Step 1: compute the restricted ideal distribution D_[w],sigma on {0, ..., w}
    weights_w = [Decimal.exp(-(Decimal(x)**2) / (Decimal(2) * sigma_d**2)) for x in range(w + 1)]
    S_w = sum(weights_w)
    D_w = [weight / S_w for weight in weights_w]

    # Step 2: compute the fixed-point probability density table PDT
    pdt_int = [0] * (w + 1)
    sum_pdt_tail = 0

    # For z >= 1, truncate by flooring.
    for z in range(1, w + 1):
        # PDT(z) = 2^-theta * floor(2^theta * D_w(z))
        val_int = int(D_w[z] * two_theta)
        pdt_int[z] = val_int
        sum_pdt_tail += val_int

    # For z = 0, subtract the remaining mass from 1 to make the sum exactly 1.
    pdt_int[0] = int(two_theta) - sum_pdt_tail

    # Step 3: derive CDT and RCDT from PDT
    cdt_int = [0] * (w + 1)
    rcdt_int = [0] * (w + 1)

    current_sum = 0
    for i in range(w + 1):
        current_sum += pdt_int[i]
        cdt_int[i] = current_sum

    total_space = cdt_int[-1]  # Must equal 2^theta.
    for i in range(w + 1):
        rcdt_int[i] = total_space - cdt_int[i]

    return pdt_int, cdt_int, rcdt_int


def calculate_renyi_divergence(pdt_int, theta, sigma, a):
    """
    Compute the order-a Renyi divergence R_a between the truncated rounded distribution and the ideal distribution.
    R_a(P || Q) = ( sum( P(x)^a / Q(x)^(a-1) ) ) ^ (1/(a-1))
    """
    a_d = Decimal(a)
    two_theta = Decimal(2) ** theta

    # Get the ideal infinite-domain distribution (compute to 100 * sigma for precision).
    Q_ideal = compute_ideal_gaussian(sigma, max_val=100 * math.ceil(sigma))

    divergence_sum = Decimal(0)

    # Only iterate over the PDT support {0, ..., w}, because P(x) = 0 outside this range.
    for x in range(len(pdt_int)):
        P_x = Decimal(pdt_int[x]) / two_theta
        if P_x == 0:
            continue

        Q_x = Q_ideal[x]

        # P(x)^a / Q(x)^(a-1)
        term = (P_x ** a_d) / (Q_x ** (a_d - Decimal(1)))
        divergence_sum += term

    # (sum)^(1/(a-1))
    r_a = divergence_sum ** (Decimal(1) / (a_d - Decimal(1)))
    return r_a


def calculate_renyi_divergence_inf(pdt_int, theta, sigma):
    """
    Compute the infinite-order Renyi divergence R_inf(P || Q) = max_x P(x) / Q(x).

    The infinite-order Renyi divergence is the finite-order limit: when a -> inf,
    R_a degenerates to the maximum pointwise ratio between P and Q.

    Intuitively, finite-order R_a takes a weighted power mean of P(x)/Q(x); higher orders emphasize the maximum ratio.
    The infinite order directly takes the maximum.
    """
    two_theta = Decimal(2) ** theta

    Q_ideal = compute_ideal_gaussian(sigma, max_val=100 * max(math.ceil(sigma), 2))

    max_ratio = Decimal(0)
    for x in range(len(pdt_int)):
        P_x = Decimal(pdt_int[x]) / two_theta
        if P_x == 0:
            continue
        Q_x = Q_ideal[x]
        ratio = P_x / Q_x
        if ratio > max_ratio:
            max_ratio = ratio

    return max_ratio


def format_divergence(r_a):
    """Format the divergence as a readable 1 + 2^(-x) expression."""
    diff = r_a - Decimal(1)
    if diff <= 0:
        return "1.0 (perfect fit)"

    # Compute x = -log2(R_a - 1).
    bits = -float(diff.ln() / Decimal(2).ln())
    return f"1 + 2^(-{bits:.2f})"


def format_signed_power_of_two(x):
    """Format a signed difference as +/- 2^(-bits)."""
    x = Decimal(x)
    if x == 0:
        return "0"
    sign = "-" if x < 0 else "+"
    bits = -float(abs(x).ln() / Decimal(2).ln())
    return f"{sign}2^(-{bits:.2f})"


def calculate_rcdt_sampler_stddev(rcdt_int, theta, ideal_sigma=1, zero_fold=True):
    """
    Compute the standard deviation induced by the actual RCDT table-lookup sampler.

    The RCDT sampler computes z = sum_i [u < RCDT[i]], where u is uniform over {0,...,2^theta-1}.
    For the keygen sigma=1 noise sampler, it also assigns a random sign and applies 1/2 zero-fold rejection when z=0;
    when zero_fold=True, the returned value is the standard deviation of the final signed output distribution.
    """
    if theta <= 0:
        raise ValueError("theta must be a positive integer")

    total = Decimal(2) ** theta
    thresholds = [int(x) for x in rcdt_int]
    while thresholds and thresholds[-1] == 0:
        thresholds.pop()

    if not thresholds:
        return {
            "support_max": 0,
            "magnitude_pmf": [Decimal(1)],
            "accept_mass": Decimal(1) if zero_fold else None,
            "variance": Decimal(0),
            "stddev": Decimal(0),
            "ideal_sigma": Decimal(str(ideal_sigma)),
            "abs_gap": -Decimal(str(ideal_sigma)),
            "rel_gap": -Decimal(1),
        }

    for i, t in enumerate(thresholds):
        if t < 0 or t > int(total):
            raise ValueError(f"RCDT[{i}] is outside the [0, 2^theta] range: {t}")
        if i > 0 and thresholds[i - 1] < t:
            raise ValueError("RCDT table must be monotonically non-increasing")

    # Pr[z >= k] = RCDT[k-1] / 2^theta, so the PMF can be recovered from adjacent tail differences.
    tail = [Decimal(t) / total for t in thresholds] + [Decimal(0)]
    magnitude_pmf = [Decimal(1) - tail[0]]
    for k in range(1, len(tail)):
        magnitude_pmf.append(tail[k - 1] - tail[k])

    second_moment = sum(Decimal(k * k) * p for k, p in enumerate(magnitude_pmf))
    if zero_fold:
        accept_mass = Decimal(1) - magnitude_pmf[0] / Decimal(2)
        if accept_mass <= 0:
            raise ValueError("zero-fold acceptance probability must be positive")
        variance = second_moment / accept_mass
    else:
        accept_mass = None
        variance = second_moment

    stddev = variance.sqrt()
    ideal_sigma_d = Decimal(str(ideal_sigma))
    abs_gap = stddev - ideal_sigma_d
    rel_gap = abs_gap / ideal_sigma_d if ideal_sigma_d != 0 else Decimal(0)

    return {
        "support_max": len(magnitude_pmf) - 1,
        "magnitude_pmf": magnitude_pmf,
        "accept_mass": accept_mass,
        "variance": variance,
        "stddev": stddev,
        "ideal_sigma": ideal_sigma_d,
        "abs_gap": abs_gap,
        "rel_gap": rel_gap,
    }


def print_rcdt_table(rcdt_values, total_bits, base_bits, table_name, drop_terminal_zero=True):
    """
    Split the rcdt table into base_bits-sized limbs and print it as a C array.
    Rules:
    - limbs = ceil(total_bits / base_bits)
    - limb0 is the least-significant group (little-endian limb order)
    - use uint32_t / U when base_bits <= 32
    - use uint64_t / ULL when base_bits > 32
    - by default, drop trailing zero RCDT entries so the output better matches common implementation tables
    """
    if base_bits <= 0:
        raise ValueError("base_bits must be a positive integer")

    values = list(rcdt_values)
    if drop_terminal_zero:
        while len(values) > 0 and values[-1] == 0:
            values = values[:-1]

    limb_count = (total_bits + base_bits - 1) // base_bits
    if limb_count <= 0:
        raise ValueError("limb_count computation error")

    c_type = "uint32_t" if base_bits <= 32 else "uint64_t"
    suffix = "U" if base_bits <= 32 else "ULL"

    # Use a fixed hexadecimal width: show each limb at full width for easier aligned reading.
    hex_width = (base_bits + 3) // 4
    limb_mask = (1 << base_bits) - 1

    print(f"\n/* {total_bits}-bit RCDT -> {limb_count}x{base_bits}-bit limbs */")
    print(f"const {c_type} {table_name}[{len(values)}][{limb_count}] = {{")

    limit = 1 << total_bits
    for i, v in enumerate(values):
        if v < 0:
            raise ValueError(f"RCDT[{i}] is negative: {v}")
        if v >= limit:
            raise ValueError(f"RCDT[{i}] exceeds {total_bits} bits: {v}")

        limbs = []
        for j in range(limb_count):
            limb = (v >> (j * base_bits)) & limb_mask
            limbs.append(f"0x{limb:0{hex_width}X}{suffix}")

        comma = "," if i != len(values) - 1 else ""
        print(f"    {{{', '.join(limbs)}}}{comma}")

    print("};")


def run_pk_gaussian_sampler():
    """PK Gaussian sampler tables and divergence analysis for SHUTTLE-NGCC-SUF 128, 256, and 512."""
    SIGMA = 825 / 256
    THETA = 96
    # For SHUTTLE-NGCC-SUF 512
    SIS_Security = 523
    A_ORDER = SIS_Security * 2 - 1
    target_w = math.ceil(11 * SIGMA)

    print(f"=== SHUTTLE-NGCC-SUF 128/256/512: sigma={SIGMA}, theta={THETA} bits, a={A_ORDER} ===")

    pdt, cdt, rcdt = compute_base_sampler_tables(SIGMA, target_w, THETA)
    r_a = calculate_renyi_divergence(pdt, THETA, SIGMA, A_ORDER)
    r_inf = calculate_renyi_divergence_inf(pdt, THETA, SIGMA)
    print_rcdt_table(
        rcdt_values=rcdt,
        total_bits=96,
        base_bits=32,
        table_name="GAUSS0_96_3x32"
    )
    print(f"\nRenyi divergence R_{A_ORDER}: {format_divergence(r_a)}")
    print(f"Renyi divergence R_inf:  {format_divergence(r_inf)}")
    for w_test in range(math.ceil(9 * SIGMA), math.ceil(12 * SIGMA)):
        test_pdt, _, _ = compute_base_sampler_tables(SIGMA, w_test, THETA)
        test_ra = calculate_renyi_divergence(test_pdt, THETA, SIGMA, A_ORDER)
        test_rinf = calculate_renyi_divergence_inf(test_pdt, THETA, SIGMA)
        print(f"w = {w_test:2d} | R_{A_ORDER}: {format_divergence(test_ra)} | R_inf: {format_divergence(test_rinf)}")


def run_sk_gaussian_sampler():
    """SK Gaussian sampler tables, divergence analysis, and actual standard deviation for SHUTTLE-NGCC-SUF."""
    SIGMA = [0.85, 0.9, 1]
    THETA = 96
    # We did not care about the Renyi divergence for the sk Gaussian sampler.
    A_ORDER = 512 * 2 - 1

    for each_sigma in SIGMA:
        target_w = math.ceil(11 * each_sigma)
        print(f"=== SHUTTLE-NGCC-SUF sk Gaussian Sampler: sigma={each_sigma}, theta={THETA} bits, a={A_ORDER} ===")

        pdt, cdt, rcdt = compute_base_sampler_tables(each_sigma, target_w, THETA)
        r_a = calculate_renyi_divergence(pdt, THETA, each_sigma, A_ORDER)
        r_inf = calculate_renyi_divergence_inf(pdt, THETA, each_sigma)
        print_rcdt_table(
            rcdt_values=rcdt,
            total_bits=96,
            base_bits=32,
            table_name="GAUSS0_96_3x32"
        )
        print(f"\nRenyi divergence R_{A_ORDER}: {format_divergence(r_a)}")
        print(f"Renyi divergence R_inf:  {format_divergence(r_inf)}")
        for w_test in range(math.ceil(9 * each_sigma), math.ceil(12 * each_sigma)):
            test_pdt, _, _ = compute_base_sampler_tables(each_sigma, w_test, THETA)
            test_ra = calculate_renyi_divergence(test_pdt, THETA, each_sigma, A_ORDER)
            test_rinf = calculate_renyi_divergence_inf(test_pdt, THETA, each_sigma)
            print(f"w = {w_test:2d} | R_{A_ORDER}: {format_divergence(test_ra)} | R_inf: {format_divergence(test_rinf)}")

        std_stats = calculate_rcdt_sampler_stddev(
            rcdt_int=rcdt,
            theta=THETA,
            ideal_sigma=each_sigma,
            zero_fold=True,
        )
        print("\n--- Actual RCDT standard deviation (keygen sigma=1 noise sampler) ---")
        print(f"RCDT support range: [-{std_stats['support_max']}, {std_stats['support_max']}]")
        print(f"zero-fold acceptance probability: {std_stats['accept_mass']:.30f}")
        print(f"actual variance: {std_stats['variance']:.30f}")
        print(f"actual standard deviation: {std_stats['stddev']:.30f}")
        print(f"absolute gap from ideal standard deviation {each_sigma}: {std_stats['abs_gap']:.6E} ({format_signed_power_of_two(std_stats['abs_gap'])})")
        print(f"absolute gap from ideal standard deviation {each_sigma} relative gap: {std_stats['rel_gap']:.6E} ({format_signed_power_of_two(std_stats['rel_gap'])})")


def main():
    run_pk_gaussian_sampler()
    print()
    run_sk_gaussian_sampler()


if __name__ == "__main__":
    log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "log")
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, "BaseSampler.txt")
    with open(log_path, "w", encoding="utf-8") as f:
        with redirect_stdout(f):
            main()
    print(f"Output written to {log_path}", file=sys.stderr)
