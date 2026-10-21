The fixture site the extension host tests serve on fixture.test: answers for
the test extension in host/testext. Images are only given to requests that
carry the extension's own header (X-Test-Source), which proves they went
through its client.

`anime/` holds the answers for the test anime extension (host/testext,
src/anime): listings, an anime with seasons, episodes, hosters (one lazy,
one with its videos inline), videos, and the pages that resolve a video's
address.
