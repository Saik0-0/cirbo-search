#pragma once

#include <algorithm>
#include <memory>
#include <random>
#include <set>
#include <vector>

#include "core/structures/mutable_circuit.hpp"

namespace cirbo::minimization::genetic
{

template<class CircuitT>
class Mutation
{
public:
    /**
     * @brief Constructs a mutation operator for a given circuit.
     * @param circuit_ Reference to the circuit that will be modified by mutations.
     *
     * Initializes the mutation object and seeds the internal random
     * number generator used for selecting mutation operations.
     */
    Mutation(CircuitT& circuit_)
        : circuit(circuit_)
    {
        rng.seed(std::random_device{}());
    }

    /**
     * @brief Changes the type of a randomly selected gate.
     *
     * Replaces the gate type with another compatible binary gate
     * while preserving the existing operands.
     */
    void changeGateTypeMutation()
    {
        if (circuit.getActualNumberOfGates() <= 1)
        {
            return;
        }

        size_t gate = randomGate();

        if (isInputGate(gate))
        {
            return;
        }

        auto operands = circuit.getGateOperands(gate);
        if (operands.size() != 2)
        {
            return;
        }

        std::vector<GateType> possible_types;

        possible_types = {GateType::AND, GateType::OR, GateType::XOR, GateType::NAND, GateType::NOR, GateType::NXOR};

        GateType current_type = circuit.getGateType(gate);

        possible_types.erase(
            std::remove(possible_types.begin(), possible_types.end(), current_type), possible_types.end());

        GateType new_type = possible_types[randomIndex(possible_types.size())];

        circuit.changeGateType(gate, new_type);
    }

    /**
     * @brief Reconnects one operand of a randomly selected gate.
     *
     * One operand of the gate is replaced with another valid gate
     * while preventing cycles in the circuit.
     */
    void reconnectOperandMutation()
    {
        if (circuit.getActualNumberOfGates() <= 1)
        {
            return;
        }

        size_t gate = randomGate();

        if (isInputGate(gate) || circuit.isOutputGate(gate))
        {
            return;
        }

        auto operands = circuit.getGateOperands(gate);

        if (operands.size() < 2)
        {
            return;
        }

        size_t operand_index = randomIndex(operands.size());
        GateId old_operand   = operands[operand_index];

        GateId new_operand     = old_operand;
        int const max_attempts = 20;
        for (int attempt = 0; attempt < max_attempts && new_operand == old_operand; ++attempt)
        {
            GateId candidate = randomOperand(gate);
            if (!isDescendant(gate, candidate))
            {
                new_operand = candidate;
            }
        }

        if (new_operand == old_operand)
        {
            return;
        }

        operands[operand_index] = new_operand;
        auto users              = circuit.getGateUsers(gate);
        bool was_output         = circuit.isOutputGate(gate);
        GateType type           = circuit.getGateType(gate);

        if (!isValidArity(type, operands))
        {
            return;
        }

        circuit.addGate(type, operands, was_output);
        GateId new_gate_id = circuit.getNumberOfGates() - 1;

        for (GateId user_id : users)
        {
            if (!isGateExists(user_id))
            {
                continue;
            }

            if (isDescendant(new_operand, user_id))
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
    void insertNotMutation()
    {
        if (circuit.getActualNumberOfGates() <= 1)
        {
            return;
        }

        size_t gate = randomGate();

        if (isInputGate(gate))
        {
            return;
        }

        auto operands = circuit.getGateOperands(gate);

        if (operands.empty())
        {
            return;
        }

        size_t operand_index  = randomIndex(operands.size());
        GateId target_operand = operands[operand_index];

        circuit.addGate(GateType::NOT, {target_operand}, false);
        GateId not_gate_id = circuit.getNumberOfGates() - 1;

        operands[operand_index] = not_gate_id;

        auto users      = circuit.getGateUsers(gate);
        bool was_output = circuit.isOutputGate(gate);
        GateType type   = circuit.getGateType(gate);

        if (!isValidArity(type, operands))
        {
            return;
        }

        circuit.addGate(type, operands, was_output);
        GateId new_gate_id = circuit.getNumberOfGates() - 1;

        for (GateId user_id : users)
        {
            if (isGateExists(user_id) && user_id != new_gate_id)
            {
                if (!isDescendant(not_gate_id, user_id))
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
    void addRandomGateMutation()
    {
        if (circuit.getActualNumberOfGates() <= 1)
        {
            return;
        }

        GateId existing_gate = randomGate();

        std::vector<GateType> possible_types = {
            GateType::AND, GateType::OR, GateType::XOR, GateType::NAND, GateType::NOR, GateType::NXOR, GateType::NOT};

        GateType new_type = possible_types[randomIndex(possible_types.size())];
        std::vector<GateId> operands;

        if (new_type == GateType::NOT)
        {
            GateId operand = randomOperand(existing_gate);
            operands       = {operand};
        }
        else
        {
            GateId operand1 = randomOperand(existing_gate);
            GateId operand2 = randomOperand(existing_gate);

            int const max_attempts = 2;
            for (int attempt = 0; attempt < max_attempts && operand2 == operand1; ++attempt)
            {
                operand2 = randomOperand(existing_gate);
            }

            operands = {operand1, operand2};
        }

        if (!isValidArity(new_type, operands))
        {
            return;
        }

        circuit.addGate(new_type, operands, false);
        GateId new_gate_id = circuit.getNumberOfGates() - 1;

        bool should_connect = randomDouble() < 0.7;
        if (should_connect && circuit.getGateUsers(existing_gate).size() > 0)
        {
            auto users = circuit.getGateUsers(existing_gate);
            if (!users.empty())
            {
                GateId random_user = users[randomIndex(users.size())];

                if (!isGateExists(random_user))
                {
                    return;
                }

                if (random_user == new_gate_id)
                {
                    return;
                }

                if (isDescendant(existing_gate, random_user))
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
    void duplicateGateMutation()
    {
        if (circuit.getActualNumberOfGates() <= 1)
        {
            return;
        }

        size_t gate = randomGate();

        if (isInputGate(gate))
        {
            return;
        }

        auto operands = circuit.getGateOperands(gate);
        auto type     = circuit.getGateType(gate);

        for (auto op : operands)
        {
            if (!isGateExists(op))
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
    void removeRandomGateMutation()
    {
        if (circuit.getActualNumberOfGates() <= 1)
        {
            return;
        }

        size_t gate = randomGate();

        if (isInputGate(gate) || circuit.isOutputGate(gate))
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
    void changeOutputGateMutation()
    {
        if (circuit.getActualNumberOfGates() <= 1)
        {
            return;
        }

        auto current_outputs = circuit.getOutputGates();

        size_t output_index = randomIndex(current_outputs.size());
        GateId old_output   = current_outputs[output_index];

        GateId new_output      = old_output;
        int const max_attempts = 20;
        for (int attempt = 0; attempt < max_attempts && new_output == old_output; ++attempt)
        {
            GateId candidate = randomGate();
            if (!circuit.isOutputGate(candidate) && !isInputGate(candidate))
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
     * @brief Applies one randomly selected mutation.
     *
     * Randomly chooses one of the available mutation types
     * and applies it to the circuit.
     */
    void applyRandomMutation()
    {
        if (circuit.getActualNumberOfGates() <= 3)
        {
            return;
        }

        std::uniform_int_distribution<int> dist(0, 6);
        int mutation_type = dist(rng);

        switch (mutation_type)
        {
            case 0:
                changeGateTypeMutation();
                break;
            case 1:
                reconnectOperandMutation();
                break;
            case 2:
                insertNotMutation();
                break;
            case 3:
                addRandomGateMutation();
                break;
            case 4:
                duplicateGateMutation();
                break;
            case 5:
                removeRandomGateMutation();
                break;
            case 6:
                changeOutputGateMutation();
                break;
        }
    }

private:
    CircuitT& circuit;
    std::mt19937 rng;

    /**
     * @brief Checks whether a node is reachable from another node.
     * @param ancestor Starting gate in the traversal.
     * @param candidate Gate to check for reachability.
     * @return true if candidate is reachable from ancestor, false otherwise.
     *
     * Used to prevent creation of cycles in the circuit graph.
     */
    bool isDescendant(GateId ancestor, GateId candidate) const
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

            if (!isGateExists(current))
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
    bool isValidArity(GateType type, std::vector<GateId> const& operands)
    {
        if (type == GateType::NOT)
        {
            return operands.size() == 1;
        }

        if (type == GateType::AND || type == GateType::OR || type == GateType::XOR || type == GateType::NAND ||
            type == GateType::NOR || type == GateType::NXOR)
        {
            return operands.size() == 2;
        }

        return true;
    }

    /**
     * @brief Checks whether a gate exists in the circuit.
     * @param id Identifier of the gate.
     * @return true if the gate exists, false otherwise.
     */
    bool isGateExists(GateId id) const
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
     * @brief Selects a random non-input gate from the circuit.
     * @return Identifier of a randomly selected gate.
     *
     * If no valid gate exists, returns 0.
     */
    size_t randomGate()
    {
        std::vector<GateId> valid;

        for (GateId i = 0; i < circuit.getNumberOfGates(); ++i)
        {
            if (isGateExists(i) && !isInputGate(i))
            {
                valid.push_back(i);
            }
        }

        if (valid.empty())
        {
            return 0;
        }

        return valid[randomIndex(valid.size())];
    }

    /**
     * @brief Generates a random index in range [0, size).
     */
    size_t randomIndex(size_t size)
    {
        if (size == 0)
        {
            return 0;
        }
        std::uniform_int_distribution<size_t> dist(0, size - 1);
        return dist(rng);
    }

    /**
     * @brief Selects a random operand candidate for a gate.
     * @param gate_id Identifier of the gate whose operands are being chosen.
     * @return Identifier of a randomly selected valid operand gate.
     */
    GateId randomOperand(size_t gate_id)
    {
        std::vector<GateId> candidates;
        for (GateId i = 0; i < gate_id; ++i)
        {
            if (isGateExists(i))
            {
                candidates.push_back(i);
            }
        }

        if (candidates.empty())
        {
            return 0;
        }

        return candidates[randomIndex(candidates.size())];
    }

    double randomDouble()
    {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng);
    }

    bool isInputGate(size_t gate)
    {
        if (!isGateExists(gate))
        {
            return false;
        }
        return circuit.getGateType(gate) == GateType::INPUT;
    }
};

}  // namespace cirbo::minimization::genetic