# CHPDHubProbe: PD Hub Probe

A guided bench test for powered USB-C hubs on the Rev 1.5: what a hub does
unpowered, when its charger goes in, and when it is plugged in already
powered, and whether the board can take its power (and charge) while it
stays the USB host. Everything it sees goes into three logs for a PC.

It is a tool for trying a new hub, not a game: the USB HOST app
([CHUSBHost](../CHUSBHost/README.md)) does all of this by itself. Use the
probe when a hub does not behave, and send its log.

## Controls

| Button | Action |
|---|---|
| A | The next step |
| B | Straight to the end (PC mode, the log ready) |
| SELECT | Start again from step 1 |
| START (hold) | The SD game menu |

## The steps

1. **Plug in the hub without its charger**, a keyboard on it, and type:
   the board powers it from its battery.
2. **Plug the charger into the hub** while it stays plugged in: a hub that
   can, asks for a PD power role swap and powers the board from then on.
3. **Unplug the hub.**
4. **Plug it in with its charger on**, and type: the board is a sink a
   moment first (Try.SNK), the hub becomes the source, hands the board the
   host role (DR_Swap), and the board reads the keyboard on the hub's
   power.

Then plug the board into a PC and run, from the repository's root:

```bash
python libraries/CHUSBHost/examples/CHUSBHost/tools/usblog.py --out out/hubprobe.txt
```

The text has the app's timeline (each step, each status, each decision,
with milliseconds), the USB host's probe log (the devices' ids and
descriptors) and the PD log (every message, the source's offer with its
raw flags, each swap, each time the host cut its 5 V and why).

## What it found on Apple's USB-C Digital AV Multiport Adapter

- Unpowered it is a 2-port USB 2.0 hub (05AC:100F) behind a sink's Rd.
- Powered it is a dual-role port: it sinks when it sees our Rp, sources
  when it sees our Rd. Its offer is one PDO, 5 V 0.1 A, dual-role power
  and data, unconstrained power; it then asks for VCONN_Swap (rejected)
  and DR_Swap (accepted: the board becomes the host).
- With its charger plugged in while the board powers it, it asks for a
  PR_Swap 6 to 7 s later, once our PD offer has been heard.
- Its hub chip runs on the cable's 5 V, so a power swap restarts it and
  its devices enumerate again (about 2 s).

## Developer notes

- The port runs itself (`servicePort()`): no VBUS, battery host mode with
  Try.SNK and `host::PD_SWAP`; VBUS from the port, the PD sink with
  data-role swaps allowed, then `host::enterPowered()` once we are DFP,
  or, if no swap comes, the host tried anyway to see whether the hub
  answers regardless.
- RAM is close to full (17.9 of 18.4 KB): the timeline is 32 lines, and
  it names `HUB` (one level) rather than `HUBS`.

## Credits

By bateske, MIT ([LICENSE](LICENSE)). Built on the CHGame library, CHGfx,
CHPower and CHUSBHost.
