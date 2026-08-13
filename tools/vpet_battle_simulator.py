#!/usr/bin/env python3
"""Simulate a second VPet badge for Ghostwire's BLE PvP battle system.

Runs the GATT server side of the wire protocol in
include/familiar_battle_service.h so a real Cardputer can discover this
script (via its Familiar screen's Tab -> PvP Battle -> Find Opponent) and
battle against it. The simulator is always the *responder*, never the
challenger -- the real badge always connects to it, not the other way
around -- so it only needs BLE peripheral/GATT-server support, never
scanning or connecting out.

It intentionally does not try to broadcast the VPET manufacturer-data
payload the firmware itself sends (playerId/stage/level baked into the
advertisement) -- BLE peripheral libraries don't expose custom
manufacturer data as consistently across platforms as NimBLE does on the
device side. Registering the GATT service still advertises its UUID on
every platform, which the firmware's scan/discovery already accepts as a
fallback (see parseAdvertisement() in familiar_battle_service.cpp) -- the
simulator just shows up as "Unknown VPet" in the Find Opponent list until
you connect, at which point the real HELLO handshake exchanges its actual
stats, same as it would for a second real badge.

Requires `bless`, the peripheral/GATT-server counterpart to `bleak`
(`bleak` itself is central-only -- it can scan and connect, but can't
advertise or run a GATT server, so it's the wrong tool for this side):

    pip install bless

Usage:
    python3 vpet_battle_simulator.py
    python3 vpet_battle_simulator.py --level 20 --stage 3 --interactive
"""

from __future__ import annotations

import argparse
import asyncio
import random
import struct
import sys

try:
    from bless import (
        BlessServer,
        BlessGATTCharacteristic,
        GATTCharacteristicProperties,
        GATTAttributePermissions,
    )
except ImportError:
    print("This script needs `bless`: pip install bless", file=sys.stderr)
    sys.exit(1)


# Same Nordic UART Service UUID trio the firmware uses (see
# familiar_battle_service.cpp) -- a de facto industry-wide UUID scheme,
# safe to reuse directly, not anything Ghostwire- or vendor-specific.
SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
WRITE_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NOTIFY_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

MSG_HELLO = 0x01
MSG_MOVE = 0x02

ATTACK, DEFEND, SPECIAL, FLEE = range(4)
MOVE_NAMES = {ATTACK: "Attack", DEFEND: "Defend", SPECIAL: "Special", FLEE: "Flee"}


def derive_max_hp(level: int) -> int:
    return 20 + level * 3


def derive_attack(level: int, stage_index: int) -> int:
    return min(255, 4 + level + stage_index * 2)


def derive_defense(level: int, stage_index: int) -> int:
    return min(255, 2 + level // 2 + stage_index)


def c_div(a: int, b: int) -> int:
    """Truncating division matching C++'s `/` on int32_t (round toward
    zero), not Python's floor-dividing `//` -- keeps resolve_turn()'s
    arithmetic bit-for-bit consistent with resolveTurnIfReady() in
    familiar_battle_service.cpp for the rare negative-intermediate cases
    (a very tanky defender vs. a weak attacker)."""
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


class Xorshift32:
    """Mirrors FamiliarBattleService::nextRandom() exactly, so the turn-by-
    turn damage this script prints matches what the real badge computes
    from the same shared seed."""

    def __init__(self, seed: int):
        self.state = (seed & 0xFFFFFFFF) or 1

    def next(self) -> int:
        x = self.state
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        self.state = x & 0xFFFFFFFF
        return self.state


class Battle:
    """One battle's worth of state -- a fresh instance is created each time
    a HELLO arrives, mirroring how the firmware's FamiliarBattleService
    resets between battles."""

    def __init__(self, player_id: int, stage_index: int, level: int, interactive: bool):
        self.player_id = player_id
        self.stage_index = stage_index
        self.level = level
        self.max_hp = derive_max_hp(level)
        self.hp = self.max_hp
        self.attack = derive_attack(level, stage_index)
        self.defense = derive_defense(level, stage_index)
        self.interactive = interactive

        self.opponent_id = 0
        self.opponent_stage = 0
        self.opponent_level = 0
        self.opponent_hp = 0
        self.opponent_max_hp = 0
        self.opponent_attack = 0
        self.opponent_defense = 0

        self.prng: Xorshift32 | None = None
        self.turn = 1
        self.my_move: int | None = None
        self.opponent_move: int | None = None
        self.done = False

    def handle_hello(self, payload: bytes) -> bytes:
        (self.opponent_id, self.opponent_stage, self.opponent_level,
         opponent_hp, seed) = struct.unpack(">IBBHI", payload)
        self.opponent_max_hp = derive_max_hp(self.opponent_level)
        self.opponent_hp = opponent_hp or self.opponent_max_hp
        self.opponent_attack = derive_attack(self.opponent_level, self.opponent_stage)
        self.opponent_defense = derive_defense(self.opponent_level, self.opponent_stage)
        self.prng = Xorshift32(seed)
        print(f"[battle] Challenger connected: player {self.opponent_id:#010x}, "
              f"Lv{self.opponent_level} stage {self.opponent_stage} "
              f"(HP {self.opponent_hp}/{self.opponent_max_hp})")
        # The seed field in this response is unused by the real badge --
        # only the challenger's originally-chosen seed ever drives the
        # shared PRNG (see handleIncomingMessage()'s HELLO case).
        return struct.pack(">BIBBHI", MSG_HELLO, self.player_id,
                           self.stage_index, self.level, self.hp, 0)

    def choose_move(self) -> int:
        if self.interactive:
            while True:
                raw = input("Your move [a]ttack/[d]efend/[s]pecial/[f]lee: ").strip().lower()
                if raw[:1] in ("a", "d", "s", "f"):
                    return {"a": ATTACK, "d": DEFEND, "s": SPECIAL, "f": FLEE}[raw[:1]]
        # Simple canned AI so a full battle can run unattended.
        return random.choices([ATTACK, DEFEND, SPECIAL], weights=[60, 25, 15])[0]

    def resolve_turn(self):
        assert self.prng is not None
        order = [("me", self.my_move), ("opp", self.opponent_move)]
        # Fixed processing order (lower playerId first) so both sides draw
        # from the shared PRNG in the same sequence -- symmetric with
        # resolveTurnIfReady()'s `myPlayerId_ > opponent_.playerId` swap.
        if self.player_id > self.opponent_id:
            order.reverse()
        my_defending = self.my_move == DEFEND
        opponent_defending = self.opponent_move == DEFEND

        for who, move in order:
            if move not in (ATTACK, SPECIAL):
                continue
            atk = self.attack if who == "me" else self.opponent_attack
            deff = self.opponent_defense if who == "me" else self.defense
            target_defending = opponent_defending if who == "me" else my_defending
            roll = self.prng.next()
            is_special = move == SPECIAL
            label = "You" if who == "me" else "Opponent"
            if is_special and roll % 100 < 15:
                possessive = "Your" if who == "me" else "Opponent's"
                print(f"[battle] {possessive} Special missed!")
                continue
            damage = atk - c_div(deff, 2)
            if is_special:
                damage = c_div(damage * 3, 2)
            variance = (roll % 41) - 20  # +/-20%
            damage += c_div(damage * variance, 100)
            if target_defending:
                damage = c_div(damage, 2)
            damage = max(1, damage)
            if who == "me":
                self.opponent_hp = max(0, self.opponent_hp - damage)
            else:
                self.hp = max(0, self.hp - damage)
            action = "Special" if is_special else "Attack"
            print(f"[battle] {label} used {action} for {damage} dmg. "
                 f"(You {self.hp}/{self.max_hp}  Opp {self.opponent_hp}/{self.opponent_max_hp})")

        self.turn += 1
        self.my_move = None
        self.opponent_move = None
        if self.hp == 0 or self.opponent_hp == 0:
            self.done = True
            print("[battle] You win!" if self.opponent_hp == 0 else "[battle] You lose.")


async def run(args: argparse.Namespace) -> None:
    battle: Battle | None = None
    server: BlessServer

    def notify(data: bytes) -> None:
        characteristic = server.get_characteristic(NOTIFY_CHAR_UUID)
        characteristic.value = bytearray(data)
        server.update_value(SERVICE_UUID, NOTIFY_CHAR_UUID)

    async def submit_my_move_if_needed() -> None:
        assert battle is not None
        if battle.my_move is not None or battle.done:
            return
        move = await asyncio.get_event_loop().run_in_executor(None, battle.choose_move)
        battle.my_move = move
        print(f"[battle] You chose {MOVE_NAMES.get(move, '?')}.")
        if move == FLEE:
            notify(bytes([MSG_MOVE, FLEE]))
            battle.done = True
            print("[battle] You fled the battle.")
            return
        notify(bytes([MSG_MOVE, move]))
        if battle.opponent_move is not None:
            battle.resolve_turn()

    async def handle_write(data: bytes) -> None:
        nonlocal battle
        if not data:
            return
        msg_type = data[0]
        if msg_type == MSG_HELLO:
            if len(data) < 13:
                return
            battle = Battle(args.player_id, args.stage, args.level, args.interactive)
            notify(battle.handle_hello(data[1:13]))
            await submit_my_move_if_needed()
        elif msg_type == MSG_MOVE:
            if battle is None or battle.done or len(data) < 2:
                return
            move = data[1]
            if move == FLEE:
                print("[battle] Opponent fled the battle.")
                battle.done = True
                return
            battle.opponent_move = move
            print(f"[battle] Opponent chose {MOVE_NAMES.get(move, '?')}.")
            if battle.my_move is None:
                await submit_my_move_if_needed()
            else:
                battle.resolve_turn()

    def write_request(characteristic: BlessGATTCharacteristic, value, **_kwargs) -> None:
        # bless's write callback runs synchronously; hand off to the event
        # loop rather than blocking it (handle_write can end up waiting on
        # stdin in --interactive mode).
        asyncio.get_event_loop().create_task(handle_write(bytes(value)))

    def read_request(characteristic: BlessGATTCharacteristic, **_kwargs):
        return characteristic.value

    server_kwargs = {}
    if args.adapter:
        server_kwargs["adapter"] = args.adapter
    server = BlessServer(name=args.name, **server_kwargs)
    server.read_request_func = read_request
    server.write_request_func = write_request

    await server.add_new_service(SERVICE_UUID)
    await server.add_new_characteristic(
        SERVICE_UUID, WRITE_CHAR_UUID,
        GATTCharacteristicProperties.write
        | GATTCharacteristicProperties.write_without_response,
        None,
        GATTAttributePermissions.writeable,
    )
    await server.add_new_characteristic(
        SERVICE_UUID, NOTIFY_CHAR_UUID,
        GATTCharacteristicProperties.notify | GATTCharacteristicProperties.read,
        bytearray(1),
        GATTAttributePermissions.readable,
    )

    await server.start()
    print(f"[sim] {args.name}: Lv{args.level} stage {args.stage} "
         f"(player id {args.player_id:#010x})")
    print("[sim] Waiting for a Cardputer to challenge via Find Opponent...")
    print("[sim] Ctrl+C to stop.")
    try:
        while True:
            await asyncio.sleep(1)
    except (KeyboardInterrupt, asyncio.CancelledError):
        pass
    finally:
        await server.stop()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--name", default="Ghostwire VPet Sim",
                        help="BLE device name to advertise")
    parser.add_argument("--player-id", type=lambda v: int(v, 0), default=0xC0FFEE,
                        help="fake player ID shown in the Find Opponent list "
                             "before you connect (default: 0xC0FFEE)")
    parser.add_argument("--stage", type=int, default=2, choices=range(6),
                        help="evolution stage index (0=Script Sprite .. "
                             "5=Hex Familiar), affects attack/defense")
    parser.add_argument("--level", type=int, default=10,
                        help="Familiar level (1-99), affects HP/attack/defense")
    parser.add_argument("--interactive", action="store_true",
                        help="prompt on stdin for each move instead of "
                             "playing a canned AI")
    parser.add_argument("--adapter", default=None,
                        help="BlueZ adapter to advertise from (e.g. hci1) "
                             "-- only meaningful on Linux with more than "
                             "one Bluetooth adapter. bless's own default is "
                             "hci0 specifically, not whatever your system's "
                             "actual default adapter is (`bluetoothctl "
                             "list` shows [default]) -- if the real badge "
                             "can't find this script, try pointing it at "
                             "your default adapter explicitly.")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
