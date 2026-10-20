Aspia iOS Client
================

Неофициальная сборка клиента [Aspia](https://github.com/dchapyshev/aspia) для iPhone и iPad. С ней можно подключаться к компьютерам с Aspia Host с телефона: напрямую или по ID через Aspia Router и Relay.

## [⬇ Скачать Aspia-iOS.ipa](../../releases/latest/download/Aspia-iOS.ipa)

Ссылка всегда ведёт на самую свежую сборку. Прошлые сборки лежат на странице [Releases](../../releases).

- **Установка:** через [Sideloadly](https://sideloadly.io) с вашим Apple ID. Подробная инструкция на русском — в файле [installer/ios/README.ru.md](installer/ios/README.ru.md).
- **Требования:** iOS 17 или новее.
- **Что работает:** удалённый рабочий стол с сенсорным управлением и панелью спецклавиш, передача файлов, чат, информация о системе (как в Windows-клиенте), звук, буфер обмена, адресная книга с мастер-паролем и Face ID, подключение через Router.
- **Ограничения:** только клиент (управлять самим iPhone нельзя); в свёрнутом виде сессия держится около 30 секунд; с бесплатным Apple ID приложение нужно переустанавливать раз в 7 дней.

Сервер (Host, Router, Relay) и клиенты для других систем — в [основном репозитории Aspia](https://github.com/dchapyshev/aspia/releases).

> Это любительский порт, автор Aspia его не поддерживает. С вопросами по iOS-версии обращайтесь в [Issues](../../issues) этого репозитория.

## Сборка через Xcode

Собрать и поставить приложение на свой iPhone можно и самому, без Sideloadly. Подойдёт бесплатный Apple ID: приложение, подписанное им, работает 7 дней, потом достаточно ещё раз нажать **Run** в Xcode.

### 1. Что понадобится

- Mac на Apple Silicon (M1 и новее) с macOS 14 или новее, около 30 ГБ свободного места.
- [Xcode](https://apps.apple.com/app/xcode/id497799835) 16 или новее. После установки запустите его один раз и согласитесь с лицензией.
- [Homebrew](https://brew.sh) и инструменты из него:

```bash
xcode-select --install
brew install cmake ninja pkg-config autoconf automake libtool python
```

### 2. Скачать исходники

```bash
# Подмодуль vcpkg4aspia прописан по SSH; без SSH-ключа GitHub его можно забрать по HTTPS
git config --global url."https://github.com/".insteadOf git@github.com:

git clone --recursive https://github.com/BomJ-J/aspia-ios-client.git
cd aspia-ios-client
```

### 3. Установить Qt для iOS

Нужен Qt 6.11.1 для iOS и для macOS: из второго берутся утилиты сборки (moc, rcc и другие).

```bash
python3 -m pip install --user aqtinstall
python3 -m aqt install-qt mac ios 6.11.1 ios --autodesktop -O ~/Qt

export QT_IOS_PATH=~/Qt/6.11.1/ios
export QT_HOST_PATH=~/Qt/6.11.1/macos
```

Если `python3 -m pip` откажет из-за «externally-managed-environment», добавьте `--break-system-packages`. Переменные `QT_IOS_PATH` и `QT_HOST_PATH` действуют только в текущем окне Терминала — в новом окне их нужно задать снова.

### 4. Создать проект Xcode

```bash
./vcpkg4aspia/bootstrap-vcpkg.sh -disableMetrics
cmake --preset local-ios -DASPIA_IOS_BUNDLE_ID=com.yourname.aspia
```

Вместо `com.yourname.aspia` впишите свой идентификатор: с бесплатным Apple ID он должен быть уникальным, например `kz.mycompany.aspia`.

**Первый запуск идёт долго — 1–2 часа:** собираются все библиотеки (OpenSSL, protobuf, кодеки и другие). Повторные запуски берут их из кэша и проходят за минуты.

### 5. Собрать и запустить в Xcode

1. Откройте проект: `open builds/local-ios/aspia.xcodeproj`
2. **Xcode → Settings → Accounts** — нажмите **+** и войдите своим Apple ID.
3. Вверху окна выберите схему **aspia_client**, а рядом — свой iPhone, подключённый кабелем. Телефон должен быть разблокирован, на вопрос «Доверять этому компьютеру?» ответьте «Доверять».
4. В списке слева выберите проект **aspia** → цель **aspia_client** → вкладка **Signing & Capabilities** → в поле **Team** выберите себя (*Personal Team*).
5. Нажмите **Run** (▶ или ⌘R). Первая сборка самого приложения занимает 10–20 минут.
6. При первом запуске на iPhone включите **Настройки → Конфиденциальность и безопасность → Режим разработчика** (телефон перезагрузится) и разрешите разработчика: **Настройки → Основные → VPN и управление устройством → ваш Apple ID → Доверять**.

Команда в Терминале с `-sdk iphoneos` и неподписанный IPA для Sideloadly описаны в [installer/ios/README.ru.md](installer/ios/README.ru.md).

### Если что-то пошло не так

- **«No profiles for … were found» или «Failed to register bundle identifier»** — идентификатор занят. Повторите шаг 4 с другим `ASPIA_IOS_BUNDLE_ID` и снова выберите Team.
- **«Untrusted Developer» на iPhone** — см. пункт 6 выше.
- **Ошибка на шаге 4 при сборке библиотеки** — подробный лог лежит в `vcpkg4aspia/buildtrees/<имя библиотеки>/`. Чаще всего не хватает одного из инструментов из шага 1.
- **Xcode пересоздал проект и забыл Team** — после изменений в `CMakeLists.txt` проект генерируется заново. Выберите Team ещё раз или сразу передайте его при создании: `-DASPIA_IOS_TEAM_ID=<ваш Team ID>`. Team ID — это 10 символов. Его видно в **Связке ключей**: откройте сертификат «Apple Development: …», поле **Подразделение организации**.

---

About Aspia
-----------
Remote desktop, file transfer and system information tool.

With Aspia, you can create your own NAT traversal infrastructure (using Router and Relay servers) with connection by ID or use direct connections. Aspia supports many features. Among them, detailed information about the system, terminal, audio, text chat.

Currently supported
-------------------
- Remote desktop management
- File transfer
- Remote terminal
- System information
- Text chat
- Task manager
- Encryption
- Authorization (it is possible to add users with different access rights)
- Address book with encryption and master-password
- <b>NAT traversal with connection by ID</b> (with using Aspia Router and Aspia Relay)
- Direct connections
- Audio support
- Video recording
- Host and Client for Windows, MacOSX, Linux and Android
- Router/Relay for Windows and Linux
- Client for iOS (this repository)

Contacts of the original project
--------------------------------
Website: [aspia.org](https://aspia.org)

E-Mail: dmitry@aspia.ru

Group in Telegram: [@aspia_talks](https://t.me/aspia_talks)

News in Telegram: [@aspia_news](https://t.me/aspia_news)

Licensing
---------
Project code is available under the GNU General Public License 3.

For more information, see [license agreement](LICENSE.md).
