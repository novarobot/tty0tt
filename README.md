# tty0tt
My tty0tty (https://github.com/freemed/tty0tty) fork and minimal modify!

# mytty0tty

`mytty0tty` egy Linux kernel modul, amely összekapcsolt virtuális soros port párokat hoz létre, az eredeti `tty0tty` projekt módosított változataként.

Az eredeti projekt forrása:
- https://github.com/freemed/tty0tty

Eredeti szerző:
- Luis Claudio Gamboa Lopes <lcgamboa@yahoo.com>

Köszönet az eredeti `tty0tty` projektért és az alapként szolgáló megoldásért Luis Claudio Gamboa Lopesnek.

## Licenc

Ez a projekt GPL v2 licencű.

Az eredeti kód GPL alapú projektből származik, és ez a módosított változat is GPL v2 alatt kerül közzétételre.

## Szerzők

- Eredeti szerző: Luis Claudio Gamboa Lopes <lcgamboa@yahoo.com>
- Jelenlegi módosított változat szerzője: Novarobot

## Mi ez?

A modul virtuális nullmodem-szerű soros port párokat hoz létre, például:

- `/dev/mytnt0` ↔ `/dev/mytnt1`
- `/dev/mytnt2` ↔ `/dev/mytnt3`
- `/dev/mytnt4` ↔ `/dev/mytnt5`
- `/dev/mytnt6` ↔ `/dev/mytnt7`

Az egyik oldalon küldött adat a pár másik oldalán jelenik meg.

## Az eredeti projekthez képesti működésbeli különbségek

Az eredeti `tty0tty` viselkedése röviden:

- `RTS` → túloldali `CTS`
- `DTR` → túloldali `DSR`
- `DTR` → túloldali `CD/DCD`

A `mytty0tty` módosított viselkedése:

- `RTS` → túloldali `CTS`
- `DTR` → túloldali `DSR`
- a `CD/DCD` már **nem** követi automatikusan a `DTR` jelet
- az `RI` és `CD/DCD` jelek külsőleg, páronként állíthatók sysfs fájlokon keresztül
- külön modul- és eszköznév-prefixet használ, ezért az eredeti `tty0tty` mellett is betölthető

## Sysfs vezérlés

Minden portpárhoz tartozik egy külön mask fájl:

- `/sys/kernel/mytty0tty/mytnt0_mytnt1_mask`
- `/sys/kernel/mytty0tty/mytnt2_mytnt3_mask`
- `/sys/kernel/mytty0tty/mytnt4_mytnt5_mask`
- `/sys/kernel/mytty0tty/mytnt6_mytnt7_mask`

A mask 4 bites érték, `0..15` között:

- bit0 = első oldal `CD/DCD`
- bit1 = első oldal `RI`
- bit2 = második oldal `CD/DCD`
- bit3 = második oldal `RI`

Példa a `mytnt0_mytnt1_mask` fájlra:

- `0` = minden alacsony
- `1` = csak `mytnt0` oldalon `CD/DCD`
- `2` = csak `mytnt0` oldalon `RI`
- `3` = `mytnt0` oldalon `CD/DCD + RI`
- `4` = csak `mytnt1` oldalon `CD/DCD`
- `8` = csak `mytnt1` oldalon `RI`
- `12` = `mytnt1` oldalon `CD/DCD + RI`
- `15` = mindkét oldalon `CD/DCD + RI`

## Tesztelt rendszer

A jelenlegi változat Debian 12 alatt lett tesztelve.

## Fordítás

Szükséges csomagok Debian 12 alatt:

```bash
sudo apt install build-essential linux-headers-$(uname -r)
