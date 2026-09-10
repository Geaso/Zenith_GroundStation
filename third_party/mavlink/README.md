# Vendored MAVLink C headers

Upstream project: [MAVLink](https://github.com/mavlink/mavlink), maintained by the MAVLink project and its contributors.

## Exact source

- Generated C library: [`mavlink/c_library_v2@46e5824a066ec56f3fe03bf855df1f2cdc9e84fd`](https://github.com/mavlink/c_library_v2/tree/46e5824a066ec56f3fe03bf855df1f2cdc9e84fd).
- The [generation commit](https://github.com/mavlink/c_library_v2/commit/46e5824a066ec56f3fe03bf855df1f2cdc9e84fd) identifies its source as [`mavlink/mavlink@9166d32ba90345f2049c052c0a7849a21929d24e`](https://github.com/mavlink/mavlink/tree/9166d32ba90345f2049c052c0a7849a21929d24e).
- Included under `v2.0/`: the upstream top-level C headers and the `common`, `standard`, and `minimal` dialect directories. Other dialects, XML definitions, and generator tools are not included.
- Local header modifications: none. Each vendored file was compared with its upstream Git blob at the pinned C-library revision.

## License and retained notice

The pinned generated-library repository does not itself contain a top-level license file. [COPYING](COPYING) is an unmodified copy of the [upstream source repository's COPYING at the exact generation-source revision](https://github.com/mavlink/mavlink/blob/9166d32ba90345f2049c052c0a7849a21929d24e/COPYING).

That notice distinguishes the generator's (L)GPL v3 license from the MIT license granted to generated output by its explicit generator-output exception. The vendored dependency here is the generated C header library; the generator is not included. The [official MAVLink license overview](https://mavlink.io/en/#license) also identifies the generated C header library as MIT-licensed.

Keep `COPYING`, this attribution, and any notices present in the headers with redistributed copies. The full upstream notice is retained, including its generator-license text; no replacement copyright holder or newly worded MIT license has been invented. Application-specific Zenith extension framing is maintained outside this vendored directory.

Exact `COPYING` provenance:

- Upstream Git blob: `6f1d4c61b0e2de896454cac2a3300e6d47371254`.
- SHA-256 of the unchanged 9437-byte file: `81133ba4d3343a0e627dd0d9582134f956bc9dc82322d369c561249cc2910224`.
