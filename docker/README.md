# Official Urbit Docker Image

This is the official Docker image for [Urbit](https://urbit.org).

Urbit is a clean-slate OS and network for the 21st century.

## Using

To use this image, you should mount a volume with a keyfile, comet file, or
existing pier at `/urbit`, and map ports as described below.

### Volume Mount

This image expects a volume mounted at `/urbit`. This volume should initially
contain one of:
- A keyfile `<shipname>.key` for a galaxy, star, planet, or moon. See the 
  setup instructions for information on
  [obtaining a keyfile](https://urbit.org/getting-started/get-id).
    - e.g. `sampel-palnet.key` for the planet `~sampel-palnet`.
- An empty file with the extension `.comet`. This will cause Urbit to boot a
  [comet](https://urbit.org/docs/glossary/comet/) in a pier named for the
  `.comet` file (less the extension).
    - e.g. starting with an empty file `my-urbit-bot.comet` will result in Urbit
      booting a comet into the pier `my-urbit-bot` under your volume.
- An existing pier as a directory `<shipname>`. You can migrate an existing ship
  to a new docker container in this way by placing its pier under the volume.
    - e.g. if your ship is `sampel-palnet` then you likely have a directory
      `sampel-palnet` whose path you pass to `./urbit` when starting.
      [Move your pier](https://operators.urbit.org/manual/os/basics#moving-your-pier)
      directory to the volume and then start the container.

The first two options result in Urbit attempting to boot either the ship named
by the keyfile, or a comet. In both cases, after that boot is successful, the
`.key` or `.comet` file will be removed from the volume and the pier will take
its place.

Therefore, it is safe to remove the Docker container and start a new container
which mounts the same volume (e.g. to upgrade the version of the Urbit binary
by running a newer image). It is also possible to stop the container and then
move the pier out of the Docker volume (e.g. to run it using an Urbit binary
directly). If you do this, make sure to delete the Docker volume after you move
your pier; if you launch a container using this same pier after moving it and
launching it elsewhere, you will likely need to perform a
[breach](https://developers.urbit.org/reference/glossary/reset).

### Ports

The image includes `EXPOSE` directives for TCP port `80` and UDP port `34343`.
Port `80` is used for Urbit's HTTP interface for both
[Landscape](https://urbit.org/docs/glossary/landscape/) and for
[API calls](https://developers.urbit.org/guides/additional/http-api-guide) to
the ship. Port `34343` is set by default to be used by
[Ames](https://urbit.org/docs/glossary/ames/) for ship-to-ship communication.

You can either pass the `-P` flag to docker to map ports directly to the
corresponding ports on the host, or map them individually with `-p` flags. For
local testing the latter is often convenient, for instance to remap port `80` to
an unprivileged port.

For best performance, you must map the Ames UDP port to the *same* port on the
host. If you map to a different port Ames will not be able to make direct
connections and your network performance may suffer somewhat. Note that using
the same port is required for direct connections but is not by itself sufficient
for them. If you are behind a NAT router or the host is not on a public IP
address (or you are firewalled), you may not achieve direct connections
regardless.

For this reason, you can force Ames to use a custom port.
`/bin/start-urbit --port=$AMES_PORT` can be passed as an argument to the
`docker start` command. Passing `/bin/start-urbit --port=13436` for example,
would use port `13436`. Note that you must pass the full script command
`/bin/start-urbit` in order to also pass arguments. If the script is omitted,
your container will not start.

You can also set the http port using `--http-port=$HTTP_PORT`. Passing
`/bin/start-urbit --http-port=8085` for example, would use port `8085`. The
default http port is `8080`.

### Variable Loom Size

You can also set a variable loom size (Urbit memory size) using
`--loom=$LOOM_SIZE`. Passing `/bin/start-urbit --loom=32` for example, would set
up a 4GiB loom (`2^32 bytes = 4GiB`). The default loom size is `31` (2GiB).

### Examples

Creating a volume for `~sampel-palnet`:
```
docker volume create sampel-palnet
```

Copying key to `~sampel-palnet`'s volume (assumes default Docker location):
```
sudo cp ~/sampel-palnet.key /var/lib/docker/volumes/sampel-palnet/_data/sampel-palnet.key
```

Using that volume and launching `~sampel-palnet` on host port `8080` with Ames
talking on the default host port `34343`:
```
docker run -d -p 8080:80 -p 34343:34343/udp --name sampel-palnet \
    --mount type=volume,source=sampel-palnet,destination=/urbit \
    tloncorp/vere
```

Using host port `8088` with Ames talking on host port `23232`:
```
docker run -d -p 8088:80 -p 23232:23232/udp --name sampel-palnet \
    --mount type=volume,source=sampel-palnet,destination=/urbit \
    tloncorp/vere /bin/start-urbit --port=23232
```

### Getting and resetting the Landscape `+code`

This docker image includes tools for retrieving and resetting the Landscape
login code belonging to a ship, for programmatic use so the container does not
need a tty. These scripts can be called using `docker container exec`.

Getting the code:
```
$ docker container exec sampel-palnet /bin/get-urbit-code
sampel-sampel-sampel-sampel
```

Resetting the code:
```
$ docker container exec sampel-palnet /bin/reset-urbit-code
OK
```

## Extending

You likely do not want to extend this image. External applications which
interact with Urbit do so primarily via an HTTP API, which should be exposed as
described above. For containerized applications using Urbit, it is more
appropriate to use a container orchestration service such as Docker Compose or
Kubernetes to run Urbit alongside other containers which will interface with its
API.
