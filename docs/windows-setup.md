# Studio PC setup: OBS 32 on Windows 11

Status: first test version of the plugin. It has not run inside OBS on a real PC yet.

Goal: `x32 Out` goes to the stream on track 1 and to the recording on track 2. The guard removes it
from track 1 only. The recording keeps its audio.

Menu names come from OBS knowledge and the OBS source. They are not checked on the studio PC.

## Before you start

1. Make sure that no stream and no recording is running.
2. Back up the setup: **Profile > Export** and **Scene Collection > Export**.
3. Take a photo of the current Output and Advanced settings.

## 1. Output (Settings > Output)

Set **Output Mode** to **Advanced**. Then check your bitrate and encoder again.

| Tab | Setting | Value |
|---|---|---|
| Streaming | Audio Track | 1 |
| Recording | Audio Track | 2 only |
| Recording | Recording Format | MKV |
| Audio | Track 1 name | Stream |
| Audio | Track 2 name | Recording |

If you use the Replay Buffer or Backtrack, check which audio track it saves. It must use track 2.

## 2. Audio tracks (Audio Mixer > gear icon > Advanced Audio Properties)

| Source | Track 1 | Track 2 |
|---|---|---|
| `x32 Out` | on | on |
| Every other audio source | off | your choice |

Any other source on track 1 stays on air when the guard mutes. Check **Settings > Audio** and set
unused Desktop Audio and Mic devices to **Disabled**.

## 3. Stream delay (Settings > Advanced > Stream Delay)

| Setting | Value |
|---|---|
| Enable | on |
| Duration | 120 seconds |
| Preserve cutoff point when reconnecting | on |

What changes during a show:

- Viewers see everything 120 seconds late.
- A normal **Stop Streaming** sends the last 120 seconds first. Let it finish.
- **Stop Streaming (discard delay)** throws the last 120 seconds away.

## 4. Dump button (works without the plugin)

1. Open **Settings > Hotkeys**.
2. Find **Stop Streaming (discard delay)**.
3. Set a key that you cannot press by accident, for example `Ctrl+Shift+F12`.

If you hear a released song, press it. It ends the stream.

## 5. Test without YouTube

Never test against YouTube.

Recording test:

1. Play audio through the X32.
2. Record for 1 minute.
3. Untick track 1 for `x32 Out` for 20 seconds, then tick it again.
4. Play the recording. The audio must have no gap.

Stream test:

1. Run a local RTMP server on the PC. [MediaMTX](https://github.com/bluenviron/mediamtx) is one.
2. In **Settings > Stream**, pick **Custom**. Server `rtmp://localhost/live`, key `test`.
3. Start the stream. Open `rtmp://localhost/live/test` in VLC.
4. Untick track 1 for `x32 Out`. VLC must go silent about 120 seconds later.
5. Press the dump key. VLC must not play the last 120 seconds.
6. Set **Settings > Stream** back to YouTube.

## 6. Install the plugin

1. Close OBS.
2. Download the `windows-x64` file from the newest run on the GitHub **Actions** page. Sign in to
   GitHub first.
3. Unzip it. Unzip the zip inside it too.
4. Copy the folder `obs-stream-guard` to `C:\ProgramData\obs-studio\plugins\`.
5. Start OBS. Open **Docks > Stream Guard**.

To remove it, close OBS and delete that folder.

## 7. Plugin settings

| Setting | Value | Reason |
|---|---|---|
| Source | `x32 Out` | The only audio source |
| Guard on | ticked | Off means no checks and no actions |
| On a match, high risk show | Mute + stop stream | Drops the delayed video |
| On a match, other shows | Mute | Stream stays up |
| Unmute after | 120 s | Gaps between matches can pass 60 seconds |

| Dock status | Meaning |
|---|---|
| OFF | Guard on is not ticked |
| NOT ARMED | A setting is unsafe. The line below says which one |
| GUARDING | Checking every 10 seconds |
| STREAM MUTED: SONG | A song matched |
| STREAM MUTED: CHECKS FAILING (flashing red) | No answer for 60 seconds |
| PAUSED: NOT CHECKING (flashing yellow) | Nothing is checked |
| STREAM STOPPED: SONG | The stream was stopped. Start it again when the song ends |

## 8. Plugin test

Use the local RTMP server from part 5. Tick every line:

1. [ ] Play a known song. The dock mutes within 45 seconds.
2. [ ] The recording has full audio during the mute.
3. [ ] In stop mode, the stream stops and VLC never plays the song.
4. [ ] 120 seconds after the song, the stream audio comes back.
5. [ ] Disconnect the network. The dock flashes red in about 70 seconds.
6. [ ] Put the recording on track 1. The dock shows NOT ARMED.
7. [ ] Close OBS during a mute. After restart, track 1 is ticked again for `x32 Out`.

Note any failure and send **Help > Log Files > Show Log Files**.

## Show day checklist

1. Stream delay is on at 120 seconds.
2. Only `x32 Out` is on track 1.
3. Recording is on track 2.
4. The dump key works in your head and on the keyboard.
5. A person watches YouTube Live Control Room for the whole stream.

This setup lowers the risk. It does not remove the risk. Shazam does not know every song.
