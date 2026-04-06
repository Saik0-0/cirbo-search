#pragma once

#include "core/structures/icircuit.hpp"

namespace cirbo::minimization::genetic
{

/**
 * @brief Fitness function used to evaluate candidate circuits.
 *
 * Compares the behavior of a test circuit with the initial circuit
 * for all input combinations and rewards smaller correct circuits.
 */
template<class CircuitT>
class FitnessFunction
{
public:
    CircuitT const& initial_circuit;
    size_t inputs_amount;
    size_t outputs_amount;
    GateIdContainer inputs;
    GateIdContainer outputs;
    std::vector<std::vector<GateState>> initial_result_states;

    static constexpr double CORRECTNESS_WEIGHT = 1000.0;
    static constexpr double SIZE_WEIGHT        = 1.0;

    /**
     * @brief Constructs the fitness function for a given circuit.
     * @param circuit Reference circuit used as the baseline for comparison.
     *
     * Initializes circuit metadata and precomputes output values
     * for all possible input combinations.
     */
    FitnessFunction(CircuitT const& circuit)
        : initial_circuit(circuit)
        , inputs_amount(circuit.getInputGates().size())
        , outputs_amount(circuit.getOutputGates().size())
        , inputs(circuit.getInputGates())
        , outputs(circuit.getOutputGates())
    {
        initialEvaluateAllInputs();
    }

    /**
     * @brief Evaluates the fitness of a candidate circuit.
     * @param test_circuit Circuit whose fitness will be evaluated.
     * @return Fitness score representing correctness and circuit size.
     *
     * The score is based on how many input vectors produce identical
     * outputs compared to the initial circuit. If fully correct,
     * additional reward or penalty is applied based on circuit size.
     */
    double evaluateFitness(CircuitT const& test_circuit) const
    {
        size_t correct_vectors = 0;

        auto test_outputs       = test_circuit.getOutputGates();
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
    /**
     * @brief Evaluates circuit outputs for all input combinations.
     * @param test_circuit Circuit to evaluate.
     * @param test_outputs Output gates of the circuit.
     * @return Matrix of output states for every input vector.
     *
     * Each row corresponds to one input combination and contains
     * the resulting states of all output gates.
     */
    std::vector<std::vector<GateState>> evaluateCircuitOutputs(
        CircuitT const& test_circuit,
        GateIdContainer const& test_outputs) const
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

    /**
     * @brief Precomputes outputs of the initial circuit.
     */
    void initialEvaluateAllInputs() { initial_result_states = evaluateCircuitOutputs(initial_circuit, outputs); }

    /**
     * @brief Computes output values for a given input assignment.
     * @param circuit Circuit to evaluate.
     * @param input_assigment Assignment of input gate values.
     * @param outputs Output gate identifiers.
     * @return Vector containing the states of all output gates.
     */
    std::vector<GateState> getOutputValues(
        CircuitT const& circuit,
        VectorAssignment<> const& input_assigment,
        GateIdContainer const& outputs) const
    {
        ICircuit const& base_circuit = static_cast<ICircuit const&>(circuit);
        auto result_assigment        = base_circuit.evaluateCircuit<VectorAssignment<>>(input_assigment);
        std::vector<GateState> output_states;
        output_states.reserve(outputs.size());
        for (GateId output_id : outputs) { output_states.push_back(result_assigment->getGateState(output_id)); }
        return output_states;
    }
};
}  // namespace cirbo::minimization::genetic