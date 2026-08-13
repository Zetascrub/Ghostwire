# VPet PvP Battle System

## Overview

The VPet badge will support automatic PvP encounters between nearby ESP32-based badges using **Bluetooth Low Energy (BLE)**.

The intention is to create a system where players can encounter one another naturally while walking around an event, convention, meetup, or other shared space. A user does not need to manually pair devices or select another player before an encounter can occur.

BLE is preferred over normal Wi-Fi because it:

* Does not require an access point or shared network.
* Uses relatively little power.
* Supports passive device discovery.
* Provides signal-strength information through RSSI.
* Allows badges to establish temporary direct connections automatically.
* Leaves open the possibility of future phone/app integration.

## General Operation

Each badge periodically performs two BLE functions:

1. **Advertises** its presence and basic VPet information.
2. **Scans** for advertisements from other compatible badges.

A BLE advertisement could contain compact information such as:

```text
Protocol: VPET
Version: 1
Player ID: 4821
Pet ID: 07
Level: 14
Battle Enabled: true
Encounter Nonce: 93842
```

Advertisements should remain small and should only contain information required to determine whether an encounter is possible.

Detailed pet stats and battle information are exchanged only after an encounter has been established.

## Automatic PvP Encounters

When another VPet badge is detected, the receiving badge checks whether it is eligible for a random encounter.

Example flow:

```text
Badge discovers nearby VPet
        ↓
Check protocol/version
        ↓
Check Battle Enabled flag
        ↓
Check RSSI / proximity
        ↓
Check encounter cooldown
        ↓
Perform random encounter roll
        ↓
Select encounter participant
        ↓
Exchange challenge acknowledgement
        ↓
Establish BLE connection
        ↓
Exchange full pet/battle data
        ↓
Begin battle
```

The BLE connection itself should only be created after an encounter has been selected. This reduces unnecessary connections and power consumption.

## Proximity Detection

BLE RSSI can be used as an approximate indication of distance.

For example:

```text
RSSI > -55 dBm
Very close

RSSI -55 to -70 dBm
Nearby

RSSI < -75 dBm
Generally ignore
```

These values must be tuned through real-world testing. Human bodies, clothing, bags, walls, antenna orientation, and crowded venues can significantly affect RSSI.

An encounter could require a signal above a configured threshold:

```cpp
if (rssi > -60) {
    playerIsNearby = true;
}
```

This prevents battles being triggered by badges on the opposite side of a venue.

## Random Encounter Chance

Being nearby should not necessarily guarantee an encounter.

Once another badge meets the proximity requirement, an encounter probability can be applied:

```cpp
if (rssi > -60 && random(100) < 20) {
    startEncounter();
}
```

In this example, an eligible nearby player has a 20% chance of triggering an encounter.

The probability can later become part of the game's mechanics.

Examples include:

```text
Normal Pet
Encounter Chance: 10%

Aggressive Pet
Encounter Chance: 25%

Stealth Item
Encounter Chance: 2%

Battle Beacon Enabled
Encounter Chance: 75%
```

This allows the wireless behaviour to become part of the VPet gameplay rather than simply being a transport mechanism.

## Multiple Nearby Players

A badge may detect several players simultaneously.

For example:

```text
Alice    -48 dBm
Bob      -54 dBm
Charlie  -67 dBm
```

The badge should first filter devices based on:

* Valid VPet protocol.
* Battle availability.
* RSSI threshold.
* Cooldown status.

It can then select the strongest eligible signal as the preferred opponent.

This approximates choosing the physically closest badge.

## Collision Prevention

Both badges may discover one another at approximately the same time.

A deterministic rule should therefore decide which device is responsible for initiating the encounter.

For example:

```text
Badge A ID: 4821
Badge B ID: 7314

4821 < 7314

Badge A becomes encounter initiator.
```

The lower Player ID, Device ID, or another stable unique identifier can become the initiator.

This prevents both badges attempting to initiate independent connections simultaneously.

The process becomes:

```text
A detects B
B detects A
      ↓
Both compare IDs
      ↓
Only designated initiator performs encounter roll
      ↓
Initiator sends challenge
      ↓
Other badge acknowledges
```

## Encounter Cooldowns

Cooldowns are required to prevent two nearby players repeatedly triggering battles.

Suggested starting values:

```text
Global Encounter Cooldown:
2 minutes

Same Player Cooldown:
10 minutes
```

The global cooldown prevents a player immediately entering another random encounter after finishing one.

The per-player cooldown prevents two people standing next to each other from continuously battling.

These values should be configurable.

## Battle Connection

Once an encounter has been accepted, one ESP32 establishes a temporary BLE connection with the other.

The devices can then exchange the complete battle state.

Example handshake:

```text
HELLO
Protocol Version
Player ID
Pet ID
Pet Level
Pet Stats
Passive Abilities
Current HP
Battle Nonce
```

During combat, the devices exchange compact battle messages such as:

```text
MOVE:ATTACK
MOVE:DEFEND
MOVE:SPECIAL
MOVE:ITEM
MOVE:FLEE
```

The connection remains active until:

* The battle finishes.
* A player flees.
* Communication times out.
* One badge leaves communication range.

Afterwards, the BLE connection is closed and both badges return to their normal roaming state.

## Battle State Validation

Where practical, both badges should independently calculate the battle state rather than trusting one device to report the result.

For example:

```text
Player A selects ATTACK
        ↓
Move sent to Player B
        ↓
Both devices calculate damage
        ↓
Both calculate new HP
        ↓
State hashes compared
```

This makes simple firmware modification or forged packets less effective.

Random battle events should use a shared seed or agreed random value so that both devices calculate identical results.

## Badge States

A badge could maintain a simple state machine:

```text
ROAMING
   ↓
ENCOUNTER_DETECTED
   ↓
CHALLENGING
   ↓
CONNECTING
   ↓
BATTLING
   ↓
RESULT
   ↓
COOLDOWN
   ↓
ROAMING
```

During `ROAMING`, BLE advertising and scanning operate normally.

During `BATTLING`, automatic encounter scanning can be temporarily disabled.

## Player Experience

The intended experience is passive.

A player can simply wear the badge normally.

When another compatible badge comes within range, the devices may trigger an encounter:

```text
⚠ SIGNAL DETECTED

A hostile CYBERBUN appeared!

LV 12
██████████
```

Both badges can provide feedback using whichever hardware is available:

* Screen animation.
* LED flash.
* Vibration.
* Buzzer/sound.
* Notification icon.

The players then interact with their own badge to conduct the battle.

## Manual Battles

The same BLE infrastructure can also support deliberate challenges.

Possible battle modes:

### Random Encounter

Automatically triggered by proximity and encounter probability.

### Direct Challenge

The player selects a nearby badge from a menu and challenges it directly.

### Friend Battle

Players intentionally initiate a battle using a separate interaction such as NFC, IR, or a menu option.

This allows automatic encounters without making them the only way to play PvP.

## Future Extensions

BLE provides several opportunities beyond basic battles.

Potential future functionality includes:

* Player profiles.
* Friends lists.
* Win/loss records.
* Pet trading.
* Item trading.
* Nearby-player discovery.
* Rare encounter mechanics.
* Team battles.
* Event-specific bosses.
* BLE phone companion application.
* Leaderboard synchronisation when internet access becomes available.
* Firmware/configuration through a mobile application.

## Initial Prototype

The first implementation should remain deliberately simple:

```text
BLE advertising
+
BLE scanning
+
RSSI threshold
+
Unique Player ID
+
Encounter cooldown
+
Random encounter roll
+
Automatic BLE connection
+
Simple turn-based battle
```

More advanced features such as NFC, trading, anti-cheat mechanisms, phone integration, and persistent online profiles can then be added without fundamentally changing the underlying PvP architecture.
