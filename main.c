#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#define let const auto
#define MAX_DAYS 101
#define CLEAR_STDIN                                                            \
    while (getc(stdin) != '\n')                                                \
        ;

typedef struct {
    float susceptible, infected, recovered;
} population_t;

typedef struct {
    uint32_t day;
    float populationHistory[MAX_DAYS][3]; // AoS
    float dailyInfections[MAX_DAYS], dailyRecoveries[MAX_DAYS];
    float infectionRate, recoveryRate;
} state_t;

typedef enum {
    MENU_SIMULATE_NEXT_DAY = 1,
    MENU_CHANGE_MODEL_PARAMETERS = 2,
    MENU_SHOW_SIMULATION_HISTORY = 3,
    MENU_EXIT = 4,
} menu_option_t;

population_t normalizePopulation(population_t p) {
    let sum = p.susceptible + p.infected + p.recovered;

    if (sum == 0.0f)
        return (population_t){0};

    let susceptible = roundf(p.susceptible * 100.0f / sum);

    let remainingSum = sum - p.susceptible;
    let remainingPopulation = 100.0f - susceptible;

    if (remainingSum == 0.0f)
        return (population_t){susceptible, 0.0f, 0.0f};

    let infected = roundf(p.infected * remainingPopulation / remainingSum);

    let recovered = remainingPopulation - infected;

    return (population_t){
        .susceptible = susceptible, .infected = infected, .recovered = recovered
    };
}

void showPopulation(uint32_t day, state_t state) {
    let population = *(population_t *)state.populationHistory[day];

    let normalized = normalizePopulation(population);

    printf(
        "Day: %u\n"
        "Susceptible: %g\n"
        "Infected: %g\n"
        "Recovered: %g\n",
        day,
        normalized.susceptible,
        normalized.infected,
        normalized.recovered
    );
}

uint32_t showHistoryLoop(state_t state, uint32_t fromDay) {
    if (fromDay > state.day)
        return 0;

    let population =
        normalizePopulation(*(population_t *)state.populationHistory[fromDay]);
    printf(
        "%3u   %11g   %8g   %9g   %14g   %14g\n",
        fromDay,
        population.susceptible,
        population.infected,
        population.recovered,
        state.dailyInfections[fromDay],
        state.dailyRecoveries[fromDay]
    );

    return showHistoryLoop(state, fromDay + 1);
}

void showHistory(state_t state) {
    printf(
        "Day   Susceptible   Infected   Recovered   New infections   New "
        "recoveries\n"
    );
    (void)showHistoryLoop(state, 0);
}

menu_option_t menu() {
    uint32_t choice;
    printf(
        "\n"
        "=== Epidemic Simulator ===\n"
        "1. Simulate next day\n"
        "2. Change model parameters\n"
        "3. Show simulation history\n"
        "4. Exit\n"
        "Your choice: "
    );

    scanf("%u", &choice);
    CLEAR_STDIN

    switch (choice) {
    case 1:
        return MENU_SIMULATE_NEXT_DAY;
    case 2:
        return MENU_CHANGE_MODEL_PARAMETERS;
    case 3:
        return MENU_SHOW_SIMULATION_HISTORY;
    case 4:
        return MENU_EXIT;
    default:
        printf("\nError: Incorrect option\n\n");
        [[clang::musttail]] return menu();
    }
}

bool checkPopulation(population_t population) {
    return population.susceptible >= 0 && population.infected >= 0 &&
           population.recovered >= 0 &&
           population.susceptible + population.infected +
                   population.recovered ==
               100;
}

state_t nextDay(state_t state) {
    state_t s = state;
    s.day++;

    let population = *(population_t *)state.populationHistory[state.day];

    let newInfections =
        fmin(population.susceptible, population.infected * state.infectionRate);
    let newRecoveries =
        fmin(population.infected, population.recovered * state.recoveryRate);

    let newDay = (population_t *)s.populationHistory[s.day];

    *newDay = (population_t){
        .susceptible = population.susceptible - newInfections,
        .infected = population.infected + (newInfections - newRecoveries),
        .recovered = population.recovered + newRecoveries,
    };

    let normalizedNewPopulation = normalizePopulation(*newDay);
    let normalizedPopulation = normalizePopulation(population);

    s.dailyInfections[s.day] = fmaxf(
        normalizedNewPopulation.infected - normalizedPopulation.infected, 0.0f
    );
    s.dailyRecoveries[s.day] = fmaxf(
        normalizedNewPopulation.recovered - normalizedPopulation.recovered, 0.0f
    );

    return s;
}

state_t alterState(state_t state, float infectionRate, float recoveryRate) {
    state_t s = state;
    s.infectionRate = infectionRate;
    s.recoveryRate = recoveryRate;

    return s;
}

uint32_t nextStep(state_t state) {
    switch (menu()) {
    case MENU_SIMULATE_NEXT_DAY: {
        let newState = nextDay(state);
        let newPopulation =
            *(population_t *)newState.populationHistory[newState.day];
        if (!checkPopulation(normalizePopulation(newPopulation))) {
            printf("Error: population corrupted on day %u\n", newState.day);
            showPopulation(newState.day, newState);
            return EXIT_FAILURE;
        }
        showPopulation(newState.day, newState);
        if (!(newState.day < MAX_DAYS)) {
            printf(
                "Reached maximal number of days that program can simulate. "
                "Exiting...\n"
            );
            return EXIT_SUCCESS;
        }
        [[clang::musttail]] return nextStep(newState);
    }
    case MENU_CHANGE_MODEL_PARAMETERS: {
        float newInfectionRate = -1.0f, newRecoveryRate = -1.0f;
        do {
            printf("Enter infection rate (0-1): ");
            if (scanf("%g", &newInfectionRate) == 0) {
                CLEAR_STDIN
            }
        } while (newInfectionRate < 0 || newInfectionRate > 1);
        do {
            printf("Enter recovery rate (0-1): ");
            if (scanf("%g", &newRecoveryRate) == 0) {
                CLEAR_STDIN
            }
        } while (newRecoveryRate < 0 || newRecoveryRate > 1);
        printf("Parameters updated.\n");
        [[clang::musttail]] return nextStep(
            alterState(state, newInfectionRate, newRecoveryRate)
        );
    }
    case MENU_SHOW_SIMULATION_HISTORY: {
        showHistory(state);
        [[clang::musttail]] return nextStep(state);
    }
    case MENU_EXIT:
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

int main() {
    population_t population = {80, 10, 10};

    state_t state = {
        .dailyInfections = {0},
        .dailyRecoveries = {0},
        .day = 0,
        .populationHistory = {0},
        .infectionRate = 0.20f,
        .recoveryRate = 0.10f,
    };

    *(population_t *)state.populationHistory[state.day] = population;

    showPopulation(state.day, state);
    return nextStep(state);
}
