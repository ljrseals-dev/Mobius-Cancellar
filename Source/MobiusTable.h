#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>

/** Linear sieve for the Möbius function μ(n), for 1 <= n <= maxN. */
class MobiusTable
{
public:
    explicit MobiusTable (int maxN) : maxN_ (maxN)
    {
        if (maxN_ < 1)
            throw std::invalid_argument ("MobiusTable maxN must be at least 1");
        compute();
    }

    int8_t operator[] (int n) const
    {
        if (n < 1 || n > maxN_)
            throw std::out_of_range ("MobiusTable index outside [1, maxN]");
        return mu_[static_cast<size_t> (n)];
    }

    int getMaxN() const noexcept { return maxN_; }

private:
    void compute()
    {
        mu_.assign (static_cast<size_t> (maxN_) + 1, 0);
        std::vector<int> primes;
        std::vector<uint8_t> composite (static_cast<size_t> (maxN_) + 1, 0);
        mu_[1] = 1;

        for (int i = 2; i <= maxN_; ++i)
        {
            if (composite[static_cast<size_t> (i)] == 0)
            {
                primes.push_back (i);
                mu_[static_cast<size_t> (i)] = -1;
            }

            for (const int p : primes)
            {
                const int64_t product = static_cast<int64_t> (i) * p;
                if (product > maxN_)
                    break;

                const auto index = static_cast<size_t> (product);
                composite[index] = 1;

                if (i % p == 0)
                {
                    mu_[index] = 0;
                    break;
                }

                mu_[index] = static_cast<int8_t> (-mu_[static_cast<size_t> (i)]);
            }
        }
    }

    int maxN_;
    std::vector<int8_t> mu_;
};
