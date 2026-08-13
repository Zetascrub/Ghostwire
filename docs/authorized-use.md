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

A Familiar Handshake Mission runs unattended too, but unlike Patrol it
actively transmits deauthentication frames -- every target it works through
was explicitly picked by the operator on-device before the mission starts, and
that selection step is the authorization checkpoint. Wi-Fi does not respect
property lines: confirm every checked network is actually in scope, not just
in radio range, before starting. Do not leave a mission running unattended
somewhere a bystander's or neighboring network's connectivity could be
disrupted without their knowledge. Captured PCAPs and the mission's Markdown
report are the same category of sensitive assessment data as any other
handshake capture.

Evil Portal is the credential-capture function line 9 refers to: it clones a
single operator-selected SSID as an open access point and serves a sign-in
page to whatever connects, logging anything submitted. It is never started by
anything but the operator explicitly picking one target AP and confirming --
there is no unattended or multi-target form of it. This only belongs in an
engagement whose scope specifically names credential-harvesting/phishing
assessment, with the client's informed consent that real users may be
prompted for real credentials against a network they trust; running it
against a network merely because it's in range is not authorization. Anyone
who submits to the portal has had their input captured without a chance to
tell it apart from the real network -- treat that as you would any other
social-engineering technique requiring separate, explicit sign-off from a
general recon/assessment scope. Captured submissions land in
`/ghostwire/logs/portal_creds_NNNN.csv` as plain text and are the most
sensitive category of data this device can produce; retrieve or securely
erase them as soon as the assessment allows, and never let them leave the
device by any path other than the engagement's approved evidence-handling
process.
