#pragma once

#include "core/structures/mutable_circuit.hpp"
#include "fitness.hpp"
#include "mutation.hpp"
#include <iostream>
#include <algorithm>
#include <random>
#include <memory>
#include <vector>
#include <limits>
#include <cmath>
#include <map>

namespace cirbo::minimization::genetic
{
    struct GeneticParams {
        size_t generations = 100;
        size_t population_size = 10;

        double mutation_rate = 0.8;
        double elite_rate = 0.1;
        double crossover_rate = 0.0;
    };

    template<class CircuitT>
    class GeneticAlgorithm {
    private:
        std::mt19937 rng{std::random_device{}()};
        
        void applyRandomMutation(CircuitT& circuit)
        {
            Mutation<CircuitT> mutator(circuit);
            mutator.applyRandomMutation();
        }
        
        size_t tournamentSelection(const std::vector<double>& fitness_results, const std::vector<std::unique_ptr<CircuitT>>& population, size_t tournament_size = 5)
        {
            size_t best_idx = randomIndex(fitness_results.size());
            double best_fitness = fitness_results[best_idx];
            size_t best_size = population[best_idx]->getActualNumberOfGates();
            
            for (size_t i = 1; i < tournament_size; ++i)
            {
                size_t idx = randomIndex(fitness_results.size());
                double fitness = fitness_results[idx];
                size_t size = population[idx]->getActualNumberOfGates();
                
                bool a_correct = isCircuitCorrect(best_fitness);
                bool b_correct = isCircuitCorrect(fitness);
                
                if (a_correct && b_correct)
                {
                    if (size < best_size)
                    {
                        best_fitness = fitness;
                        best_idx = idx;
                        best_size = size;
                    }
                }
                else if (b_correct && !a_correct)
                {
                    best_fitness = fitness;
                    best_idx = idx;
                    best_size = size;
                }
                else if (!b_correct && !a_correct)
                {
                    if (fitness > best_fitness)
                    {
                        best_fitness = fitness;
                        best_idx = idx;
                        best_size = size;
                    }
                }
            }
            
            return best_idx;
        }
        
        size_t randomIndex(size_t size)
        {
            std::uniform_int_distribution<size_t> dist(0, size - 1);
            return dist(rng);
        }
        
        double randomDouble()
        {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            return dist(rng);
        }

        bool isCircuitCorrect(double fitness)
        {
            return fitness >= FitnessFunction<CircuitT>::CORRECTNESS_WEIGHT;
        }

        void printGateStats(const std::vector<std::unique_ptr<CircuitT>>& population, 
                           const std::vector<double>& fitness_results,
                           size_t generation)
        {
            std::map<size_t, size_t> size_distribution;
            std::map<size_t, size_t> correct_size_distribution;
            
            size_t total_gates = 0;
            size_t min_gates = std::numeric_limits<size_t>::max();
            size_t max_gates = 0;
            size_t correct_count = 0;
            
            for (size_t i = 0; i < population.size(); ++i)
            {
                size_t gates = population[i]->getActualNumberOfGates();
                total_gates += gates;
                min_gates = std::min(min_gates, gates);
                max_gates = std::max(max_gates, gates);
                
                size_distribution[gates]++;
                
                if (isCircuitCorrect(fitness_results[i]))
                {
                    correct_count++;
                    correct_size_distribution[gates]++;
                }
            }
                        
            std::cout << "\n--- Generation " << generation << " ---\n";
            std::cout << "Population size: " << population.size() << "\n";
            std::cout << "Gate count: min=" << min_gates << ", max=" << max_gates << "\n";
            std::cout << "Correct circuits: " << correct_count << "/" << population.size() << "\n";
            
            std::cout << "\nSize distribution (all circuits):\n";
            for (const auto& [size, count] : size_distribution)
            {
                double percentage = 100.0 * count / population.size();
                std::cout << "  " << size << " gates: " << count << " (" << percentage << "%)";
                
                if (correct_size_distribution.count(size))
                {
                    double correct_percentage = 100.0 * correct_size_distribution[size] / count;
                    std::cout << " - correct: " << correct_size_distribution[size] 
                              << " (" << correct_percentage << "%)";
                }
                std::cout << "\n";
            }
            
            size_t best_correct_size = std::numeric_limits<size_t>::max();
            for (const auto& [size, count] : correct_size_distribution)
            {
                if (size < best_correct_size && count > 0)
                {
                    best_correct_size = size;
                }
            }
            
            if (best_correct_size < std::numeric_limits<size_t>::max())
            {
                std::cout << "\nBest correct circuit size: " << best_correct_size << " gates\n";
            }
            
            std::cout << "================================\n";
        }

    public:
        std::unique_ptr<CircuitT> run(const CircuitT& initial_circuit, const GeneticParams& parameters = GeneticParams{})
        {
            std::cout << "Initial circuit size: " << initial_circuit.getActualNumberOfGates() << " gates\n";
            
            std::vector<std::unique_ptr<CircuitT>> population;
            population.reserve(parameters.population_size);

            for (size_t i = 0; i < parameters.population_size; ++i)
            {
                population.push_back(std::make_unique<CircuitT>(initial_circuit));
            }

            FitnessFunction<CircuitT> fitness(initial_circuit);
            
            std::unique_ptr<CircuitT> best_circuit_ever = std::make_unique<CircuitT>(initial_circuit);
            size_t best_size_ever = initial_circuit.getActualNumberOfGates();
            
            size_t correct_count = 0;
            
            size_t last_best_size = initial_circuit.getActualNumberOfGates();

            for (size_t generation = 0; generation < parameters.generations; ++generation)
            {
                std::vector<double> fitness_results;
                fitness_results.reserve(population.size());
                
                correct_count = 0;
                size_t best_this_gen_size = std::numeric_limits<size_t>::max();
                size_t best_this_gen_idx = 0;

                for (size_t i = 0; i < population.size(); ++i)
                {
                    double fitness_result = fitness.evaluateFitness(*population[i]);
                    fitness_results.push_back(fitness_result);
                    
                    if (isCircuitCorrect(fitness_result))
                    {
                        correct_count++;
                        size_t circuit_size = population[i]->getActualNumberOfGates();
                        
                        if (circuit_size < best_this_gen_size)
                        {
                            best_this_gen_size = circuit_size;
                            best_this_gen_idx = i;
                        }
                    }
                }

                if (best_this_gen_size < best_size_ever)
                {
                    best_size_ever = best_this_gen_size;
                    best_circuit_ever = std::make_unique<CircuitT>(*population[best_this_gen_idx]);
                    
                    std::cout << "\n*** NEW BEST CORRECT CIRCUIT at gen " << generation 
                              << ": " << best_size_ever << " gates ***\n";
                    std::cout << "Improved from " << last_best_size << " gates\n";
                    last_best_size = best_size_ever;
                    
                }

                auto best_it = std::max_element(fitness_results.begin(), fitness_results.end());
                size_t best_idx = std::distance(fitness_results.begin(), best_it);
                double current_best_fitness = fitness_results[best_idx];
                size_t current_best_size = population[best_idx]->getActualNumberOfGates();
                
                if (generation % 50 == 0 || generation == 0)
                {
                    printGateStats(population, fitness_results, generation);
                }
                else if (generation % 10 == 0)
                {
                    size_t total_gates = 0;
                    size_t min_gates = std::numeric_limits<size_t>::max();
                    size_t max_gates = 0;
                    
                    for (size_t i = 0; i < population.size(); ++i)
                    {
                        size_t gates = population[i]->getActualNumberOfGates();
                        total_gates += gates;
                        min_gates = std::min(min_gates, gates);
                        max_gates = std::max(max_gates, gates);
                    }
                    
                    double avg_gates = static_cast<double>(total_gates) / population.size();
                    
                    std::cout << "Gen " << generation 
                              << " | sizes: min=" << min_gates 
                              << " avg=" << avg_gates 
                              << " max=" << max_gates
                              << " | correct=" << correct_count << "/" << population.size()
                              << " | best_ever=" << best_size_ever
                              << " | best_fit=" << current_best_fitness << "\n";
                }

                std::vector<std::unique_ptr<CircuitT>> new_population;
                new_population.reserve(parameters.population_size);

                size_t elite_count = std::max(static_cast<size_t>(parameters.population_size * parameters.elite_rate), size_t(1));
                
                std::vector<size_t> indices(population.size());
                for (size_t i = 0; i < population.size(); ++i) indices[i] = i;
                
                std::sort(indices.begin(), indices.end(),
                    [&](size_t a, size_t b) {
                        bool a_correct = isCircuitCorrect(fitness_results[a]);
                        bool b_correct = isCircuitCorrect(fitness_results[b]);
                        size_t a_size = population[a]->getActualNumberOfGates();
                        size_t b_size = population[b]->getActualNumberOfGates();
                        
                        if (a_correct && b_correct)
                        {
                            if (a_size != b_size)
                                return a_size < b_size;
                            return fitness_results[a] > fitness_results[b];
                        }
                        else if (a_correct && !b_correct)
                        {
                            return true;
                        }
                        else if (!a_correct && b_correct)
                        {
                            return false;
                        }
                        else
                        {
                            return fitness_results[a] > fitness_results[b];
                        }
                    });
                
                for (size_t i = 0; i < elite_count && i < indices.size(); ++i)
                {
                    new_population.push_back(std::make_unique<CircuitT>(*population[indices[i]]));
                }

                while (new_population.size() < parameters.population_size)
                {
                    size_t parent_idx;
                    parent_idx = tournamentSelection(fitness_results, population);
                    
                    auto offspring = std::make_unique<CircuitT>(*population[parent_idx]);
                    
                    if (randomDouble() < parameters.mutation_rate)
                    {
                        applyRandomMutation(*offspring);
                    }
                    
                    new_population.push_back(std::move(offspring));
                }

                population = std::move(new_population);
            }

            std::cout << "\n=== FINAL RESULT ===\n";
            std::cout << "Initial gates: " << initial_circuit.getActualNumberOfGates() << "\n";
            std::cout << "Final gates: " << best_size_ever << "\n";
            if (best_size_ever < initial_circuit.getActualNumberOfGates())
            {
                std::cout << "IMPROVEMENT: saved " << initial_circuit.getActualNumberOfGates() - best_size_ever << " gates!\n";
            }
            else
            {
                std::cout << "No improvement found.\n";
            }
            
            double final_fitness = fitness.evaluateFitness(*best_circuit_ever);
            std::cout << "Final circuit fitness: " << final_fitness << "\n";
            if (isCircuitCorrect(final_fitness))
            {
                std::cout << "Final circuit is CORRECT\n";
            }
            else
            {
                std::cout << "Final circuit is INCORRECT\n";
            }
            
            return std::move(best_circuit_ever);
        }
    };
}