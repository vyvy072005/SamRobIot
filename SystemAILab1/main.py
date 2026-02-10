import numpy as np
import matplotlib.pyplot as plt
import random

# --- Генерация данных ---
np.random.seed(42)

n_points = 10  # число пунктов производства
k_cities = 8   # число городов

production_capacities = np.random.randint(50, 150, size=n_points)
city_demands = np.random.randint(30, 100, size=k_cities)
distances = np.random.randint(10, 100, size=(n_points, k_cities))
costs = distances * 0.5

assert sum(production_capacities) >= sum(city_demands), "Общего производства недостаточно!"

# --- Параметры ---
POP_SIZE = 50
GENERATIONS = 100
MUTATION_RATE = 0.1


def create_individual():
    individual = np.zeros((n_points, k_cities))
    for j in range(n_points):
        remaining = production_capacities[j]
        for i in range(k_cities):
            if remaining <= 0:
                break
            allocate = np.random.randint(0, remaining+1)
            individual[j, i] = allocate
            remaining -= allocate
        # Распределяем оставшиеся
        for i in range(k_cities):
            if remaining <= 0:
                break
            add = np.random.randint(0, remaining+1)
            individual[j, i] += add
            remaining -= add
        # Ограничение по производству
        total = sum(individual[j])
        if total > production_capacities[j]:
            individual[j] = individual[j] * (production_capacities[j] / total)
            individual[j] = np.round(individual[j]).astype(int)
    return individual

# --- Функция оценки ---
def fitness(individual):
    total_delivered = individual.sum(axis=0)
    penalty = 0
    for i in range(k_cities):
        diff = total_delivered[i] - city_demands[i]
        if diff < 0:
            penalty += abs(diff)*100
        elif diff > 0:
            penalty += diff*50
    total_cost = np.sum(individual * costs)
    return total_cost + penalty

# --- Разные стратегии скрещивания ---
def crossover_uniform(parent1, parent2):
    child = np.zeros_like(parent1)
    for j in range(n_points):
        for i in range(k_cities):
            child[j, i] = parent1[j, i] if np.random.rand() < 0.5 else parent2[j, i]
    return child

def crossover_one_point(parent1, parent2):
    point = np.random.randint(1, n_points)
    child = np.vstack((parent1[:point], parent2[point:]))
    return child

def crossover_two_point(parent1, parent2):
    point1 = np.random.randint(1, n_points-1)
    point2 = np.random.randint(point1+1, n_points)
    child = np.vstack((parent1[:point1], parent2[point1:point2], parent1[point2:]))
    return child

# --- Разные стратегии мутаций ---
def mutate_swap(individual):
    j1, j2 = np.random.choice(n_points, 2, replace=False)
    i = np.random.randint(k_cities)
    individual[j1, i], individual[j2, i] = individual[j2, i], individual[j1, i]
    return individual

def mutate_increase_decrease(individual):
    j = np.random.randint(n_points)
    i = np.random.randint(k_cities)
    delta = np.random.randint(-10, 11)
    new_value = individual[j, i] + delta
    new_value = max(0, min(new_value, production_capacities[j]))
    individual[j, i] = new_value
    return individual

def mutate_random_reset(individual):
    j = np.random.randint(n_points)
    i = np.random.randint(k_cities)
    individual[j, i] = np.random.randint(0, production_capacities[j]+1)
    return individual

# --- Основной алгоритм ---
def genetic_algorithm(crossover_func, mutation_func):
    population = [create_individual() for _ in range(POP_SIZE)]
    best_fitness = float('inf')
    best_individual = None
    fitness_history = []

    for gen in range(GENERATIONS):
        fitness_values = [fitness(ind) for ind in population]
        min_idx = np.argmin(fitness_values)
        if fitness_values[min_idx] < best_fitness:
            best_fitness = fitness_values[min_idx]
            best_individual = population[min_idx]
        fitness_history.append(best_fitness)

        # Турнирная селекция
        new_population = []
        for _ in range(POP_SIZE):
            i1, i2 = np.random.choice(POP_SIZE, 2, replace=False)
            if fitness_values[i1] < fitness_values[i2]:
                new_population.append(population[i1])
            else:
                new_population.append(population[i2])

        # Создание потомков
        children = []
        for _ in range(0, POP_SIZE, 2):
            parent1 = random.choice(new_population)
            parent2 = random.choice(new_population)
            child1 = crossover_func(parent1, parent2)
            child2 = crossover_func(parent2, parent1)

            # Мутации
            if np.random.rand() < MUTATION_RATE:
                child1 = mutation_func(child1)
            if np.random.rand() < MUTATION_RATE:
                child2 = mutation_func(child2)

            children.extend([child1, child2])

        population = children[:POP_SIZE]
    return best_individual, best_fitness, fitness_history

# --- Запуск экспериментов ---
crossover_methods = [
    (crossover_uniform, "Uniform"),
    (crossover_one_point, "One-Point"),
    (crossover_two_point, "Two-Point")
]

mutation_methods = [
    (mutate_swap, "Swap"),
    (mutate_increase_decrease, "Inc-Dec"),
    (mutate_random_reset, "Random Reset")
]

results = []

for c_func, c_name in crossover_methods:
    for m_func, m_name in mutation_methods:
        print(f"Запуск с {c_name} скрещиванием и {m_name} мутацией")
        best_ind, best_cost, history = genetic_algorithm(c_func, m_func)
        results.append({
            'label': f"{c_name}-{m_name}",
            'cost': best_cost,
            'history': history
        })

# --- Визуализация ---
plt.figure(figsize=(12, 8))
for res in results:
    plt.plot(res['history'], label=res['label'])
plt.xlabel('Поколения')
plt.ylabel('Лучший фитнес')
plt.title('Сравнение стратегий скрещивания и мутации')
plt.legend()
plt.grid()
plt.show()


# --- Дополнительный график: финальные стоимости по стратегиям ---
labels = [res['label'] for res in results]
costs = [res['cost'] for res in results]

plt.figure(figsize=(10,6))
plt.barh(labels, costs, color='skyblue')
plt.xlabel('Финальная стоимость')
plt.title('Финальные стоимости по стратегиям')
plt.gca().invert_yaxis()
plt.show()
# --- Итоговые результаты ---
for res in results:
    print(f"{res['label']}: Финальная стоимость = {res['cost']}")