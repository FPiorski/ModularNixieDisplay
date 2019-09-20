# ESP32 Control software

### Because the Nixie boards are modular I though controlling them should reflect that

So I drive them with an ESP32 through UDP.

#### How to build

After installing [esp-idf](https://github.com/espressif/esp-idf), cloning this repo and exporting env variables ($IDF_PATH/export.sh) just configure your SSID and password, used pins, udp port, then build and flash.
```
idf.py menuconfig
idf.py build
idf.py -p PORTPATH flash
```

#### How to use
Just send a UDP packet to the ESP32 over WiFi containg what you want to display, colons are automatically converted to fives.
```
watch -n1 "date +%T%N | head -c10 | nc -u -q0 192.168.0.115 3333"
```

### How it works
It's literally the udp_server example with some added GPIO use for manually shifting out the data to chained 74HC595's, nothing fancy.
