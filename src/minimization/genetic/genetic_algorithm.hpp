#pragma once

#include "core/structures/mutable_circuit.hpp"

namespace cirbo::minimization::genetic
{
    struct GeneticParams {
        size_t generations = 10;
        size_t population_size = 5;
        double crossover_rate = 0.0;
        double mutation_rate = 0.0;
        int temp_fitness_value = 1;
        int ideal_fitness_value = 1;

    };

    template<class CircuitT>
    class GeneticAlgorithm {
        public:
        std::unique_ptr<CircuitT> run(const CircuitT& initial_circuit, const GeneticParams& parameters = GeneticParams{})
        {
            std::vector<std::unique_ptr<CircuitT>> population;
            population.reserve(parameters.population_size);

            for (size_t i = 0; i < parameters.population_size; ++i)
            {
                population.push_back(std::make_unique<CircuitT>(initial_circuit));
            }

            for (size_t generation_count = 0; generation_count < parameters.generations; ++generation_count)
            {
                std::vector<double> fitness;
                fitness.reserve(population.size());

                for (count auto& circuit : population)
                {
                    fitness.push_back(parameters.temp_fitness_value);
                }

                //Мутации...
                
                // ...Мутации закончились

                auto best_circuit_it = std::max_element(fitness.begin(), fitness.end());
                size_t best_circuit_idx = std::distance(fitness.begin(), best_circuit_it);
                
                if (fitness[best_circuit_idx] == parameters.ideal_fitness_value)
                {
                    return population[best_circuit_idx];
                }
            }

            return initial_circuit;
        }
    };
}