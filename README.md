# OBS Stream Guard

An OBS plugin for Windows that listens to one audio source. When it recognizes a released song, it
mutes that source on the stream track. It can also stop the stream and drop the delayed video. Your
recording keeps the audio.

Status: first test version. Not yet run inside OBS on a real PC.

It lowers the risk of a copyright problem. It does not prevent one.

## How it works

1. Every 10 seconds it fingerprints the last 12 seconds of the source with
   [vibra](https://github.com/BayernMuller/vibra).
2. It asks the unofficial Shazam service for a match.
3. On a match it removes the source from the stream track, or also stops the stream.
4. After 120 seconds with no match it puts the audio back.

If checks fail for 60 seconds, it mutes and flashes red. It never stops the stream for a failure.

## Limits

- Shazam does not know every song. Another service can flag a song that Shazam misses.
- The Shazam access is unofficial. It can stop at any time.
- The plugin sends audio fingerprints of the source, live bands included, to Shazam.

## Install and set up

See [docs/windows-setup.md](docs/windows-setup.md).

## Develop

The logic and the recognizer build without OBS:

```
cmake -S tests -B build-tests
cmake --build build-tests
./build-tests/core_tests
```

GitHub Actions builds the Windows plugin for OBS 32.

## License

GPL-3.0. See [LICENSE](LICENSE). Project layout from
[obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate).
