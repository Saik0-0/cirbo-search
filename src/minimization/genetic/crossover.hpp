#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <queue>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/structures/mutable_circuit.hpp"


namespace cirbo::minimization::genetic
{

template <typename CircuitT>
std::vector<GateId> buildTopologicalOrder(CircuitT const& circuit)
{
    std::unordered_map<GateId, size_t> indegree;

    for (GateId id = 0; id < circuit.getNumberOfGates(); ++id)
    {
        if (!circuit.isGateExists(id))
        {
            continue;
        }

        indegree[id] = circuit.getGateOperands(id).size();
    }

    std::queue<GateId> q;

    for (auto const& [id, deg] : indegree)
    {
        if (deg == 0)
        {
            q.push(id);
        }
    }

    std::vector<GateId> topo;

    while (!q.empty())
    {
        GateId v = q.front();
        q.pop();

        topo.push_back(v);

        for (GateId user : circuit.getGateUsers(v))
        {
            if (!indegree.contains(user))
            {
                continue;
            }

            if (--indegree[user] == 0)
            {
                q.push(user);
            }
        }
    }

    return topo;
}

template <typename CircuitT>
std::unique_ptr<CircuitT> layerCrossover(
    CircuitT const& parent1,
    CircuitT const& parent2,
    std::mt19937& rng)
{
    // std::cerr << "\n========== LAYER CROSSOVER ==========\n";

    auto topo1 = buildTopologicalOrder(parent1);
    auto topo2 = buildTopologicalOrder(parent2);

    // std::cerr << "[CROSSOVER] parent1 topo size = "
    //           << topo1.size() << "\n";

    // std::cerr << "[CROSSOVER] parent2 topo size = "
    //           << topo2.size() << "\n";

    if (topo1.size() < 2 || topo2.size() < 2)
    {
        // std::cerr << "[CROSSOVER] topo too small, fallback\n";
        return std::make_unique<CircuitT>(parent1);
    }

    size_t max_cut =
        std::min(topo1.size(), topo2.size()) - 1;

    std::uniform_int_distribution<size_t> dist(1, max_cut);

    size_t cut_index = dist(rng);

    // std::cerr << "[CROSSOVER] cut index = "
    //           << cut_index << "\n";

    auto child = std::make_unique<CircuitT>();

    std::unordered_map<GateId, GateId> map1;
    std::unordered_map<GateId, GateId> map2;

    std::unordered_set<GateId> already_copied;

    /*
     * COPY UPPER PART FROM PARENT1
     */

    for (size_t i = 0; i < cut_index; ++i)
    {
        GateId old_id = topo1[i];

        if (already_copied.contains(old_id))
        {
            continue;
        }

        already_copied.insert(old_id);

        GateType type = parent1.getGateType(old_id);

        GateIdContainer operands;

        bool can_build = true;

        for (GateId op : parent1.getGateOperands(old_id))
        {
            if (!map1.contains(op))
            {
                can_build = false;
                break;
            }

            operands.push_back(map1[op]);
        }

        if (!can_build)
        {
            // std::cerr
            //     << "[CROSSOVER] skip upper gate "
            //     << old_id
            //     << " because operand not mapped\n";

            continue;
        }

        bool is_output =
            parent1.isOutputGate(old_id);

        child->addGate(
            type,
            operands,
            is_output);

        GateId new_id =
            child->getNumberOfGates() - 1;

        map1[old_id] = new_id;

        // std::cerr << "[CROSSOVER] upper copy "
        //           << old_id
        //           << " -> "
        //           << new_id;

        // if (is_output)
        // {
        //     std::cerr << " [OUTPUT]";
        // }

        // std::cerr << "\n";
    }

    /*
     * FRONTIER MAP
     */

    for (size_t i = 0;
         i < cut_index && i < topo2.size();
         ++i)
    {
        GateId p2_gate = topo2[i];

        if (i < topo1.size())
        {
            GateId p1_gate = topo1[i];

            if (map1.contains(p1_gate))
            {
                map2[p2_gate] = map1[p1_gate];

                // std::cerr
                //     << "[CROSSOVER] frontier "
                //     << p2_gate
                //     << " -> "
                //     << map2[p2_gate]
                //     << "\n";
            }
        }
    }

    /*
     * COPY LOWER PART FROM PARENT2
     */

    bool progress = true;

    while (progress)
    {
        progress = false;

        for (size_t i = cut_index;
             i < topo2.size();
             ++i)
        {
            GateId old_id = topo2[i];

            if (map2.contains(old_id))
            {
                continue;
            }

            GateType type =
                parent2.getGateType(old_id);

            GateIdContainer operands;

            bool ready = true;

            for (GateId op :
                 parent2.getGateOperands(old_id))
            {
                if (!map2.contains(op))
                {
                    ready = false;

                    // std::cerr
                    //     << "[CROSSOVER] unresolved operand "
                    //     << op
                    //     << " for gate "
                    //     << old_id
                    //     << "\n";

                    break;
                }

                operands.push_back(map2[op]);
            }

            if (!ready)
            {
                continue;
            }

            bool is_output =
                parent2.isOutputGate(old_id);

            child->addGate(
                type,
                operands,
                is_output);

            GateId new_id =
                child->getNumberOfGates() - 1;

            map2[old_id] = new_id;

            progress = true;

            // std::cerr
            //     << "[CROSSOVER] lower copy "
            //     << old_id
            //     << " -> "
            //     << new_id;

            // if (is_output)
            // {
            //     std::cerr << " [OUTPUT]";
            // }

            // std::cerr << "\n";
        }
    }

    /*
     * VALIDATE OUTPUTS
     */

    if (child->getOutputGates().empty())
    {
        // std::cerr
        //     << "[CROSSOVER] no outputs in child, fallback\n";

        return std::make_unique<CircuitT>(parent1);
    }

    /*
     * VALIDATE ALL OUTPUTS RESOLVED
     */

    for (GateId out : parent2.getOutputGates())
    {
        if (!map2.contains(out))
        {
            // std::cerr
            //     << "[CROSSOVER] unresolved output "
            //     << out
            //     << ", fallback\n";

            return std::make_unique<CircuitT>(parent1);
        }
    }

    // std::cerr << "[CROSSOVER] SUCCESS\n";

    return child;
}

} // namespace cirbo::minimization::genetic