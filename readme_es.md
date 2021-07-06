# Lokinet

[Ingles](readme.md)

Lokinet es la implementación referente de LLARP (low latency anonymous routing protocol, protocolo de enrutado anónimo de baja latencia), un protocolo de enrutado onion de capa 3.

Puede aprender a grandes razgos sobre el diseño de LLARP [aquí](docs/high-level.txt) , documento en idioma ingles.

Y puede leer las especificaciones del protocolo [aquí](docs/proto_v0.txt) , documento técnico en idioma ingles.

Puede ver la documentación, en ingles, de como empezar [aqui](https://oxen-io.github.io/loki-docs/Lokinet/LokinetOverview/) .

[![Build Status](https://ci.oxen.rocks/api/badges/oxen-io/lokinet/status.svg?ref=refs/heads/dev)](https://ci.oxen.rocks/oxen-io/lokinet)


## Uso

Vea, en ingles, [documentación](https://oxen-io.github.io/loki-docs/Lokinet/LokinetOverview/) en como comenzar.

También lea, en ingles, [La guia de pruebas publicas](https://lokidocs.com/Lokinet/Guides/PublicTestingGuide/#1-lokinet-installation) para la instalación y mas información util.

## Corriendo en Linux

**NO CORRER COMO ROOT**, correr como usuario normal. Esto requiere que el binario tenga asignado por `make install` los setcaps apropiados el binario.

para ejecutar como cliente:

    $ lokinet -g
    $ lokinet-bootstrap
    $ lokinet

para correr un relay:

    $ lokinet -r -g
    $ lokinet-bootstrap
    $ lokinet

## Corriendo en macOS/UNIX/BSD

**USTED TIENE QUE CORRER COMO ROOT**, correr usando sudo. Los privilegios elevados son necesarios para crear una interfaz de tunel virtual.

El instalador de macOS coloca los binarios normales (`lokinet` y `lokinet-bootstrap`) en `/usr/local/bin` los que pudieran estar en ruta, asi que usted puede usar los binarios facilmente desde su terminal. El instalador tambien truena sus configuraciones y llaves previas, y descarga la semilla de arranque mas actual.

para correr como cliente:

    $ lokinet -g
    $ lokinet-bootstrap
    $ sudo lokinet

para correr como relay:

    $ lokinet -r -g
    $ lokinet-bootstrap
    $ sudo lokinet


## Corriendo en Windows:

**NO CORRER COMO USUARIO ELEVADO**, correr como un usuario normal.

para correr como usuario, correr el archivo en lote `run-lokinet.bat` como su usuario normal.


## Compilando

Requerimientos de compilación:

* GNU Make
* CMake
* Compilador C++ que pueda usar C++ 17
* gcovr (para generar la covertura de prueba en gcc)
* libuv >= 1.27.0
* libsodium >= 1.0.18
* libunbound
* libzmq
* sqlite3

### Linux

compilando:

    $ sudo apt install build-essential cmake git libcap-dev curl libuv1-dev libsodium-dev pkg-config
    $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON
    $ make

instalando:

    $ sudo make install


como alternativa hacer un paquete debian con:

    $ debuild -uc -us -b

esto coloca el paquete compilado en `../`

### macOS

compilando:
    este seguro que usted tiene cmake, libuv y las herramientas de terminal de xcode ya instaladas

     $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON
    $ make -j$(sysctl -n hw.ncpu)

instalando:

    $ sudo make install

### Windows

compilar (donde `$ARCH` es su plataforma - `i686` or `x86_64`):

    $ pacman -Sy base-devel mingw-w64-$ARCH-toolchain git libtool autoconf mingw-w64-$ARCH-cmake
    $ git clone https://github.com/oxen-io/lokinet.git
    $ cd lokinet
    $ mkdir -p build; cd build
    $ cmake .. -DCMAKE_BUILD_TYPE=[Debug|Release] -DSTATIC_LINK_RUNTIME=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -G 'Unix Makefiles'

instalando (con priviligios elevados) en `$PROGRAMFILES/lokinet` o `$ProgramFiles(x86)/lokinet`:

    $ make install

si usa compilacion cruzada (cross-compiling), instale mingw-w64 desde su administrador de paquetes, o [compile desde el codigo fuente](https://sourceforge.net/p/mingw-w64/wiki2/Cross%20Win32%20and%20Win64%20compiler/), then:

    $ mkdir -p build; cd build
    $ export COMPILER=clang # si esta usando clang para windows
    $ cmake .. -DCMAKE_BUILD_TYPE=[Debug|Release] -DSTATIC_LINK_RUNTIME=ON -DCMAKE_CROSSCOMPILING=ON -DCMAKE_TOOLCHAIN_FILE=../contrib/cross/mingw[32].cmake

esto crea un binario que puede ser instalado en cualquier parte, con ninguna otra dependencia aparte de libc (v6.1 minimo)

### Solaris 2.10+

NOTA: Los usuarios de Solaris de Oracle necesitan descargar/compilar el controlador TAP de http://www.whiteboard.ne.jp/~admin2/tuntap/

Los binarios generados _podrían_ funcionar en Solaris 2.10 o anteriores, va por su cuenta. (Recomendable: `-static-libstdc++ -static-libgcc`, y el controlador TAP si aun no esta instalado en el sistema de destino.)

Compilar en un sistema anterior a v2.10 en sistemas previos no esta soportado, e incluso puede que no funcione, lanzamientos recientes de GCC han estado desechando el soporte en lanzamientos mas viejos del sistema.

compilando:

    $ sudo pkg install build-essential gcc8 wget tuntap cmake (opcional: ninja ccache - de los extra de omnios) (OmniOS CE)
    $ sudo pkg install base-developer-utilities developer-gnu developer-studio-utilities gcc-7 wget cmake (Solaris de Oracle, ver notas)
    $ sudo pkg install build-essential wget gcc-8 documentation/tuntap header-tun tun (opcional: ninja ccache) (todos los demas SunOS)
    $ git clone https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ gmake -j8

instalando:

    $ sudo make install


### NetBSD (y otras plataformas donde pkgsrc es _el_ mgr nativo de paquetes)

PENDIENTE: agregar instrucciones para pkgsrc

### OpenBSD (usa el administrador pkg legado de NetBSD)

compilando:

    # pkg_add curl cmake git (opcional: ninja ccache)
    $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON
    $ make

instalando (root):

    # gmake install

### FreeBSD

compilando:

    $ pkg install cmake git curl libuv-1.27.0 libsodium
    $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON
    $ make

instalando (root):

    # gmake install
