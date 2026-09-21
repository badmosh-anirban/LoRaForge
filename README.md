# Gateway-Synchronized Single-Channel LoRa Network

> **A low-cost, collision-aware LoRa communication prototype using gateway synchronization and TDMA over a single shared channel.**

![LoRa](https://img.shields.io/badge/LoRa-SX1262-blue)
![ESP32](https://img.shields.io/badge/MCU-ESP32-green)
![ESP32S3](https://img.shields.io/badge/MCU-ESP32S3-green)
![Protocol](https://img.shields.io/badge/Protocol-Raw%20LoRa-orange)
![MAC](https://img.shields.io/badge/MAC-Synchronized%20TDMA-purple)
![Status](https://img.shields.io/badge/Status-Prototype-yellow)

---

## Overview

This project is a prototype communication architecture designed for **underground coal-mine monitoring(SIH26025)**, where multiple sensor nodes need to periodically send environmental and safety information to a central gateway.

The prototype uses:

- **2 LoRa sensor nodes**
- **1 ESP32-S3-based gateway**
- **SX1262 LoRa transceivers**
- **A single shared LoRa communication channel**
- **Gateway-based time synchronization**
- **TDMA (Time Division Multiple Access)**
- **Dedicated transmission slots for each node**
- **Periodic 5-second synchronization frames**
- **Sensor data collection and packet transmission**
- **Optional gateway-side monitoring/dashboard**

The primary goal of this prototype is to demonstrate how a network of low-cost LoRa nodes can coordinate their transmissions over a **single shared channel without requiring every node to transmit randomly**.

---

# System Architecture

The prototype consists of three main devices:

```text
                    ┌──────────────────────────┐
                    │       GATEWAY            │
                    │                          │
                    │       XIAO ESP32S3       │
                    │           +              │
                    │         SX1262           |
                    |  (0.96' OLED optional)   │
                    │                          │
                    │  • Sends SYNC            │
                    │  • Receives packets      │
                    │  • Tracks nodes          │
                    │  • Dashboard             │
                    └────────────┬─────────────┘
                                 │
                    Single LoRa Channel
                                 │
              ┌──────────────────┴──────────────────┐
              │                                     │
              ▼                                     ▼
   ┌────────────────────┐                ┌────────────────────┐
   │     NODE 1         │                │     NODE 2         │
   │                    │                │                    │
   │ ESP32/MCU          │                │ ESP32/MCU          │
   │       +            │                │       +            │
   │     SX1262         │                │     SX1262         │
   │                    │                │                    │
   │ Sensors             │                │ Sensors             │
   │ • MPU6050           │                │ • MPU6050           │
   │ • Flame             │                │ • Flame             │
   │ • Vibration         │                │ • Vibration         │
   │ • Gas sensor        │                │ • Gas sensor        │
   │ • BMP280            │                │ • BMP280            │
   └────────────────────┘                └────────────────────┘
```

The communication is **direct node → gateway**.

The current prototype does **not implement node-to-node packet relaying**. Relay/multi-hop communication is considered part of the future expansion of the architecture.

- MPU6050 - for overall movement detection
- BMP280 - for temperature and pressure

> [!IMPORTANT]
> The sensors used in this prototype are intended for demonstration, experimentation, and proof-of-concept purposes only. The selected modules, including the gas, flame, vibration, temperature, pressure, and motion sensors, are low-cost hobby/development-grade components and are not industrial-grade or certified safety instruments.

---

# Why a Single-Channel Architecture?

## The original requirement

A production deployment for a large underground mine would ideally use a more capable LoRa gateway architecture with appropriate radio/concentrator hardware.

However, during prototype development, one of the practical constraints was **cost**.

A proper multi-channel LoRa gateway/concentrator based on hardware such as an SX1302/SX1303-class solution is considerably more expensive and more complex than a simple SX1262 transceiver.

For our prototype and hackathon demonstration, purchasing a complete multi-channel gateway was not practical within the available budget.

Therefore, we designed a **low-cost single-channel prototype architecture**.

### Our approach

Instead of:

```text
Multiple channels
      ↓
Multi-channel LoRa concentrator
      ↓
Gateway
```

we use:

```text
                 One LoRa channel
                       │
        ┌──────────────┼──────────────┐
        │              │              │
      Node 1         Node 2         Gateway
```

All nodes operate using the same LoRa radio parameters and communication channel.

This significantly simplifies the hardware and reduces the cost of the prototype.

---

# The Problem With a Single Channel

Using one shared channel introduces an important problem:

## What happens if two nodes transmit simultaneously?

Suppose Node 1 and Node 2 both detect an event and attempt to transmit at exactly the same time:

```text
Node 1 ──────────────── TX ────────────────┐
                                           │
Node 2 ──────────────── TX ────────────────┤
                                           ▼
                                     COLLISION
                                           │
                                      Gateway
```

Because both packets occupy the same channel at the same time, the gateway may not successfully decode either packet.

This becomes particularly problematic as the number of nodes increases.

Therefore, simply putting multiple SX1262 devices on the same frequency is not sufficient.

We needed a mechanism to coordinate **when each node is allowed to transmit**.

---

# Our Solution: Gateway-Synchronized TDMA

To solve the single-channel collision problem, the prototype uses a **Time Division Multiple Access (TDMA)** approach.

More specifically:

> **Gateway-synchronized TDMA over a single LoRa channel.**

The gateway periodically sends a synchronization packet.

Each node uses this synchronization packet to determine the timing of the current communication frame and its assigned transmission slot.

---

# 5-Second Synchronization Cycle

The gateway broadcasts a synchronization packet every:

```text
5 seconds
```

Therefore:

```text
SYNC
  ↓
5-second communication frame
  ↓
SYNC
  ↓
5-second communication frame
  ↓
SYNC
  ↓
...
```

The synchronization packet provides the nodes with a common timing reference.

---

# TDMA Frame Structure

For the current two-node prototype, the communication frame can conceptually be represented as:

```text
                    5 SECOND FRAME
┌──────────────────────────────────────────────────────┐
│ SYNC │ Guard │ Node 1 Slot │ Guard │ Node 2 Slot │  │
└──────────────────────────────────────────────────────┘
          ↑                    ↑
          │                    │
       Node 1 TX            Node 2 TX
```

The exact slot duration can be configured according to the expected packet airtime, processing time and required guard interval.

The important principle is:

> **Each node gets a predefined time window in which it is allowed to transmit.**

---

# How Synchronization Works

The gateway periodically transmits a `SYNC` packet.

The packet contains timing information such as:

- Frame number
- Synchronization information
- Timing reference
- Any additional network configuration required by the nodes

When a node receives the synchronization packet, it records the time at which the synchronization event was received.

The node then calculates its future transmission time.

Conceptually:

```text
SYNC received
      │
      ▼
Read frame number
      │
      ▼
Determine node's slot
      │
      ▼
Calculate transmission time
      │
      ▼
Wait
      │
      ▼
Transmit sensor packet
```

---

### Example

Assume:

```text
Node ID = 1
Frame = 100
SYNC received at = 50,000 ms
Node 1 slot offset = 700 ms
```

The node calculates:

```text
transmitAt = 50,000 + 700
           = 50,700 ms
```

Node 1 waits until approximately `50,700 ms` and then transmits.

Node 2 has a different slot:

```text
Node 2 offset = 2,200 ms
```

Therefore:

```text
Node 2 transmitAt = 50,000 + 2,200
                  = 52,200 ms
```

The two nodes therefore do not intentionally transmit at the same time.

---

# Guard Time

> [!NOTE]
> This feature is not yet implemented in the current version

A practical TDMA implementation needs some tolerance between slots.

Real embedded systems are not perfectly synchronized because of:

- Processing delays
- Radio state transition time
- Interrupt latency
- Clock differences
- Packet reception delays
- MCU timing inaccuracies
- LoRa transmission/receive turnaround time

Therefore, **guard intervals** can be introduced between transmission slots.

Conceptually:

```text
| Node 1 TX | Guard | Node 2 TX | Guard |
```

The guard time reduces the possibility of one node's transmission overlapping with another node's assigned slot.

---

# LoRa Radio Configuration

The prototype uses common LoRa parameters for all participating radios.

Example configuration:

| Parameter        |     Value |
| ---------------- | --------: |
| Frequency        | 866.5 MHz |
| Bandwidth        |   125 kHz |
| Spreading Factor |       SF7 |
| Coding Rate      |       4/5 |
| Sync Word        |    `0x12` |
| TX Power         |    10 dBm |
| Preamble Length  | 8 symbols |

These values are configurable in the firmware.

> **Important:** Frequency selection must comply with the applicable radio regulations for the deployment region. The values above are prototype configuration values and should not automatically be treated as deployment settings.

---

# Why Raw LoRa Instead of LoRaWAN?

This prototype intentionally uses **raw LoRa communication** rather than a complete LoRaWAN stack.

The reason is architectural control.

We wanted to directly experiment with:

- Packet structure
- Synchronization
- TDMA
- Slot allocation
- Timing
- Collision avoidance
- Node scheduling
- Gateway behavior

With raw LoRa, we can implement our own lightweight MAC/timing mechanism.

Therefore:

```text
SX1262
  ↓
Raw LoRa
  ↓
Custom packet format
  ↓
Custom synchronization
  ↓
Custom TDMA scheduling
```

This should not be confused with a standard LoRaWAN gateway.

---

# "Fake Gateway" vs Actual Gateway

The gateway used in this prototype is sometimes referred to as a **fake gateway**.

This terminology means that it is acting as the **central coordinator for our prototype**, rather than being a full production LoRaWAN gateway/concentrator.

Our prototype gateway consists of:

```text
XIAO ESP32S3
      +
SX1262
```

It performs functions such as:

- Synchronization
- Packet reception
- Node identification
- Frame tracking
- Data processing
- Demonstration/dashboard functionality

A production implementation could use an appropriate multi-channel LoRa gateway/concentrator depending on the required network architecture.

---

# Hardware

## Nodes

Each sensor node consists of:

```text
MCU
 │
 ├── SX1262
 │
 ├── MPU6050
 ├── Flame Sensor
 ├── Vibration Sensor
 ├── Gas Sensor
 └── BMP280
```

Depending on the final node configuration, not every sensor needs to be populated.

---

## Gateway

The demonstration gateway uses:

```text
XIAO ESP32S3
       +
SX1262
       │
       ├── Receive LoRa packets
       ├── Send SYNC packets
       ├── Process node data
       └── Host local monitoring interface
```

An optional OLED can also be connected for local debugging/status monitoring.

---

# 🔌 SX1262 Node Wiring

The standalone SX1262 modules communicate with the MCU using SPI.

Typical signals include:

| SX1262 | MCU             |
| ------ | --------------- |
| MOSI   | SPI MOSI        |
| MISO   | SPI MISO        |
| SCK    | SPI Clock       |
| NSS/CS | Chip Select     |
| RESET  | Reset           |
| BUSY   | Busy            |
| DIO1   | Interrupt/Event |

The exact GPIO assignment depends on the selected MCU development board.

---

# Components

| Component                                  | Quantity | Purpose                       | Price / Unit |  Total |
| ------------------------------------------ | -------: | ----------------------------- | -----------: | -----: |
| SX1262 LoRa Module                         |        2 | LoRa communication            |        ₹ 662 | ₹ 1324 |
| Seeed Studio XIAO ESP32S3 & Wio-SX1262 Kit |        1 | Gateway MCU                   |       ₹ 1419 | ₹ 1419 |
| MPU6050                                    |        2 | Motion/acceleration detection |        ₹ 146 |  ₹ 146 |
| Flame Sensor                               |        2 | Flame detection               |         ₹ 33 |   ₹ 62 |
| Vibration Sensor                           |        2 | Vibration detection           |         ₹ 42 |   ₹ 79 |
| MQ-2 / MQ-135                              |        2 | Gas/smoke sensing             |        ₹ 109 |  ₹ 218 |
| BMP280                                     |        2 | Temperature/pressure sensing  |         ₹ 40 |   ₹ 80 |
| OLED SSD1306 _(optional)_                  |        1 | Local gateway display         |        ₹ 156 |  ₹ 156 |
| LEDs                                       |       2+ | Node status indication        |         ₹ 37 |   ₹ 37 |
| Bread board                                |        2 | Circuit assembly              |         ₹ 65 |  ₹ 130 |
| Jumper wires / connectors                  |       20 | Prototyping                   |          ₹ 3 |   ₹ 60 |

### Approximate Prototype Cost

**Total hardware cost:** ₹3,771 ± 500

The cost comparison is one of the reasons this architecture was useful for the prototype: it allowed us to demonstrate the synchronization and communication concept without requiring a more expensive multi-channel gateway setup.

---

# Project Structure

A suggested project structure is:

```text
LoRaForge/
│
├── README.md
│
├── gateway/
│   ├── platformio.ini
│   └── src/
│   │   ├── main.cpp
│   └── include/
│       ├── config.h
│       └── protocol.h
│
├── node/
│   ├── platformio.ini
│   └── src/
│   │   ├── main.cpp
│   └── include/
│       ├── config.h
│       └── protocol.h
│
├── docs/
│   ├── architecture.png
│
├── images&videos/
│   ├── node.jpg
│   ├── gateway.jpg
│   └── prototype.jpg
│
└── LICENSE
```

---

# Packet Architecture

The communication can be separated into two primary packet types:

## 1. SYNC Packet

Sent periodically by the gateway.

Conceptual structure:

```text
SYNC
├── Packet Type
├── Frame Number
└── Timing Information
```

Its purpose is to give all nodes a common reference.

---

## 2. DATA Packet

Sent by an individual node during its assigned slot.

Conceptual structure:

```text
DATA
├── Packet Type
├── Node ID
├── Frame Number
├── Packet Counter
├── Sensor Data
└── Optional Status Information
```

The **Node ID** allows the gateway to identify the source.

The **Frame Number** allows the gateway to associate the packet with a synchronization cycle.

The **Packet Counter** is useful for detecting missed packets.

For example:

```text
Node 1:

Packet 101
Packet 102
Packet 103
Packet 105
```

The missing `104` can indicate that a packet was not successfully received.

---

# How the Code Works

## Gateway

The gateway operates approximately as follows:

```text
START
 │
 ▼
Initialize ESP32
 │
 ▼
Initialize SX1262
 │
 ▼
Initialize dashboard/display
 │
 ▼
Create frame
 │
 ▼
Send SYNC
 │
 ▼
Listen for node packets
 │
 ▼
Identify Node ID
 │
 ▼
Process sensor data
 │
 ▼
Update dashboard/display
 │
 ▼
Wait until next synchronization cycle
 │
 └──────────────► Send next SYNC
```

---

# Gateway Synchronization

The gateway maintains a frame counter.

Conceptually:

```cpp
currentFrame++;
sendSync(currentFrame);
```

The synchronization packet tells nodes:

> "A new communication frame has started."

The gateway then listens for the expected node transmissions.

---

# Node Operation

Each node continuously waits for synchronization.

```text
START
 │
 ▼
Initialize MCU
 │
 ▼
Initialize sensors
 │
 ▼
Initialize SX1262
 │
 ▼
Wait for SYNC
 │
 ▼
Receive SYNC
 │
 ▼
Read frame number
 │
 ▼
Calculate assigned slot
 │
 ▼
Wait
 │
 ▼
Read sensors
 │
 ▼
Transmit DATA
 │
 ▼
Return to receive mode
 │
 └──────────────► Wait for next SYNC
```

---

#Timing Calculation

A key part of the node firmware is determining when its slot begins.

The concept is:

```cpp
transmitAt = syncReceivedAt + nodeSlotOffset;
```

For example:

```text
SYNC received
     │
     ├──── Node 1 offset ────► Node 1 transmission
     │
     └──────── Node 2 offset ───────────► Node 2 transmission
```

The code uses wraparound-safe time comparison rather than relying on a simple `now >= target` comparison.

For example:

```cpp
bool timeReached(uint32_t now, uint32_t target)
{
    return static_cast<int32_t>(now - target) >= 0;
}
```

This is important because `millis()` eventually wraps around.

---

# Example Timing

For illustration:

```text
SYNC interval = 5000 ms

SYNC
│
├── Node 1 slot
│      offset ≈ 700 ms
│
├── Node 2 slot
│      offset ≈ 2200 ms
│
└── Remaining time
       ↓
   Next SYNC
```

The actual slot offsets and durations are configurable.

The important requirement is:

```text
Node 1 transmission
        ≠
Node 2 transmission
```

and sufficient time must be reserved for each packet to complete.

---

# How to Replicate the Prototype

You can reproduce this architecture using inexpensive LoRa hardware.

## Step 1 — Prepare the hardware

You need:

- 2 × SX1262 for nodes
- 1 × SX1262 for gateway
- 3 × compatible MCUs
- Sensors as required
- Antennas suitable for the selected frequency
- Power supplies
- Jumper wires/connectors

---

# Step 2 — Configure all LoRa radios

Make sure all devices use compatible:

```text
Frequency
Bandwidth
Spreading Factor
Coding Rate
Sync Word
Preamble Length
```

For example:

```text
Frequency:       866.5 MHz
Bandwidth:       125 kHz
Spreading Factor: SF7
Coding Rate:     4/5
Sync Word:       0x12
TX Power:        10 dBm
Preamble:        8
```

---

# Step 3 — Assign Node IDs

Each node must have a unique identifier.

Example:

```cpp
#define NODE_ID 1
```

and:

```cpp
#define NODE_ID 2
```

Do not assign the same ID to multiple nodes.

---

# Step 4 — Assign TDMA slots

Create a slot table.

Example:

```text
Node 1 → 700 ms
Node 2 → 2200 ms
```

For a larger network:

```text
Node 1 → 500 ms
Node 2 → 1200 ms
Node 3 → 1900 ms
Node 4 → 2600 ms
Node 5 → 3300 ms
```

The exact values must be selected based on the packet airtime and guard requirements.

---

# Step 5 — Configure the gateway

The gateway should:

1. Initialize the SX1262.
2. Start the frame counter.
3. Send a SYNC packet.
4. Listen for node packets.
5. Process received data.
6. Repeat the synchronization cycle.

---

# Step 6 — Configure the nodes

Each node should:

1. Start the SX1262.
2. Start its sensors.
3. Wait for SYNC.
4. Record the synchronization time.
5. Read the frame number.
6. Calculate its assigned slot.
7. Wait until the slot.
8. Read/prepare sensor data.
9. Transmit.
10. Return to receive mode.

---

# Step 7 — Test with one node first

Before connecting both nodes, test:

```text
Gateway ←→ Node 1
```

Verify:

- SYNC reception
- Frame numbers
- Timing
- Packet reception
- RSSI
- SNR
- Packet counter

Then add Node 2.

---

# Step 8 — Test simultaneous operation

Run:

```text
Gateway
   ↑
   ├── Node 1
   └── Node 2
```

Observe whether the nodes consistently transmit during their assigned slots.

A serial monitor can be useful for debugging:

```text
SYNC received
Node = 1
Frame = 25
Sync time = 125000
Slot offset = 700
Transmit at = 125700
```

---

# Testing Methodology

The prototype can be evaluated using several metrics.

## 1. Packet Reception

Count:

```text
Packets transmitted
Packets received
Packets missed
```

---

## 2. Packet Loss Rate

A basic measurement is:

```text
Packet Loss Rate =
(Missed Packets / Transmitted Packets) × 100
```

---

## 3. Timing Accuracy

Measure the difference between:

```text
Expected TX time
```

and:

```text
Actual TX time
```

---

## 4. Collision Testing

Compare:

### Without TDMA

```text
Node 1 ───── TX ─────
Node 2 ───── TX ─────
             ↓
        Possible collision
```

### With TDMA

```text
Node 1 ─── TX ───
                 Guard
                      Node 2 ─── TX ───
```

This demonstrates the motivation behind the scheduling mechanism.

---

# Limitations

This is a prototype and therefore has several limitations.

## 1. Single-channel operation

All nodes share the same LoRa channel.

As the number of nodes increases, scheduling becomes more difficult.

### Possible improvement

Use:

- Multiple channels
- A multi-channel LoRa concentrator
- More sophisticated MAC scheduling

---

# 2. Synchronization Accuracy

The nodes rely on receiving periodic gateway synchronization packets.

If a node misses the synchronization packet, its timing reference may become outdated.

### Possible improvement

Use:

- More robust synchronization
- Timestamped synchronization
- Periodic resynchronization
- Clock drift compensation
- Higher precision timing sources

---

# 3. Guard Time Overhead

Guard periods improve reliability but reduce the amount of time available for useful transmissions.

### Possible improvement

Optimize guard time based on measured:

- MCU timing
- Radio latency
- Packet airtime
- Clock drift

---

# 4. Gateway Dependency

The network depends on the gateway for synchronization.

If the gateway stops transmitting synchronization packets, nodes may eventually lose timing alignment.

### Possible improvement

Implement:

- Synchronization timeout detection
- Backup timing
- Local oscillator-based scheduling
- Redundant gateway mechanisms

---

# 5. Scalability

The current prototype was designed around a small number of nodes.

A 5-second frame containing two nodes is straightforward.

A network containing hundreds of nodes would require much more sophisticated scheduling.

### Possible improvement

Use:

- Dynamic slot allocation
- Multiple channels
- Hierarchical gateways
- Multi-hop/relay architecture
- Adaptive TDMA
- Event-priority scheduling

---

# 6. No Full Multi-Hop Routing

The current implementation is primarily:

```text
Node → Gateway
```

It does not yet implement full:

```text
Node → Relay → Relay → Gateway
```

routing.

### Possible improvement

Add relay nodes with:

- Routing tables
- Forwarding rules
- Hop counters
- Duplicate packet detection
- Route discovery/maintenance

---

# 7. Prototype Hardware

The prototype uses development boards and breakout modules.

These are useful for demonstrating the concept but are not optimized for:

- Size
- Power consumption
- Environmental protection
- Long-term deployment
- Industrial certification

### Possible improvement

Develop a custom PCB with:

- Integrated MCU
- SX1262
- Sensor interfaces
- Power management
- Battery monitoring
- Protection circuitry
- Appropriate enclosure

---

# 8. Mine Environment

Real underground mining environments can introduce additional challenges such as:

- Radio attenuation
- Obstructions
- Reflections
- Long tunnels
- Electromagnetic interference
- Temperature variation
- Dust
- Moisture
- Battery constraints

Therefore, successful bench-top communication does not by itself establish real mine-scale performance.

Field testing and appropriate safety/certification requirements would be necessary for deployment.

---

# Future Improvements

The prototype provides a foundation for a more advanced architecture.

Possible future development includes:

### Multi-channel Gateway

Replace the single SX1262 gateway with an appropriate multi-channel LoRa concentrator.

```text
              Multi-channel Gateway
                 /     |     \
              Ch1     Ch2     Ch3
               │       │       │
             Nodes   Nodes   Nodes
```

---

### Multi-hop Relay Network

Add relay nodes:

```text
Sensor
  │
  ▼
Relay
  │
  ▼
Relay
  │
  ▼
Gateway
```

This can potentially extend communication through large underground areas.

---

### Dynamic TDMA

Instead of permanently assigning slots:

```text
Node 1 → Slot 1
Node 2 → Slot 2
Node 3 → Slot 3
```

the gateway could dynamically allocate slots according to:

- Number of active nodes
- Priority
- Sensor events
- Network congestion
- Battery status

---

### Emergency Priority

Safety-critical events could be given special treatment.

For example:

```text
Normal data
     ↓
Regular TDMA slot

Emergency event
     ↓
Priority transmission mechanism
```

Any such mechanism would need to be designed carefully so that priority traffic does not undermine the collision-control mechanism.

---

### Better Time Synchronization

Future versions could investigate:

- More precise timestamps
- Clock drift estimation
- Adaptive synchronization
- Hardware timers
- GPS/GNSS timing where practical
- Network-wide time distribution

---

# Why This Architecture Is Useful

The important contribution of this prototype is not simply the use of LoRa.

The prototype demonstrates how a **low-cost single-channel radio architecture can be coordinated using software-based synchronization and scheduled access**.

The design separates the problem into:

```text
Hardware cost constraint
        ↓
Single shared channel
        ↓
Collision problem
        ↓
Gateway synchronization
        ↓
TDMA scheduling
        ↓
Dedicated node slots
        ↓
Coordinated communication
```

This makes the prototype useful as a proof-of-concept for exploring low-cost coordinated LoRa networks.

---

# Demonstration Video

## Prototype Demonstration

> **Video:** [INSERT YOUTUBE / GOOGLE DRIVE / GITHUB VIDEO LINK HERE]

The demonstration should show:

1. Gateway startup
2. Gateway SYNC transmission
3. Node synchronization
4. Node 1 transmission
5. Node 2 transmission
6. Gateway packet reception
7. Sensor data
8. Packet/frame counter
9. Dashboard/local monitoring

Example:

```markdown
## 🎥 Demo Video

[![Watch the prototype demonstration](https://img.youtube.com/vi/VIDEO_ID/0.jpg)](https://www.youtube.com/watch?v=VIDEO_ID)
```

Replace `VIDEO_ID` with the actual YouTube video ID.

---

# Prototype Images

Add photographs of the actual hardware here.

```markdown
## Prototype

![Gateway](images/gateway.jpg)

![Sensor Node](images/node.jpg)

![Complete Prototype](images/prototype.jpg)
```

Recommended photographs:

- Complete setup
- Gateway close-up
- Node 1
- Node 2
- Sensor connections
- Dashboard
- Serial monitor showing synchronization

---

# Example Serial Output

A typical node debugging output may look like:

```text
LoRa initialized
Waiting for SYNC...

SYNC received:
Node = 1
Frame = 42
Sync time = 210000
Slot offset = 700
Transmit at = 210700

Sending sensor packet...
Packet counter = 42
Transmission complete

Waiting for next SYNC...
```

Gateway:

```text
Gateway started
Frame = 42
SYNC transmitted

Waiting for node packets...

Packet received
Node ID = 1
Frame = 42
Counter = 42
RSSI = -XX dBm
SNR = XX dB

Packet received
Node ID = 2
Frame = 42
Counter = 42
RSSI = -XX dBm
SNR = XX dB
```

The exact output depends on the firmware version.

---

# Frequently Asked Questions

## Why didn't you use a normal LoRaWAN gateway?

The prototype was designed under a limited hardware budget. A full multi-channel gateway/concentrator would add considerable hardware cost and complexity.

The objective of this prototype was to demonstrate the communication and synchronization concept using lower-cost SX1262 hardware.

---

## Why only one channel?

The single-channel architecture reduces hardware complexity and cost for the prototype.

The trade-off is that nodes must coordinate their access to the shared channel.

---

## Doesn't using one channel cause collisions?

It can if nodes transmit simultaneously.

That is why this prototype uses **gateway-synchronized TDMA**.

Each node is assigned a different transmission slot.

---

## Is this TDMA?

Yes.

The communication mechanism is a form of **synchronized TDMA**, where nodes are assigned predetermined time windows for transmission.

---

## Is this LoRaWAN?

No.

The prototype uses **raw LoRa communication with a custom synchronization/TDMA mechanism**.

---

## Why does the gateway send SYNC every 5 seconds?

The 5-second interval establishes a recurring timing reference for the nodes and also creates a predictable communication frame.

The value is configurable and is primarily a prototype design choice.

---

## What happens if a node misses SYNC?

If a node does not receive a synchronization packet, its timing reference may become stale.

A production implementation should include mechanisms such as synchronization timeout handling, resynchronization and possibly clock-drift compensation.

---

## Why not simply use random delays?

Random delays can reduce the probability of repeated collisions but do not provide deterministic scheduling.

TDMA provides a predictable transmission schedule.

---

## Can more nodes be added?

Yes.

Additional nodes can be assigned additional slots.

For example:

```text
Frame
│
├── Node 1
├── Node 2
├── Node 3
├── Node 4
└── Node 5
```

However, the available frame time and LoRa airtime impose practical limits.

---

## Can this be used in a real coal mine?

This prototype demonstrates the communication concept but should not be considered a production-certified mining communication system.

A real deployment would require extensive RF testing, environmental testing, power optimization, reliability testing, appropriate equipment certification and compliance with applicable mining and radio regulations.

---

## Can the system support relay nodes?

The current prototype is primarily a direct node-to-gateway architecture.

Relay/multi-hop functionality can be added as a future extension.

---

## Can the gateway communicate with all nodes?

Yes, provided the nodes are within the radio link budget and are operating with compatible LoRa parameters.

The gateway coordinates the nodes through synchronization packets.

---

## Why use SX1262?

SX1262 provides a relatively low-cost LoRa transceiver suitable for point-to-point LoRa experimentation.

It is also useful for implementing custom communication protocols because the application can control the radio behavior directly.

---

# Key Concepts

This project combines several important embedded/networking concepts:

### LoRa

Long-range, low-power wireless modulation/communication technology.

### Raw LoRa

Direct use of LoRa radio communication without requiring LoRaWAN.

### Single-Channel Communication

Multiple nodes share the same radio frequency/channel.

### TDMA

Multiple nodes share a communication medium by transmitting during different time intervals.

### Synchronization

A common timing reference is distributed from the gateway to the nodes.

### Guard Time

A small timing margin between slots to account for clock and processing inaccuracies.

### Frame

A repeating communication period containing synchronization and node transmission slots.

---

# Safety & Deployment Disclaimer

This repository is a **prototype/research/hackathon implementation**.

It should not be directly deployed as a safety-critical mine monitoring system without appropriate engineering validation.

For real-world deployment, additional work would be required in:

- RF characterization
- Network reliability
- Environmental testing
- Power management
- Sensor calibration
- Fail-safe behavior
- Redundancy
- Cybersecurity
- Hardware certification
- Mining safety requirements
- Radio regulatory compliance

---

# Contributing

Contributions and improvements are welcome.

Potential areas for contribution include:

- Improved TDMA scheduling
- Dynamic slot allocation
- Multi-hop routing
- Packet acknowledgement
- Retransmission mechanisms
- Power optimization
- Better synchronization
- Multi-channel gateway support
- Dashboard improvements
- Sensor integration

---

# License

Add your preferred open-source license here.

For example:

```text
MIT License
```

See [`LICENSE`](LICENSE) for details.

---

# 👥 Project Team

| Role                   | Name          |
| ---------------------- | ------------- |
| Embedded / LoRa        | Anirban Giri  |
| Hardware / Sensors     | Subhendu Hait |
| Software / Dashboard   | Agniva Roy    |
| Documentation          | Kriti Gupta   |
| Research               | Rounak Ali    |
| Deployment and testing | Santu Samanta |

---

# Summary

This prototype explores a practical way of building a **low-cost synchronized LoRa sensor network** when a full multi-channel gateway is not available.

The core idea is:

```text
              LOW-COST HARDWARE
                     │
                     ▼
             SINGLE CHANNEL
                     │
                     ▼
             COLLISION PROBLEM
                     │
                     ▼
          GATEWAY SYNCHRONIZATION
                     │
                     ▼
                  TDMA
                     │
             ┌───────┴───────┐
             ▼               ▼
          NODE 1           NODE 2
          SLOT 1           SLOT 2
             │               │
             └───────┬───────┘
                     ▼
                  GATEWAY
                     │
                     ▼
               MONITORING
```

The prototype demonstrates that even with constrained hardware, communication coordination can be moved partly into the software/protocol layer.

The next stage toward a deployable system would involve **multi-channel gateway hardware, robust synchronization, scalable scheduling, relay/multi-hop communication, extensive RF testing, and deployment-specific safety requirements**.

---

## 📌 Repository Links

- **Source Code:**
- **Demo Video:** here
- **Presentation:** here
- **Project Problem Statement:** here

---

**Built as a prototype for exploring synchronized LoRa communication for underground monitoring applications.**
