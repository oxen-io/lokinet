# Lokinet

[English](readme.md) [Español](readme_es.md)

Lokinet - реализация LLARP (протокол анонимной маршрутизации с малой задержкой), протокола трёхуровневой луковой маршрутизации.

Почитать о дизайне высокого уровня LLARP [здесь](docs/high-level.txt)

Почитать спецификацию протокола LLARP [здесь](docs/proto_v0.txt)

Почитать документацию о том, как начать работу [здесь](https://oxen-io.github.io/loki-docs/Lokinet/LokinetOverview/)

[![Build Status](https://drone.lokinet.dev/api/badges/oxen-io/lokinet/status.svg?ref=refs/heads/master)](https://drone.lokinet.dev/oxen-io/lokinet)

## Использование

О том как начать работу см. [Документацию](https://oxen-io.github.io/loki-docs/Lokinet/LokinetOverview/)

Также прочтите [Public Testing Guide](https://lokidocs.com/Lokinet/Guides/PublicTestingGuide/#1-lokinet-installation) для установки и другой полезной информации.

### Создание стандартной конфигурации

настроить как клиент:

    $ lokinet -g
    $ lokinet-bootstrap

настроить как транслятор:

    $ lokinet -r -g
    $ lokinet-bootstrap


## Запуск в Linux

** НЕ ЗАПУСКАЙТЕ С ПРАВАМИ СУПЕРПОЛЬЗОВАТЕЛЯ **, запускайте как обычный пользователь. Это требуется для того, чтобы в исполняемом файле были установлены правильные ограничения, установленные командой make install.

для запуска, после создания конфигурации:

    $ lokinet

## Запуск в macOS/UNIX/BSD

** ВЫ ДОЛЖНЫ ЗАПУСКАТЬ С ПРАВАМИ СУПЕРПОЛЬЗОВАТЕЛЯ **, запускайте с помощью `sudo`. Для создания интерфейса виртуального туннеля необходимы повышенные привилегии.

Установщик macOS помещает исполняемые файлы (`lokinet` и` lokinet-bootstrap`) в `/usr/local/bin`, благодаря этому вы можете легко использовать исполняемые файлы в своем терминале. Установщик также уничтожает вашу предыдущую конфигурацию и ключи, устанавливает новую конфигурацию и загружает актуальную версию bootstrap.

для запуска, после создания конфигурации:

    $ sudo lokinet

## Запуск в Windows

** НЕ ЗАПУСКАЙТЕ ОТ ПРИВИЛИГЕРОВАННОГО ПОЛЬЗОВАТЕЛЯ **, запускайте его от имени обычного пользователя.

## Сборка

Требования:

* Git
* CMake
* C++ 17 capable C++ compiler
* libuv >= 1.27.0
* libsodium >= 1.0.18
* libunbound
* libzmq
* sqlite3

### Linux

сборка:

    $ sudo apt install build-essential cmake git libcap-dev curl libuv1-dev libsodium-dev pkg-config
    $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON
    $ make -j$(nproc)

установка:

    $ sudo make install

### macOS

сборка:
    убедитесь, что у вас установлены инструменты командной строки cmake, libuv и xcode

    $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON
    $ make -j$(sysctl -n hw.ncpu)

установка:

    $ sudo make install

### Windows

сборки Windows кросс-скомпилированы из ubuntu linux

дополнительные требования:

* nsis
* cpack

настроить:

    $ sudo apt install build-essential cmake git pkg-config mingw-w64 nsis

сборка:

    $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ mkdir build-windows
    $ cd build-windows
    $ cmake -DBUILD_STATIC_DEPS=ON -DNATIVE_BUILD=OFF -DCMAKE_BUILD_TYPE=Release -DBUILD_PACKAGE=ON -DCMAKE_TOOLCHAIN_FILE='../contrib/cross/mingw64.cmake' -DWITH_TESTS=OFF -DCMAKE_CROSSCOMPILING=ON ..
    $ cpack -D CPACK_MONOLITHIC_INSTALL=1 -G NSIS ..

### Solaris 2.10+

ПРИМЕЧАНИЕ. Пользователи Oracle Solaris должны загрузить/скомпилировать драйвер TAP с http://www.whiteboard.ne.jp/~admin2/tuntap/

Сгенерированные исполняемые файлы _могут_ работать в Solaris 2.10 или более ранней версии, но могут быть не стабильны. (Рекомендуется: `-static-libstdc ++ -static-libgcc` и драйвер TAP, если он еще не установлен в целевой системе.)

Сборка на системе v2.10 или более ранней версии не поддерживается и может даже не работать; в последних выпусках GCC постепенно прекращается поддержка старых выпусков системы.

сборка:

    $ sudo pkg install build-essential gcc8 wget tuntap cmake (optional: ninja ccache - from omnios extra) (OmniOS CE)
    $ sudo pkg install base-developer-utilities developer-gnu developer-studio-utilities gcc-7 wget cmake (Oracle Solaris, see note)
    $ sudo pkg install build-essential wget gcc-8 documentation/tuntap header-tun tun (optional: ninja ccache) (all other SunOS)
    $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make -j$(nproc)

установка:

    $ sudo make install

### FreeBSD

сборка:

    $ pkg install cmake git curl libuv libsodium pkgconf libunbound
    $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ mkdir build
    $ cmake -DCMAKE_BUILD_TYPE=Release ..
    $ make

установка (root):

    # make install
