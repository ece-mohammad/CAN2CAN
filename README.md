# CAN2CAN

CAN to CAN communication on `STM32F103`. A simple project using CAN and FreeRTOS to send data between 2 micro-controllers.

```text

    +--------------+           +--------------+    
    | controller 1 |           | controller 1 |
    +--------------+           +--------------+ 
            v                           v
            |                           |
            ^                           ^
    +--------------+            +--------------+
    |  CAN driver  |>----------<|  CAN driver  |
    +--------------+            +--------------+
```


## Build

## Requirements

- make
- ARM toolchain
- [stlink tools](https://github.com/stlink-org/stlink)

### Build steps

- build project
    ```shell
    make
    ```

- write to flash
    ```shell
    make flash
    ```

## Tasks

```text
+---------------------------+
| Master Task               |
+===========================+
| - Period 1000 ms          |
| - Deadline 10 ms          |
+---------------------------+
| entry:                    |
| - send CAN message        |
|   - ID: 0x007             |
|   - log state             |
+---------------------------+
| events:                   |
| - CAN message received    |
|   - ID: 0x008             |
|   - log message           |
+---------------------------+
+---------------------------+

+---------------------------+
| Slave Task                |
+===========================+
| - Period 10 ms            |
| - Deadline 10 ms          |
+---------------------------+
| entry:                    |
| - update state            |
| - send CAN message        |
|   - ID: 0x008             |
| - log sate                |
+---------------------------+
| events:                   |
| - CAN message received    |
|   - ID: 0x007             |
|   - update state          |
+---------------------------+
+---------------------------+

```