#include <cstdlib>
#include <iostream>
#include <vector>

#include "TrafficMonitor/PdhHardwareQuery/GpuMemorySelection.h"

using GpuMemorySelection::AdapterCandidate;
using GpuMemorySelection::AdapterKind;

namespace
{
    constexpr unsigned long long GiB(unsigned long long value)
    {
        return value * 1024ull * 1024ull * 1024ull;
    }

    bool ExpectPreferredLimit(const char* test_name, const std::vector<AdapterCandidate>& candidates, unsigned long long expected_limit)
    {
        unsigned long long limit{};
        const bool ok{ GpuMemorySelection::SelectPreferredAdapterMemoryLimit(candidates, limit) };
        if (!ok || limit != expected_limit)
        {
            std::cerr << test_name << " failed: expected " << expected_limit << ", got ";
            if (ok)
                std::cerr << limit;
            else
                std::cerr << "no result";
            std::cerr << '\n';
            return false;
        }
        return true;
    }

    bool ExpectNoLimit(const char* test_name, const std::vector<AdapterCandidate>& candidates)
    {
        unsigned long long limit{};
        if (GpuMemorySelection::SelectPreferredAdapterMemoryLimit(candidates, limit))
        {
            std::cerr << test_name << " failed: expected no result, got " << limit << '\n';
            return false;
        }
        return true;
    }
}

int main()
{
    if (!ExpectPreferredLimit(
            "prefer discrete gpu",
            {
                { AdapterKind::Integrated, 0, 0, 0, GiB(8), false },
                { AdapterKind::Discrete, 0, GiB(6), 0, 0, false },
            },
            GiB(6)))
    {
        return EXIT_FAILURE;
    }

    if (!ExpectPreferredLimit(
            "fallback to integrated gpu",
            {
                { AdapterKind::Integrated, 0, 0, 0, GiB(8), false },
            },
            GiB(8)))
    {
        return EXIT_FAILURE;
    }

    if (!ExpectPreferredLimit(
            "skip unreadable discrete gpu",
            {
                { AdapterKind::Discrete, 0, 0, 0, 0, false },
                { AdapterKind::Integrated, 0, 0, 0, GiB(12), false },
            },
            GiB(12)))
    {
        return EXIT_FAILURE;
    }

    if (!ExpectPreferredLimit(
            "prefer pdh limit for discrete gpu",
            {
                { AdapterKind::Discrete, GiB(10), GiB(8), 0, 0, false },
                { AdapterKind::Integrated, 0, 0, 0, GiB(12), false },
            },
            GiB(10)))
    {
        return EXIT_FAILURE;
    }

    if (!ExpectPreferredLimit(
            "ignore software adapter",
            {
                { AdapterKind::Discrete, 0, GiB(16), 0, 0, true },
                { AdapterKind::Integrated, 0, 0, 0, GiB(8), false },
            },
            GiB(8)))
    {
        return EXIT_FAILURE;
    }

    if (!ExpectNoLimit(
            "fail closed on unknown adapter kind",
            {
                { AdapterKind::Unknown, GiB(8), 0, 0, 0, false },
            }))
    {
        return EXIT_FAILURE;
    }

    if (!ExpectNoLimit(
            "hide when all adapters unreadable",
            {
                { AdapterKind::Discrete, 0, 0, 0, 0, false },
                { AdapterKind::Integrated, 0, 0, 0, 0, false },
            }))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
