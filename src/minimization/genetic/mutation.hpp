#pragma once

#include "core/structures/mutable_circuit.hpp"
#include <random>
#include <vector>
#include <algorithm>
#include <memory>
#include <set>

namespace cirbo::minimization::genetic
{

template<class CircuitT>
class Mutation
{
public:

    Mutation(CircuitT& circuit_)
        : circuit(circuit_)
    {
        rng.seed(std::random_device{}());
    }

    /*
        Mutation 1
        Replace gate type (keep valid arity)
    */
    void changeGateTypeMutation()
    {
        if (circuit.getNumberOfGates() <= 1)
            return;

        size_t gate = randomGate();

        if (isInputGate(gate))
            return;

        auto operands = circuit.getGateOperands(gate);
        if (operands.size() != 2)
            return;

        std::vector<GateType> possible_types;

        possible_types = {
            GateType::AND,
            GateType::OR,
            GateType::XOR,
            GateType::NAND,
            GateType::NOR,
            GateType::NXOR
        };

        GateType current_type = circuit.getGateType(gate);

        possible_types.erase(
            std::remove(possible_types.begin(), possible_types.end(), current_type),
            possible_types.end()
        );

        GateType new_type = possible_types[randomIndex(possible_types.size())];

        circuit.changeGateType(gate, new_type);

    }

    /*
        Mutation 2
        Reconnect operand
    */
    void reconnectOperandMutation()
    {
        if (circuit.getNumberOfGates() <= 1)
            return;

        size_t gate = randomGate();

        if (isInputGate(gate) || circuit.isOutputGate(gate))
            return;

        auto operands = circuit.getGateOperands(gate);

        if (operands.size() < 2)
            return;

        size_t operand_index = randomIndex(operands.size());
        GateId old_operand = operands[operand_index];

        GateId new_operand = old_operand;
        const int max_attempts = 20;
        for (int attempt = 0; attempt < max_attempts && new_operand == old_operand; ++attempt)
        {
            GateId candidate = randomOperand(gate);
            if (!isDescendant(gate, candidate))
                new_operand = candidate;
        }

        if (new_operand == old_operand)
            return;

        operands[operand_index] = new_operand;
        auto users = circuit.getGateUsers(gate);
        bool was_output = circuit.isOutputGate(gate);
        GateType type = circuit.getGateType(gate);
        
        if (!isValidArity(type, operands))
            return;

        circuit.addGate(type, operands, was_output);
        GateId new_gate_id = circuit.getNumberOfGates() - 1;

        for (GateId user_id : users)
        {
            if (!isGateExists(user_id))
                continue;

            if (isDescendant(new_operand, user_id))
                continue;

            circuit.replaceOperand(user_id, gate, new_gate_id);
        }

        if (!circuit.isOutputGate(gate) && !circuit.isGateHasUsers(gate))
        {
            circuit.removeGate(gate);
        }
    }

    /*
        Mutation 3
        Insert NOT gate before gate
    */
    void insertNotMutation()
    {
        if (circuit.getNumberOfGates() <= 1)
            return;

        size_t gate = randomGate();

        if (isInputGate(gate))
            return;

        auto operands = circuit.getGateOperands(gate);
        
        if (operands.empty())
            return;

        size_t operand_index = randomIndex(operands.size());
        GateId target_operand = operands[operand_index];

        circuit.addGate(GateType::NOT, {target_operand}, false);
        GateId not_gate_id = circuit.getNumberOfGates() - 1;

        operands[operand_index] = not_gate_id;
        
        auto users = circuit.getGateUsers(gate);
        bool was_output = circuit.isOutputGate(gate);
        GateType type = circuit.getGateType(gate);
        
        if (!isValidArity(type, operands))
            return;

        circuit.addGate(type, operands, was_output);
        GateId new_gate_id = circuit.getNumberOfGates() - 1;
        
        for (GateId user_id : users)
        {
            if (isGateExists(user_id) && user_id != new_gate_id) 
            {
                if (!isDescendant(not_gate_id, user_id))
                    circuit.replaceOperand(user_id, gate, new_gate_id);
            }
        }
        
        if (!circuit.isOutputGate(gate) && !circuit.isGateHasUsers(gate))
        {
            circuit.removeGate(gate);
        }
    }

    /*
        Mutation 4
        Add random gate
    */
    void addRandomGateMutation()
    {
        if (circuit.getNumberOfGates() <= 1)
            return;

        GateId existing_gate = randomGate();

        std::vector<GateType> possible_types = {
            GateType::AND, GateType::OR, GateType::XOR,
            GateType::NAND, GateType::NOR, GateType::NXOR,
            GateType::NOT
        };

        GateType new_type = possible_types[randomIndex(possible_types.size())];
        std::vector<GateId> operands;

        if (new_type == GateType::NOT)
        {
            GateId operand = randomOperand(existing_gate);
            operands = {operand};
        }
        else
        {
            GateId operand1 = randomOperand(existing_gate);
            GateId operand2 = randomOperand(existing_gate);

            const int max_attempts = 2;
            for (int attempt = 0; attempt < max_attempts && operand2 == operand1; ++attempt)
            {
                operand2 = randomOperand(existing_gate);
            }

            operands = {operand1, operand2};
        }

        if (!isValidArity(new_type, operands))
            return;

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
                    return;

                if (random_user == new_gate_id)
                    return;

                if (isDescendant(existing_gate, random_user))
                    return;

                circuit.replaceOperand(random_user, existing_gate, new_gate_id);
            }
        }
    }

    /*
        Mutation 5
        Duplicate substructure (original)
    */
    void duplicateGateMutation()
    {
        if (circuit.getNumberOfGates() <= 1)
            return;

        size_t gate = randomGate();

        if (isInputGate(gate))
            return;

        auto operands = circuit.getGateOperands(gate);
        auto type = circuit.getGateType(gate);

        for (auto op : operands)
        {
            if (!isGateExists(op))
                return;
        }

        circuit.addGate(type, operands, false);
    }

    /*
        Mutation 6
        Remove random gate
    */
    void removeRandomGateMutation()
    {
        if (circuit.getNumberOfGates() <= 1)
            return;

        size_t gate = randomGate();

        if (isInputGate(gate) || circuit.isOutputGate(gate))
            return;

        auto users = circuit.getGateUsers(gate);
        if (!users.empty())
            return;
        
        circuit.removeGate(gate);
    }

    /*
        Mutation 7
        Change output gate
    */
    void changeOutputGateMutation()
    {
        if (circuit.getNumberOfGates() <= 1)
            return;

        auto current_outputs = circuit.getOutputGates();
            
        size_t output_index = randomIndex(current_outputs.size());
        GateId old_output = current_outputs[output_index];
        
        GateId new_output = old_output;
        const int max_attempts = 20;
        for (int attempt = 0; attempt < max_attempts && new_output == old_output; ++attempt)
        {
            GateId candidate = randomGate();
            if (!circuit.isOutputGate(candidate) && !isInputGate(candidate))
            {
                new_output = candidate;
            }
        }
        
        if (new_output == old_output)
            return;
        
        circuit.replaceOutput(old_output, new_output);
        
    }

    void applyRandomMutation()
    {
        if (circuit.getNumberOfGates() <= 3)
            return;

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

    bool isDescendant(GateId ancestor, GateId candidate) const
    {
        if (ancestor == candidate)
            return true;

        std::stack<GateId> stack;
        std::unordered_set<GateId> visited;
        stack.push(ancestor);

        while (!stack.empty())
        {
            GateId current = stack.top();
            stack.pop();

            if (current == candidate)
                return true;

            if (!visited.insert(current).second)
                continue;

            if (!isGateExists(current))
                continue;

            for (GateId user_id : circuit.getGateUsers(current))
            {
                if (!visited.count(user_id))
                    stack.push(user_id);
            }
        }
        return false;
    }
    

    bool isValidArity(GateType type, const std::vector<GateId>& operands)
    {
        if (type == GateType::NOT)
            return operands.size() == 1;

        if (type == GateType::AND ||
            type == GateType::OR  ||
            type == GateType::XOR ||
            type == GateType::NAND||
            type == GateType::NOR ||
            type == GateType::NXOR)
            return operands.size() == 2;

        return true;
    }

    bool isGateExists(GateId id) const
    {
        try {
            circuit.getGateType(id);
            return true;
        } catch (const std::out_of_range&) {
            return false;
        }
    }

    size_t randomGate()
    {
        std::vector<GateId> valid;

        for (GateId i = 0; i < circuit.getNumberOfGates(); ++i)
            if (isGateExists(i) && !isInputGate(i))
                valid.push_back(i);

        if (valid.empty())
            return 0;

        return valid[randomIndex(valid.size())];
    }

    size_t randomIndex(size_t size)
    {
        if (size == 0)
            return 0;
        std::uniform_int_distribution<size_t> dist(0, size - 1);
        return dist(rng);
    }

    GateId randomOperand(size_t gate_id)
    {
        std::vector<GateId> candidates;
        for (GateId i = 0; i < gate_id; ++i)
        {
            if (isGateExists(i))
                candidates.push_back(i);
        }

        if (candidates.empty())
            return 0;

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
            return false;
        return circuit.getGateType(gate) == GateType::INPUT;
    }
};

}