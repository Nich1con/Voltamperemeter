/*
   Скетч для проекта AVR - вольтамперметра на базе INA219 и семисегментных индикаторов
   МК - ATmega8, но возможно и применение ATmega328p/168p/88p
*/

/* Настройки */
#define SHUNT_R       0.1f  // Сопротивление шунта в Омах
#define MAX_I         3.2f  // Максимальный ожидаемый ток (3.2 А)
#define CAL_STEP      25    // Шаг калибровки тока при нажатии кнопок

#define UPD_PERIOD    200   // Период обновления показаний, мс
#define DISP_PERIOD   2000  // Период динамической индикации, мкс

#define EEKEY         0xFA  // Ключ EEPROM (для проверки при первом включении)
#define EEKEY_ADDR    10    // Адрес ключа в EEPROM
#define CAL_ADDR      11    // Адрес калибровки в EEPROM
#define CAL_TIMEOUT   3000  // Таймаут калибрвоки, мс

#define UP_BTN_PIN    11    // Пин кнопки вверх
#define DWN_BTN_PIN   10    // Пин кнопки вниз

/* Private - определения */
#define _LSB_I (MAX_I / 32768.0f)
#define _CAL (0.04096f / (_LSB_I * SHUNT_R))
#define _MAX_VSHUNT (MAX_I * SHUNT_R)

/* Библиотеки (необходимо установить из папки libraries)*/
#include <EEPROM.h>         // Библиотека EEPROM
#include <GyverFilters.h>   // Библиотека цифровых фильтров
#include <GyverButton.h>    // Библиотека для работы с кнопками
#include <avr/wdt.h>        // Встроенная библиотека watchdog
#include "INA219.h"         // Класс для работы с INA219 (НЕ библиотека!)

/* Обьекты */
INA219 sensor;
GMedian3 <uint16_t> mvMedian;
GMedian3 <uint16_t> maMedian;
GButton upBtn(UP_BTN_PIN);
GButton dwnBtn(DWN_BTN_PIN);

/* Битовые маски цифр для дисплея */
const uint8_t dispMasks[] = {
  0b01111110, // 0
  0b00110000, // 1
  0b01101101, // 2
  0b01111001, // 3
  0b00110011, // 4
  0b01011011, // 5
  0b01011111, // 6
  0b01110000, // 7
  0b01111111, // 8
  0b01111011  // 9
};

uint8_t dispBuff[8];          // Буфер дисплея
uint16_t currentCal = _CAL;   // Переменная калибровки тока

void setup() {
  wdt_disable();  // Останавливаем wdt
  displayBegin(); // Инициализируем работу с дисплеем

  if (!sensor.begin()) {      // Если INA219 не отвечает - ошибка
    dispBuff[1] = 0b01001111; // 'E'
    dispBuff[2] = 0b00000101; // 'r'
    dispBuff[3] = 0b10000101; // 'r.'
    wdt_enable(WDTO_1S);      // RESET через 1 секунду
    while (1);
  }

  pinMode(UP_BTN_PIN, INPUT_PULLUP);  // Настраиваем кнопки
  pinMode(DWN_BTN_PIN, INPUT_PULLUP);
  upBtn.setStepTimeout(150);
  dwnBtn.setStepTimeout(150);
  upBtn.setTickMode(AUTO);
  dwnBtn.setTickMode(AUTO);

  if (EEPROM.read(EEKEY_ADDR) != EEKEY) { // Читаем калибровку из EEPROM
    EEPROM.write(EEKEY_ADDR, EEKEY);
    EEPROM.put(CAL_ADDR, currentCal);
  } else {
    EEPROM.get(CAL_ADDR, currentCal);
  }

  sensor.setCalibration(currentCal);      // Калибруем INA219

  wdt_reset();                            // Сбрасываем WDT
  wdt_enable(WDTO_1S);                    // Запускаем WDT
}

void loop() {
  buttonsPoll();                          // Проверяем кнопки

  static uint32_t dispTimer = millis();   // Таймер вывода на дисплей
  if (millis() - dispTimer >= UPD_PERIOD) {
    dispTimer = millis();
    float mv = sensor.getVoltage();       // Получаем напряжение и ток
    float ma = sensor.getCurrent();
    mv = mvMedian.filtered(mv);           // Прогоняем через медианный фильтр
    ma = maMedian.filtered(ma);
    displayVolts(mv);                     // Выводим показания
    displayMilliamperes(ma);
  }

  wdt_reset();                            // Сбрасываем wdt каждый цикл
}

void buttonsPoll() {
  static uint32_t timeout = millis();     // Переменная таймаута
  static bool needSave = false;           // Флаг сохранения в EEPROM

  if (needSave and millis() - timeout >= CAL_TIMEOUT) { // Если нужен сейв и таймаут прошел
    EEPROM.put(CAL_ADDR, currentCal);     // Сохраняем калибровку
    needSave = false;                     // Сбрасываем флаг
  }

  if (upBtn.isClick() or upBtn.isStep()) { // Нажатие на верх. кнопку
    currentCal = constrain(currentCal - CAL_STEP, _CAL * 0.7f, _CAL / 0.7f); // Калибруем на указ. шаг
    sensor.setCalibration(currentCal);     // Записываем калибровку в INA219
    needSave = true;                       // Устанавливаем флаг записи
    timeout = millis();                    // Пишем время последнего обращения
  }

  if (dwnBtn.isClick() or dwnBtn.isStep()) { // Нажатие на ниж. кнопку
    currentCal = constrain(currentCal + CAL_STEP, _CAL * 0.7f, _CAL / 0.7f); // Калибруем на указ. шаг
    sensor.setCalibration(currentCal);     // Записываем калибровку в INA219
    needSave = true;;                      // Устанавливаем флаг записи
    timeout = millis();                    // Пишем время последнего обращения
  }
}

void displayVolts(uint16_t mv) {          // Выводим напряжение (кормим милливольты)
  mv /= 10;                               // Приводим к вольтам с 2мя знаками после запятой
  for (uint8_t i = 0; i < 4; i++) {       // Рекурсивно разбиваем на цифры
    dispBuff[3 - i] = dispBuff[3 - i] & 0x80 | dispMasks[mv % 10]; // Пишем цифры в буфер
    mv /= 10;
  }
  dispBuff[0] |= 1 << 7;                  // Добавляем точку
}

void displayMilliamperes(uint16_t ma) {   // Выводим ток (кормим миллиамперы)
  for (uint8_t i = 0; i < 4; i++) {       // Рекурсивно разбиваем на цифры
    dispBuff[7 - i] = dispBuff[7 - i] & 0x80 | dispMasks[ma % 10]; // Пишем цифры в буфер
    ma /= 10;
  }
}

void displayTick() {                      // Динамическая индикация
  static uint8_t displayPtr = 0;          // Указатель на текущее знакоместо
  static uint8_t lastPtr = 0;             // Указатель на предыдущее знакоместо
  const uint8_t catPins[] = {16, 17, 18, 19, 8, 9, 14, 15}; // Список пинов - катодов индикаторов

  digitWrite(catPins[lastPtr], false);    // Отключаем предыдущий индикатор
  digitWrite(catPins[displayPtr], true);  // Подключаем текущий индикатор
  PORTD = dispBuff[displayPtr];           // Выводим значение буфера на текущий
  lastPtr = displayPtr;                   // Запомнинаем указатель
  if (++displayPtr > 7) displayPtr = 0;   // Двигаем указатель по дисплею
}

void displayBegin(void) {                 // Инициализация дисплея
  PORTC &= ~ 1 << PC0 |  1 << PC1 |  1 << PC2 |  1 << PC3; // Отключаем все индикаторы
  PORTB &= ~ 1 << PB0 |  1 << PB1 |  1 << PB6 |  1 << PB7;
  PORTD = 0x00;                           // Отключаем все сегменты
  DDRD = 0xFF;                            // настраиваем пины сегментов как выходы
#if defined (__AVR_ATmega8__)                 // Для меги 8
  TCCR2 = 1 << WGM21 | 1 << CS22 | 1 << CS20; // Настраиваем таймер-2 на прерывания
  OCR2 = constrain(DISP_PERIOD / 16, 1, 255); // Грубо считаем период индикации
  TIMSK |= 1 << OCIE2;                        // Подрубаем прерывания
#else if defined (__AVR_ATmega328p__) || (__AVR_ATmega168p__) || (__AVR_ATmega88p__)
  TCCR2A = 1 << WGM21;                         // Настраиваем таймер-2 на прерывания
  TCCR2B = 1 << CS22 | 1 << CS20;
  OCR2A = constrain(DISP_PERIOD / 16, 1, 255); // Грубо считаем период индикации
  TIMSK2 |= 1 << OCIE2A;                       // Подрубаем прерывания
#endif
}

void digitWrite(uint8_t num, bool state) {    // Свой pinMode - чтобы задействовать пины PB6 и PB7
  if (num < 16) bitWrite(DDRB, (num - 8), state);
  else if (num < 22) bitWrite(DDRC, (num - 16), state);
}

#if defined (__AVR_ATmega8__)                 // Для меги 8
ISR(TIMER2_COMP_vect) {
#else if defined (__AVR_ATmega328p__) || (__AVR_ATmega168p__) || (__AVR_ATmega88p__)
ISR(TIMER2_COMPA_vect) {
#endif
  displayTick(); // Обработчик прерывания динамической индикации
}
