#pragma once

#include <vector>

namespace GpuMemorySelection
{
    enum class AdapterKind
    {
        Discrete,
        Integrated,
        Unknown
    };

    struct AdapterCandidate
    {
        AdapterKind kind{ AdapterKind::Unknown };
        unsigned long long pdh_dedicated_limit{};
        unsigned long long dedicated_video_memory{};
        unsigned long long dedicated_system_memory{};
        unsigned long long shared_system_memory{};
        bool is_software{};
    };

    inline unsigned long long GetCandidateMemoryLimit(const AdapterCandidate& candidate)
    {
        if (candidate.pdh_dedicated_limit > 0)
            return candidate.pdh_dedicated_limit;
        if (candidate.dedicated_video_memory > 0)
            return candidate.dedicated_video_memory;
        if (candidate.shared_system_memory > 0)
            return candidate.shared_system_memory;
        return candidate.dedicated_system_memory;
    }

    inline unsigned long long GetDiscreteMemoryLimit(const AdapterCandidate& candidate)
    {
        if (candidate.pdh_dedicated_limit > 0)
            return candidate.pdh_dedicated_limit;
        return candidate.dedicated_video_memory;
    }

    inline unsigned long long GetIntegratedMemoryLimit(const AdapterCandidate& candidate)
    {
        if (candidate.pdh_dedicated_limit > 0)
            return candidate.pdh_dedicated_limit;
        if (candidate.shared_system_memory > 0)
            return candidate.shared_system_memory;
        return candidate.dedicated_system_memory;
    }

    inline bool SelectBestMemoryLimitByKind(const std::vector<AdapterCandidate>& candidates, AdapterKind kind, unsigned long long& limit)
    {
        limit = 0;
        bool found{};
        for (const auto& candidate : candidates)
        {
            if (candidate.is_software || candidate.kind != kind)
                continue;

            unsigned long long candidate_limit{};
            switch (kind)
            {
            case AdapterKind::Discrete:
                candidate_limit = GetDiscreteMemoryLimit(candidate);
                break;
            case AdapterKind::Integrated:
                candidate_limit = GetIntegratedMemoryLimit(candidate);
                break;
            default:
                candidate_limit = GetCandidateMemoryLimit(candidate);
                break;
            }

            if (candidate_limit == 0)
                continue;

            if (!found || candidate_limit > limit)
            {
                limit = candidate_limit;
                found = true;
            }
        }
        return found;
    }

    inline bool SelectPreferredAdapterMemoryLimit(const std::vector<AdapterCandidate>& candidates, unsigned long long& limit)
    {
        if (SelectBestMemoryLimitByKind(candidates, AdapterKind::Discrete, limit))
            return true;
        if (SelectBestMemoryLimitByKind(candidates, AdapterKind::Integrated, limit))
            return true;
        limit = 0;
        return false;
    }
}
