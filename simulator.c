#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#define let const auto
#define MAX_DAYS 100
#define CLEAR_STDIN                                                            \
    while (getc(stdin) != '\n')                                                \
        ;
#if defined(__clang__)
#define musttail [[clang::musttail]]
#elif defined(_MSC_VER) && _MSC_VER >= 1950
#define musttail [[msvc::musttail]]
#elif defined(__GNUC__) && __GNUC__ >= 15
#define musttail [[gnu::musttail]]
#else
#warning "Guaranteed tail calls are not supported by this compiler"
#define musttail
#endif

#define Nothing {0}
#define Just(value) {1, value}
typedef struct {
    bool haveValue;
    uint32_t just;
} maybe_uint32_t;

typedef struct {
    bool haveValue;
    float just;
} maybe_float32_t;

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

typedef enum {
    OPTION_CODE_CONTINUE,
    OPTION_CODE_SUCCESS,
    OPTION_CODE_FAILURE,
} option_code_t;

typedef struct {
    option_code_t code;
    state_t state;
} option_t;

static population_t normalizePopulation(population_t p) {
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

    return (population_t){.susceptible = susceptible,
                          .infected = infected,
                          .recovered = recovered};
}

static inline population_t f32x3ToPopulation(const float loosePopulation[3]) {
    return (population_t){
        loosePopulation[0], loosePopulation[1], loosePopulation[2]};
}

static void showPopulation(uint32_t day, state_t state) {
    let population = f32x3ToPopulation(state.populationHistory[day]);

    let normalized = normalizePopulation(population);

    printf("Day: %u\n"
           "Susceptible: %g\n"
           "Infected: %g\n"
           "Recovered: %g\n",
           day,
           normalized.susceptible,
           normalized.infected,
           normalized.recovered);
}

static uint32_t showHistoryLoop(state_t state, uint32_t fromDay) {
    if (fromDay > state.day)
        return 0;

    let population = normalizePopulation(
        f32x3ToPopulation(state.populationHistory[fromDay]));
    printf("%3u   %11g   %8g   %9g   %14g   %14g\n",
           fromDay,
           population.susceptible,
           population.infected,
           population.recovered,
           state.dailyInfections[fromDay],
           state.dailyRecoveries[fromDay]);

    musttail return showHistoryLoop(state, fromDay + 1);
}

static void showHistory(state_t state) {
    printf("Day   Susceptible   Infected   Recovered   New infections   New "
           "recoveries\n");
    (void)showHistoryLoop(state, 0);
}

static inline bool isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v';
}

static inline bool isUntil(bool(*callback)(char), char guardian, char* ptr) {
    if(callback(*ptr)) {
        musttail return isUntil(callback, guardian, ptr + 1);
    }

    return *ptr == guardian;
}

static maybe_uint32_t uint32Input(FILE *file) {
    char buffer[32], *end;
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        return (maybe_uint32_t)Nothing;
    }
    errno = 0;

    let value = strtoul(buffer, &end, 10);

    if (end == buffer || errno == ERANGE || value > UINT32_MAX)
        return (maybe_uint32_t)Nothing;

    if(!isUntil(isWhitespace, '\0', end)) {
        return (maybe_uint32_t)Nothing;
    }

    return (maybe_uint32_t)Just((uint32_t)value);
}

static maybe_float32_t float32Input(FILE *file) {
    char buffer[64], *end;
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        return (maybe_float32_t)Nothing;
    }
    errno = 0;

    let value = strtof(buffer, &end);

    if (end == buffer || errno == ERANGE || !isfinite(value))
        return (maybe_float32_t)Nothing;

    if(!isUntil(isWhitespace, '\0', end)) {
        return (maybe_float32_t)Nothing;
    }

    return (maybe_float32_t)Just(value);
}

static menu_option_t menu() {
    printf("\n"
           "=== Epidemic Simulator ===\n"
           "1. Simulate next day\n"
           "2. Change model parameters\n"
           "3. Show simulation history\n"
           "4. Exit\n"
           "Your choice: ");

    let result = uint32Input(stdin);

    if (result.haveValue) {
        switch (result.just) {
        case 1:
            return MENU_SIMULATE_NEXT_DAY;
        case 2:
            return MENU_CHANGE_MODEL_PARAMETERS;
        case 3:
            return MENU_SHOW_SIMULATION_HISTORY;
        case 4:
            return MENU_EXIT;
        default:
            // Failed to match, pass-trough switch will run code after if statement
        }
    }
    printf("\nError: Incorrect option\n\n");
    musttail return menu();
}

static inline bool checkPopulation(population_t population) {
    return population.susceptible >= 0 && population.infected >= 0 &&
           population.recovered >= 0 &&
           population.susceptible + population.infected +
                   population.recovered ==
               100;
}


static inline state_t updateDay(state_t state,
                                uint32_t day,
                                population_t population,
                                float newInfections,
                                float newRecoveries) {
    state_t s = state;
    s.day = day;
    s.populationHistory[day][0] = population.susceptible;
    s.populationHistory[day][1] = population.infected;
    s.populationHistory[day][2] = population.recovered;
    s.dailyInfections[day] = newInfections;
    s.dailyRecoveries[day] = newRecoveries;
    return s;
}

static state_t nextDay(state_t state) {
    let population = f32x3ToPopulation(state.populationHistory[state.day]);

    let newInfections = fminf(population.susceptible,
                              population.infected * state.infectionRate);
    let newRecoveries =
        fminf(population.infected, population.recovered * state.recoveryRate);

    let newPopulation = (population_t){
        .susceptible = population.susceptible - newInfections,
        .infected = population.infected + (newInfections - newRecoveries),
        .recovered = population.recovered + newRecoveries,
    };

    let normalizedNewPopulation = normalizePopulation(newPopulation);
    let normalizedPopulation = normalizePopulation(population);

    let newDayInfections = fmaxf(
        normalizedNewPopulation.infected - normalizedPopulation.infected, 0.0f);
    let newDayRecoveries = fmaxf(normalizedNewPopulation.recovered -
                                     normalizedPopulation.recovered,
                                 0.0f);

    return updateDay(state,
                     state.day + 1,
                     newPopulation,
                     newDayInfections,
                     newDayRecoveries);
}

static state_t inline alterState(state_t state,
                                 float infectionRate,
                                 float recoveryRate) {
    state_t s = state;
    s.infectionRate = infectionRate;
    s.recoveryRate = recoveryRate;

    return s;
}

static float newRateFromInput(const char *rateName) {
    printf("Enter %s rate (0-1): ", rateName);

    let result = float32Input(stdin);
    if (result.haveValue) {
        if (result.just >= 0 && result.just <= 1) {
            return result.just;
        }
    }
    musttail return newRateFromInput(rateName);
}

static inline option_t optionSimulate(state_t state) {
    let newState = nextDay(state);

    let newPopulation =
        f32x3ToPopulation(newState.populationHistory[newState.day]);
    if (!checkPopulation(normalizePopulation(newPopulation))) {
        printf("Error: population corrupted on day %u\n", newState.day);
        showPopulation(newState.day, newState);
        return (option_t){OPTION_CODE_FAILURE, state};
    }
    showPopulation(newState.day, newState);
    if (!(newState.day < MAX_DAYS)) {
        printf("Reached maximal number of days that program can simulate. "
               "Exiting...\n");
        return (option_t){OPTION_CODE_SUCCESS, newState};
    }

    return (option_t){OPTION_CODE_CONTINUE, newState};
}

static inline option_t optionChangeParameters(state_t state) {
    let newInfectionRate = newRateFromInput("infection");
    let newRecoveryRate = newRateFromInput("recovery");
    printf("Parameters updated.\n");
    return (option_t){OPTION_CODE_CONTINUE,
                      alterState(state, newInfectionRate, newRecoveryRate)};
}

static inline option_t optionShowHistory(state_t state) {
    showHistory(state);
    return (option_t){OPTION_CODE_CONTINUE, state};
}

static inline option_t optionExitProgram(state_t state) {
    return (option_t){OPTION_CODE_SUCCESS, state};
}

static inline int nextStep(state_t state) {
    static option_t (*const options[])(state_t) = {
        [MENU_SIMULATE_NEXT_DAY] = optionSimulate,
        [MENU_CHANGE_MODEL_PARAMETERS] = optionChangeParameters,
        [MENU_SHOW_SIMULATION_HISTORY] = optionShowHistory,
        [MENU_EXIT] = optionExitProgram,
    };

    let result = options[menu()](state);
    switch (result.code) {
    case OPTION_CODE_CONTINUE:
        musttail return nextStep(result.state);
    case OPTION_CODE_SUCCESS:
        return EXIT_SUCCESS;
    case OPTION_CODE_FAILURE:
        return EXIT_FAILURE;
    }
}

int setup() {
    let state = (state_t){
        .dailyInfections = { 0 },
        .dailyRecoveries = { 0 },
        .day = 0,
        .populationHistory = {{80, 10, 10}},
        .infectionRate = 0.20f,
        .recoveryRate = 0.10f,
    };

    showPopulation(state.day, state);
    return nextStep(state);
}
