# Диаграммы последовательности — WebSocket Game Server

Документ описывает основные сценарии взаимодействия компонентов сервера в
runtime. Диаграммы выполнены в нотации UML Sequence Diagram с помощью
[Mermaid](https://mermaid.js.org/) (GitHub рендерит их автоматически).

## Архитектура и потоки исполнения

Сервер разделён на две подсистемы, связанные потокобезопасными очередями
(`TSQueue<std::unique_ptr<ServerPacket>>`). Обмен идёт по схеме
producer–consumer между тремя группами потоков:

| Поток | Точка входа | Роль |
|-------|-------------|------|
| **IO-потоки** | `ioc.run()` | Приём соединений, WebSocket-рукопожатие, чтение пакетов от клиентов |
| **Игровой поток** | `World::game_loop()` | Игровой тик: обработка ввода, физика, генерация исходящих пакетов |
| **Sender-потоки** | `Server::sender()` | Диспетчеризация исходящих пакетов по сессиям и запись клиентам |

Поток данных между подсистемами:

```
Клиент ──read──▶ Session ──push──▶ NetworkSubsystem.in_queue
                                           │  ts_swap (в начале тика)
                                           ▼
                                 GameSubsystem.in_queue ──▶ World::process_input
                                                                     │
                                 GameSubsystem.out_queue ◀──── World::update
                                           │  перенос в конце тика
                                           ▼
                          NetworkSubsystem.out_queue ──▶ Server::sender ──write──▶ Клиент
```

> **Примечание.** Модуль авторизации (`db/LoginDataBase`, `SQLConnection`,
> `cryptography/sha256`) присутствует в кодовой базе и покрыт тестами, но пока
> не подключён к основному сетевому/игровому циклу, поэтому в диаграммы ниже
> не включён.

---

## 1. Запуск сервера (bootstrap)

```mermaid
sequenceDiagram
    autonumber
    participant main as main()
    participant cfg as Config
    participant net as NetworkSubsystem
    participant game as GameSubsystem
    participant world as World
    participant srv as Server
    participant ioc as io_context

    main->>cfg: get_instance(argv[1])
    main->>net: make_shared<NetworkSubsystem>()
    main->>game: make_shared<GameSubsystem>()
    main->>world: make_shared<World>(net, game, game_config)
    note over world: строит карту (map_) из config

    main->>ioc: io_context{io_threads}
    main->>srv: make_shared<Server>(ioc, ssl_ctx, net, game)
    main->>srv: start_listen(endpoint)
    srv-->>main: true (сокет слушает)
    main->>srv: run()
    note over srv: acceptor_.async_accept(...)

    main->>world: jthread → game_loop()
    loop net_threads
        main->>srv: jthread → sender()
    end
    loop io_threads
        main->>ioc: jthread → ioc.run()
    end
```

---

## 2. Подключение клиента и WebSocket-рукопожатие

```mermaid
sequenceDiagram
    autonumber
    participant Client
    participant acc as Server::acceptor_
    participant srv as Server
    participant sock as WSSocket
    participant sess as Session
    participant net as NetworkSubsystem.in_queue

    Client->>acc: TCP connect
    acc-->>srv: async_accept → (ec, socket)
    srv->>sock: make_shared<WSSocket>(socket)
    srv->>sess: make_shared<Session>(server, socket, id)
    srv->>sess: run()
    sess->>sock: async_accept(handler)
    Client-->>sock: WebSocket handshake
    sock-->>sess: handler(ec)

    alt рукопожатие успешно
        sess->>sess: set_connected()
        sess->>sess: process_state() [Connected]
        sess->>srv: add_session(self)
        srv->>net: push(ServerPacket{CreatePlayer, id})
        sess->>sess: read_packet_head()
    else ошибка
        sess->>sess: set_disconnecting()
        sess->>sess: process_state() [Disconnecting]
    end

    srv->>acc: run() (готов принять следующего)
```

---

## 3. Приём входящего пакета от клиента

Пакет читается в два этапа: сначала заголовок фиксированного размера
(`PacketHead`), затем — тело переменной длины, если оно есть. Оба чтения
дочитывают буфер до конца (частичные `async_read_some` продолжаются).

```mermaid
sequenceDiagram
    autonumber
    participant Client
    participant sock as WSSocket
    participant sess as Session
    participant ph as PacketHandler
    participant srv as Server
    participant net as NetworkSubsystem.in_queue

    sess->>sock: async_read_some(head_buf, head_size_left)
    Client-->>sock: bytes
    sock-->>sess: handler(ec, size)

    sess->>ph: update_head_size(size)
    alt заголовок ещё не дочитан
        ph-->>sess: false
        sess->>sess: read_packet_head() (продолжить)
    else заголовок получен и валиден
        ph->>ph: is_valid_header(), resize_body(head_size)
        ph-->>sess: true

        alt есть тело (body_size_left > 0)
            sess->>sock: async_read_some(body_buf, body_size_left)
            Client-->>sock: bytes
            sock-->>sess: handler(ec, size)
            sess->>ph: update_body_size(size)
            ph-->>sess: true (тело дочитано)
        end

        sess->>ph: extract_packet()
        ph-->>sess: NetPacket
        sess->>srv: push_packet(ServerPacket{packet, id})
        srv->>net: push(packet)
        sess->>sess: read_packet_head() (следующий пакет)
    end
```

---

## 4. Игровой тик (обработка ввода + физика)

`game_loop()` вызывает `tick(dt)` с частотой `config.tick_rate`. В начале тика
входящая очередь сети «двойной буферизацией» перебрасывается в игровую
подсистему (`ts_swap`), в конце — исходящие пакеты переносятся в сетевую
очередь.

```mermaid
sequenceDiagram
    autonumber
    participant loop as World::game_loop
    participant tick as World::tick
    participant netin as NetworkSubsystem.in_queue
    participant gin as GameSubsystem.in_queue
    participant player as Player
    participant col as Collision
    participant gout as GameSubsystem.out_queue
    participant netout as NetworkSubsystem.out_queue

    loop каждый tick (1/tick_rate сек)
        loop->>tick: tick(dt)
        tick->>netin: ts_swap(gin, netin)
        note over gin,netin: входящие пакеты попадают в игровую очередь

        loop по каждому пакету в gin
            tick->>tick: process_input(packet, dt)
            alt CreatePlayer
                tick->>player: make_shared<Player>(...)
                tick->>tick: add_player(player)
                tick->>gout: push(CreatePlayer)  [Rpc]
                tick->>gout: push(SpawnPlayers)  [Rpc]
                tick->>gout: push(AddPlayer)     [RpcOthers]
            else RemovePlayer
                tick->>tick: remove_player(id)
                tick->>gout: push(RemovePlayer)
            else MoveLeft / MoveRight / Jump
                tick->>player: set_vel(...)
            end
        end

        loop по каждому игроку
            tick->>tick: update(player, dt)
            note over tick: применяет гравитацию
            tick->>col: swept_axis(player, map_, vel)  (X, затем Y)
            col-->>tick: SweptData{entry_time, hit}
            tick->>player: move(...) / set_on_ground(...)
            tick->>gout: push(MovePlayer)  [Broadcast]
        end

        loop по каждому пакету в gout
            tick->>netout: push(packet)
        end
    end
```

---

## 5. Отправка пакетов клиентам (sender-поток)

```mermaid
sequenceDiagram
    autonumber
    participant snd as Server::sender
    participant netout as NetworkSubsystem.out_queue
    participant sessMap as Server::sessions_
    participant sess as Session
    participant sock as WSSocket
    participant Client

    loop бесконечно
        snd->>netout: wait_and_pop()
        netout-->>snd: ServerPacket
        snd->>snd: net_packet.make_buffer()

        alt PacketType::Rpc
            snd->>sess: sessions_[id]->push_to_send(buf)
        else PacketType::Broadcast
            loop по всем сессиям
                snd->>sess: push_to_send(buf)
            end
        else PacketType::RpcOthers
            loop по сессиям, кроме id
                snd->>sess: push_to_send(buf)
            end
        end

        sess->>sess: send()
        note over sess: проверки: connected? / очередь пуста? / уже идёт отправка?
        sess->>sock: async_write(buf)
        sock-->>Client: bytes
        sock-->>sess: handler(ec, size)
        alt успешно и очередь не пуста
            sess->>sess: send() (следующий буфер)
        else ошибка записи
            sess->>sess: set_disconnecting() → process_state()
        end
    end
```

---

## 6. Отключение клиента

Отключение инициируется любой ошибкой ввода-вывода (закрытие сокета клиентом,
ошибка чтения/записи).

```mermaid
sequenceDiagram
    autonumber
    participant sock as WSSocket
    participant sess as Session
    participant srv as Server
    participant net as NetworkSubsystem.in_queue

    sock-->>sess: handler(ec) — ошибка / websocket::error::closed
    sess->>sess: set_disconnecting()
    sess->>sess: process_state() [Disconnecting]
    sess->>sock: close()
    note over sock: cancel() + async_close(normal)
    sess->>srv: close_session(id)
    srv->>srv: sessions_.erase(id)
    srv->>net: push(ServerPacket{RemovePlayer, id})
    sess->>sess: set_disconnected()

    note over net: RemovePlayer будет обработан в ближайшем игровом тике
```

---

## Ключевые компоненты

| Компонент | Файл | Ответственность |
|-----------|------|-----------------|
| `main` | `src/main.cpp` | Инициализация подсистем, запуск потоков |
| `Server` | `src/net/server/server.cpp` | Приём соединений, реестр сессий, `sender()` |
| `Session` | `src/net/session/session.cpp` | Жизненный цикл клиента, чтение/запись пакетов |
| `WSSocket` | `src/net/socket/ws_socket.cpp` | Обёртка над Boost.Beast WebSocket (`WSSSocket` — TLS-вариант) |
| `PacketHandler` | `src/net/session/packet_handler.cpp` | Сборка `NetPacket` из потока байт |
| `NetworkSubsystem` / `GameSubsystem` | `src/shared/subsystems/*.hpp` | Пары очередей `in/out` между сетью и игрой |
| `World` | `src/game/world/world.cpp` | Игровой цикл, физика, управление игроками |
| `Player` / `Tile` / `Collision` | `src/game/**` | Игровые сущности и swept-AABB коллизии |
