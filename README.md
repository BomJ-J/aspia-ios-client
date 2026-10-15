Aspia iOS Client
================

Неофициальная сборка клиента [Aspia](https://github.com/dchapyshev/aspia) для iPhone и iPad. С ней можно подключаться к компьютерам с Aspia Host с телефона: напрямую или по ID через Aspia Router и Relay.

## [⬇ Скачать Aspia-iOS.ipa](../../releases/latest/download/Aspia-iOS.ipa)

Ссылка всегда ведёт на самую свежую сборку. Прошлые сборки лежат на странице [Releases](../../releases).

- **Установка:** через [Sideloadly](https://sideloadly.io) с вашим Apple ID. Подробная инструкция на русском — в файле [installer/ios/README.ru.md](installer/ios/README.ru.md).
- **Требования:** iOS 17 или новее.
- **Что работает:** удалённый рабочий стол с сенсорным управлением и панелью спецклавиш, передача файлов, чат, звук, буфер обмена, адресная книга с мастер-паролем и Face ID, подключение через Router.
- **Ограничения:** только клиент (управлять самим iPhone нельзя); в свёрнутом виде сессия держится около 30 секунд; с бесплатным Apple ID приложение нужно переустанавливать раз в 7 дней.

Сервер (Host, Router, Relay) и клиенты для других систем — в [основном репозитории Aspia](https://github.com/dchapyshev/aspia/releases).

> Это любительский порт, автор Aspia его не поддерживает. С вопросами по iOS-версии обращайтесь в [Issues](../../issues) этого репозитория.

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
