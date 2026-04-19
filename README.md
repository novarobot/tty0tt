# tty0tt

My tty0tty (https://github.com/freemed/tty0tty) fork and minimal modify!

# mytty0tty

## English

`mytty0tty` is a Linux kernel module derived from the original `tty0tty` project.
It creates linked virtual serial port pairs and adds external per-pair control for the `CD/DCD` and `RI` modem signals through sysfs.

The original project is available here:
[https://github.com/freemed/tty0tty](https://github.com/freemed/tty0tty)

Original author:
Luis Claudio Gamboa Lopes [lcgamboa@yahoo.com](mailto:lcgamboa@yahoo.com)

Special thanks to Luis Claudio Gamboa Lopes for the original `tty0tty` project and for the codebase this project is based on.

## License

This project is licensed under **GPL v2**.

Because this project is a modified derivative of the original GPL-based `tty0tty` code, this modified version is also distributed under GPL v2.

## Authors

* Original author: Luis Claudio Gamboa Lopes [lcgamboa@yahoo.com](mailto:lcgamboa@yahoo.com)
* Current modified version author: Novarobot

## What is this?

This module creates linked virtual null-modem style serial port pairs, for example:

* `/dev/mytnt0` ↔ `/dev/mytnt1`
* `/dev/mytnt2` ↔ `/dev/mytnt3`
* `/dev/mytnt4` ↔ `/dev/mytnt5`
* `/dev/mytnt6` ↔ `/dev/mytnt7`

Data written to one side appears on the paired side.

## Functional differences compared to the original project

Original `tty0tty` behavior, in short:

* `RTS` → peer `CTS`
* `DTR` → peer `DSR`
* `DTR` → peer `CD/DCD`

Modified `mytty0tty` behavior:

* `RTS` → peer `CTS`
* `DTR` → peer `DSR`
* `CD/DCD` no longer automatically follows `DTR`
* `RI` and `CD/DCD` can be controlled externally, per pair, through sysfs
* separate module name and separate device prefix are used, so it can coexist with the original `tty0tty` module

## Sysfs control

Each serial port pair has its own mask file:

* `/sys/kernel/mytty0tty/mytnt0_mytnt1_mask`
* `/sys/kernel/mytty0tty/mytnt2_mytnt3_mask`
* `/sys/kernel/mytty0tty/mytnt4_mytnt5_mask`
* `/sys/kernel/mytty0tty/mytnt6_mytnt7_mask`

The mask is a 4-bit value from `0` to `15`:

* bit0 = first side `CD/DCD`
* bit1 = first side `RI`
* bit2 = second side `CD/DCD`
* bit3 = second side `RI`

For example, for `mytnt0_mytnt1_mask`:

* `0` = all low
* `1` = only `mytnt0` side `CD/DCD`
* `2` = only `mytnt0` side `RI`
* `3` = `mytnt0` side `CD/DCD + RI`
* `4` = only `mytnt1` side `CD/DCD`
* `8` = only `mytnt1` side `RI`
* `12` = `mytnt1` side `CD/DCD + RI`
* `15` = both sides `CD/DCD + RI`

Example command:

```bash
printf '15\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

## Tested system

This version was tested on **Debian 12**.

## Build

Required packages on Debian 12:

```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

Build the module:

```bash
make
```

## Load the module

Example: create 4 linked serial port pairs:

```bash
sudo insmod ./mytty0tty.ko pairs=4
```

Check that it is loaded:

```bash
lsmod | grep mytty0tty
ls -l /dev/mytnt*
find /sys/kernel/mytty0tty -maxdepth 1 -type f | sort
```

## Installation

Install the module into the system module tree:

```bash
sudo make install
```

Enable automatic loading at boot by editing `/etc/modules` or by creating a modules-load configuration file:

```bash
echo mytty0tty | sudo tee /etc/modules-load.d/mytty0tty.conf
```

Alternatively, you can add the module name manually to `/etc/modules`.

```bash
echo 'mytty0tty' /etc/modules
```
Load the installed module manually:

```bash
sudo modprobe mytty0tty
```

## Usage

### Data transfer test

Terminal 1:

```bash
cat /dev/mytnt1
```

Terminal 2:

```bash
echo HELLO > /dev/mytnt0
```

### Read current mask

```bash
cat /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

### Set a new mask

```bash
printf '15\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

### Read it back

```bash
cat /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

### More examples

Enable `CD/DCD + RI` only on the `mytnt0` side:

```bash
printf '3\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

Enable `CD/DCD + RI` only on the `mytnt1` side:

```bash
printf '12\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

Enable `CD/DCD + RI` on both sides:

```bash
printf '15\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

Disable everything:

```bash
printf '0\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

## Unload / remove the module

If the module was loaded manually or with `modprobe`, unload it from the running kernel with:

```bash
sudo modprobe -r mytty0tty
```

Alternative:

```bash
sudo rmmod mytty0tty
```

If the module was also installed into the system module tree, remove the installed files too:

```bash
sudo modprobe -r mytty0tty
sudo rm -f /lib/modules/$(uname -r)/extra/mytty0tty.ko
sudo rm -f /etc/modules-load.d/mytty0tty.conf
sudo depmod -a
```

If you added the module name manually to `/etc/modules`, remove that line from `/etc/modules` as well.


## Notes

The goal of this project is to preserve the useful behavior of the original `tty0tty` driver while adding:

* a separate module name
* a separate device prefix
* external shell-based control of the `CD/DCD` and `RI` lines

---

## Magyar

A `mytty0tty` egy Linux kernel modul, amely az eredeti `tty0tty` projekt módosított változata.
Összekapcsolt virtuális soros port párokat hoz létre, és páronként külső vezérlést ad a `CD/DCD` és `RI` modemjelekhez sysfs-en keresztül.

Az eredeti projekt itt érhető el:
[https://github.com/freemed/tty0tty](https://github.com/freemed/tty0tty)

Eredeti szerző:
Luis Claudio Gamboa Lopes [lcgamboa@yahoo.com](mailto:lcgamboa@yahoo.com)

Külön köszönet Luis Claudio Gamboa Lopesnek az eredeti `tty0tty` projektért és azért a kódbázisért, amelyre ez a projekt épül.

## Licenc

Ez a projekt **GPL v2** licenc alatt érhető el.

Mivel ez a projekt az eredeti GPL alapú `tty0tty` kód módosított származéka, ezért ez a változat is GPL v2 alatt kerül közzétételre.

## Szerzők

* Eredeti szerző: Luis Claudio Gamboa Lopes [lcgamboa@yahoo.com](mailto:lcgamboa@yahoo.com)
* Jelenlegi módosított változat szerzője: Novarobot

## Mi ez?

A modul összekapcsolt virtuális nullmodem-szerű soros port párokat hoz létre, például:

* `/dev/mytnt0` ↔ `/dev/mytnt1`
* `/dev/mytnt2` ↔ `/dev/mytnt3`
* `/dev/mytnt4` ↔ `/dev/mytnt5`
* `/dev/mytnt6` ↔ `/dev/mytnt7`

Az egyik oldalon küldött adat a pár másik oldalán jelenik meg.

## Működésbeli különbségek az eredeti projekthez képest

Az eredeti `tty0tty` viselkedése röviden:

* `RTS` → túloldali `CTS`
* `DTR` → túloldali `DSR`
* `DTR` → túloldali `CD/DCD`

A módosított `mytty0tty` viselkedése:

* `RTS` → túloldali `CTS`
* `DTR` → túloldali `DSR`
* a `CD/DCD` már nem követi automatikusan a `DTR` jelet
* az `RI` és `CD/DCD` jelek külsőleg, páronként állíthatók sysfs fájlokon keresztül
* külön modulnevet és külön eszközprefixet használ, ezért az eredeti `tty0tty` modullal együtt is betölthető

## Sysfs vezérlés

Minden portpárhoz tartozik egy külön mask fájl:

* `/sys/kernel/mytty0tty/mytnt0_mytnt1_mask`
* `/sys/kernel/mytty0tty/mytnt2_mytnt3_mask`
* `/sys/kernel/mytty0tty/mytnt4_mytnt5_mask`
* `/sys/kernel/mytty0tty/mytnt6_mytnt7_mask`

A mask egy 4 bites érték `0` és `15` között:

* bit0 = első oldal `CD/DCD`
* bit1 = első oldal `RI`
* bit2 = második oldal `CD/DCD`
* bit3 = második oldal `RI`

Például a `mytnt0_mytnt1_mask` esetén:

* `0` = minden alacsony
* `1` = csak `mytnt0` oldalon `CD/DCD`
* `2` = csak `mytnt0` oldalon `RI`
* `3` = `mytnt0` oldalon `CD/DCD + RI`
* `4` = csak `mytnt1` oldalon `CD/DCD`
* `8` = csak `mytnt1` oldalon `RI`
* `12` = `mytnt1` oldalon `CD/DCD + RI`
* `15` = mindkét oldalon `CD/DCD + RI`

Példa parancs:

```bash
printf '15\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

## Tesztelt rendszer

Ez a változat **Debian 12** alatt lett tesztelve.

## Fordítás

Szükséges csomagok Debian 12 alatt:

```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

A modul fordítása:

```bash
make
```

## Modul betöltése

Példa: 4 összekapcsolt portpár létrehozása:

```bash
sudo insmod ./mytty0tty.ko pairs=4
```

Ellenőrzés:

```bash
lsmod | grep mytty0tty
ls -l /dev/mytnt*
find /sys/kernel/mytty0tty -maxdepth 1 -type f | sort
```

## Telepítés

A modul telepítése a rendszer modul könyvtárába:

```bash
sudo make install
```

Automatikus betöltés engedélyezése rendszerindításkor az `/etc/modules` szerkesztésével vagy egy modules-load konfigurációs fájl létrehozásával:

```bash
echo mytty0tty | sudo tee /etc/modules-load.d/mytty0tty.conf
```

Alternatív megoldásként a modul neve kézzel is beírható az `/etc/modules` fájlba.

```bash
echo 'mytty0tty' /etc/modules
```

A telepített modul kézi betöltése:

```bash
sudo modprobe mytty0tty
```

## Használat

### Adatkapcsolat teszt

1. terminál:

```bash
cat /dev/mytnt1
```

2. terminál:

```bash
echo HELLO > /dev/mytnt0
```

### Aktuális mask kiolvasása

```bash
cat /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

### Új mask beállítása

```bash
printf '15\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

### Visszaellenőrzés

```bash
cat /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

### További példák

Csak a `mytnt0` oldalon `CD/DCD + RI`:

```bash
printf '3\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

Csak a `mytnt1` oldalon `CD/DCD + RI`:

```bash
printf '12\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

Mindkét oldalon `CD/DCD + RI`:

```bash
printf '15\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

Minden kikapcsolása:

```bash
printf '0\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
```

## Modul eltávolítása

Ha a modult kézzel vagy `modprobe`-bal töltötted be, így távolítsd el a futó kernelből:

```bash
sudo modprobe -r mytty0tty
```

Alternatív megoldás:

```bash
sudo rmmod mytty0tty
```

Ha a modul telepítve is lett a rendszer modul könyvtárába, akkor a telepített fájlokat is törölni kell:

```bash
sudo modprobe -r mytty0tty
sudo rm -f /lib/modules/$(uname -r)/extra/mytty0tty.ko
sudo rm -f /etc/modules-load.d/mytty0tty.conf
sudo depmod -a
```

Ha a modul neve kézzel lett hozzáadva az `/etc/modules` fájlhoz, akkor onnan is törölni kell a megfelelő sort.

## Megjegyzés

A projekt célja az eredeti `tty0tty` hasznos működésének megtartása úgy, hogy közben hozzáadja:

* a külön modulnevet
* a külön eszközprefixet
* a `CD/DCD` és `RI` vonalak külső, shellből vezérelhető kezelését
