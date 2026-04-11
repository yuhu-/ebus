### Active Path: ###
The Controller or Managers enqueue messages into the Scheduler. The Scheduler waits for
the Bus to be free, then tells the Handler to send. The Handler coordinates with the
Request object to win arbitration before streaming bytes.

### Passive Path: ###
The Bus receives bytes from the wire. The BusHandler background thread picks them up and
feeds them into the Handler and Request state machines. When a full telegram is validated,
the Handler fires a callback that reaches the Controller and DeviceManager. 

### Bridge Path: ###
The ClientManager monitors network sockets. When a remote tool (like ebusd) sends a byte,
it bypasses the Scheduler and attempts to request the bus via the Request tracker
immediately, mimicking a local master.

### Data Path: ###

                                 [ User Application ]
                                          |
                                          v
                                 [ Controller (API) ]
                                          |
          +-------------------------------+--------------------------------+
          |                               |                                |
          v                               v                                v
  [ PollManager ]                 [ DeviceScanner ]                [ ClientManager ]
  (Recurring Jobs)                (Auto-Discovery)                 (Network Bridge)
          |                               |                                |
          +-------------------------------+--------------------------------+
                                          |
                                          v
                                   [  Scheduler  ] <--- [ ResultCallback ]
                                   (Priority Queue)
                                          |
                                          v
                                   [   Handler   ] <--- [ TelegramCallback ]
                                   ( Protocol FSM )           (ErrorCallback)
                                          |
          +-------------------------------+--------------------------------+
          |                               |                                |
          v                               v                                v
    [ Sequence ]                    [  Request  ]                  [ TimingStats ]
    (CRC/Escaping)                  (Arbitration)                  (Metrics/Stats)
          |                               |                                ^
          +-------------------------------+--------------------------------+
                                          |
                                          v
                                [ Bus Abstraction ]
                               /                 \
                    [ BusPosix (Linux) ]    [ BusFreeRtos (ESP32) ]
                           |                         |
                    (/dev/ttyUSBx)            (UART Hardware)
