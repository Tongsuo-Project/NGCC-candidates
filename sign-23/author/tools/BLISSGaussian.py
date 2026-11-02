"""BLISSGaussian: BLISS-style Gaussian sampler acceptance rate and performance analysis.

Converted from BLISSGaussian.ipynb. All output is redirected to log/BLISSGaussian.txt.
Run: python BLISSGaussian.py
"""

import math
import os
import sys
from contextlib import redirect_stdout

import numpy as np

# numpy >= 2.0 provides np.trapezoid; older versions only have np.trapz, so support both.
if not hasattr(np, "trapezoid"):
    np.trapezoid = np.trapz


def run_basic_acceptance():
    """Theoretical average acceptance/rejection rates for the baseline parameters (sigma=16)."""
    # 1. Define parameters
    sigma = 16
    x_max = 64  # CDT has 64 entries, so x ranges from 0 to 63

    # 2. Compute the probability distribution P(X=x).
    x_vals = np.arange(x_max)
    weights = np.exp(-(x_vals**2) / (2 * sigma**2))
    p_x = weights / np.sum(weights)

    # 3. Define the acceptance-probability function for given x and u (numerical integration).
    def integrate_accept(x, n_points=1000):
        u_vals = np.linspace(0, 1, n_points)
        f_vals = np.exp(-(u_vals**2 / 512) - (x * u_vals / 256))
        return np.trapezoid(f_vals, u_vals)

    # 4. Compute the total expected acceptance probability.
    expected_acceptance = sum(prob * integrate_accept(x) for x, prob in zip(x_vals, p_x))
    average_rejection_rate = 1 - expected_acceptance

    # 5. Print results
    print(f"Theoretical average acceptance probability: {expected_acceptance:.6f}")
    print(f"Theoretical average rejection rate:   {average_rejection_rate:.6f} ({average_rejection_rate * 100:.2f}%)")


# ----- Comprehensive performance table -----

target_samples = 1280
prng_cost_per_byte = 2.0
approx_exp_cost = 26.0


def integrate_accept_prob(x, sigma2, n_points=1000):
    u_vals = np.linspace(0, 1, n_points)
    f_vals = np.exp(-(u_vals**2 + 2 * u_vals * x) / (2 * sigma2**2))
    return np.trapezoid(f_vals, u_vals)


def binomial_shortfall_log2_prob(total_calls, target_successes, accept_prob):
    if target_successes <= 0:
        return float('-inf')
    if accept_prob <= 0.0:
        return 0.0
    if accept_prob >= 1.0:
        return float('-inf') if total_calls >= target_successes else 0.0

    upper = min(target_successes - 1, total_calls)
    if upper < 0:
        return float('-inf')

    log_p = math.log(accept_prob)
    log_q = math.log1p(-accept_prob)
    log_terms = []

    for success_count in range(upper + 1):
        log_term = (
            math.lgamma(total_calls + 1)
            - math.lgamma(success_count + 1)
            - math.lgamma(total_calls - success_count + 1)
            + success_count * log_p
            + (total_calls - success_count) * log_q
        )
        log_terms.append(log_term)

    max_log_term = max(log_terms)
    scaled_sum = sum(math.exp(log_term - max_log_term) for log_term in log_terms)
    return (max_log_term + math.log(scaled_sum)) / math.log(2.0)


def format_probability_as_power_of_two(log2_probability):
    if math.isinf(log2_probability) and log2_probability < 0:
        return "$2^{-\\infty}$"
    if abs(log2_probability) < 0.05:
        return "$2^{0}$"
    return f"$2^{{{log2_probability:.1f}}}$"


def get_k_log2(case_config, k_value, sigma2_value):
    if case_config["k_format"] == "power_of_two":
        return case_config["sigma_log2"] - int(np.log2(sigma2_value))
    return math.log2(k_value)


def format_k_value(case_config, k_value, sigma2_value):
    if case_config["k_format"] == "power_of_two":
        return f"$2^{{{int(get_k_log2(case_config, k_value, sigma2_value))}}}$"
    return str(k_value)


def format_sigma2_value(case_config, sigma2_value):
    if case_config["sigma2_format"] == "int":
        return str(int(sigma2_value))
    return f"{sigma2_value:.4f}"


def estimate_cost(case_config, k_value, sigma2_value):
    k_log2 = get_k_log2(case_config, k_value, sigma2_value)
    cost1 = 12 * prng_cost_per_byte
    cost2 = sigma2_value * 11 * 3.1 / 8
    cost3 = k_log2 * prng_cost_per_byte / 8
    cost4 = approx_exp_cost
    return cost1 + cost2 + cost3 + cost4


def render_table(case_config, table_index):
    print("\\begin{table}[H]")
    print("    \\centering")
    print("    \\renewcommand{\\arraystretch}{1.2}")
    print("    \\resizebox{\\textwidth}{!}{")
    print("    \\begin{tabular}{|c|c|c|c|c|c|c|c|c|}")
    print("        \\hline")
    print("        $\\sigma_2$ & $k$ & \\textbf{Avg. Acc.} & \\textbf{Avg. Rej.} & \\textbf{Avg. Iter.} & \\textbf{P($>2N$)} & \\textbf{Max. Exp.} & \\textbf{Cost} & \\textbf{All Cost} \\\\")
    print("        \\hline")

    for k, s2 in zip(case_config["k_vals"], case_config["sigma2_vals"]):
        x_max_integral = int(np.ceil(8 * s2))
        x_vals = np.arange(x_max_integral)
        weights = np.exp(-(x_vals**2) / (2 * s2**2))
        p_x = weights / np.sum(weights)

        expected_acceptance = 0.0
        for x, prob in zip(x_vals, p_x):
            expected_acceptance += prob * integrate_accept_prob(x, s2)

        rejection_rate = 1.0 - expected_acceptance
        avg_iters = target_samples / expected_acceptance
        x_max_worst = math.ceil(11 * s2)
        # max_exponent = (1 + 2 * x_max_worst) / (2 * s2**2)
        max_exponent = (k-1)*(k-1+2*k*x_max_worst)/(2*(s2*k)**2)
        log2_prob_gt_2n = binomial_shortfall_log2_prob(
            2 * target_samples, target_samples, expected_acceptance
        )
        cost = estimate_cost(case_config, k, s2)
        all_cost = cost * avg_iters

        sigma2_str = format_sigma2_value(case_config, s2)
        k_str = format_k_value(case_config, k, s2)
        row = f"        {sigma2_str} & {k_str} & {expected_acceptance*100:.1f}\\% & {rejection_rate*100:.1f}\\% & {avg_iters:.2f} & {format_probability_as_power_of_two(log2_prob_gt_2n)} & {max_exponent:.4f} & {cost:.2f} & {all_cost:.2f} \\\\"
        print(row)

    print("        \\hline")
    print("    \\end{tabular}")
    print("    }")
    print(
        f"    \\caption{{Gaussian sampler performance for different $\\sigma_2$ and $k$, with $\\sigma={case_config['sigma_label']}$."
        f"Avg. Iter. is the average number of calls needed to obtain N=1280 valid samples when the average acceptance rate is p, i.e., N/p. "
        f"P($>2N$) is the probability that obtaining N=1280 valid samples requires more than 2N calls. "
        f"Max. Exp. is the maximum exponent at $u=1, x=11\\sigma_2$. "
        f"Cost is the estimated cost, where the PRNG cost per byte is y=2 cycles and $cost=cost1+cost2+cost3+cost4$, "
        f"$cost1=12y$, $cost2=11\\sigma_2\\cdot 3.1/8$, $cost3=\\log_2(k)\\cdot y/8$, and $cost4=26$ cycles. "
        f"All Cost is defined as the product of Cost and Avg. Iter..}}"
    )
    print(f"    \\label{{tab:rejection-rates-comprehensive-{table_index}}}")
    print("\\end{table}")
    print()


def run_comprehensive_table():
    cases = [
        # SHUTTLE-NGCC-SUF 128,256,512
        {
            "sigma_label": "825",
            "sigma": 825,
            "k_vals": [256, 512, 1024],
            "sigma2_vals": [825 / k for k in [256, 512, 1024]],
            "k_format": "int",
            "sigma2_format": "float",
        }
    ]

    for index, case_config in enumerate(cases, start=1):
        render_table(case_config, index)


def main():
    run_basic_acceptance()
    print()
    run_comprehensive_table()


if __name__ == "__main__":
    log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "log")
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, "BLISSGaussian.txt")
    with open(log_path, "w", encoding="utf-8") as f:
        with redirect_stdout(f):
            main()
    print(f"Output written to {log_path}", file=sys.stderr)
