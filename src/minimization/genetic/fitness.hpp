#pragma once

#include "core/structures/icircuit.hpp"

namespace cirbo::minimization::genetic
{
template<class CircuitT>
class FitnessFunction
{
    public:
    const CircuitT& initial_circuit;
    size_t inputs_amount;
    size_t outputs_amount;
    GateIdContainer inputs;
    GateIdContainer outputs;
    std::vector<std::vector<GateState>> initial_result_states;
    
    static constexpr double CORRECTNESS_WEIGHT = 1000.0;
    static constexpr double SIZE_WEIGHT = 1.0;


    FitnessFunction(const CircuitT& circuit)
    : initial_circuit(circuit)
    , inputs_amount(circuit.getInputGates().size())
    , outputs_amount(circuit.getOutputGates().size())
    , inputs(circuit.getInputGates())
    , outputs(circuit.getOutputGates())
    {
        initialEvaluateAllInputs();
    }

    double evaluateFitness(const CircuitT& test_circuit) const
    {
        size_t correct_vectors = 0;

        auto test_outputs = test_circuit.getOutputGates();
        auto test_result_states = evaluateCircuitOutputs(test_circuit, test_outputs);

        for (size_t i = 0; i < initial_result_states.size(); ++i)
        {
            if (initial_result_states[i] == test_result_states[i])
            {
                ++correct_vectors;
            }
        }

        double correctness = static_cast<double>(correct_vectors) / initial_result_states.size();
        
        if (correctness < 1.0)
        {
            return correctness * CORRECTNESS_WEIGHT;
        }
        
        
        size_t initial_size = initial_circuit.getActualNumberOfGates();
        size_t current_size = test_circuit.getActualNumberOfGates();
        
        double fitness = correctness * CORRECTNESS_WEIGHT;
        
        if (current_size < initial_size)
        {
            double size_bonus = (initial_size - current_size) * SIZE_WEIGHT;
            fitness += size_bonus;
        }
        else if (current_size > initial_size)
        {
            double size_penalty = (current_size - initial_size) * SIZE_WEIGHT;
            fitness -= size_penalty;
        }
        
        return fitness;
    }

    private:
    std::vector<std::vector<GateState>> evaluateCircuitOutputs(const CircuitT& test_circuit, const GateIdContainer& test_outputs) const
    {
        size_t all_combinations = 1ULL << inputs_amount;
        std::vector<std::vector<GateState>> result_states;
        result_states.reserve(all_combinations);

        for (size_t input_mask = 0; input_mask < all_combinations; ++input_mask)
        {
            VectorAssignment<> input_assigment;
            input_assigment.ensureCapacity(test_circuit.getNumberOfGates());
            for (size_t i = 0; i < inputs_amount; ++i)
            {
                bool value = (input_mask >> i) & 1;
                input_assigment.assign(inputs[i], value ? GateState::TRUE : GateState::FALSE);
            }

            result_states.push_back(getOutputValues(test_circuit, input_assigment, test_outputs));
        }

        return result_states;
    }

    void initialEvaluateAllInputs()
    {
        initial_result_states = evaluateCircuitOutputs(initial_circuit, outputs);
    }

    std::vector<GateState> getOutputValues(const CircuitT& circuit, const VectorAssignment<>& input_assigment, const GateIdContainer& outputs) const
    {
        const ICircuit& base_circuit = static_cast<const ICircuit&>(circuit);
        auto result_assigment = base_circuit.evaluateCircuit<VectorAssignment<>>(input_assigment);
        std::vector<GateState> output_states;
        output_states.reserve(outputs.size());
        for (GateId output_id : outputs)
        {
            output_states.push_back(result_assigment->getGateState(output_id));
        }
        return output_states;
    }
};
}