/*
  РОЗУМНИЙ ІНДИКАТОР ТРИВОГИ + БУДИЛЬНИКИ — ESP32, версія 14
  ---------------------------------------------------------------------------
  Ця версія ЗАМІНЮЄ всі попередні файли.

  ВИПРАВЛЕННЯ v14.2 (надійність будильника):
   - TLS-рукостискання обмежено 5 с (типово було 120 с — довше за watchdog,
     завислий сервер міг перезавантажити плату саме в час будильника).
   - Старт без Wi-Fi: якщо мережу вже збережено, портал не відкривається, пристрій
     працює без мережі й сам перепідключається. Раніше без роутера він безкінечно
     перезавантажувався (портал 3 хв -> рестарт) і будильники не працювали.
   - Пропущені хвилини (цикл зайнятий > 1 хв, стрибок годинника) перевіряються
     заднім числом, до 5 хв — будильник не пропадає. Восени 03:xx не дзвонить двічі.
   - Антибрязкіт кнопок (40 мс): завада на дроті більше не вимикає дзвінок.
   - Дзвінок на відбій — лише якщо відбій прийшов не пізніше 4 год після
     пропущеного будильника (PENDING_MAX_MS), щоб не будити посеред ночі.
   - Відбій зараховується після 2 успішних відповідей «тривоги немає» підряд
     (CLEAR_CONFIRM_COUNT): одна хибна відповідь API не будить під час тривоги.
   - Стан будильників (остання перевірена хвилина, «чекає на відбій») — у
     RTC-пам'яті: після перезавантаження watchdog'ом будильник, що припав на
     час перезавантаження, дзвонить одразу після старту, а дзвінок на відбій
     не губиться (у т.ч. якщо відбій настав, поки плата стартувала).
   - Без інтернету (Wi-Fi є, а DNS/сервер не відповідає) запити більше не йдуть
     один за одним без паузи: інтервал рахується від кінця запиту.
   - Дані про тривогу, старші за 49 діб, не стають знову «свіжими» через
     переповнення millis().
   - Опитування раз на хвилину (було 20 с): при 20 с сервер відхиляв ~40%
     запитів з HTTP 401 при правильному токені, при 30 с — поодинокі. Тому запас
     «свіжості» даних (STALE_MS) збільшено до 5 хв — це 5 збоїв підряд.
     У журнал пишуться лише серії від 2 збоїв підряд; поодинокі — лише в лічильниках
     і «найдовшій серії» вгорі /log.

  ВИПРАВЛЕННЯ v14.1 (за результатами код-рев'ю):
   - Будильник з «Не дзвонити, якщо активна тривога» тепер довіряє останній
     УСПІШНІЙ відповіді API протягом 2 хв (dataStale), як LED і сторінка, а не
     лише самому останньому запиту. Раніше разовий збій запиту будив навіть
     під час тривоги, що триває годинами, хоча сторінка показувала «ТРИВОГА».
   - Втрата Wi-Fi більше не блокує пристрій: перепідключення без блокування,
     портал налаштувань при старті має таймаут (потім перезавантаження).
     Виправлено висячий вказівник WiFiManagerParameter.
   - Запит до API має таймаути і не виконується під час дзвінка. Відповідь без
     поля activeAlerts — тепер збій, а не «чисто». JSON розбирається з фільтром.
   - TLS-сертифікат UkraineAlarm перевіряється (див. TLS_VERIFY).
   - Безпека: екранування HTML, cookie HttpOnly+SameSite, криптостійкий токен
     сесії, блокування після 5 невдалих входів, /resetwifi лише POST, попередження
     про типовий пароль, обрізання пробілів у токені.
   - BOOT-скидання тепер справді стирає й токен (як і було задокументовано).
   - Журнал подій і діагностика на сторінці /log (кільцевий буфер у RAM).
   - Дзвінок повторюється до 2 разів через 5 хв, якщо його ніхто не вимкнув кнопкою.
   - Watchdog, delay(5) у циклі, час без блокуючого getLocalTime, резервування HTML.

  Зміни v14 порівняно з v13:

  0) ГОЛОВНЕ: уночі (коли зелений/синій LED і так вимкнені нічним режимом —
     див. п.1 нижче) короткий зумер-сигнал на ПОЧАТОК і ВІДБІЙ тривоги
     (звичайний beepPattern — 3 коротких гудки / 1 довгий) тепер НЕ лунає.
     Логіка: якщо ми свідомо гасимо світло, щоб не заважати спати, то й
     звуковий сигнал про саму зміну стану (не про будильник!) вночі так само
     зайвий — людина однаково не бачить LED, а лише реагувати на короткий
     писк, не розуміючи, зелено чи червоно, немає сенсу.
       - Це стосується ВИКЛЮЧНО короткого сигналу-сповіщення про зміну
         кольору. Самі БУДИЛЬНИКИ (і звичайні за розкладом, і дзвінок на
         підтверджений відбій) вночі й далі дзвонять як завжди, повним
         дзвінком із зумером — це окрема, критично важлива функція
         («прокинутись і встигнути в школу»), яку ніч жодним чином не
         вимикає і не приглушує.
       - Якщо нічний режим вимкнено (nightModeEnabled == false) або зараз
         не нічна година — сигнал на зміну стану лунає, як і раніше.

  1) Нічний режим для LED. Пристрій часто стоїть у спальні
     (типовий випадок — у кімнаті дитини), і вночі зелений чи синій LED —
     просто зайве світло, коли й так усе гаразд або дані ще не оновились.
       - Налаштовується на головній сторінці: вимикач «Нічний режим» і
         дві години — початок і кінець «нічної тиші» (типово 22:00–07:00,
         вікно коректно рахується і через північ).
       - Уночі ЗЕЛЕНИЙ і СИНІЙ гаснуть повністю (LED вимкнені), навіть
         якщо логічно мали б світитись.
       - ЧЕРВОНИЙ (підтверджена активна тривога) світить ЗАВЖДИ, незалежно
         від часу доби і нічного режиму — це єдиний стан LED, що несе
         критично важливу інформацію, і його свідомо ніколи не гасимо.
         Основний спосіб розбудити при тривозі — все одно гучний дзвінок
         будильника (7.2/7.5 у гайді), а не сам факт світіння LED; нічний
         режим стосується виключно «фонового» зеленого/синього світла.
       - Автоматика керується тим самим годинником (NTP), що вже є для
         будильників — жодного нового датчика чи компонента не треба.
         Години налаштовуються один раз вручну (за замовчуванням розумні
         значення), а далі працює само щоночі.

  Усе інше — як у v12 (успадковано з v7-v11):
  2) Будильник тепер дзвонить НА ВІДБІЙ, а не просто мовчки
     чекає наступного дня. Типовий сценарій: дитина-школярка, будильник
     на 07:00 з увімкненим «Не дзвонити, якщо активна тривога» — якщо
     рівно о 07:00 йде тривога, вставати на пари ще рано, і раніше
     пристрій просто пропускав цей виклик до завтра. Тепер — інакше:
       - Якщо будильник пропущено САМЕ через підтверджену активну тривогу
         (respectAlert І свіжі дані І alertActive), він позначається
         як «очікує на відбій» (alarmPendingClear[i] = true).
       - Щойно надходить ПІДТВЕРДЖЕНЕ (не «здогад», а реальна успішна
         відповідь API) повідомлення, що тривоги більше немає, і є хоч
         один будильник, що чекав на відбій — пристрій ПОВНОЦІННО дзвонить
         (не тихий сигнал-нагадування, а справжній дзвінок із зумером і
         блиманням, як завжди — щоб дійсно розбудити), і знімає позначку
         з усіх будильників, що чекали.
       - Якщо тривога тривала кілька днів — пристрій так само чекатиме,
         скільки потрібно, і задзвонить одразу по відбою.
       - Обмеження: позначка «очікує на відбій» живе в RTC-пам'яті (не в
         NVS) — переживає програмне перезавантаження (watchdog, збій), але
         губиться при повному вимкненні живлення: тоді дзвінка на відбій
         для цього випадку не станеться (далі — як завжди за розкладом).

  3) Третій, чесний стан індикатора — «не впевнені» (синій, окрім нічних
     годин — див. п.1), коли понад 5 хвилин (STALE_MS) немає жодного
     успішного запиту до UkraineAlarm. Стосується лише кольору LED, не
     логіки будильників.
  4) Джерело даних про тривогу — UkraineAlarm API (api.ukrainealarm.com),
     GET /api/v3/alerts/{regionId}, заголовок Authorization без "Bearer".
     Тривога = є запис з "type":"AIR" у масиві activeAlerts. ArduinoJson
     потрібен для розбору відповіді.
  5) Токен API і ID регіону редагуються прямо на головній сторінці (і в
     порталі первинного налаштування). Поточний час пристрою (Київ, NTP)
     показано на сторінці.
  6) Будильник з увімкненим «Не дзвонити, якщо активна тривога» мовчить
     ТІЛЬКИ якщо ми точно (свіжі дані, !dataStale()) знаємо, що тривога
     активна. Якщо стан невідомий — дзвонить як завжди (обережність понад
     усе: краще зайвий раз розбудити).

  Мережа «Будильник-Setup» під паролем адміністратора (типово 12345678,
  мінімум 8 символів), до 8 будильників з вибором днів тижня, зовнішня
  кнопка зупинки дзвінка на GPIO4 (+ дублююча BOOT), авто-зупинка дзвінка
  через 30с.

  Довге утримання кнопки BOOT ~3с САМЕ ПРИ УВІМКНЕННІ — скидає Wi-Fi,
  токен і пароль до типових значень (потрібна саме кнопка BOOT на платі,
  не зовнішня).

  Зумер на GPIO32 (не GPIO25 — на цій платі GPIO25 не працює як вихід,
  перевірено тестовим LED), сигнал іде через транзистор 2N2222.

  Бібліотеки, які треба встановити (Arduino IDE → Менеджер бібліотек):
    - WiFiManager (автор tzapu)
    - ArduinoJson (автор Benoit Blanchon, версія 6.x або 7.x)
  (WebServer, Preferences, ESPmDNS, HTTPClient, WiFiClientSecure, time.h —
   вже вбудовані в ядро ESP32, встановлювати окремо не треба.)
*/

#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include <stdarg.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

// ================== ПІНИ (під типовий ESP32 DevKit) ==================
const int PIN_LED_GREEN   = 27;
const int PIN_LED_RED     = 26;
const int PIN_LED_BLUE    = 14;
const int PIN_BUZZER      = 32;   // на цій платі GPIO25 не працює як вихід (перевірено тестовим LED) — сигнал іде через транзистор 2N2222
const int PIN_BOOT_BTN    = 0;    // штатна кнопка BOOT на платі (дублює зупинку дзвінка)
const int PIN_DISMISS_BTN = 4;    // зовнішня кнопка «Зупинити будильник» — GPIO4 до GND

const unsigned long CHECK_INTERVAL_MS = 60000; // опитування UkraineAlarm раз на хвилину (частіше сервер відхиляв частину запитів з HTTP 401)
const int FAIL_LOG_STREAK = 2;                 // у журнал пишемо лише серію збоїв підряд від стількох (поодинокі 401 витісняли з журналу рішення будильників)
const unsigned long STALE_MS = 300000UL; // 5 хв без УСПІШНОГО запиту (5 опитувань підряд) — вважаємо стан тривоги невідомим (LED синій, будильник дзвонить)

const char* DEFAULT_ADMIN_PASSWORD = "12345678";
const long  DEFAULT_UID = 152; // Черкаський район — regionId у довіднику UkraineAlarm (api/v3/regions)
const char* TZ_KYIV = "EET-2EEST,M3.5.0/3,M10.5.0/4"; // Київський час, з переходом на літній/зимовий

// Перевірка TLS-сертифіката UkraineAlarm (захист токена і відповіді від підміни в мережі).
// Довіряємо кореню GTS Root R4 (Google Trust Services, чинний до 2036). Якщо колись
// сервіс змінить центр сертифікації і стан назавжди стане «невідомий (HTTP -1)» —
// поставте 0 (працює без перевірки, як у v14) і оновіть PEM нижче.
#define TLS_VERIFY 1
const char* ROOT_CA_PEM =
"-----BEGIN CERTIFICATE-----\n"
"MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD\n"
"VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG\n"
"A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw\n"
"WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz\n"
"IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi\n"
"AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi\n"
"QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR\n"
"HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n"
"BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D\n"
"9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8\n"
"p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD\n"
"-----END CERTIFICATE-----\n";

const unsigned long HTTP_CONNECT_TIMEOUT_MS = 3000; // без цього запит міг висіти ~10 с і глушити дзвінок
const unsigned long HTTP_TIMEOUT_MS = 4000;
const unsigned long HTTP_HANDSHAKE_TIMEOUT_S = 5;   // TLS-рукостискання: типово 120 с — довше за watchdog (60 с)
const unsigned long WIFI_PORTAL_TIMEOUT_S = 180;    // портал налаштувань при старті: після таймауту — перезавантаження
const unsigned long WIFI_RECONNECT_MS = 10000;      // як часто пробувати перепідключитись під час роботи
const uint32_t WDT_TIMEOUT_S = 60;                  // watchdog: зависання циклу довше — перезавантаження

const int  RING_REPEAT_MAX = 2;                     // якщо дзвінок скінчився сам (не кнопкою) — повторити стільки разів; 0 = вимкнено
const unsigned long RING_REPEAT_DELAY_MS = 300000UL; // через 5 хв

const unsigned long PENDING_MAX_MS = 4UL * 3600000UL; // дзвінок на відбій — лише протягом 4 год після пропущеного будильника (щоб не будити о 2-й ночі)
const int  CLEAR_CONFIRM_COUNT = 2;                 // відбій зараховуємо лише після стількох успішних відповідей «тривоги немає» підряд
const unsigned long BTN_DEBOUNCE_MS = 40;           // кнопка має бути натиснута стабільно стільки мс (захист від завад на дроті)
const long ALARM_CATCHUP_MIN = 5;                   // якщо цикл пропустив хвилини — перевірити будильники за стільки останніх хвилин

// Захист входу: після LOGIN_MAX_FAILS невдалих спроб підряд вхід блокується
const int LOGIN_MAX_FAILS = 5;
const unsigned long LOGIN_LOCK_MS = 300000UL;

const int MAX_ALARMS = 8;
const unsigned long RING_TIMEOUT_MS = 30000UL; // 30 с — авто-зупинка дзвінка, якщо не натиснути кнопку
const unsigned long RING_TOGGLE_MS  = 500;     // період біп/пауза під час дзвінка

// ================== БУДИЛЬНИКИ (зберігаються одним блоком у NVS) ==================
struct Alarm {
  uint8_t enabled;      // 0/1
  uint8_t hour;         // 0-23
  uint8_t minute;       // 0-59
  uint8_t days;         // біт i = день тижня за tm_wday: 0=Нд,1=Пн,2=Вт,3=Ср,4=Чт,5=Пт,6=Сб
  uint8_t respectAlert; // 0/1 — «Не дзвонити, якщо активна тривога»
};
Alarm alarms[MAX_ALARMS];
// Стан будильників у RTC-пам'яті: переживає програмне перезавантаження (watchdog,
// збій), як і сам годинник, але скидається при повному вимкненні живлення.
// Тож якщо плата перезавантажилась саме в хвилину будильника, він не пропаде
// (див. checkAlarms), і позначка «чекає на відбій» теж збережеться.
const uint32_t RTC_STATE_MAGIC = 0xB0D1A1E2;
RTC_NOINIT_ATTR uint32_t rtcStateMagic;
RTC_NOINIT_ATTR time_t alarmPendingSince[MAX_ALARMS]; // коли будильник i пропущено через тривогу (0 = не чекає на відбій)
RTC_NOINIT_ATTR long alarmLastFiredKey[MAX_ALARMS]; // локальні «рік+день+хвилина» останнього спрацювання — щоб восени (03:xx двічі) не дзвонити двічі
RTC_NOINIT_ATTR long lastCheckedEpochMinute;         // остання перевірена хвилина (epoch/60), -1 = ще жодної
const char* DAY_LABELS[7] = {"Нд","Пн","Вт","Ср","Чт","Пт","Сб"}; // індекс = tm_wday

// ================== ЗБЕРЕЖЕНІ НАЛАШТУВАННЯ (NVS) ==================
Preferences prefs;
String apiToken;
long   myUid;          // regionId у довіднику UkraineAlarm (api/v3/regions)
String myLabel;        // довільна назва для відображення, на роботу не впливає
String adminPassword;
bool    nightModeEnabled; // чи гасити зелений/синій LED у нічні години
uint8_t nightStartHour;   // 0-23, початок «нічної тиші»
uint8_t nightEndHour;     // 0-23, кінець «нічної тиші» (може бути менше за start — через північ)

// ================== СТАН ==================
bool alertActive = false;
bool lastFetchFailed = true; // на старті вважаємо стан НЕВІДОМИМ, доки не отримаємо першу успішну відповідь
bool firstCheckDone = false;
unsigned long lastCheckMs = 0;
String lastCheckedAt = "—";
String sessionToken = ""; // порожньо = ніхто не увійшов

bool hadSuccess = false;        // чи був хоч раз УСПІШНИЙ запит з моменту ввімкнення
unsigned long lastSuccessMs = 0; // millis() моменту останнього успішного запиту

bool alarmRinging = false;
unsigned long ringStartMs = 0;
unsigned long lastRingToggleMs = 0;
bool ringBuzzerState = false;
struct Button {
  uint8_t pin;
  bool stable;              // підтверджений (після антибрязкоту) стан: true = натиснута
  bool lastRaw;
  unsigned long changedMs;  // коли сире значення востаннє змінилось
};
Button bootBtn    = { (uint8_t) PIN_BOOT_BTN, false, false, 0 };
Button dismissBtn = { (uint8_t) PIN_DISMISS_BTN, false, false, 0 };
int clearVotes = 0;                 // скільки успішних відповідей «тривоги немає» підряд прийшло під час тривоги
int ringRepeatsDone = 0;            // скільки повторів дзвінка вже було для поточного будильника
unsigned long ringRepeatAtMs = 0;   // коли повторити дзвінок (0 = не заплановано)
int loginFails = 0;
unsigned long loginLockedUntilMs = 0;
unsigned long lastWifiTryMs = 0;
bool wifiWasDown = false;
WiFiManagerParameter* tokenParam = nullptr; // живе весь час роботи (WiFiManager зберігає лише вказівник)

WebServer server(80);
WiFiManager wm;

// ================== ЖУРНАЛ ПОДІЙ (кільцевий буфер у RAM, без купи) ==================
// Статичний буфер: не фрагментує купу і не блокує цикл. Живе до перезавантаження.
const int LOG_SIZE = 40;
const int LOG_LEN  = 120;
struct LogEntry {
  time_t epoch;        // 0, якщо час ще не синхронізовано
  unsigned long ms;    // millis() на момент запису
  char text[LOG_LEN];
};
LogEntry logBuf[LOG_SIZE];
int logHead = 0;
int logCount = 0;
int lastHttpCode = 0;              // код останнього запиту до API (<0 — помилка з'єднання)
unsigned long fetchOkCount = 0;    // лічильники запитів з моменту ввімкнення
unsigned long fetchFailCount = 0;
int failStreak = 0;                // невдалих запитів підряд зараз
int maxFailStreak = 0;             // найдовша серія невдалих запитів з моменту ввімкнення

void logEvent(const char* fmt, ...) {
  LogEntry &e = logBuf[logHead];
  e.ms = millis();
  time_t n = time(nullptr);
  e.epoch = (n > 1700000000) ? n : 0;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(e.text, LOG_LEN, fmt, ap);
  va_end(ap);
  logHead = (logHead + 1) % LOG_SIZE;
  if (logCount < LOG_SIZE) logCount++;
  Serial.println(e.text);
}

// ---------------------------------------------------------------------

String two(int v) {
  return (v < 10 ? "0" : "") + String(v);
}

// Поточний час пристрою (Київ, з NTP). Повертає текст-пояснення, якщо
// час іще не синхронізувався (наприклад, щойно ввімкнули без інтернету).
// Час вважаємо синхронізованим, коли epoch «після 2023». Без очікування
// (getLocalTime(&t, 50) міг блокувати цикл до 50 мс на кожен виклик).
bool timeSynced() {
  return time(nullptr) > 1700000000;
}

bool localNow(struct tm* t) {
  if (!timeSynced()) return false;
  time_t n = time(nullptr);
  localtime_r(&n, t);
  return true;
}

// HTML-екранування значень, які користувач ввів сам (токен, назва)
String htmlEscape(const String& s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      case '\'': o += "&#39;"; break;
      default: o += c;
    }
  }
  return o;
}

String currentTimeString() {
  struct tm t;
  if (!localNow(&t)) return "ще не синхронізовано";
  char buf[32];
  strftime(buf, sizeof(buf), "%H:%M:%S, %d.%m.%Y", &t);
  return String(buf);
}

// Чи можемо ми зараз ДОВІРЯТИ значенню alertActive: токен введено, і
// ОСТАННІЙ (щойно зроблений) запит до UkraineAlarm пройшов успішно.
// Реагує миттєво на кожну окрему невдалу спробу — використовується
// логікою будильників (обережність понад усе).
bool statusKnown() {
  return apiToken.length() > 0 && !lastFetchFailed;
}

// Чи «застаріли» дані настільки, що індикатору вже не варто показувати
// зелений/червоний за старим значенням alertActive. На відміну від
// statusKnown(), тут є запас у STALE_MS — щоб один невдалий хвилинний
// опит не змушував LED смикатись синім щохвилини. Впливає ЛИШЕ на колір
// індикатора, не на логіку будильників.
bool dataStale() {
  if (apiToken.length() == 0) return true;
  if (!hadSuccess) return true;
  if ((millis() - lastSuccessMs) > STALE_MS) {
    // «Забуваємо» старий успіх: інакше через 49,7 доби без жодного успішного
    // запиту millis() переповнився б, і дуже старі дані знову здавались би свіжими.
    hadSuccess = false;
    return true;
  }
  return false;
}

// Чи зараз («за годинником» NTP) triває вікно нічної тиші для LED.
// Коректно рахує вікна, що перетинають північ (напр. 22 -> 7).
// Якщо час ще не синхронізовано або нічний режим вимкнено — вважаємо,
// що зараз НЕ ніч (безпечніше показати LED, ніж помилково згасити).
bool isNightNow() {
  if (!nightModeEnabled) return false;
  if (nightStartHour == nightEndHour) return false; // вікно нульової довжини — вважаємо вимкненим
  struct tm t;
  if (!localNow(&t)) return false;
  int h = t.tm_hour;
  if (nightStartHour < nightEndHour) {
    return h >= nightStartHour && h < nightEndHour;      // вікно в межах доби, напр. 1..5
  } else {
    return h >= nightStartHour || h < nightEndHour;       // вікно через північ, напр. 22..7
  }
}

void setLeds(bool wifiConnecting, bool alarmOn) {
  bool unsure = wifiConnecting || dataStale();
  bool night = isNightNow();
  // ЧЕРВОНИЙ (підтверджена тривога) світить завжди, навіть уночі — єдиний
  // стан LED, що несе критично важливу інформацію, і його не гасимо.
  // ЗЕЛЕНИЙ і СИНІЙ уночі вимикаємо повністю — це просто фонове світло,
  // яке лише заважає спати, коли й так усе гаразд або стан невідомий.
  bool showRed   = !unsure && alarmOn;
  bool showGreen = !unsure && !alarmOn && !night;
  bool showBlue  = unsure && !night;
  digitalWrite(PIN_LED_RED,   showRed   ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN, showGreen ? HIGH : LOW);
  digitalWrite(PIN_LED_BLUE,  showBlue  ? HIGH : LOW);
}

void beepPattern(bool alertStarted) {
  if (alertStarted) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_BUZZER, HIGH); delay(200);
      digitalWrite(PIN_BUZZER, LOW);  delay(200);
    }
  } else {
    digitalWrite(PIN_BUZZER, HIGH); delay(800);
    digitalWrite(PIN_BUZZER, LOW);
  }
}

// ================== БУДИЛЬНИКИ — ЛОГІКА ДЗВІНКА ==================

void startRinging(bool isRepeat = false) {
  logEvent(isRepeat ? "Дзвінок: СТАРТ (повтор %d)" : "Дзвінок: СТАРТ", ringRepeatsDone);
  if (!isRepeat) ringRepeatsDone = 0;
  ringRepeatAtMs = 0;
  alarmRinging = true;
  ringStartMs = millis();
  lastRingToggleMs = 0;
  ringBuzzerState = false;
}

// byTimeout = дзвінок скінчився сам (ніхто не натиснув кнопку) — тоді, якщо
// дозволено, плануємо повтор: дитина могла проспати перший дзвінок.
void stopRinging(bool byTimeout = false) {
  alarmRinging = false;
  digitalWrite(PIN_BUZZER, LOW);
  if (byTimeout && ringRepeatsDone < RING_REPEAT_MAX) {
    ringRepeatsDone++;
    ringRepeatAtMs = millis() + RING_REPEAT_DELAY_MS;
    if (ringRepeatAtMs == 0) ringRepeatAtMs = 1;
    logEvent("Дзвінок: СТОП (авто), повтор %d/%d за %lu с", ringRepeatsDone, RING_REPEAT_MAX, RING_REPEAT_DELAY_MS / 1000UL);
  } else {
    ringRepeatAtMs = 0;
    logEvent(byTimeout ? "Дзвінок: СТОП (авто)" : "Дзвінок: СТОП (кнопка)");
  }
  setLeds(WiFi.status() != WL_CONNECTED, alertActive);
}

void serviceRinging() {
  unsigned long now = millis();
  if (!alarmRinging) {
    if (ringRepeatAtMs != 0 && (long)(now - ringRepeatAtMs) >= 0) startRinging(true);
    return;
  }
  if (now - ringStartMs >= RING_TIMEOUT_MS) {
    stopRinging(true);
    return;
  }
  if (now - lastRingToggleMs >= RING_TOGGLE_MS) {
    lastRingToggleMs = now;
    ringBuzzerState = !ringBuzzerState;
    digitalWrite(PIN_BUZZER, ringBuzzerState ? HIGH : LOW);
    digitalWrite(PIN_LED_RED, ringBuzzerState ? HIGH : LOW);
  }
}

// Коротке натискання кнопки BOOT або зовнішньої кнопки під час роботи
// (не при увімкненні) — зупиняє дзвінок. Обидві кнопки рівноправні.
// Антибрязкіт: true лише в момент, коли кнопка СТАБІЛЬНО натиснута BTN_DEBOUNCE_MS.
// Одинична завада на довгому дроті (раніше вистачало одного зчитування) дзвінок не вимикає.
bool buttonPressedEdge(Button &b, unsigned long now) {
  bool raw = (digitalRead(b.pin) == LOW);
  if (raw != b.lastRaw) {
    b.lastRaw = raw;
    b.changedMs = now;
  }
  if (raw != b.stable && now - b.changedMs >= BTN_DEBOUNCE_MS) {
    b.stable = raw;
    return b.stable;
  }
  return false;
}

void serviceDismissButtons() {
  unsigned long now = millis();
  bool bootEdge = buttonPressedEdge(bootBtn, now);
  bool dismissEdge = buttonPressedEdge(dismissBtn, now);

  if (bootEdge || dismissEdge) {
    if (alarmRinging) stopRinging();
    else if (ringRepeatAtMs != 0) { ringRepeatAtMs = 0; logEvent("Повтор дзвінка скасовано кнопкою"); }
  }
}

void checkAlarmsAt(const struct tm &t);

// Перевіряє кожну хвилину від останньої перевіреної до поточної (не більше
// ALARM_CATCHUP_MIN): якщо цикл був зайнятий довше хвилини чи годинник
// стрибнув уперед, будильник не пропадає.
void checkAlarms() {
  if (!timeSynced()) return; // час ще не синхронізовано по NTP
  long nowMinute = (long) (time(nullptr) / 60);
  if (nowMinute == lastCheckedEpochMinute) return; // цю хвилину вже перевіряли

  long from = lastCheckedEpochMinute + 1;
  // перший запуск, стрибок назад або завеликий розрив — лише поточна хвилина
  if (lastCheckedEpochMinute < 0 || nowMinute < from || nowMinute - from >= ALARM_CATCHUP_MIN) from = nowMinute;
  else if (from < nowMinute) logEvent("Будильники: пропущено %ld хв, перевіряю їх", nowMinute - from);
  lastCheckedEpochMinute = nowMinute;

  for (long m = from; m <= nowMinute; m++) {
    time_t tt = (time_t) m * 60;
    struct tm t;
    localtime_r(&tt, &t);
    checkAlarmsAt(t);
  }
}

void checkAlarmsAt(const struct tm &t) {
  long key = ((long) (t.tm_year % 100) * 366 + t.tm_yday) * 1440L + t.tm_hour * 60 + t.tm_min;

  for (int i = 0; i < MAX_ALARMS; i++) {
    Alarm &a = alarms[i];
    if (!a.enabled) continue;
    if (!(a.days & (1 << t.tm_wday))) continue;
    if (a.hour != t.tm_hour || a.minute != t.tm_min) continue;
    if (alarmLastFiredKey[i] == key) continue; // восени 03:xx буває двічі — дзвонимо один раз
    alarmLastFiredKey[i] = key;

    unsigned long ageSec = hadSuccess ? (millis() - lastSuccessMs) / 1000UL : 0;
    // Довіряємо останній УСПІШНІЙ відповіді протягом STALE_MS (як і LED/сторінка), а не
    // лише самому останньому запиту: разова невдача запиту (таймаут, обрив TLS) раніше
    // будила навіть під час тривоги, що триває годинами, хоч сторінка показувала «ТРИВОГА».
    if (a.respectAlert && !dataStale() && alertActive) {
      // мовчить ЛИШЕ якщо ми знаємо (свіжі дані), що тривога активна — і
      // запам'ятовуємо, що саме ЦЕЙ будильник чекає на відбій (7.4/7.5)
      logEvent("Будильник %d: МОВЧИТЬ (тривога), вік даних %lus", i + 1, ageSec);
      alarmPendingSince[i] = time(nullptr);
      continue;
    }
    logEvent("Будильник %d: ДЗВОНИТЬ resp=%d known=%d alert=%d lastFail=%d stale=%d вік=%lus HTTP=%d",
             i + 1, a.respectAlert, statusKnown(), alertActive, lastFetchFailed, dataStale(), ageSec, lastHttpCode);
    // Якщо стан тривоги невідомий (немає токена чи зв'язку з API) —
    // будильник дзвонить, навіть із увімкненим «Не дзвонити...»: краще
    // розбудити зайвий раз, ніж мовчати на основі даних, яких нема.
    startRinging();
  }
}

// Знімає позначку «очікує на відбій» з усіх будильників і повідомляє,
// чи хоч один такий був не давніше PENDING_MAX_MS. Викликається лише коли
// ПІДТВЕРДЖЕНО (успішними запитами), що тривога щойно закінчилась — див. loop().
bool clearPendingAlarms() {
  bool any = false;
  time_t now = time(nullptr);
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarmPendingSince[i] != 0) {
      long ageMin = (long) ((now - alarmPendingSince[i]) / 60);
      alarmPendingSince[i] = 0;
      if (ageMin >= 0 && (unsigned long) ageMin * 60000UL <= PENDING_MAX_MS) any = true;
      else logEvent("Будильник %d: відбій через %ld хв — задовго, на відбій не дзвонимо", i + 1, ageMin);
    }
  }
  return any;
}

// ================== АВТЕНТИФІКАЦІЯ (проста, на cookie) ==================

bool isLoggedIn() {
  if (sessionToken == "") return false;
  if (!server.hasHeader("Cookie")) return false;
  // точний збіг cookie (а не підрядок): "; session=<токен>;"
  String cookie = "; " + server.header("Cookie") + ";";
  return cookie.indexOf("; session=" + sessionToken + ";") != -1;
}

// Криптостійкий випадковий токен сесії (апаратний генератор ESP32), 128 біт
String newSessionToken() {
  char buf[33];
  for (int i = 0; i < 4; i++) snprintf(buf + 8 * i, 9, "%08x", (unsigned) esp_random());
  return String(buf);
}

void redirectToLogin(const char* suffix = "") {
  server.sendHeader("Location", String("/login") + suffix);
  server.send(303);
}

void clearSessionCookie() {
  sessionToken = "";
  server.sendHeader("Set-Cookie", "session=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
}

void handleLoginPage() {
  bool err = server.arg("err") == "1";
  bool locked = server.arg("err") == "2";
  bool changed = server.hasArg("changed");

  String html = "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Вхід — Розумний індикатор</title>";
  html += "<style>body{font-family:sans-serif;max-width:360px;margin:60px auto;padding:0 16px;}";
  html += "h1{font-size:20px} input,button{font-size:16px;padding:8px;margin-top:6px;width:100%;box-sizing:border-box}";
  html += ".err{color:#c1442c} .ok{color:#2e8b57} .hint{color:#777;font-size:13px;margin-top:20px}</style></head><body>";
  html += "<h1>Розумний індикатор тривоги</h1>";
  if (err) html += "<p class='err'>Невірний пароль.</p>";
  if (locked) html += "<p class='err'>Забагато невдалих спроб. Зачекайте кілька хвилин.</p>";
  if (changed) html += "<p class='ok'>Пароль змінено. Увійдіть новим паролем.</p>";
  html += "<form method='POST' action='/login'>";
  html += "<label>Пароль адміністратора:</label>";
  html += "<input type='password' name='password' autofocus required>";
  html += "<button type='submit'>Увійти</button>";
  html += "</form>";
  html += "<p class='hint'>Це той самий пароль, що й для мережі «Будильник-Setup» при повторному налаштуванні Wi-Fi. Забули пароль? Утримайте кнопку BOOT на пристрої ~3 секунди під час увімкнення — це скине Wi-Fi, токен і пароль до типових значень.</p>";
  html += "</body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

void handleLoginSubmit() {
  unsigned long now = millis();
  if (loginLockedUntilMs != 0) {
    if ((long)(now - loginLockedUntilMs) < 0) { redirectToLogin("?err=2"); return; }
    loginLockedUntilMs = 0; // блокування минуло
  }
  String pass = server.arg("password");
  if (pass.length() > 0 && pass == adminPassword) {
    loginFails = 0;
    sessionToken = newSessionToken();
    server.sendHeader("Set-Cookie", "session=" + sessionToken + "; Path=/; HttpOnly; SameSite=Strict");
    server.sendHeader("Location", "/");
    server.send(303);
  } else {
    loginFails++;
    logEvent("Вхід: невірний пароль (%d/%d)", loginFails, LOGIN_MAX_FAILS);
    if (loginFails >= LOGIN_MAX_FAILS) {
      loginFails = 0;
      loginLockedUntilMs = now + LOGIN_LOCK_MS;
      if (loginLockedUntilMs == 0) loginLockedUntilMs = 1;
      logEvent("Вхід: заблоковано на %lu с", LOGIN_LOCK_MS / 1000UL);
      redirectToLogin("?err=2");
      return;
    }
    redirectToLogin("?err=1");
  }
}

void handleLogout() {
  clearSessionCookie();
  redirectToLogin();
}

// ================== ВЕБ-СТОРІНКА СТАНУ Й НАЛАШТУВАНЬ ==================

void handleRoot() {
  if (!isLoggedIn()) { redirectToLogin(); return; }

  bool passErr = server.hasArg("passerr");
  String statusText, statusColor;
  if (apiToken.length() == 0) {
    statusText = "ТОКЕН НЕ ВВЕДЕНО";
    statusColor = "#8a8a8a";
  } else if (dataStale()) {
    statusText = (lastHttpCode == 401 || lastHttpCode == 403) ? "ТОКЕН ВІДХИЛЕНО (HTTP " + String(lastHttpCode) + ")"
               : "СТАН НЕВІДОМИЙ (немає свіжих даних)";
    statusColor = "#3a6ea5";
  } else if (alertActive) {
    statusText = "ТРИВОГА";
    statusColor = "#c1442c";
  } else {
    statusText = "Чисто";
    statusColor = "#2e8b57";
  }

  String html;
  html.reserve(12000); // без цього сторінка ~10 КБ збирається десятками реалокацій і фрагментує купу
  html = "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Розумний індикатор</title>";
  html += "<style>body{font-family:sans-serif;max-width:460px;margin:24px auto;padding:0 16px;}";
  html += "h1{font-size:20px} .badge{display:inline-block;padding:6px 14px;border-radius:999px;color:#fff;font-weight:600}";
  html += "select,input,button{font-size:16px;padding:8px;margin-top:6px;width:100%;box-sizing:border-box}";
  html += "input[type=checkbox]{width:auto;padding:0;margin:0}";
  html += ".err{color:#c1442c} .hint{color:#777;font-size:13px} hr{margin:24px 0;border:none;border-top:1px solid #ddd}";
  html += ".alarmcard{border:1px solid #ddd;border-radius:10px;padding:10px 12px;margin-top:10px}";
  html += ".alarmhead{display:flex;align-items:center;gap:8px;font-weight:600}";
  html += ".days{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px}";
  html += ".daybtn{display:flex;flex-direction:column;align-items:center;font-size:12px;gap:2px}";
  html += ".respect{display:flex;align-items:center;gap:8px;margin-top:8px;font-size:13.5px}</style></head><body>";
  html += "<h1>Розумний індикатор тривоги</h1>";
  if (adminPassword == DEFAULT_ADMIN_PASSWORD) {
    html += "<p class='err'><b>Увага:</b> встановлено типовий пароль. Змініть його нижче — ним захищена й мережа «Будильник-Setup».</p>";
  }
  html += "<p>Стан: <span class='badge' style='background:" + statusColor + "'>" + statusText + "</span></p>";
  html += "<p>ID регіону: <b>" + String(myUid) + "</b>" + (myLabel.length() ? " (" + htmlEscape(myLabel) + ")" : "") + "<br>Остання перевірка: " + lastCheckedAt + "</p>";
  html += "<p>Поточний час пристрою: <b>" + currentTimeString() + "</b></p>";

  html += "<form method='POST' action='/save'>";
  html += "<label>Токен API UkraineAlarm:</label><input type='text' name='token' value='" + htmlEscape(apiToken) + "'>";
  html += "<label>ID регіону (regionId UkraineAlarm):</label><input type='number' name='uid' value='" + String(myUid) + "' required>";
  html += "<label>Назва (за бажанням, лише для відображення):</label><input type='text' name='label' value='" + htmlEscape(myLabel) + "'>";
  html += "<p class='hint'>ID регіону шукайте через запит GET https://api.ukrainealarm.com/api/v3/regions зі своїм токеном (Ctrl+F за назвою району чи громади, поле regionId). Типове значення 152 — Черкаський район. Токен видає UkraineAlarm за запитом.</p>";
  html += "<button type='submit'>Зберегти</button></form>";

  html += "<hr><p><b>Будильники</b></p>";
  html += "<form method='POST' action='/savealarms'>";
  for (int i = 0; i < MAX_ALARMS; i++) {
    Alarm &a = alarms[i];
    String p = "a" + String(i) + "_";
    html += "<div class='alarmcard'>";
    html += "<label class='alarmhead'><input type='checkbox' name='" + p + "en'" + (a.enabled ? " checked" : "") + "> Будильник " + String(i + 1) + "</label>";
    html += "<input type='time' name='" + p + "time' value='" + two(a.hour) + ":" + two(a.minute) + "'>";
    html += "<div class='days'>";
    for (int d = 0; d < 7; d++) {
      bool checked = a.days & (1 << d);
      html += "<label class='daybtn'><input type='checkbox' name='" + p + "d" + String(d) + "'" + (checked ? " checked" : "") + ">" + DAY_LABELS[d] + "</label>";
    }
    html += "</div>";
    html += "<label class='respect'><input type='checkbox' name='" + p + "resp'" + (a.respectAlert ? " checked" : "") + "> Не дзвонити, якщо активна тривога</label>";
    html += "</div>";
  }
  html += "<button type='submit' style='margin-top:14px'>Зберегти будильники</button>";
  html += "</form>";

  html += "<hr><p><b>Нічний режим LED</b></p>";
  html += "<form method='POST' action='/savenight'>";
  html += "<label class='respect'><input type='checkbox' name='nighton'" + String(nightModeEnabled ? " checked" : "") + "> Гасити зелений і синій LED уночі</label>";
  html += "<label>Нічна тиша з (година, 0-23):</label><input type='number' name='nightstart' min='0' max='23' value='" + String(nightStartHour) + "'>";
  html += "<label>До (година, 0-23):</label><input type='number' name='nightend' min='0' max='23' value='" + String(nightEndHour) + "'>";
  html += "<p class='hint'>Діє автоматично щоночі за годинником пристрою (той самий NTP-час, що й для будильників) — вікно можна задати через північ, напр. з 22 до 7. Червоний LED (підтверджена тривога) світить завжди, незалежно від цього налаштування — гасне лише «фонове» зелене/синє світло.</p>";
  html += "<button type='submit'>Зберегти нічний режим</button></form>";

  html += "<hr>";
  html += "<p><b>Пароль адміністратора</b></p>";
  if (passErr) html += "<p class='err'>Паролі не збіглися або закороткі (мінімум 8 символів).</p>";
  html += "<p class='hint'>Цей самий пароль захищає й мережу початкового налаштування «Будильник-Setup» — тому мінімум 8 символів.</p>";
  html += "<form method='POST' action='/setpass'>";
  html += "<label>Новий пароль:</label><input type='password' name='newpass1' minlength='8' required>";
  html += "<label>Повторіть новий пароль:</label><input type='password' name='newpass2' minlength='8' required>";
  html += "<button type='submit'>Змінити пароль</button></form>";

  html += "<hr>";
  html += "<p><a href='/log'>Журнал подій і діагностика</a> (запити API: ✓" + String(fetchOkCount) + " / ✗" + String(fetchFailCount) + ", останній HTTP " + String(lastHttpCode) + ")</p>";
  html += "<form method='POST' action='/resetwifi' onsubmit=\"return confirm('Скинути Wi-Fi, токен і пароль та перезавантажити пристрій?')\">";
  html += "<button type='submit'>Скинути Wi-Fi і токен наново</button></form>";
  html += "<p><a href='/logout'>Вийти</a></p>";
  html += "</body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

// Журнал подій: найновіші зверху. Віддається шматками, без великого String.
void handleLog() {
  if (!isLoggedIn()) { redirectToLogin(); return; }
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");
  server.sendContent("<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'><title>Журнал</title>"
    "<style>body{font-family:monospace;font-size:13px;max-width:900px;margin:16px auto;padding:0 12px}"
    "p{margin:3px 0}.t{color:#777}</style></head><body>"
    "<p><a href='/'>← Назад</a> · <a href='/log'>Оновити</a></p>");
  char line[LOG_LEN + 64];
  snprintf(line, sizeof(line), "<p>Запити до API: успішних %lu, невдалих %lu (найдовша серія %d), останній HTTP %d, вільна купа %u Б, аптайм %lu с</p><hr>",
           fetchOkCount, fetchFailCount, maxFailStreak, lastHttpCode, (unsigned) ESP.getFreeHeap(), millis() / 1000UL);
  server.sendContent(line);
  for (int k = 0; k < logCount; k++) {
    int idx = (logHead - 1 - k + LOG_SIZE) % LOG_SIZE; // від найновішого
    LogEntry &e = logBuf[idx];
    char ts[24];
    if (e.epoch) {
      struct tm t;
      localtime_r(&e.epoch, &t);
      strftime(ts, sizeof(ts), "%d.%m %H:%M:%S", &t);
    } else {
      snprintf(ts, sizeof(ts), "+%lus", e.ms / 1000UL);
    }
    snprintf(line, sizeof(line), "<p><span class='t'>%s</span> %s</p>", ts, e.text);
    server.sendContent(line);
  }
  server.sendContent("</body></html>");
  server.sendContent(""); // завершує chunked-відповідь
}

void handleSave() {
  if (!isLoggedIn()) { redirectToLogin(); return; }
  if (server.hasArg("token")) {
    apiToken = server.arg("token");
    apiToken.trim(); // пробіл після вставки давав тихий HTTP 401
    prefs.putString("token", apiToken);
  }
  if (server.hasArg("uid")) {
    long newUid = server.arg("uid").toInt();
    if (newUid > 0) {
      myUid = newUid;
      prefs.putLong("uid", myUid);
    }
  }
  if (server.hasArg("label")) {
    myLabel = server.arg("label");
    myLabel.trim();
    prefs.putString("label", myLabel);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSaveAlarms() {
  if (!isLoggedIn()) { redirectToLogin(); return; }
  for (int i = 0; i < MAX_ALARMS; i++) {
    String p = "a" + String(i) + "_";
    Alarm &a = alarms[i];
    a.enabled = server.hasArg(p + "en") ? 1 : 0;
    a.respectAlert = server.hasArg(p + "resp") ? 1 : 0;

    String t = server.arg(p + "time"); // формат "HH:MM" з <input type=time>
    int hh = 0, mm = 0;
    int colon = t.indexOf(':');
    if (colon > 0) {
      hh = t.substring(0, colon).toInt();
      mm = t.substring(colon + 1).toInt();
    }
    a.hour = (uint8_t) constrain(hh, 0, 23);
    a.minute = (uint8_t) constrain(mm, 0, 59);

    uint8_t days = 0;
    for (int d = 0; d < 7; d++) {
      if (server.hasArg(p + "d" + String(d))) days |= (1 << d);
    }
    a.days = days;
  }
  prefs.putBytes("alarms", alarms, sizeof(alarms));
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSaveNight() {
  if (!isLoggedIn()) { redirectToLogin(); return; }
  nightModeEnabled = server.hasArg("nighton"); // чекбокс: відсутній в POST, якщо знято галочку
  if (server.hasArg("nightstart")) {
    int v = server.arg("nightstart").toInt();
    if (v >= 0 && v <= 23) nightStartHour = (uint8_t) v;
  }
  if (server.hasArg("nightend")) {
    int v = server.arg("nightend").toInt();
    if (v >= 0 && v <= 23) nightEndHour = (uint8_t) v;
  }
  prefs.putBool("nightOn", nightModeEnabled);
  prefs.putUChar("nightStart", nightStartHour);
  prefs.putUChar("nightEnd", nightEndHour);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetPass() {
  if (!isLoggedIn()) { redirectToLogin(); return; }
  String p1 = server.arg("newpass1");
  String p2 = server.arg("newpass2");
  if (p1.length() >= 8 && p1 == p2) {
    adminPassword = p1;
    prefs.putString("adminpass", adminPassword);
    clearSessionCookie(); // вимагаємо новий вхід уже з новим паролем
    redirectToLogin("?changed=1");
  } else {
    server.sendHeader("Location", "/?passerr=1");
    server.send(303);
  }
}

void handleResetWifi() {
  if (!isLoggedIn()) { redirectToLogin(); return; }
  server.send(200, "text/html; charset=utf-8",
    "<html><body><p>Скидаю Wi-Fi, токен і пароль, пристрій перезавантажиться...</p></body></html>");
  delay(500);
  wm.resetSettings();
  prefs.remove("token");
  prefs.remove("adminpass");
  ESP.restart();
}

// ================== ПЕРЕВІРКА ТРИВОГИ (UkraineAlarm API) ==================

bool fetchAlertState() {
  if (apiToken.length() == 0) {
    lastFetchFailed = true; // токен не введено — запит навіть не сенс робити
    return alertActive;
  }
#if TLS_VERIFY
  if (!timeSynced()) {
    // сертифікат перевіряється за датою: без NTP-часу запит марний, а помилку в журнал не сипемо
    lastFetchFailed = true;
    return alertActive;
  }
#endif

  WiFiClientSecure client;
#if TLS_VERIFY
  client.setCACert(ROOT_CA_PEM);
#else
  client.setInsecure();
#endif
  client.setHandshakeTimeout(HTTP_HANDSHAKE_TIMEOUT_S); // інакше завислий сервер тримав цикл до 120 с і watchdog перезавантажував плату

  HTTPClient https;
  https.setConnectTimeout((uint16_t) HTTP_CONNECT_TIMEOUT_MS); // інакше мертвий API міг «заморожувати» цикл на ~10 с
  https.setTimeout((uint16_t) HTTP_TIMEOUT_MS);
  String url = "https://api.ukrainealarm.com/api/v3/alerts/" + String(myUid);

  bool result = alertActive;
  bool ok = false;
  int code = -100;             // -100 — https.begin() не вдалось
  const char* why = "begin";
  if (https.begin(client, url)) {
    https.addHeader("Authorization", apiToken); // саме так, БЕЗ префікса "Bearer"
    code = https.GET();
    why = "http";
    if (code == 200) {
      why = "структура";
      String payload = https.getString();
      // Відповідь — масив з ОДНИМ об'єктом (бо запитуємо конкретний regionId), приклад:
      // [{"regionId":"152","regionType":"District","regionName":"Черкаський район",
      //   "lastUpdate":"...","activeAlerts":[{"type":"AIR","activeAlertLevels":[...]}]}]
      // Коли тривоги немає — "activeAlerts" порожній масив [].
      // Фільтр лишає тільки activeAlerts[].type: документ маленький і не переповнюється
      // (раніше 2048 Б могло не вистачити -> «стан невідомий» саме під час тривоги).
      StaticJsonDocument<64> filter;
      filter[0]["activeAlerts"][0]["type"] = true;
      StaticJsonDocument<1024> doc;
      DeserializationError jsonErr = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
      if (jsonErr) {
        why = jsonErr.c_str();
      } else if (doc.is<JsonArray>()) {
        bool sawAlertsField = false;
        bool airRaid = false;
        for (JsonObject region : doc.as<JsonArray>()) {
          JsonArray activeAlerts = region["activeAlerts"].as<JsonArray>();
          if (activeAlerts.isNull()) continue;
          sawAlertsField = true;
          for (JsonObject al : activeAlerts) {
            const char* type = al["type"] | "";
            if (strcmp(type, "AIR") == 0) { // саме повітряна тривога
              airRaid = true;
              break;
            }
          }
        }
        // Відповідь без поля activeAlerts — це НЕ «чисто», а невпізнаний формат: збій, а не хибний зелений.
        if (sawAlertsField) {
          result = airRaid;
          ok = true;
        } else {
          why = "немає activeAlerts";
        }
      }
    }
    https.end();
  }
  lastHttpCode = code;
  // Поодинокі збої рахуємо (видно вгорі /log), а в журнал пишемо лише серію від
  // FAIL_LOG_STREAK підряд — інакше 40 записів журналу за 20 хв заповнювались
  // одними «API: ЗБІЙ», і рішення будильників («МОВЧИТЬ»/«ДЗВОНИТЬ») зникали з нього.
  if (ok) {
    if (fetchOkCount == 0 || failStreak >= FAIL_LOG_STREAK)
      logEvent("API: відновлено (HTTP %d) після %d збоїв підряд", code, failStreak);
    fetchOkCount++;
    failStreak = 0;
  } else {
    fetchFailCount++;
    failStreak++;
    if (failStreak > maxFailStreak) maxFailStreak = failStreak;
    if (failStreak == FAIL_LOG_STREAK)
      logEvent("API: %d збоїв підряд, HTTP=%d причина=%s купа=%u", failStreak, code, why, (unsigned) ESP.getFreeHeap());
  }
  lastFetchFailed = !ok;
  if (ok) {
    hadSuccess = true;
    lastSuccessMs = millis();
  }
  return result;
}

// ================== НАЛАШТУВАННЯ WI-FI ПРИ ПЕРШОМУ ЗАПУСКУ ==================

// Викликається WiFiManager, коли пристрій відкриває портал налаштувань
// (немає збережених даних, вони не спрацювали, або натиснуто BOOT ~3с).
// Допомагає зрозуміти через Серійний монітор, що саме відбувається.
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("=== РЕЖИМ НАЛАШТУВАННЯ ===");
  Serial.println("Підключіться Wi-Fi до мережі: " + String(myWiFiManager->getConfigPortalSSID()));
  Serial.println("Пароль до цієї мережі — поточний пароль адміністратора пристрою.");
  Serial.println("IP порталу: " + WiFi.softAPIP().toString() + " (зазвичай сторінка відкриється сама).");
}

// Викликається ОДИН раз при старті. Під час роботи втрату Wi-Fi обробляє loop()
// без блокування (раніше повторний виклик відкривав портал без таймауту і
// «вимикав» будильники, поки хтось не втрутиться).
void setupWifi() {
  setLeds(true, false);

  // довге утримання кнопки BOOT ~3с при СТАРТІ — примусово відкрити портал
  // налаштувань і скинути Wi-Fi, токен ТА пароль адміністратора до типових
  // значень. Це відрізняється від короткого натискання (BOOT або зовнішньої
  // кнопки) під час роботи, яке лише зупиняє дзвінок будильника
  // (див. serviceDismissButtons).
  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  if (digitalRead(PIN_BOOT_BTN) == LOW) {
    delay(3000);
    if (digitalRead(PIN_BOOT_BTN) == LOW) {
      wm.resetSettings();
      prefs.remove("token");
      prefs.remove("adminpass");
      apiToken = "";
      adminPassword = DEFAULT_ADMIN_PASSWORD;
      logEvent("Скидання BOOT: Wi-Fi, токен і пароль стерто");
    }
  }

  // Параметр створюємо один раз і тримаємо в купі: WiFiManager запам'ятовує лише
  // вказівник, а локальна змінна зникала б після виходу з функції.
  if (!tokenParam) {
    tokenParam = new WiFiManagerParameter("token", "Токен API UkraineAlarm", apiToken.c_str(), 60);
    wm.addParameter(tokenParam);
    wm.setAPCallback(configModeCallback);
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S); // без таймауту портал висів би вічно
    wm.setConnectTimeout(20);
  }

  Serial.println("Пробую підключитись до збереженої Wi-Fi мережі (якщо є)...");
  // Мережа порталу налаштувань захищена поточним паролем адміністратора —
  // щоб сторонній поруч не міг підключитись і змінити Wi-Fi/токен пристрою.
  // Wi-Fi (WPA2) вимагає мінімум 8 символів у паролі точки доступу —
  // підстраховка на випадок, якщо десь лишився коротший пароль.
  String apPass = (adminPassword.length() >= 8) ? adminPassword : String(DEFAULT_ADMIN_PASSWORD);
  // Якщо мережу вже збережено — портал НЕ відкриваємо: роутер міг просто ще не
  // піднятись (типово після відключення світла). Раніше тут був цикл «портал 3 хв ->
  // перезавантаження», і поки роутер не працював, будильники не працювали взагалі.
  // Портал відкривається лише при першому налаштуванні або після скидання (BOOT / сторінка).
  bool wifiSaved = wm.getWiFiIsSaved();
  wm.setEnableConfigPortal(!wifiSaved);
  bool connected = wm.autoConnect("Будильник-Setup", apPass.c_str());
  if (!connected) {
    if (!wifiSaved) {
      // перше налаштування, а портал закрився без даних — відкриємо його знову
      Serial.println("Wi-Fi не налаштовано — перезавантаження");
      delay(3000);
      ESP.restart();
    }
    // Мережа збережена, але недоступна — працюємо без неї: будильники йдуть за
    // годинником (він переживає програмне перезавантаження), а loop() сам
    // перепідключиться, щойно роутер з'явиться.
    logEvent("Wi-Fi: немає при старті, працюю без мережі");
    wifiWasDown = true;
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    lastWifiTryMs = millis();
  } else {
    Serial.println("Підключено до Wi-Fi: " + WiFi.SSID());
    Serial.println("IP-адреса пристрою: " + WiFi.localIP().toString());
  }
  WiFi.setAutoReconnect(true);

  // якщо портал відкривався і токен ввели — збережемо
  String enteredToken = tokenParam->getValue();
  enteredToken.trim();
  if (enteredToken.length() > 0) {
    apiToken = enteredToken;
    prefs.putString("token", apiToken);
  }

  setLeds(!connected, alertActive);
}

// Watchdog: якщо цикл завис довше WDT_TIMEOUT_S — плата сама перезавантажиться.
// Вмикається ПІСЛЯ setupWifi(), бо портал налаштувань може працювати довго.
void setupWatchdog() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t cfg = { WDT_TIMEOUT_S * 1000, 0, true };
  esp_task_wdt_reconfigure(&cfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);
}

// ================== SETUP / LOOP ==================

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_DISMISS_BTN, INPUT_PULLUP);

  randomSeed(esp_random());

  prefs.begin("budylnyk", false);
  apiToken      = prefs.getString("token", "");
  myUid         = prefs.getLong("uid", DEFAULT_UID);
  myLabel       = prefs.getString("label", "Черкаський район");
  adminPassword = prefs.getString("adminpass", DEFAULT_ADMIN_PASSWORD);
  nightModeEnabled = prefs.getBool("nightOn", true);
  nightStartHour   = (uint8_t) prefs.getUChar("nightStart", 22);
  nightEndHour     = (uint8_t) prefs.getUChar("nightEnd", 7);

  memset(alarms, 0, sizeof(alarms));
  prefs.getBytes("alarms", alarms, sizeof(alarms)); // якщо ще не збережено — лишаться нулі (усі вимкнені)
  // Стан у RTC-пам'яті довіряємо лише після програмного перезавантаження; після
  // ввімкнення живлення там сміття — починаємо з чистого (ніхто не «чекає на відбій»).
  esp_reset_reason_t rr = esp_reset_reason();
  if (rtcStateMagic != RTC_STATE_MAGIC || rr == ESP_RST_POWERON || rr == ESP_RST_BROWNOUT || rr == ESP_RST_UNKNOWN) {
    rtcStateMagic = RTC_STATE_MAGIC;
    lastCheckedEpochMinute = -1;
    for (int i = 0; i < MAX_ALARMS; i++) {
      alarmPendingSince[i] = 0;
      alarmLastFiredKey[i] = -1;
    }
  } else {
    int pending = 0;
    for (int i = 0; i < MAX_ALARMS; i++) if (alarmPendingSince[i] != 0) pending++;
    logEvent("Перезавантаження (причина %d): стан будильників збережено, чекають на відбій: %d", (int) rr, pending);
  }

  setupWifi();
  setupWatchdog();

  configTzTime(TZ_KYIV, "pool.ntp.org", "time.google.com");

  if (MDNS.begin("budylnyk")) {
    Serial.println("mDNS: http://budylnyk.local");
  }

  const char* headerKeys[] = {"Cookie"};
  server.collectHeaders(headerKeys, 1);

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/savealarms", HTTP_POST, handleSaveAlarms);
  server.on("/savenight", HTTP_POST, handleSaveNight);
  server.on("/setpass", HTTP_POST, handleSetPass);
  server.on("/resetwifi", HTTP_POST, handleResetWifi); // POST: щоб чужа сторінка не могла скинути пристрій посиланням/картинкою
  server.on("/log", HTTP_GET, handleLog);
  server.on("/login", HTTP_GET, handleLoginPage);
  server.on("/login", HTTP_POST, handleLoginSubmit);
  server.on("/logout", HTTP_GET, handleLogout);
  server.begin();
}

void loop() {
  esp_task_wdt_reset();
  server.handleClient();
  serviceRinging();
  serviceDismissButtons();
  checkAlarms();

  unsigned long now = millis();
  bool wifiUp = (WiFi.status() == WL_CONNECTED);
  if (!wifiUp) {
    // Без блокування: цикл (а з ним будильники й кнопки) продовжує працювати,
    // а перепідключення просто підштовхуємо раз на WIFI_RECONNECT_MS.
    if (!wifiWasDown) {
      wifiWasDown = true;
      logEvent("Wi-Fi: втрачено");
      if (!alarmRinging) setLeds(true, false);
    }
    if (now - lastWifiTryMs >= WIFI_RECONNECT_MS) {
      lastWifiTryMs = now;
      WiFi.reconnect();
    }
  } else if (wifiWasDown) {
    wifiWasDown = false;
    logEvent("Wi-Fi: відновлено, IP %s", WiFi.localIP().toString().c_str());
    MDNS.end();
    MDNS.begin("budylnyk");
    lastCheckMs = 0; // одразу оновити стан тривоги
  }

  // Під час дзвінка запит не робимо: він блокує цикл (дзвінок і кнопку) на кілька секунд.
  if (wifiUp && !alarmRinging && (now - lastCheckMs >= CHECK_INTERVAL_MS || lastCheckMs == 0)) {
    lastCheckMs = now;
    bool newState = fetchAlertState();
    lastCheckedAt = currentTimeString();
    // Одна помилкова відповідь «тривоги немає» посеред тривоги не повинна ні
    // будити (дзвінок на відбій), ні скасовувати «мовчання» будильника —
    // відбій зараховуємо після CLEAR_CONFIRM_COUNT успішних відповідей підряд.
    if (!lastFetchFailed) {
      if (alertActive && !newState) {
        clearVotes++;
        if (clearVotes < CLEAR_CONFIRM_COUNT) {
          logEvent("Відбій: чекаю підтвердження (%d/%d)", clearVotes, CLEAR_CONFIRM_COUNT);
          newState = true;
        } else {
          clearVotes = 0;
        }
      } else {
        clearVotes = 0;
      }
    }
    bool changed = firstCheckDone && newState != alertActive;
    if (changed) logEvent("Тривога: %s -> %s", alertActive ? "є" : "немає", newState ? "є" : "немає");
    // Тривога ПІДТВЕРДЖЕНО закінчилась (не здогад — реальна успішна відповідь
    // API), і хоч один будильник чекав на відбій? Дзвонимо по-справжньому, а не
    // тихим сигналом — саме заради цього й чекали. Не лише в момент переходу:
    // якщо плата перезавантажилась під час тривоги, а відбій настав, поки вона
    // стартувала, першої ж успішної відповіді «немає» досить.
    bool wokeUpPending = !lastFetchFailed && !newState && clearPendingAlarms();
    if (wokeUpPending) {
      // Дзвінок будильника на відбій — не звичайний сигнал-сповіщення,
      // а сама функція будильника. Ніч його не приглушує.
      logEvent("Відбій: дзвоню будильник, що чекав на відбій");
      if (!alarmRinging) startRinging();
    } else if (changed && !alarmRinging) {
      // Короткий зумер про зміну стану — той самий "фоновий" сигнал,
      // що й зелений/синій LED, тож уночі (коли LED і так вимкнені)
      // мовчимо так само, як мовчить світлодіод.
      if (!isNightNow()) beepPattern(newState);
    }
    firstCheckDone = true;
    alertActive = newState;
    if (!alarmRinging) setLeds(false, alertActive);
    // Інтервал рахуємо від КІНЦЯ запиту: коли інтернету немає, кожен запит може
    // висіти десятки секунд (DNS), і без цього вони йшли б один за одним без паузи —
    // кнопки й сторінка майже не отримували б часу.
    lastCheckMs = millis();
    if (lastCheckMs == 0) lastCheckMs = 1;
  }

  delay(5); // не крутимо процесор на 100% (менше споживання від акумулятора)
}
