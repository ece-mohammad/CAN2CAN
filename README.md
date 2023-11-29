# CAN2CAN

CAN to CAN communication on `STM32F103`. A simple project using CAN and FreeRTOS to send data between 2 micro-controllers.

```text
    +--------------+           +--------------+    
    |    master    |           |     slave    |
    +--------------+           +--------------+ 
            v                           v
            |                           |
            ^                           ^
    +--------------+            +--------------+
    |  CAN driver  |>----------<|  CAN driver  |
    +--------------+            +--------------+
```

Currently, I have no CAN drivers, so I decided to use 1 controller in loopback mode, and write both the master's and the slave's code on the same controller. it's not exactly the same as using 2 separate controllers, but it's close enough. I also decided to use `FreeRTOS`, cause that allows the code to be separated nicely in 2 different files: `can2can_master` and `can2can_slave`.

```text
    +-----------------------------------+ 
    | +--------------+ +--------------+ |
    | |    master    | |     slave    | |
    | +--------------+ +--------------+ |
    +-----------------------------------+ 
            v                 v
            |                 |
            ^                 ^
            +--- Loopback ----+
```

### Master

Master sends a CAN data frame of `1` byte, either `0x55` or `0xAA` every `1000` milliseconds (`1` second), using standard ID: `0x300`. Then receives messages from the slave. The master uses `FIFO0` to receive messages from the salve.

### Slave

The salve starts in IDLE mode, and waits for a message from the master. After the data message is received, the slave sends `10` messages every `100` milliseconds. The messages are CAN data frames of `2` bytes of data, on standard ID `0x301`. The slave uses `FIFO1` to receive messages from the master.


```
time (ms)    Master              Slave
                |                   |
       0        +------> 0xAA >----->
                <---< 0x01 0x01 <---+
                |                   |
     100        <---< 0x01 0x02 <---+
                |                   |
     200        <---< 0x01 0x03 <---+
                .                   .
                .                   .
                .                   .
     900        <---< 0x01 0x0A <---+
                |                   |
    1000        +------> 0x55 >----->
                <---< 0x00 0x09 <---+
                |                   |
    1100        <---< 0x00 0x08 <---+
                .                   .
                .                   .
                .                   .
    1900        <---< 0x00 0x00 <---+
                |                   |
    2000        +------> 0xAA >----->
                <---< 0x01 0x01 <---+
```

## Build

## Requirements

- GNU Make or CMake + Ninja.
- Any ARM toolchain, I'm using `arm-none-eab-gcc` version 11.3.1.
- [buck50](https://github.com/thanks4opensource/buck50/) + PulseView: for capturing CAN frames.
- `OpenOCD`: for software debugging (optional).
- [stlink tools](https://github.com/stlink-org/stlink) (optional).
- Your preferred IDE, as long as you can set it up for debugging.

### Building Using GNU Make

#### Prerequisites

- Any ARM toolchain.
- GNU Make.

#### Steps:

- get Make version

    ```
    make --version
    ```

    > If that step threw an error, then either Make is not installed or, is not in `PATH`. Install Make and/or make sure it's in `PATH`.

- build project
    ```shell
    make
    ```

- build project and write to flash (only if `stlink tools` is installed and available in `PATH`)
    ```shell
    make flash
    ```

### Building Using CMake

#### prerequisites

- Any ARM toolchain.
- CMake.
- Ninja generator is optional, but preferred.

#### Steps

- get CMake version
    ```
    cmake --version
    ```
    > If that command threw an error, either `CMake` is not installed, or is not in `PATH`. Install CMake and/or make sure it's in `PATH`.

- edit toolchain file `cmake/gcc-arm-none-eabi.cmake`: 
  - set `TOOLCHAIN_PREFIX` to toolchain's prefix. If `arm-none-eabi-gcc` is not in `PATH`, you can set `TOOLCHAIN_PREFIX` to `path/to/gcc/toolchain-prefix-`

- configure project
    ```
    cmake DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=./cmake/gcc-arm-none-eabi.cmake -S . -B ./cmake_build/debug
    ```

  - if `Ninja` is installed:
    ```
    cmake DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=./cmake/gcc-arm-none-eabi.cmake -S . -B ./cmake_build/debug -G Ninja
    ```

- build project:
    ```
    cmake --build ./cmake_build/debug --verbose --target CAN2CAN
    ```
