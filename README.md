# ehttpd

ehttpd is a simple http server runs on ESP32 device. It only uses about 20K static
allocated memory and [Ragel](http://www.colm.net/open-source/ragel) generated state machine for URL routing.
[picohttpparser](https://github.com/h2o/picohttpparser) is used for HTTP header parsing. [uriparser](https://github.com/anatol/uriparser) for http request path parsing. it also can be compiled to a host side simple http server for testing
purpose.

## Build

### Setup ESP32-IDF environment
It uses ESP32-IDF to build and flash, please follow [ESP-IDF Programming Guide](http://esp-idf.readthedocs.io/en/latest/get-started/index.html) to setup toolchain and build environment, make sure you can build hello world project by following [Start a Project](http://esp-idf.readthedocs.io/en/latest/get-started/index.html#start-a-project)

### Install prerequisites
[Ragel](http://www.colm.net/open-source/ragel) is used to auto-generate C files, please install it

    sudo apt install ragel

or build from scratch

    wget http://www.colm.net/files/ragel/ragel-6.10.tar.gz
    tar xpf ragel-6.10.tar.gz
    ./configure --prefix=/usr/local
    make
    sudo make install

### build ehttpd
get the code:

    git clone https://github.com/DavidPu/ehttpd.git $IDF_PATH/ehttpd

config the ehttpd:

    cd $IDF_PATH/ehttpd
    make menuconfig
    # select your ESP32 USB serial port. make sure it can be read/write without sudo permission
    # Serial flasher config
        # (/dev/ttyUSB0) default serial port
        # Flash size ---> 4M
    # Example Configuration
      # setup your WIFI SSID and WiFIi password ESP32 board will connect to.

build and flash basic hello world echo server:

    cd $IDF_PATH/ehttpd
    make ehttpd_gen_c flash

ehttpd includes a [Makefile.blockly.spiffs](../blob/master/Makefile.blockly.spiffs) Makefile to build a complete Google [Blockly-Games/maze](http://blockly-games.appspot.com/maze) web application and run on ESP-32.
Run beow commands to build all related images and flash to ESP32 board:

    sudo apt install zopfli
    make -f Makefile.blockly.spiffs
