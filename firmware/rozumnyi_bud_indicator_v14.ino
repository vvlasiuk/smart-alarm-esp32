/*
  РОЗУМНИЙ ІНДИКАТОР ТРИВОГИ + БУДИЛЬНИКИ — ESP32, версія 14
  ---------------------------------------------------------------------------
  Ця версія ЗАМІНЮЄ всі попередні файли. Зміни порівняно з v13:

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
         (respectAlert І statusKnown() І alertActive), він позначається
         як «очікує на відбій» (alarmPendingClear[i] = true).
       - Щойно надходить ПІДТВЕРДЖЕНЕ (не «здогад», а реальна успішна
         відповідь API) повідомлення, що тривоги більше немає, і є хоч
         один будильник, що чекав на відбій — пристрій ПОВНОЦІННО дзвонить
         (не тихий сигнал-нагадування, а справжній дзвінок із зумером і
         блиманням, як завжди — щоб дійсно розбудити), і знімає позначку
         з усіх будильників, що чекали.
       - Якщо тривога тривала кілька днів — пристрій так само чекатиме,
         скільки потрібно, і задзвонить одразу по відбою.
       - Обмеження: позначка «очікує на відбій» живе лише в оперативній
         пам'яті (не в NVS) — якщо пристрій перезавантажиться під час
         очікування (наприклад, зникло й повернулось живлення), позначку
         буде втрачено, і дзвінка на відбій для цього конкретного випадку
         не станеться (наступного разу спрацює як завжди за розкладом).

  3) Третій, чесний стан індикатора — «не впевнені» (синій, окрім нічних
     годин — див. п.1), коли понад 2 хвилини (STALE_MS) немає жодного
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
     ТІЛЬКИ якщо ми точно (statusKnown() і свіжі дані) знаємо, що тривога
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
    - ArduinoJson (автор Benoit Blanchon, версія 6.x)
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

// ================== ПІНИ (під типовий ESP32 DevKit) ==================
const int PIN_LED_GREEN   = 27;
const int PIN_LED_RED     = 26;
const int PIN_LED_BLUE    = 14;
const int PIN_BUZZER      = 32;   // на цій платі GPIO25 не працює як вихід (перевірено тестовим LED) — сигнал іде через транзистор 2N2222
const int PIN_BOOT_BTN    = 0;    // штатна кнопка BOOT на платі (дублює зупинку дзвінка)
const int PIN_DISMISS_BTN = 4;    // зовнішня кнопка «Зупинити будильник» — GPIO4 до GND

const unsigned long CHECK_INTERVAL_MS = 20000; // опитування UkraineAlarm раз на 20 секунд
const unsigned long STALE_MS = 120000UL; // 2 хв без УСПІШНОГО запиту — вважаємо стан тривоги невідомим (LED синій)

const char* DEFAULT_ADMIN_PASSWORD = "12345678";
const long  DEFAULT_UID = 152; // Черкаський район — regionId у довіднику UkraineAlarm (api/v3/regions)
const char* TZ_KYIV = "EET-2EEST,M3.5.0/3,M10.5.0/4"; // Київський час, з переходом на літній/зимовий

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
bool alarmPendingClear[MAX_ALARMS]; // будильник i пропущено через тривогу — задзвонити одразу по відбою
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
bool bootWasPressed = false;
bool dismissWasPressed = false;
long lastCheckedEpochMinute = -1;

WebServer server(80);
WiFiManager wm;

// ---------------------------------------------------------------------

String two(int v) {
  return (v < 10 ? "0" : "") + String(v);
}

// Поточний час пристрою (Київ, з NTP). Повертає текст-пояснення, якщо
// час іще не синхронізувався (наприклад, щойно ввімкнули без інтернету).
String currentTimeString() {
  struct tm t;
  if (!getLocalTime(&t, 50)) return "ще не синхронізовано";
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
// statusKnown(), тут є запас у STALE_MS — щоб один невдалий 20-секундний
// опит не змушував LED смикатись синім щохвилини. Впливає ЛИШЕ на колір
// індикатора, не на логіку будильників.
bool dataStale() {
  if (apiToken.length() == 0) return true;
  if (!hadSuccess) return true;
  return (millis() - lastSuccessMs) > STALE_MS;
}

// Чи зараз («за годинником» NTP) triває вікно нічної тиші для LED.
// Коректно рахує вікна, що перетинають північ (напр. 22 -> 7).
// Якщо час ще не синхронізовано або нічний режим вимкнено — вважаємо,
// що зараз НЕ ніч (безпечніше показати LED, ніж помилково згасити).
bool isNightNow() {
  if (!nightModeEnabled) return false;
  if (nightStartHour == nightEndHour) return false; // вікно нульової довжини — вважаємо вимкненим
  struct tm t;
  if (!getLocalTime(&t, 50)) return false;
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

void startRinging() {
  alarmRinging = true;
  ringStartMs = millis();
  lastRingToggleMs = 0;
  ringBuzzerState = false;
}

void stopRinging() {
  alarmRinging = false;
  digitalWrite(PIN_BUZZER, LOW);
  setLeds(false, alertActive);
}

void serviceRinging() {
  if (!alarmRinging) return;
  unsigned long now = millis();
  if (now - ringStartMs >= RING_TIMEOUT_MS) {
    stopRinging();
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
void serviceDismissButtons() {
  bool bootPressedNow = (digitalRead(PIN_BOOT_BTN) == LOW);
  bool dismissPressedNow = (digitalRead(PIN_DISMISS_BTN) == LOW);

  bool bootEdge = bootPressedNow && !bootWasPressed;
  bool dismissEdge = dismissPressedNow && !dismissWasPressed;

  if ((bootEdge || dismissEdge) && alarmRinging) {
    stopRinging();
  }

  bootWasPressed = bootPressedNow;
  dismissWasPressed = dismissPressedNow;
}

void checkAlarms() {
  time_t nowEpoch = time(nullptr);
  long nowMinute = nowEpoch / 60;
  if (nowMinute == lastCheckedEpochMinute) return; // цю хвилину вже перевіряли
  lastCheckedEpochMinute = nowMinute;

  struct tm t;
  if (!getLocalTime(&t, 50)) return; // час ще не синхронізовано по NTP

  for (int i = 0; i < MAX_ALARMS; i++) {
    Alarm &a = alarms[i];
    if (!a.enabled) continue;
    if (!(a.days & (1 << t.tm_wday))) continue;
    if (a.hour != t.tm_hour || a.minute != t.tm_min) continue;

    if (a.respectAlert && statusKnown() && alertActive) {
      // мовчить ЛИШЕ якщо ми точно знаємо, що тривога активна — і
      // запам'ятовуємо, що саме ЦЕЙ будильник чекає на відбій (7.4/7.5)
      alarmPendingClear[i] = true;
      continue;
    }
    // Якщо стан тривоги невідомий (немає токена чи зв'язку з API) —
    // будильник дзвонить, навіть із увімкненим «Не дзвонити...»: краще
    // розбудити зайвий раз, ніж мовчати на основі даних, яких нема.
    startRinging();
  }
}

// Знімає позначку «очікує на відбій» з усіх будильників і повідомляє,
// чи хоч один такий був. Викликається лише коли ПІДТВЕРДЖЕНО (успішним
// запитом), що тривога щойно закінчилась — див. loop().
bool clearPendingAlarms() {
  bool any = false;
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarmPendingClear[i]) {
      any = true;
      alarmPendingClear[i] = false;
    }
  }
  return any;
}

// ================== АВТЕНТИФІКАЦІЯ (проста, на cookie) ==================

bool isLoggedIn() {
  if (sessionToken == "") return false;
  if (!server.hasHeader("Cookie")) return false;
  String cookie = server.header("Cookie");
  return cookie.indexOf("session=" + sessionToken) != -1;
}

void redirectToLogin(const char* suffix = "") {
  server.sendHeader("Location", String("/login") + suffix);
  server.send(303);
}

void clearSessionCookie() {
  sessionToken = "";
  server.sendHeader("Set-Cookie", "session=; Path=/; Max-Age=0");
}

void handleLoginPage() {
  bool err = server.hasArg("err");
  bool changed = server.hasArg("changed");

  String html = "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Вхід — Розумний індикатор</title>";
  html += "<style>body{font-family:sans-serif;max-width:360px;margin:60px auto;padding:0 16px;}";
  html += "h1{font-size:20px} input,button{font-size:16px;padding:8px;margin-top:6px;width:100%;box-sizing:border-box}";
  html += ".err{color:#c1442c} .ok{color:#2e8b57} .hint{color:#777;font-size:13px;margin-top:20px}</style></head><body>";
  html += "<h1>Розумний індикатор тривоги</h1>";
  if (err) html += "<p class='err'>Невірний пароль.</p>";
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
  String pass = server.arg("password");
  if (pass.length() > 0 && pass == adminPassword) {
    sessionToken = String(millis()) + "-" + String(random(100000000, 999999999));
    server.sendHeader("Set-Cookie", "session=" + sessionToken + "; Path=/");
    server.sendHeader("Location", "/");
    server.send(303);
  } else {
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
    statusText = "СТАН НЕВІДОМИЙ (немає свіжих даних)";
    statusColor = "#3a6ea5";
  } else if (alertActive) {
    statusText = "ТРИВОГА";
    statusColor = "#c1442c";
  } else {
    statusText = "Чисто";
    statusColor = "#2e8b57";
  }

  String html = "<!doctype html><html><head><meta charset='utf-8'>";
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
  html += "<p>Стан: <span class='badge' style='background:" + statusColor + "'>" + statusText + "</span></p>";
  html += "<p>ID регіону: <b>" + String(myUid) + "</b>" + (myLabel.length() ? " (" + myLabel + ")" : "") + "<br>Остання перевірка: " + lastCheckedAt + "</p>";
  html += "<p>Поточний час пристрою: <b>" + currentTimeString() + "</b></p>";

  html += "<form method='POST' action='/save'>";
  html += "<label>Токен API UkraineAlarm:</label><input type='text' name='token' value='" + apiToken + "'>";
  html += "<label>ID регіону (regionId UkraineAlarm):</label><input type='number' name='uid' value='" + String(myUid) + "' required>";
  html += "<label>Назва (за бажанням, лише для відображення):</label><input type='text' name='label' value='" + myLabel + "'>";
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
  html += "<p><a href='/resetwifi'>Скинути Wi-Fi і токен наново</a></p>";
  html += "<p><a href='/logout'>Вийти</a></p>";
  html += "</body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

void handleSave() {
  if (!isLoggedIn()) { redirectToLogin(); return; }
  if (server.hasArg("token")) {
    apiToken = server.arg("token");
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

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = "https://api.ukrainealarm.com/api/v3/alerts/" + String(myUid);

  bool result = alertActive;
  bool ok = false;
  if (https.begin(client, url)) {
    https.addHeader("Authorization", apiToken); // саме так, БЕЗ префікса "Bearer"
    int code = https.GET();
    if (code == 200) {
      String payload = https.getString();
      // Відповідь — масив з ОДНИМ об'єктом (бо запитуємо конкретний regionId), приклад:
      // [{"regionId":"152","regionType":"District","regionName":"Черкаський район",
      //   "lastUpdate":"...","activeAlerts":[{"type":"AIR","activeAlertLevels":[...]}]}]
      // Коли тривоги немає — "activeAlerts" порожній масив [].
      // Перевірено наживо реальним запитом до api.ukrainealarm.com.
      StaticJsonDocument<2048> doc;
      DeserializationError jsonErr = deserializeJson(doc, payload);
      if (!jsonErr && doc.is<JsonArray>() && doc.size() > 0) {
        JsonArray activeAlerts = doc[0]["activeAlerts"].as<JsonArray>();
        bool airRaid = false;
        for (JsonObject a : activeAlerts) {
          const char* type = a["type"] | "";
          if (strcmp(type, "AIR") == 0) { // саме повітряна тривога
            airRaid = true;
            break;
          }
        }
        result = airRaid;
        ok = true;
      }
    }
    https.end();
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

void setupWifi() {
  setLeds(true, false);

  WiFiManagerParameter custom_token("token", "Токен API UkraineAlarm", apiToken.c_str(), 60);
  wm.addParameter(&custom_token);
  wm.setAPCallback(configModeCallback);

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
      prefs.remove("adminpass");
      adminPassword = DEFAULT_ADMIN_PASSWORD;
    }
  }

  Serial.println("Пробую підключитись до збереженої Wi-Fi мережі (якщо є)...");
  // Мережа порталу налаштувань захищена поточним паролем адміністратора —
  // щоб сторонній поруч не міг підключитись і змінити Wi-Fi/токен пристрою.
  // Wi-Fi (WPA2) вимагає мінімум 8 символів у паролі точки доступу —
  // підстраховка на випадок, якщо десь лишився коротший пароль.
  String apPass = (adminPassword.length() >= 8) ? adminPassword : String(DEFAULT_ADMIN_PASSWORD);
  bool connected = wm.autoConnect("Будильник-Setup", apPass.c_str());
  if (!connected) {
    Serial.println("Не вдалось підключитись — перезавантаження");
    delay(3000);
    ESP.restart();
  }

  Serial.println("Підключено до Wi-Fi: " + WiFi.SSID());
  Serial.println("IP-адреса пристрою: " + WiFi.localIP().toString());

  // якщо портал відкривався і токен ввели — збережемо
  String enteredToken = custom_token.getValue();
  if (enteredToken.length() > 0) {
    apiToken = enteredToken;
    prefs.putString("token", apiToken);
  }

  setLeds(false, alertActive);
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

  randomSeed(micros());

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
  memset(alarmPendingClear, 0, sizeof(alarmPendingClear)); // на старті жоден будильник не «чекає на відбій»

  setupWifi();

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
  server.on("/resetwifi", handleResetWifi);
  server.on("/login", HTTP_GET, handleLoginPage);
  server.on("/login", HTTP_POST, handleLoginSubmit);
  server.on("/logout", HTTP_GET, handleLogout);
  server.begin();
}

void loop() {
  server.handleClient();
  serviceRinging();
  serviceDismissButtons();
  checkAlarms();

  if (WiFi.status() != WL_CONNECTED) {
    setupWifi();
  }

  unsigned long now = millis();
  if (now - lastCheckMs >= CHECK_INTERVAL_MS || lastCheckMs == 0) {
    lastCheckMs = now;
    bool newState = fetchAlertState();
    lastCheckedAt = currentTimeString();

    if (firstCheckDone && newState != alertActive) {
      // Тривога щойно ПІДТВЕРДЖЕНО закінчилась (не здогад — реальна успішна
      // відповідь API), і хоч один будильник чекав на відбій? Дзвонимо
      // по-справжньому, а не тихим сигналом — саме заради цього й чекали.
      bool wokeUpPending = (newState == false) && clearPendingAlarms();
      if (wokeUpPending) {
        // Дзвінок будильника на відбій — не звичайний сигнал-сповіщення,
        // а сама функція будильника. Ніч його не приглушує.
        if (!alarmRinging) startRinging();
      } else if (!alarmRinging) {
        // Короткий зумер про зміну стану — той самий "фоновий" сигнал,
        // що й зелений/синій LED, тож уночі (коли LED і так вимкнені)
        // мовчимо так само, як мовчить світлодіод.
        if (!isNightNow()) beepPattern(newState);
      }
    }
    firstCheckDone = true;
    alertActive = newState;
    if (!alarmRinging) setLeds(false, alertActive);
  }
}
