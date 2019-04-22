PiWx: Raspberry Pi Aviation Weather
===================================

PiWx is a configurable aviation METAR display designed to work with the Adafruit
320x240 PiTFT framebuffer display. While PiWx is hard-coded to work with the
PiTFT, it should not be difficult to modify it for any display that uses a
framebuffer device.

For more information about the PiTFT: https://www.adafruit.com/product/2298.

The amazing weather icons are from the Weather Underground Icons project:
https://github.com/manifestinteractive/weather-underground-icons

Building
--------

PiWx uses CMake 3.6 to build. The following dependencies must be installed:

  * Bison (https://www.gnu.org/software/bison/)
  * Flex (https://github.com/westes/flex)
  * LibXml2 (http://www.xmlsoft.org/)
  * WiringPi (http://wiringpi.com/)
  * libcURL (https://curl.haxx.se/libcurl/)
  * libpng (http://www.libpng.org/pub/png/libpng.html)
  * jansson (http://www.digip.org/jansson/)

After cloning the repository, use the following commands to perform a simple
build:

    % git clone https://github.com/slakpi/piwx.git
    % cd piwx
    % mkdir build
    % cd build
    % cmake -DCMAKE_INSTALL_PREFIX="/usr/local/piwx" -DCMAKE_BUILD_TYPE=Release ..
    % make
    % sudo make install

Configuration
-------------

Configuration is relatively simple. Refer to the sample configuration file that
installs to the `etc` directory under the install prefix. Simply copy the sample
to a new file called `piwx.conf`.

Running Automatically
---------------------

To run PiWx automatically on boot, refer to the sample service file in the
repository. This file must be placed in `/lib/systemd/system` and renamed to
`piwx.service`.

Use `sudo systemctl start piwx.service` to test starting PiWx and use
`sudo systemctl stop piwx.service` to stop it. To enable automatically starting
PiWx on boot, use `sudo systemctl enable piwx.service`.
