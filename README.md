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
