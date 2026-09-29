# Карта владения — Game Server

Кто владеет объектами в runtime (и потому определяет их время жизни), а кто лишь
ссылается. После перехода на корутины сессии хранятся **по значению**, а
`shared_ptr` в сетевом слое больше нет (прежний цикл `Server ↔ Session` устранён).

## Легенда

| Обозначение | Смысл |
|-------------|-------|
| сплошная стрелка `-->` | **владеет** (by value / внутри контейнера-владельца) |
| пунктир `-.->` | **ссылка** (не владеет; обязан пережить объект) |

## Граф владения

```mermaid
flowchart TD
    main["main() — стек<br/>(корень всех времён жизни)"]
    cfg["Config (singleton)"]
    io["io_context"]
    q["incoming_msgs<br/>(очередь ClientEvent)"]
    srv["Server (IClientGateway)"]
    world["World"]
    wstate["WorldState<br/>(персонажи, тела Unit, игровые сессии,<br/>территория, снаряды, события)"]
    sindex["SpatialIndex<br/>(живые тела Unit, пересборка каждый тик)"]
    sess["Session<br/>(в sessions_, by value)"]
    sock["websocket::stream"]
    ch["send_queue (канал сессии)"]
    coro["корутины<br/>do_read / do_send / run_session"]

    main -->|by value| io
    main -->|by value| q
    main -->|by value| srv
    main -->|by value| world
    main -.->|ссылка| cfg

    srv -->|by value в map| sess
    sess -->|by value| sock
    sess -->|by value| ch
    srv -.->|ссылка| q
    srv -.->|ссылка| io

    world -->|by value| wstate
    world -->|by value| sindex
    world -.->|ссылка| q
    world -.->|ссылка на IClientGateway| srv

    sess -.->|ссылка, не владеет| srv
    coro -.->|сырой this| sess
```

## Таблица владения

| Объект | Владелец | Чем владеет сам | Когда удаляется |
|--------|----------|-----------------|-----------------|
| **Config** | статический в `get_instance` (singleton) | `net_config` / `game_config` by value | конец программы |
| **incoming_msgs** | `main` (by value) | очередь `ClientEvent` | конец программы |
| **Server** | `main` (by value) | `sessions_`, `acceptor_`, `config_` (копия NetConfig); ссылки на `io_` / `incoming_msgs_` — не владеет | конец программы |
| **Session** | `Server::sessions_[id]` (**by value**) | `socket_`, `send_queue_`; ссылка `server_` — не владеет | `sessions_.erase(id)` в `run_session`, после завершения обеих корутин |
| **World** | `main` (by value) | `config_` (копия GameConfig); `connections_` — `Connection` на каждую открытую сессию (бюджет сообщений, таймауты); `closing_` / `released_` — сессии, которые мир закрывает; `state_` — `WorldState`: `characters` (персонаж: id = `player_id`, имя), `units` (боевые тела: позиция, HP, жизнь, фракция, кулдауны, намерение `Intent`; тело игрока — с id его персонажа, есть от спавна до ухода), `sessions` (`ClientSession` на соединение: какого персонажа ведёт, пер-тик очередь ввода `InputQueue` и состояние доставки клиенту `ClientSync` — последняя сообщённая видимость, флаг resync, тик отказа); сетка территорий, снаряды в полёте, события до снапшота; `alive_index_` — `SpatialIndex` (производные данные, пересобирается каждый тик); `metrics_` — `Metrics` (счётчики и времена тиков текущего периода, обнуляются строкой `metrics` в лог); ссылки на `incoming_msgs_` / `gateway_` — не владеет | конец программы |
| **ClientEvent** | тот, кто держит сейчас (очередь / локальная) | `ClientMessage` by value (+ `session_id`, `kind`) | перемещается по очереди; удаляется после обработки в тике |
| **исходящие байты** (`vector<byte>`) | `send_queue`, затем локально в `do_send` | сам буфер | после `async_write` |
| **game_thread** (`jthread`) | `main` | — (ссылается на `world` через `[&world]`) | выход `main` → stop-token → join |

## Ключевые моменты

1. **Сессии — по значению в `Server::sessions_`.** Никаких `shared_ptr` и прежнего
   цикла `Server ↔ Session`: `Session::server_` — обычная ссылка (Server владеет
   сессией и по построению её переживает). Узлы `unordered_map` не перемещаются,
   поэтому ссылки/указатели на `Session` переживают вставки, удаления других и
   rehash — инвалидирует только удаление именно этого элемента.

2. **Корутины не владеют сессией.** `do_read` / `do_send` / `run_session` держат
   сырой `this`/`Session*`. Безопасность даёт супервизор: `run_session` делает
   `erase` **только** после того, как `co_await (do_read() || do_send())` вернул
   управление (обе корутины завершились). Единственный, кто удаляет сессию, —
   `run_session`.

3. **World и сеть связаны только ссылками.** Очередь `incoming_msgs` принадлежит
   `main`; «шлюз» к клиентам (`IClientGateway`) — это `Server`, а `World` держит
   его по ссылке и сетевыми объектами не владеет. Это и есть шов, за которым сеть
   можно подменить (напр., мок в тестах).

4. **Конфиг копируется, а не заимствуется.** `Server` и `World` хранят *копии*
   своих срезов (`NetConfig` / `GameConfig`) by value, поэтому не зависят от
   времени жизни `Config` (прежней висячей ссылки на `GameConfig` больше нет).

5. **`main` — корень всех времён жизни.** Порядок разрушения важен: сначала
   join'ятся потоки (`io_thread_pool`, затем `game_thread`), потом уничтожаются
   `world`, `server`, `incoming_msgs`, и последним — `io_context`.
