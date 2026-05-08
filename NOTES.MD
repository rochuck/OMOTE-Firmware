###

    TODO:  ive started hooking up the websockets for home assistant. i can now toggle one light off and on.
    next steps are to receive the auth ok callback for websockets, and then to subscribe for the light events.
    to do this the websocket auth message has to be set up to call a callback in the command handler.
    we will have to register this somewhere

    next, when the auth is received, the smart home gui is called to subscribe to all the events, by calling in to the websocket interface/command handler/HAL

    too many indirections yuk


bin/ld: 
hardware/hardwarePresenter.cpp.o:src/applicationInternal/hardware/hardwarePresenter.cpp:141: multiple definition of `indev_keypad'; 

ESP32/lvgl_hal_esp32.cpp.o: lvgl_hal_esp32.cpp:7: first defined here