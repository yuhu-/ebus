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

### Priority and Preemption: ###
The Scheduler manages bus access using a priority queue. Background tasks like the
`DeviceScanner` enqueue commands with a low priority (default 5). Application-level
requests can specify higher priorities (up to 255) to preempt this background traffic,
ensuring that user commands are prioritized over routine discovery tasks.

### Data Path: ###
```
                                 [ User Application ]
                                          |
                                          v
                                 [ Controller (API) ]
                                          |
          +-------------------------------+--------------------------------+
          |                               |                                |
          v                               v                                v
  [ PollManager ]                 [ DeviceScanner ]                [ ClientManager ]
  (Recurring Jobs)                (Auto-Discovery)                  (Network Bridge)
          |                               |                                |
          +-------------------------------+--------------------------------+
                                          |
                                          v
                                   [  Scheduler  ]
                                   (Priority Queue)
                                          |
                                          v
                                   [   Handler   ] <--- [ TelegramCallback ]
                                   ( Protocol FSM )          (ErrorCallback)
                                          |
          +-------------------------------+--------------------------------+
          |                               |                                |
          v                               v                                v
    [ Sequence ]                    [  Request  ]                  [ TimingStats ]
   (CRC/Escaping)                   (Arbitration)                  (Metrics/Stats)
          |                               |                                ^
          +-------------------------------+--------------------------------+
                                          |
                                          v
                                [ Bus Abstraction ]
                               /                 \
                    [ BusPosix (Linux) ]    [ BusFreeRtos (ESP32) ]
                           |                         |
                    (/dev/ttyUSBx)            (UART Hardware)
```
