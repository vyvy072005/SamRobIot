import numpy as np

np.random.seed(42)

n_points = 10
k_cities = 8

production_capacities = np.random.randint(50, 150, size=n_points)
city_demands = np.random.randint(30, 100, size=k_cities)
distances = np.random.randint(10, 100, size=(n_points, k_cities))
costs = distances * 0.5

# Проверка, что есть достаточно производства для всех требований
if sum(production_capacities) < sum(city_demands):
    print("Недостаточно общего производства для выполнения требований.")
else:
    # Инициализация распределения
    distribution = np.zeros((n_points, k_cities))
    remaining_demands = city_demands.copy()

    # Распределяем по пунктам
    for j in range(n_points):
        # Максимум, что можем поставить с этого пункта
        max_possible = production_capacities[j]
        # Распределяем по городам
        for i in range(k_cities):
            # Вычислим, сколько можно поставить в этот город
            can_give = min(remaining_demands[i], max_possible)
            distribution[j, i] = can_give
            remaining_demands[i] -= can_give
            max_possible -= can_give

    # Проверка, выполнены ли требования
    if np.all(remaining_demands == 0):
        total_cost = np.sum(distribution * costs)
        print("Распределение груза по пунктам и городам:")
        print(distribution)
        print("Общая стоимость:", total_cost)
    else:
        print("Не удалось выполнить все требования при жадном распределении.")