#pragma once
#include <vector>
#include <cstdint>
#include <cmath>

/**
 * Linear sieve for the Möbius function μ(n) up to maxN.
 * μ(1) = 1
 * μ(n) = 0 if n has a squared prime factor
 * μ(n) = (-1)^k if n is square-free with k distinct prime factors
 */
class MobiusTable
{
public:
    explicit MobiusTable (int maxN)
        : maxN_ (maxN)
    {
        compute();
    }

    /** Returns μ(n). Requires 1 ≤ n ≤ maxN. */
    int8_t operator[] (int n) const
    {
        jassert (n >= 1 && n <= maxN_);
        return mu_[static_cast<size_t> (n)];
    }

    int getMaxN() const noexcept { return maxN_; }

private:
    void compute()
    {
        mu_.assign (static_cast<size_t> (maxN_) + 1, 0);
        std::vector<int> primes;
        std::vector<bool> isComposite (static_cast<size_t> (maxN_) + 1, false);

        mu_[1] = 1;

        for (int i = 2; i <= maxN_; ++i)
        {
            if (! isComposite[static_cast<size_t> (i)])
            {
                primes.push_back (i);
                mu_[static_cast<size_t> (i)] = -1;          // prime
            }

            for (int p : primes)
            {
                const long long prod = static_cast<long long> (i) * p;
                if (prod > maxN_)
                    break;

                isComposite[static_cast<size_t> (prod)] = true;

                if (i % p == 0)
                {
                    mu_[static_cast<size_t> (prod)] = 0;    // squared factor
                    break;
                }
                else
                {
                    mu_[static_cast<size_t> (prod)] = static_cast<int8_t> (-mu_[static_cast<size_t> (i)]);
                }
            }
        }
    }

    int maxN_;
    std::vector<int8_t> mu_;
};
