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


    FitnessFunction(const CircuitT& circuit)
    : initial_circuit(circuit)
    , inputs_amount(circuit.getInputGates().size())
    , outputs_amount(circuit.getOutputGates().size())
    , inputs(circuit.getInputGates())
    , outputs(circuit.getOutputGates())
    {
        initialEvaluateAllInputs();
    }

    size_t evaluateFitness(const CircuitT& test_circuit) const
    {
        size_t correct_outputs = 0;
        auto test_result_states = evaluateCircuitOutputs(test_circuit, test_circuit.getOutputGates());
        for (size_t i = 0; i < initial_result_states.size(); ++i)
        {
            if (initial_result_states[i] == test_result_states[i])
            {
                ++correct_outputs;
            }
        }

        return correct_outputs;
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
        auto result_assigment = std::make_unique<VectorAssignment<>>(circuit.getNumberOfGates());
        const ICircuit& base_circuit = static_cast<const ICircuit&>(circuit);
        const_cast<ICircuit&>(base_circuit).evaluateCircuit_(outputs, input_assigment, result_assigment.get());
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