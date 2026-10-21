# Bundled sources

The site engines SCANCORE runs without extensions: comics in `comics/`,
novels in `novels/`, with their sites listed in `sources/manifest.json`
(the built-in anime player is `Anime::streams`, configured under `anime`).

Everything here can be taken out at three levels:

- **In installed apps, without a release**: publish the manifest with
  `"bundled": {"enabled": false, "reason": "..."}` and a higher `version`.
  Every built-in source switches off with that reason; extensions and files
  on the device keep working, and series stay in the library and history.
- **In a build**: configure with `-DSCANCORE_WITH_BUNDLED_SOURCES=OFF`.
- **In the code**: delete this folder, the four files it adds in
  `src/CMakeLists.txt`, and the `SCANCORE_BUNDLED_SOURCES` blocks
  (`createSource` in comics and novels, `withLocalNames` in `Comics.cpp`).

Ported engines follow the open extension sources' site structure (Apache-2.0;
see the licences screen). Same limits as everywhere else: public pages only,
no paywall, login or lock bypass.
