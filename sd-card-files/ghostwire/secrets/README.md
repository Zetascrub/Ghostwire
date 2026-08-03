# AI API configuration

Copy `ai.example.json` to `ai.json`, add one or both API keys, and place this
directory at `/ghostwire/secrets` on the Cardputer microSD card.

The firmware never displays or logs these values, but the card is removable
and the file is plaintext. Use scoped, revocable keys with spending limits and
do not commit `ai.json` to source control.
