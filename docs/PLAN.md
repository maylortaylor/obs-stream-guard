# Design

## Decisions

| Topic | Decision |
|---|---|
| Form | Native C++ plugin for Windows and OBS 32 |
| Audio | One source (default `x32 Out`), the full stereo mix |
| Detector | vibra and the unofficial Shazam service |
| Check | A 12 second window every 10 seconds. Audio under -60 dB is not sent |
| Act after | 1 match |
| Modes | mute, or mute + stop (forced stop, drops the delayed video) |
| Unmute | After 120 seconds with no match, or the button |
| Failure | After 60 seconds of failed checks: mute, flash red, never stop |
| Delay | 120 seconds, set in OBS |
| Recording | Never loses audio. The plugin refuses to arm if the tracks are shared |

## Replay result

One 176 minute recorded jazz show, replayed through the same window and interval:

- 821 windows: 246 matches, 24 songs, 8 failed checks.
- 1 match acts on all 24 songs, 13 seconds after the first match.
- 2 matches within 30 seconds acts on 19 songs.
- Unmute after 60 seconds gives 14 separate mutes. After 120 seconds it gives 9.

The replay shows when the first match arrived. It does not show when each song started, or which
songs were missed.

## Acceptance criteria

1. A known song on the source mutes the stream within 45 seconds.
2. The recording has full audio during every mute and after a forced stop.
3. In stop mode with a 120 second delay, a local receiver never gets the detected song.
4. 120 seconds with no match brings the stream audio back.
5. A network cut mutes the stream within about 70 seconds, flashes red and does not stop.
6. A shared stream and recording track leaves the guard not armed, with a reason.
7. Pause stops all checks and actions, and the dock shows it.
8. Closing OBS during a mute does not leave the source off the stream track after restart.

## Open

- OBS behavior on the studio PC is untested: forced stop, track check, dock.
- The Shazam rate limit is unknown. HTTP 429 appeared at about 1 request per 2 seconds.
