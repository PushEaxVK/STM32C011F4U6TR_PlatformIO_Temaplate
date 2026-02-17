#include "stm32c0xx_hal.h"
#include "core_cm0plus.h" // або cmsis_gcc.h / cmsis_armclang.h залежно від тулчейну

// Прототипи функцій
void SystemClock_Config(void);
// void GPIO_Init(void);
static void MX_GPIO_Init(void);

volatile uint32_t counter = 0;

static void dbg_primask(const char* tag) {
    volatile uint32_t p = __get_PRIMASK();
    (void)tag;
    (void)p;
    __NOP(); // тут став брейкпоінт і дивись p
}

int main(void) {
    dbg_primask("entry");
    // 1. Ініціалізація бібліотеки HAL
    // Це скидає всі периферійні пристрої та ініціалізує Flash-інтерфейс і Systick.
    HAL_Init(); // Ініціалізація бібліотеки HAL.
    dbg_primask("after HAL_Init");

    // 2. Налаштування системи тактування
    // Для STM32C0 за замовчуванням використовується внутрішній генератор HSI48.
    SystemClock_Config(); // Налаштування тактування чипа.
    dbg_primask("after SystemClock_Config");
    // __enable_irq();

    // 3. Ініціалізація ніжки PA0 як виходу
    MX_GPIO_Init();

    // Перед початком циклу встановлюємо Високий рівень, щоб світлодіод був вимкнений.
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);

    while (1) {
        // Перемикання стану ніжки PA0
        // Коли стан змінюється на LOW (0), діод загоряється, на HIGH (1) - гасне.
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);

        // Затримка у 500 мілісекунд (0.5 секунди)

        counter++;
        HAL_Delay(100);
    }
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Увімкнення тактування порту GPIOA через регістр RCC_IOPENR.
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Налаштування параметрів піна PA0
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;  // Вихід Push-Pull (двотактний)
    GPIO_InitStruct.Pull = GPIO_NOPULL;          // Без внутрішніх резисторів (використовуємо зовнішній)
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // Низька швидкість для економії енергії
    
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** 1. Налаштування внутрішнього генератора HSI48
      * STM32C011 має вбудований 48 МГц RC-осцилятор [1-3].
      * Після скидання (Reset) система за замовчуванням працює на частоті 12 МГц 
      * через подільник HSIDIV = 4 [4, 5].
      */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1; // Встановлюємо подільник 1, щоб отримати повні 48 МГц [6]
    
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        // Помилка ініціалізації осцилятора
        while(1);
    }

    /** 2. Налаштування системних шин (SYSCLK, HCLK, PCLK1)
      * Для частоти 48 МГц необхідно встановити 1 затримку Flash (Wait State), 
      * оскільки при частотах > 24 МГц читання без затримок неможливе [7, 8].
      */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI; // Джерело - HSISYS (від HSI48) [9]
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;     // HCLK = 48 МГц [10]
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;      // PCLK1 = 48 МГц [4]

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
        // Помилка налаштування частоти шин
        while(1);
    }
}

void SysTick_Handler(void) {
    HAL_IncTick(); // Інкремент лічильника мілісекунд HAL
}
