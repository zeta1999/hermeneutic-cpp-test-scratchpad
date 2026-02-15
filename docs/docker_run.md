# `scripts/docker_run.sh`

This helper wraps `docker compose -f docker/compose.yml` and prepares the host
output directories before launching the demo stack. Supported flags:

- `--build`: (already supported) rebuild images before running `up`.
- `--alpine`: switch all services to the Alpine Dockerfiles (`docker/Dockerfile.*.alpine`).
- `--docker-suffix <suffix>`: append `<suffix>` to every service Dockerfile path;
  use `.alpine`, `.debian`, or any future variants without changing compose.
- `--`: stop processing helper flags; remaining args go straight to `docker compose`.

All other arguments are passed through unchanged. Examples:

```bash
scripts/docker_run.sh --build up           # rebuild images then start the stack
scripts/docker_run.sh up -d                # reuse images, run in detached mode
scripts/docker_run.sh --alpine --build up  # rebuild/run using Alpine Dockerfiles
scripts/docker_run.sh --docker-suffix .foo up  # use Dockerfile.*.foo variants
```

The script exports `DOCKER_BUILDKIT=1`, so Docker builds automatically take
advantage of BuildKit’s caching. Each Dockerfile mounts `/root/.cache/ccache`
plus a `/root/.cache/hermeneutic-deps` directory, so both compiler outputs and
FetchContent downloads persist between builds while staying out of Git and the
Docker build context.
