#pragma once

#include <algorithm>
#include <memory>
#include <random>
#include <set>
#include <vector>

#include "core/structures/mutable_circuit.hpp"

namespace cirbo::minimization::genetic
{

/**
 * @brief Checks whether a gate exists in the circuit.
 * @param circuit Circuit instance.
 * @param id Identifier of the gate.
 * @return true if the gate exists, false otherwise.
 */
template<class CircuitT>
bool isGateExists(CircuitT& circuit, GateId id)
{
    try
    {
        circuit.getGateType(id);
        return true;
    }
    catch (std::out_of_range const&)
    {
        return false;
    }
}

/**
 * @brief Checks whether a gate is an input gate.
 * @param circuit Circuit instance.
 * @param gate Identifier of the gate.
 * @return true if the gate is an input gate.
 */
template<class CircuitT>
bool isInputGate(CircuitT& circuit, size_t gate)
{
    if (!isGateExists(circuit, gate))
    {
        return false;
    }

    return circuit.getGateType(gate) == GateType::INPUT;
}

/**
 * @brief Checks whether a node is reachable from another node.
 * @param circuit Circuit instance.
 * @param ancestor Starting gate in the traversal.
 * @param candidate Gate to check for reachability.
 * @return true if candidate is reachable from ancestor, false otherwise.
 *
 * Used to prevent creation of cycles in the circuit graph.
 */
template<class CircuitT>
bool isDescendant(CircuitT& circuit, GateId ancestor, GateId candidate)
{
    if (ancestor == candidate)
    {
        return true;
    }

    std::stack<GateId> stack;
    std::unordered_set<GateId> visited;

    stack.push(ancestor);

    while (!stack.empty())
    {
        GateId current = stack.top();
        stack.pop();

        if (current == candidate)
        {
            return true;
        }

        if (!visited.insert(current).second)
        {
            continue;
        }

        if (!isGateExists(circuit, current))
        {
            continue;
        }

        for (GateId user_id : circuit.getGateUsers(current))
        {
            if (!visited.count(user_id))
            {
                stack.push(user_id);
            }
        }
    }

    return false;
}

/**
 * @brief Validates operand count for a given gate type.
 * @param type Type of the gate.
 * @param operands List of operands for the gate.
 * @return true if the operand count is valid for the gate type.
 */
inline bool isValidArity(GateType type, std::vector<GateId> const& operands)
{
    if (type == GateType::NOT)
    {
        return operands.size() == 1;
    }

    if (type == GateType::AND || type == GateType::OR || type == GateType::XOR ||
        type == GateType::NAND || type == GateType::NOR || type == GateType::NXOR)
    {
        return operands.size() == 2;
    }

    return true;
}

/**
 * @brief Generates a random index in range [0, size).
 * @param size Upper bound of the range.
 * @param rng Random generator.
 * @return Random index.
 */
inline size_t randomIndex(size_t size, std::mt19937& rng)
{
    if (size == 0)
    {
        return 0;
    }

    std::uniform_int_distribution<size_t> dist(0, size - 1);
    return dist(rng);
}

/**
 * @brief Generates a random floating point value in range [0.0, 1.0].
 * @param rng Random generator.
 * @return Random double value.
 */
inline double randomDouble(std::mt19937& rng)
{
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}

/**
 * @brief Selects a random non-input gate from the circuit.
 * @param circuit Circuit instance.
 * @param rng Random generator.
 * @return Identifier of a randomly selected gate.
 *
 * If no valid gate exists, returns 0.
 */
template<class CircuitT>
size_t randomGate(CircuitT& circuit, std::mt19937& rng)
{
    std::vector<GateId> valid;

    for (GateId i = 0; i < circuit.getNumberOfGates(); ++i)
    {
        if (isGateExists(circuit, i) && !isInputGate(circuit, i))
        {
            valid.push_back(i);
        }
    }

    if (valid.empty())
    {
        return 0;
    }

    return valid[randomIndex(valid.size(), rng)];
}

/**
 * @brief Selects a random operand candidate for a gate.
 * @param circuit Circuit instance.
 * @param gate_id Identifier of the gate whose operands are being chosen.
 * @param rng Random generator.
 * @return Identifier of a randomly selected valid operand gate.
 */
template<class CircuitT>
GateId randomOperand(CircuitT& circuit, size_t gate_id, std::mt19937& rng)
{
    std::vector<GateId> candidates;

    for (GateId i = 0; i < gate_id; ++i)
    {
        if (isGateExists(circuit, i))
        {
            candidates.push_back(i);
        }
    }

    if (candidates.empty())
    {
        return 0;
    }

    return candidates[randomIndex(candidates.size(), rng)];
}

/**
 * @brief Changes the type of a randomly selected gate.
 *
 * Replaces the gate type with another compatible binary gate
 * while preserving the existing operands.
 */
template<class CircuitT>
void changeGateTypeMutation(CircuitT& circuit, std::mt19937& rng)
{
    if (circuit.getActualNumberOfGates() <= 1)
    {
        return;
    }

    size_t gate = randomGate(circuit, rng);

    if (isInputGate(circuit, gate))
    {
        return;
    }

    auto operands = circuit.getGateOperands(gate);

    if (operands.size() != 2)
    {
        return;
    }

    std::vector<GateType> possible_types = {
        GateType::AND,
        GateType::OR,
        GateType::XOR,
        GateType::NAND,
        GateType::NOR,
        GateType::NXOR};

    GateType new_type = GateType::AND;

    circuit.changeGateType(gate, new_type);
}

/**
 * @brief Reconnects one operand of a randomly selected gate.
 *
 * One operand of the gate is replaced with another valid gate
 * while preventing cycles in the circuit.
 */
template<class CircuitT>
void reconnectOperandMutation(CircuitT& circuit, std::mt19937& rng)
{
    if (circuit.getActualNumberOfGates() <= 1)
    {
        return;
    }

    size_t gate = randomGate(circuit, rng);

    if (isInputGate(circuit, gate) || circuit.isOutputGate(gate))
    {
        return;
    }

    auto operands = circuit.getGateOperands(gate);

    if (operands.size() < 2)
    {
        return;
    }

    size_t operand_index = randomIndex(operands.size(), rng);
    GateId old_operand = operands[operand_index];

    GateId new_operand = old_operand;

    int const max_attempts = 20;

    for (int attempt = 0; attempt < max_attempts && new_operand == old_operand; ++attempt)
    {
        GateId candidate = randomOperand(circuit, gate, rng);

        if (!isDescendant(circuit, gate, candidate))
        {
            new_operand = candidate;
        }
    }

    if (new_operand == old_operand)
    {
        return;
    }

    operands[operand_index] = new_operand;

    auto users = circuit.getGateUsers(gate);

    bool was_output = circuit.isOutputGate(gate);

    GateType type = circuit.getGateType(gate);

    if (!isValidArity(type, operands))
    {
        return;
    }

    circuit.addGate(type, operands, was_output);

    GateId new_gate_id = circuit.getNumberOfGates() - 1;

    for (GateId user_id : users)
    {
        if (!isGateExists(circuit, user_id))
        {
            continue;
        }

        if (isDescendant(circuit, new_operand, user_id))
        {
            continue;
        }

        circuit.replaceOperand(user_id, gate, new_gate_id);
    }

    if (!circuit.isOutputGate(gate) && !circuit.isGateHasUsers(gate))
    {
        circuit.removeGate(gate);
    }
}

/**
 * @brief Inserts a NOT gate before a randomly selected operand.
 *
 * Creates a new NOT gate that negates one operand of the selected gate
 * and reconnects the circuit to use the new node.
 */
template<class CircuitT>
void insertNotMutation(CircuitT& circuit, std::mt19937& rng)
{
    if (circuit.getActualNumberOfGates() <= 1)
    {
        return;
    }

    size_t gate = randomGate(circuit, rng);

    if (isInputGate(circuit, gate))
    {
        return;
    }

    auto operands = circuit.getGateOperands(gate);

    if (operands.empty())
    {
        return;
    }

    size_t operand_index = randomIndex(operands.size(), rng);

    GateId target_operand = operands[operand_index];

    circuit.addGate(GateType::NOT, {target_operand}, false);

    GateId not_gate_id = circuit.getNumberOfGates() - 1;

    operands[operand_index] = not_gate_id;

    auto users = circuit.getGateUsers(gate);

    bool was_output = circuit.isOutputGate(gate);

    GateType type = circuit.getGateType(gate);

    if (!isValidArity(type, operands))
    {
        return;
    }

    circuit.addGate(type, operands, was_output);

    GateId new_gate_id = circuit.getNumberOfGates() - 1;

    for (GateId user_id : users)
    {
        if (isGateExists(circuit, user_id) && user_id != new_gate_id)
        {
            if (!isDescendant(circuit, not_gate_id, user_id))
            {
                circuit.replaceOperand(user_id, gate, new_gate_id);
            }
        }
    }

    if (!circuit.isOutputGate(gate) && !circuit.isGateHasUsers(gate))
    {
        circuit.removeGate(gate);
    }
}

/**
 * @brief Adds a new randomly generated gate to the circuit.
 *
 * A new gate with randomly selected type and operands is created.
 * With certain probability it may replace an operand in an existing user gate.
 */
template<class CircuitT>
void addRandomGateMutation(CircuitT& circuit, std::mt19937& rng)
{
    if (circuit.getActualNumberOfGates() <= 1)
    {
        return;
    }

    GateId existing_gate = randomGate(circuit, rng);

    std::vector<GateType> possible_types = {
        GateType::AND,
        GateType::NOT};

    GateType new_type = possible_types[randomIndex(possible_types.size(), rng)];

    std::vector<GateId> operands;

    if (new_type == GateType::NOT)
    {
        GateId operand = randomOperand(circuit, existing_gate, rng);
        operands = {operand};
    }
    else
    {
        GateId operand1 = randomOperand(circuit, existing_gate, rng);
        GateId operand2 = randomOperand(circuit, existing_gate, rng);

        int const max_attempts = 2;

        for (int attempt = 0; attempt < max_attempts && operand2 == operand1; ++attempt)
        {
            operand2 = randomOperand(circuit, existing_gate, rng);
        }

        operands = {operand1, operand2};
    }

    if (!isValidArity(new_type, operands))
    {
        return;
    }

    circuit.addGate(new_type, operands, false);

    GateId new_gate_id = circuit.getNumberOfGates() - 1;

    bool should_connect = randomDouble(rng) < 0.7;

    if (should_connect && circuit.getGateUsers(existing_gate).size() > 0)
    {
        auto users = circuit.getGateUsers(existing_gate);

        if (!users.empty())
        {
            GateId random_user = users[randomIndex(users.size(), rng)];

            if (!isGateExists(circuit, random_user))
            {
                return;
            }

            if (random_user == new_gate_id)
            {
                return;
            }

            if (isDescendant(circuit, existing_gate, random_user))
            {
                return;
            }

            circuit.replaceOperand(random_user, existing_gate, new_gate_id);
        }
    }
}

/**
 * @brief Duplicates an existing gate.
 *
 * Creates a new gate with the same type and operands
 * as a randomly selected gate in the circuit.
 */
template<class CircuitT>
void duplicateGateMutation(CircuitT& circuit, std::mt19937& rng)
{
    if (circuit.getActualNumberOfGates() <= 1)
    {
        return;
    }

    size_t gate = randomGate(circuit, rng);

    if (isInputGate(circuit, gate))
    {
        return;
    }

    auto operands = circuit.getGateOperands(gate);
    auto type = circuit.getGateType(gate);

    for (auto op : operands)
    {
        if (!isGateExists(circuit, op))
        {
            return;
        }
    }

    circuit.addGate(type, operands, false);
}

/**
 * @brief Removes a randomly selected gate.
 *
 * The gate is removed only if it is not an input, not an output,
 * and has no users in the circuit.
 */
template<class CircuitT>
void removeRandomGateMutation(CircuitT& circuit, std::mt19937& rng)
{
    if (circuit.getActualNumberOfGates() <= 1)
    {
        return;
    }

    size_t gate = randomGate(circuit, rng);

    if (isInputGate(circuit, gate) || circuit.isOutputGate(gate))
    {
        return;
    }

    auto users = circuit.getGateUsers(gate);

    if (!users.empty())
    {
        return;
    }

    circuit.removeGate(gate);
}

/**
 * @brief Replaces one output gate with another gate.
 *
 * Randomly selects an existing output gate and replaces it
 * with another valid non-input gate in the circuit.
 */
template<class CircuitT>
void changeOutputGateMutation(CircuitT& circuit, std::mt19937& rng)
{
    if (circuit.getActualNumberOfGates() <= 1)
    {
        return;
    }

    auto current_outputs = circuit.getOutputGates();

    size_t output_index = randomIndex(current_outputs.size(), rng);

    GateId old_output = current_outputs[output_index];

    GateId new_output = old_output;

    int const max_attempts = 20;

    for (int attempt = 0; attempt < max_attempts && new_output == old_output; ++attempt)
    {
        GateId candidate = randomGate(circuit, rng);

        if (!circuit.isOutputGate(candidate) && !isInputGate(circuit, candidate))
        {
            new_output = candidate;
        }
    }

    if (new_output == old_output)
    {
        return;
    }

    circuit.replaceOutput(old_output, new_output);
}

/**
 * @brief Replaces one subtree with another subtree.
 *
 * Selects a target gate and replaces all its usages with another
 * randomly selected compatible gate, effectively swapping subgraphs.
 */
template<class CircuitT>
void replaceSubtreeMutation(CircuitT& circuit, std::mt19937& rng)
{
    if (circuit.getActualNumberOfGates() <= 3)
    {
        return;
    }

    GateId target_root = randomGate(circuit, rng);
    GateId donor_root = randomGate(circuit, rng);

    if (target_root == donor_root)
    {
        return;
    }

    if (!isGateExists(circuit, target_root) || !isGateExists(circuit, donor_root))
    {
        return;
    }

    if (isInputGate(circuit, target_root))
    {
        return;
    }

    if (isDescendant(circuit, target_root, donor_root))
    {
        return;
    }

    auto users = circuit.getGateUsers(target_root);

    for (GateId user_id : users)
    {
        if (!isGateExists(circuit, user_id))
        {
            continue;
        }

        if (isDescendant(circuit, donor_root, user_id))
        {
            continue;
        }

        circuit.replaceOperand(user_id, target_root, donor_root);
    }

    if (!circuit.isOutputGate(target_root) && !circuit.isGateHasUsers(target_root))
    {
        circuit.removeGate(target_root);
    }
}

/**
 * @brief Applies one randomly selected mutation.
 *
 * Randomly chooses one of the available mutation types
 * and applies it to the circuit.
 */
template<class CircuitT>
void applyRandomMutation(CircuitT& circuit, std::mt19937& rng)
{
    if (circuit.getActualNumberOfGates() <= 3)
    {
        return;
    }

    using MutationFn = void(*)(CircuitT&, std::mt19937&);

    static MutationFn mutations[] = {
        changeGateTypeMutation<CircuitT>,
        reconnectOperandMutation<CircuitT>,
        insertNotMutation<CircuitT>,
        addRandomGateMutation<CircuitT>,
        duplicateGateMutation<CircuitT>,
        removeRandomGateMutation<CircuitT>,
        changeOutputGateMutation<CircuitT>,
        replaceSubtreeMutation<CircuitT>
    };

    std::uniform_int_distribution<int> dist(
        0,
        static_cast<int>(std::size(mutations)) - 1
    );

    int idx = dist(rng);

    mutations[idx](circuit, rng);
}

} // namespace cirbo::minimization::genetic