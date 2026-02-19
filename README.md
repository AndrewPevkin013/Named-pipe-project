## IPC Framework (Named Pipes, Windows)

Небольшой IPC- фреймворк для обмена сообщениями между процкссами через **Windows Named Pipes**.

Поддерживает фрагментацию соощений, сборку на стороне сервера и подтверждение доставки (ACK).

### Архитектура

Фреймворк состоит из двух основных компонентов:

- IPCSender - клиент (отправка сообщений)
- IPCReceiver - сервер (прием и обработка сообщений)

Дополнительно используются:

- **MessageFragmenter** - разбивает сообщение на фрагменты
- **MessageAssembler** - собирает сообщение из фрагментов
- ACK-механизм подтверждения доставки

### Пример использования сервера (Receiver)

##### Подключение:

```
#include "ipc_receiver.hpp"

int main() {
    IPCReceiver receiver;
    receiver.run();
    return 0;
}
```

##### Основной метод

**void run()**

- Создает именнованный канал
- Ожидает подключения клиентов
- Для каждого клиента запускае отдельный поток
- При колучении полного сообщения отправляет ACK

Серве автоматичекси:

- собирает фрагменты
- логирует подключение и полученные сообщения
- отправляет подтверждение доставки


### Пример использования клиента (Sender)

##### Подключение:

Пример 1:

```
#include "ipc_sender.hpp"

int main() {
    IPCSender sender;
    sender.send("Hello world");
    #ifdef _WIN32
        system("pause");
    #endif
    return 0;
}

```

Пример 2:

```
#include "ipc_sender.hpp"
#include <iostream>
#include <string>

int main() {
    std::cout << "Type ':q' to quit\n";

    IPCSender sender;

    std::string msg;

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, msg))
            break;

        if (msg == ":q")
            break;

        if (sender.send(msg)) {
            std::cout << "[ACK] Message delivered\n";
        } else {
            std::cerr << "[Client] Send failed\n";
        }
    }

    return 0;
}
```

##### Доступные методы

**IPCSender()**

- Подключается к именованному каналу

**void send(const std::string& message)** - выполняет полный цикл отправки:

- Формирует сообщение
- Делит его на фрагменты
- Отправляет каждый фрагмент
- Ожидает ACK от сервера

### Пример работы

1. Запускаем сервер
2. Запускаем клиент
3. Клиент отправляет сообщение
4. Сервер собирает фрагменты
5. Сервер отправляет ACK
6. Клиент подтверждает успешую доставку


### Сборка
```
mkdir build (если еще не создано)
cd build
cmake ..
cmake --build .
```