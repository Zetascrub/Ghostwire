# Authorized use and data handling

Ghostwire is intended for equipment you own or systems for which you have
explicit written authorization to assess. A visible SSID, reachable host, BLE
advertisement, or radio transmission is not authorization.

Before an engagement, record the permitted targets, frequencies, locations,
time window, allowed active techniques, data-retention period, and emergency
contact. Keep deauthentication, BLE advertising, HID injection, credential
capture, and future poisoning/spoofing functions disabled unless the scope
specifically permits them.

Use passive discovery first. Confirm the selected target on-device before any
transmission. Stop immediately if the target, location, or authorization is
uncertain or if safety-critical equipment may be affected.

Logs on the microSD card are unencrypted. Treat network identifiers, device
addresses, packet captures, RFID identifiers, terminal history, and GPS
coordinates as sensitive assessment data. Transfer them only to approved
storage and delete them when the retention period ends.

Familiar Patrol is intended for explicitly scoped, unattended discovery and a
bounded 100-port TCP scout pass. Confirm the detected subnet and usable-address
count before starting it, obtain permission to leave the device connected and
powered for the full assessment window, and ensure monitoring contacts know
where it is deployed. Exhaustive scanning is not part of the unattended
patrol. A response does not itself establish authorization. Retrieve or
securely erase its assessment directory when the engagement's retention period
ends.
