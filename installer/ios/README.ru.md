# Aspia Client для iPhone и iPad

Неофициальная сборка клиента [Aspia](https://github.com/dchapyshev/aspia) для iOS. Позволяет с iPhone или iPad подключаться к компьютерам с Aspia Host: напрямую или через Aspia Router.

Интерфейс тот же, что у Android-клиента: сенсорное управление, панель спецклавиш (Esc, Tab, Enter, Ctrl, Alt, Shift, Win, стрелки, Home/End, PgUp/PgDn, Del), передача файлов, чат, звук, буфер обмена, разблокировка по Face ID.

> Это любительский порт, автор Aspia его не поддерживает. С вопросами по iOS-версии обращайтесь в этот форк, а не в основной репозиторий.

## Ограничения

- **Только клиент.** Управлять самим iPhone нельзя: iOS не даёт приложениям доступ к экрану и вводу.
- **Работа в фоне.** Если свернуть приложение, сессия держится примерно 30 секунд, потом iOS её обрывает.
- **Обновления.** Встроенной проверки обновлений нет — новую версию нужно установить вручную.
- **Подпись на 7 дней.** С бесплатным Apple ID приложение работает 7 дней, потом его надо переустановить (данные сохраняются).
- Нужна iOS 17 или новее.

## Установка

### 1. Скачать IPA

[**Скачать Aspia-iOS.ipa**](../../../../releases/latest/download/Aspia-iOS.ipa) — ссылка всегда ведёт на самую свежую сборку. Прошлые сборки лежат на странице [Releases](../../../../releases).

Файл не подписан и не содержит ничьих личных данных: он собирается на серверах GitHub.

### 2. Установить через Sideloadly

Понадобится компьютер (macOS или Windows) и кабель.

1. Скачайте [Sideloadly](https://sideloadly.io) и подключите iPhone кабелем.
2. Перетащите `Aspia-iOS.ipa` в окно Sideloadly.
3. Введите Apple ID и нажмите **Start**. Sideloadly передаёт пароль только Apple, чтобы получить бесплатный сертификат. Если не хотите вводить пароль от основного аккаунта, заведите отдельный бесплатный Apple ID только для этого.
4. Если Sideloadly скажет, что идентификатор приложения занят, задайте в дополнительных настройках свой Bundle ID, например `com.yourname.aspia`.

### 3. Разрешить запуск на iPhone

1. **Настройки → Конфиденциальность и безопасность → Режим разработчика** — включить, перезагрузить телефон и подтвердить включение.
2. **Настройки → Основные → VPN и управление устройством** — выбрать свой Apple ID и нажать **Доверять**.

### 4. Первый запуск

- Приложение попросит придумать мастер-пароль: им шифруется адресная книга.
- При первом подключении в локальной сети iOS спросит доступ к локальной сети — его нужно разрешить.
- Включить вход по Face ID можно в **Настройки** приложения.

### Через 7 дней

Подключите телефон и повторите шаг 2 с тем же файлом (или с более новым). Настройки и адресная книга сохранятся.

## Где хранятся данные

Адреса роутеров, логины, пароли и адресная книга хранятся только внутри приложения на вашем телефоне, в зашифрованной базе. Они не попадают ни в IPA-файл, ни в этот репозиторий.

Файлы, переданные с компьютера, лежат в приложении **Файлы → На iPhone → Aspia**. Туда же сохраняются логи: **Настройки → О программе → Сохранить логи**.

## Собрать свою версию

### Через GitHub, без Mac

1. Сделайте **Fork** этого репозитория.
2. В своём форке откройте вкладку **Actions** и включите workflows (в форках они по умолчанию выключены).
3. Внесите изменения в ветку `master` и отправьте их (push).
4. GitHub сам соберёт приложение (workflow `.github/workflows/ios.yml`). Первая сборка идёт 1,5–2 часа — с нуля собираются все библиотеки. Следующие быстрее, потому что библиотеки берутся из кэша.
5. Готовый IPA появится в разделе **Releases** вашего форка. Логи сборки (`errors.txt`, `build-tail.log`) лежат в ветке `ci-logs-ios`.

### На своём Mac

Нужны Xcode, CMake 3.30+, Ninja, Python 3 и Git.

```bash
# Подмодуль vcpkg4aspia прописан по SSH; без SSH-ключа GitHub его можно забрать по HTTPS
git config --global url."https://github.com/".insteadOf git@github.com:

git clone --recursive https://github.com/BomJ-J/Aspia-iOS-Client.git
cd Aspia-iOS-Client

# Qt 6.11.1 для iOS вместе с Qt для macOS (из него берутся moc, rcc и прочие утилиты)
python3 -m pip install aqtinstall
python3 -m aqt install-qt mac ios 6.11.1 ios --autodesktop -O ~/Qt
export QT_IOS_PATH=~/Qt/6.11.1/ios
export QT_HOST_PATH=~/Qt/6.11.1/macos

./vcpkg4aspia/bootstrap-vcpkg.sh -disableMetrics
cmake --preset local-ios
cmake --build --preset local-ios-app -- -sdk iphoneos \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY=""
```

Готовое приложение появится здесь: `builds/local-ios/Release/Aspia.app`. Чтобы получить IPA, положите его в папку `Payload` и запакуйте в zip с расширением `.ipa`:

```bash
mkdir -p Payload && cp -R builds/local-ios/Release/Aspia.app Payload/
zip -qry Aspia-unsigned.ipa Payload
```

Сгенерированный проект можно открыть в Xcode: `builds/local-ios/aspia.xcodeproj`. Выберите цель `aspia_client`, свою команду (Personal Team) во вкладке **Signing & Capabilities** — и запускайте прямо на подключённом iPhone.

## Что изменено для iOS

Сенсорный интерфейс общий с Android (`source/client/android`, `source/common/android`). Для iOS заменены только части, которые на Android работают через JNI:

| Что | Файл |
|---|---|
| Сборка приложения, Info.plist, иконка | `source/client/ios/` |
| Face ID и хранение ключа в связке ключей | `source/client/ios/biometric_gate_ios.mm` |
| Отключение жеста «смахнуть клавиатуру» | `source/client/ios/keyboard_gesture_ios.mm` |
| Звук (Audio Queue) | `source/client/audio_output_ios.mm` |
| Удержание сессии при сворачивании | `source/client/session_keeper_ios.mm` |
| Сведения об устройстве | `source/base/sys_info_ios.mm` |
| Сохранение логов в «Файлы» | `source/common/ios/log_archiver_ios.cc` |
| Набор зависимостей vcpkg для iOS | `source/cmake/triplets/arm64-ios-aspia.cmake` |

В CMake добавлены признаки `IOS`, `MACOS` и `MOBILE`. Блоки, которые раньше проверяли `APPLE`, но относятся только к настольной macOS, теперь проверяют `MACOS`.

## Лицензия

Aspia распространяется под [GNU GPL v3](../../LICENSE.md), и эта сборка тоже. Изменённую версию можно свободно раздавать, но вместе с ней должен быть доступен исходный код — например, ссылка на ваш форк.
