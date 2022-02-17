# tweet2doom - data and tools

All data acquired by the [@tweet2doom](https://twitter.com/tweet2doom) bot is automatically stored and updated in this repository periodically.
The data can be explored and used by anyone. It includes things like all currently existing nodes, submitted commands, usernames, etc.

The repository also contains a few tools:


### State Explorer

This is a simple static webpage that provides a visualization of the current state tree of [@tweet2doom](https://twitter.com/tweet2doom).
Visit the following link to see what it looks like:

https://tweet2doom.github.io

<p align="center">
<img src="https://user-images.githubusercontent.com/1991296/139575872-af594c30-e77a-4655-9de0-9dad25a4cf39.gif"></img><br>
Fig 1. The state tree of <a href="https://twitter.com/tweet2doom">@tweet2doom</a>
</p>

### Render a tweet chain

The [render-full.sh](render-full.sh) is a simple Bash script that allows you to render a full tweet chain from start to finish. You simply need to provide the `id` of the final tweet. The script depends on `doomreplay` and `ffmpeg`.

Example:

This command will render the tweet chain that ends here: https://twitter.com/tweet2doom/status/1449157956096991234

```bash
./render-full.sh 1449157956096991234 35 105
```

The result is an .mp4 video that looks like this:

https://user-images.githubusercontent.com/92263613/137576609-638ffe97-7500-465d-8860-e40d1a43344b.mp4


### Random plays generator

The [random-plays.sh](random-plays.sh) script generates a short video with 10 plays randomly selected nodes from the entire state tree.

Example:

```bash
./render-full.sh
```

https://user-images.githubusercontent.com/92263613/137576751-c7a7cf1a-7cde-4d7d-aa6b-346a0b74d947.mp4


### Print statistics

The [stats.sh](stats.sh) script prints various statistics about the current state tree.

Example:

```bash
./stats.sh
Max depth:   56
Max frames:  7910
Num players: 416
Num nodes:   1139
```

