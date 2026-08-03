# DuckyScript files

Place `.txt` or `.duck` scripts in this directory. When the contents of
`sd-card-files` are copied to the microSD root, the firmware reads them from
`/ghostwire/scripts`.

Ghostwire supports this deliberately limited command set:

- `REM comment`
- `STRING text`
- `STRINGLN text`
- `DELAY milliseconds`
- `DEFAULT_DELAY milliseconds` (or `DEFAULTDELAY`)
- `ENTER`
- `TAB`
- `BACKSPACE`
- `SPACE`

Before execution, the firmware shows the filename, command count, unsupported
command count, and declared delay total. Execution requires confirmation and a
three-second countdown, and `Escape` cancels the countdown or any delay. A
script is limited to 500 executed lines, 65,536 bytes, 10 seconds per explicit
delay, 5 seconds of default delay per command, and 60 seconds of cumulative
delay.

Only run scripts against a focused host you own or are explicitly authorized
to test. Review every script before copying it to the card.
