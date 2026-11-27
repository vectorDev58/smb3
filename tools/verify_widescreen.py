#!/usr/bin/env python3
"""Pruebas locales de enemigos y combate panoramico; requieren la ROM PRG0."""
import hashlib
import json
import re
import struct
import subprocess

from smb3 import ROOT, executable, read_rom


def captures(path):
    data = path.read_bytes()
    assert data[:12] == b"CYCFRAME\x02\x00\x00\x00"
    offset = 12
    while offset < len(data):
        header = struct.unpack_from("<9I", data, offset)
        start = offset + 36 + 288
        end = start + sum(header[3:])
        assert end <= len(data) and header[3:6] == (2048, 2048, 8192)
        yield header[0], data[start:start + 2048], data[start + 4096:start + 4096 + 8192]
        offset = end
    assert offset == len(data)


def coop_state(path):
    """The co-op block of a save state; offsets checked by static_assert in src/coop.h."""
    data = path.read_bytes()
    at = data.index(b"smb3.coop\0") + 64
    partner = at + 21  # CoopRam order: Player_XHi, then Player_X two bytes later
    return dict(present=data[at + 1], partner_is_p1=data[at + 2], bubble=data[at + 3], buttons=data[at + 4],
                ghost=struct.unpack_from("<H", data, at + 10)[0], x=data[partner + 2] * 256 + data[partner + 4])


def projectile_metadata(cart, slot, replacement=None):
    start, count = (slot * 13, 13) if slot < 8 else (104 + (slot - 8) * 6, 6)
    value, changed = 0, bytearray(cart)
    for i in range(count):
        bit = start + i
        address = 0x1f25 + bit // 8 if bit < 64 else 0x1f2e + (bit - 64) // 4
        mask = 1 << (bit % 8 if bit < 64 else 4 + (bit - 64) % 4)
        if cart[address] & mask: value |= 1 << i
        if replacement is not None:
            changed[address] = changed[address] | mask if replacement & (1 << i) else changed[address] & ~mask
    if replacement is not None:
        return {0x6000 + i: after for i, (before, after) in enumerate(zip(cart, changed)) if before != after}
    width = 5 if slot < 8 else 4
    return value & ((1 << width) - 1), value >> width


def main():
    rom, binary = ROOT / "private/baserom.nes", executable()
    read_rom(rom)
    out = ROOT / "verification/wide-actors"
    out.mkdir(parents=True, exist_ok=True)
    (out / "result.json").unlink(missing_ok=True)

    def run(name, *options):
        result = subprocess.run([str(binary), str(rom), "--no-save", *map(str, options)],
                                cwd=binary.parent, capture_output=True, timeout=180)
        (out / f"{name}.log").write_bytes(result.stdout + result.stderr)
        assert result.returncode == 0, f"Fallo en {name}; consulta su log"
        assert b"not committed" not in result.stderr, f"Llamada rechazada en {name}"
        failures = re.search(rb"(\d+) failed", result.stdout)
        assert failures and int(failures[1]) == 0, f"Una rutina no termino en {name}"

    route = out / "idle.txt"
    route.write_text("12 START\n13 NONE\n68 START\n69 NONE\n328 RIGHT\n329 NONE\n"
                     "345 UP\n346 NONE\n361 A\n362 NONE\n", encoding="ascii")
    initial = out / "initial.cycstate"
    run("initial", "--frames", 450, "--input", route, "--aspect", "native",
        "--save-state", f"449:{initial}")
    cases = (("native", "native", "1280x720", 256, 1), ("wide", "fit", "1280x720", 426, 2),
             ("21-9", "fit", "1680x720", 560, 3), ("ultrawide", "fit", "3840x1080", 854, 5))
    counts = {}
    chest_bits = None
    for name, aspect, resolution, width, expected in cases:
        memory = out / f"{name}.bin"
        run(name, "--load-state", initial, "--frames", 475, "--aspect", aspect,
            "--present-size", resolution, "--present-out", out / f"{name}.png",
            "--frame-log", memory, "--frame-log-frames", "474:474")
        _, ram, cart = next(captures(memory))
        current_chests = bytes(value & 15 for value in cart[0x1f2e:0x1f3e])
        if chest_bits is None: chest_bits = current_chests
        assert current_chests == chest_bits, "La ampliacion altero los bits de los cofres"
        actors = [i for i in range(5) if ram[0x661 + i]]
        assert len(actors) == expected, (name, actors)
        for slot in actors:
            index = ram[0x659 + slot]
            assert index < 48 and ram[0x671 + slot] == cart[0x1b41 + index * 3], "Objeto cargado incorrectamente"
            assert ram[0x651 + slot] == 0, "Un enemigo visible conserva el recorte NES"
        assert struct.unpack_from(">II", (out / f"{name}.png").read_bytes(), 16) == (width, 240)
        counts[width] = len(actors)
    print("Cuatro anchos: 1, 2, 3 y 5 actores activos, con identidad y visibilidad comprobadas.", flush=True)

    options = ["--load-state", initial, "--frames", 700, "--aspect", "fit", "--present-size", "1280x720"]
    run("attacks", *options, "--hash-out", out / "attacks.hash", "--frame-log", out / "attacks.bin",
        "--frame-log-frames", "450:699", "--save-state", f"625:{out / 'attacks.cycstate'}")
    run("attacks-interpreter", *options, "--interp-only", "--hash-out", out / "attacks-interpreter.hash")
    assert (out / "attacks.hash").read_bytes() == (out / "attacks-interpreter.hash").read_bytes()
    run("attacks-restored", "--load-state", out / "attacks.cycstate", "--frames", 700,
        "--aspect", "fit", "--present-size", "1280x720", "--hash-out", out / "attacks-restored.hash")
    original = (out / "attacks.hash").read_text().splitlines()
    restored = (out / "attacks-restored.hash").read_text().splitlines()
    first = int(restored[0].split()[0])
    assert [line for line in original if int(line.split()[0]) >= first] == restored
    shots = []
    checkpoint_cart = None
    for frame, ram, cart in captures(out / "attacks.bin"):
        assert bytes(value & 15 for value in cart[0x1f2e:0x1f3e]) == chest_bits
        if frame == 625: checkpoint_cart = cart
        for slot in range(8):
            high, tag = projectile_metadata(cart, slot)
            if cart[0x1fc6 + slot] == tag == 5 and high < 16:
                shots.append((frame, ram[0x5c9 + slot] + high * 256))
    assert any(x >= 256 for _, x in shots) and any(x < 256 for _, x in shots)
    assert all(abs(b[1] - a[1]) <= 4 for a, b in zip(shots, shots[1:]) if b[0] == a[0] + 1)
    print("Ataque de piranha cruza 256 sin desaparecer; interprete y guardar/cargar coinciden.", flush=True)

    for name, world_high, expected_hit in (("distant-hit", 1, False), ("near-hit", 0, True)):
        controls = out / f"{name}.txt"
        patches = projectile_metadata(checkpoint_cart, 3, world_high | (5 << 5))
        controls.write_text("626 HOLD 05CC 18\n626 HOLD 05C2 90\n626 HOLD 7FD8 01\n626 HOLD 7FC9 05\n" +
                            "".join(f"626 HOLD {address:04X} {value:02X}\n" for address, value in patches.items()), encoding="ascii")
        run(name, "--load-state", out / "attacks.cycstate", "--frames", 635, "--input", controls,
            "--aspect", "fit", "--present-size", "1280x720", "--frame-log", out / f"{name}.bin",
            "--frame-log-frames", "634:634")
        _, ram, _ = next(captures(out / f"{name}.bin"))
        assert bool(ram[0xf1]) == expected_hit, f"Colision incorrecta en {name}"

    # QA asistida: los dos emisores se colocan fuera del cuadro NES en la misma sala.
    for name, kind in (("bullet", 0xbc), ("cannonball", 0xc2)):
        controls = out / f"{name}.txt"
        controls.write_text(f"450 HOLD 7B44 {kind:02X}\n450 HOLD 7B45 17\n450 HOLD 7B46 19\n", encoding="ascii")
        run(name, "--load-state", initial, "--frames", 650, "--input", controls,
            "--aspect", "fit", "--present-size", "1280x720", "--frame-log", out / f"{name}.bin",
            "--frame-log-frames", "450:649", "--present-out", out / f"{name}.png")
        fired = False
        for _, ram, cart in captures(out / f"{name}.bin"):
            if name == "bullet":
                fired |= any(ram[0x661 + i] and ram[0x671 + i] == 0x78 and
                             ram[0x76 + i] * 256 + ram[0x91 + i] >= 256 for i in range(8))
            else:
                fired |= any(cart[0x1fc6 + i] == 11 and projectile_metadata(cart, i)[0] < 16 and
                             projectile_metadata(cart, i)[0] * 256 + ram[0x5c9 + i] >= 256 for i in range(8))
        assert fired, f"{name} no disparo en el margen panoramico"
    print("Colisiones cercanas/lejos y emisores Bullet Bill/cannonball comprobados (fixtures asistidas).", flush=True)
    fire_route = out / "fire-start.txt"
    fire_route.write_text(route.read_text().replace("69 NONE\n", "69 NONE\n69 HOLD 0746 02\n"), encoding="ascii")
    fire_state = out / "fire-start.cycstate"
    run("fire-start", "--frames", 450, "--input", fire_route, "--aspect", "native",
        "--save-state", f"449:{fire_state}")
    controls = out / "player-fire.txt"
    controls.write_text("450 HOLD 0012 03\n450 HOLD 00FD 70\n450 HOLD 0075 04\n450 HOLD 0090 60\n"
                        "450 HOLD 0087 01\n450 HOLD 00A2 70\n450 HOLD 0553 FF\n"
                        "450 NONE\n452 B\n453 NONE\n", encoding="ascii")
    run("player-fire", "--load-state", fire_state, "--frames", 525, "--input", controls,
        "--aspect", "fit", "--present-size", "1280x720", "--frame-log", out / "player-fire.bin",
        "--frame-log-frames", "450:524", "--present-out", out / "player-fire.png")
    player_outside = False
    for _, ram, cart in captures(out / "player-fire.bin"):
        camera = ram[0x12] * 256 + ram[0xfd]
        player_outside |= any(cart[0x1ce1 + i] and projectile_metadata(cart, i + 8)[1] == cart[0x1ce1 + i] and
                              projectile_metadata(cart, i + 8)[0] * 256 + cart[0x1ce5 + i] >= camera + 256 for i in range(2))
    assert player_outside, "Los disparos de Mario siguen limitados al cuadro NES"
    print("El fuego de Mario tambien atraviesa el antiguo borde (posicion y traje asistidos para QA).", flush=True)

    shell = ("450 POKE 0661-0668 00\n450 HOLD 7B10-7B3F 80\n450 POKE 0674 6D\n"
             "450 POKE 0664 05\n450 POKE 0079 00\n450 POKE 0094 F8\n450 POKE 008B 01\n"
             "450 POKE 00A6 90\n450 POKE 00C1 30\n450 POKE 0675 72\n450 POKE 0665 02\n"
             "450 POKE 007A 01\n450 POKE 0095 10\n450 POKE 008C 01\n450 POKE 00A7 90\n"
             "450 POKE 00C2 00\n450 POKE 065C-065D FF\n450 HOLD 0553 FF\n")
    def fixture(name, text, frames=475, state=initial, extra=()):
        controls = out / f"{name}.txt"
        controls.write_text(text, encoding="ascii")
        run(name, "--load-state", state, "--frames", frames, "--input", controls,
            "--aspect", "fit", "--present-size", "1680x720", "--present-out", out / f"{name}.png",
            "--frame-log", out / f"{name}.bin", "--frame-log-frames", f"450:{frames - 1}", *extra)
        return list(captures(out / f"{name}.bin"))

    near = fixture("shell-near", shell)
    assert any(ram[0x665] == 6 for _, ram, _ in near), "El caparazon no golpea al otro lado de 256"
    far = fixture("shell-far", shell + "450 POKE 007A 02\n")
    assert all(ram[0x665] == 2 for _, ram, _ in far), "El caparazon golpea a una pagina de distancia"
    wall = shell.replace("0094 F8", "0094 E0") + "450 POKE 0665 00\n450 POKE 6340 67\n"
    broken = fixture("shell-wall", wall)
    assert broken[0][2][0x340] == 0x67 and broken[-1][2][0x340] == 0x80, "No rompe el ladrillo en el borde NES"
    retained = fixture("actor-retained", "450 POKE 0661-0668 00\n450 HOLD 7B10-7B3F 80\n"
        "450 POKE 0674 72\n450 POKE 0664 02\n450 POKE 065C 02\n450 HOLD 0079 01\n"
        "450 HOLD 0094 90\n450 HOLD 008B 00\n450 HOLD 00A6 D8\n450 HOLD 0553 FF\n", frames=500)
    assert all(ram[0x664] == 2 and ram[0x65c] == 2 for _, ram, _ in retained), "Se reinicia un enemigo visible"
    falling = ("450 POKE 0661-0668 00\n450 HOLD 7B10-7B3F 80\n450 UNHOLD 7B12\n"
        "450 POKE 7B12 80\n450 POKE 7B47 72\n450 POKE 7B48 19\n450 POKE 7B49 19\n"
        "450 POKE 0674 72\n450 POKE 0664 02\n450 POKE 065C 02\n450 POKE 0079 01\n"
        "450 POKE 0094 90\n450 POKE 008B 01\n450 POKE 00A6 F0\n450 POKE 00D3 40\n450 HOLD 0553 FF\n")
    dormant = fixture("actor-fallen", falling, frames=500)
    assert all(not any(ram[0x661 + i] and ram[0x659 + i] == 2 for i in range(5))
               and cart[0x1b12] & 0xc0 == 0xc0 for frame, ram, cart in dormant if frame >= 460), "Reaparece al caer dentro del panoramico"
    returned = fixture("actor-returned", falling + "500 POKE 0012 03\n500 POKE 00FD 20\n"
        "500 POKE 0075 03\n500 POKE 0090 A0\n510 POKE 0012 00\n510 POKE 00FD 00\n"
        "510 POKE 0075 00\n510 POKE 0090 80\n", frames=535)
    assert any(ram[0x661 + i] and ram[0x659 + i] == 2 for frame, ram, _ in returned if frame >= 515
               for i in range(5)), "No vuelve a activarse al salir de la zona y regresar"
    print("Caparazones: impacto, distancia real y ladrillo en 256; enemigo visible conservado (QA asistida).", flush=True)

    # Co-op: controller 2 runs the partner through the ROM's own player and object code.
    stomp = "450 2:RIGHT\n530 2:RIGHT+A\n544 2:RIGHT\n"
    stomped = fixture("coop-stomp", stomp, frames=600, extra=("--coop", "--hash-out", out / "coop-stomp.hash"))
    fixture("coop-stomp-interpreter", stomp, frames=600,
            extra=("--coop", "--interp-only", "--hash-out", out / "coop-stomp-interpreter.hash"))
    assert (out / "coop-stomp.hash").read_bytes() == (out / "coop-stomp-interpreter.hash").read_bytes()
    final = stomped[-1][1]
    assert final[0x665] != 2 and final[0x715:0x718] != bytes(3), "El compañero no pisa al Goomba"
    assert all(ram[0x75] * 256 + ram[0x90] == 24 for _, ram, _ in stomped), "El mando 2 movió al jugador 1"
    # Player 1 walks into the Goomba: the partner takes the lead instead of ending the level.
    takeover = fixture("coop-takeover", "450 1:RIGHT\n", frames=660, extra=("--coop",))
    assert all(not ram[0xf1] and not ram[0x14] for _, ram, _ in takeover), "La muerte del líder terminó el nivel"
    assert not any(ram[0x4f4] & 1 for _, ram, _ in takeover), "Sonó la música de muerte con el compañero vivo"
    print("Cooperativo: pisotón del compañero, mandos separados, relevo al morir el líder e intérprete igual.", flush=True)

    # As in Super Mario Bros. Wonder: a ghost the lead touches, a bubble back to the lead, shared dangers.
    def coop_states(name, text, frames, checkpoints, pokes=""):
        saves = [arg for frame in checkpoints for arg in ("--save-state", f"{frame}:{out / f'{name}-{frame}.cycstate'}")]
        fixture(name, text + pokes, frames=frames, extra=("--coop", *saves))
        return {frame: coop_state(out / f"{name}-{frame}.cycstate") for frame in checkpoints}
    revive = coop_states("coop-ghost", "450 1:RIGHT\n450 2:NONE\n580 2:RIGHT\n604 2:RIGHT+A\n630 2:RIGHT\n680 2:NONE\n",
                         760, (600, 740))
    assert revive[600]["ghost"] and revive[600]["partner_is_p1"], "El jugador 1 no quedó como fantasma"
    assert revive[740]["present"] and not revive[740]["ghost"] and not revive[740]["bubble"], "Tocar al fantasma no lo revive"
    behind = coop_states("coop-bubble", "450 1:RIGHT+B\n480 1:RIGHT+B+A\n505 1:RIGHT+B\n560 1:RIGHT+B+A\n"
                         "580 1:RIGHT+B\n640 1:NONE\n450 2:NONE\n", 760, (620, 740), pokes="450 HOLD 0553 FF\n")
    assert behind[620]["bubble"] and not behind[620]["partner_is_p1"], "El compañero rezagado no va en burbuja"
    assert behind[740]["present"] and not behind[740]["bubble"], "La burbuja no llega al líder"
    shot = coop_states("coop-fireball", "450 1:NONE\n450 2:RIGHT\n530 2:RIGHT+A\n544 2:RIGHT\n640 2:NONE\n",
                       900, (840, 899), pokes="450 HOLD 0553 FF\n")
    assert not shot[840]["ghost"] and shot[899]["ghost"] and not shot[899]["partner_is_p1"], "El fuego de la piraña no alcanza al compañero"
    print("Cooperativo como en Wonder: fantasma revivido al tocarlo, burbuja de vuelta y fuego enemigo para ambos.", flush=True)

    for number, map_y, tileset in ((3, 0x20, 1), (4, 0x40, 4)):
        controls = out / f"level-{number}.txt"
        controls.write_text(f"12 START\n13 NONE\n68 START\n69 NONE\n328 POKE 0079 A0\n"
                            f"328 POKE 0075 {map_y:02X}\n345 A\n346 NONE\n", encoding="ascii")
        state = out / f"level-{number}.cycstate"
        run(f"level-{number}", "--frames", 450, "--input", controls, "--aspect", "native",
            "--save-state", f"449:{state}", "--frame-log", out / f"level-{number}.bin",
            "--frame-log-frames", "449:449")
        assert next(captures(out / f"level-{number}.bin"))[1][0x70a] == tileset
        moves = "450 RIGHT+B\n480 RIGHT+B+A\n496 RIGHT+B\n540 RIGHT+B+A\n556 RIGHT+B\n600 NONE\n"
        fixture(f"level-{number}-native", moves, frames=650, state=state,
                extra=("--hash-out", out / f"level-{number}.hash"))
        fixture(f"level-{number}-interpreter", moves, frames=650, state=state,
                extra=("--interp-only", "--hash-out", out / f"level-{number}-interpreter.hash"))
        assert (out / f"level-{number}.hash").read_bytes() == (out / f"level-{number}-interpreter.hash").read_bytes()
    auto = fixture("autoscroll-edge", "450 POKE 0075 00\n450 POKE 0090 F0\n450 HOLD 0087 01\n"
        "450 HOLD 00A2 00\n450 POKE 00BD 28\n450 HOLD 0553 FF\n450 RIGHT+B\n",
        frames=480, state=out / "level-4.cycstate")
    assert auto[-1][1][0x75] * 256 + auto[-1][1][0x90] > 300
    assert all(ram[0xbd] == 40 and not ram[0xf1] for _, ram, _ in auto), "Barrera NES en el autoscroll"
    print("1-3 y 1-4: nativo/interprete coinciden; autoscroll supera el antiguo limite sin frenar.", flush=True)
    report = dict(result="passed", actor_counts=counts, enemy_projectiles_cross_native_edge=True,
                  native_interpreter_match=True, save_load_continuation=True, collision_distance_guard=True,
                  cannons=2, player_projectiles_cross_native_edge=True, chest_flags_preserved=True,
                  shell_collision_and_bricks=True, coop_partner_and_takeover=True,
                  coop_ghost_bubble_and_projectiles=True, visible_actor_retained=True, fallen_actor_waits_for_reentry=True,
                  levels_1_3_and_1_4_execution_match=True, autoscroll_boundary_expanded=True,
                  complete_campaign_playthrough=False,
                  executable_sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
