#!/usr/bin/env python3
"""Writes the AUR, Homebrew, Scoop and winget manifests for a published release.

    packaging/update-manifests.py 1.3.0            # into the repository
    packaging/update-manifests.py 1.3.0 --out dir  # somewhere else

The repository keeps them in packaging/aur, packaging/homebrew,
packaging/winget and bucket/ (Scoop); --out writes the same layout under dir.

Checksums come from the release on GitHub: the installers' digests, and the
tag's source archive for the AUR package (downloaded to hash it). Set
GITHUB_TOKEN to avoid the API's anonymous rate limit.
"""

import argparse
import hashlib
import json
import os
import pathlib
import shutil
import sys
import urllib.request

REPO = "medium-gh/scancore"
LEXBOR = ("2.7.0", "e2473ade7173f69c56731ba71291d2ba705675ede2bb4423dc939ed70bcb55b5")
DESCRIPTION = "Read comics and novels and watch anime in a native Qt app"


def fetch(url, accept="application/vnd.github+json"):
    request = urllib.request.Request(url, headers={"Accept": accept, "User-Agent": "scancore-manifests"})
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token and url.startswith("https://api.github.com/"):
        request.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(request, timeout=120) as response:
        return response.read()


def asset_digests(version):
    release = json.loads(fetch(f"https://api.github.com/repos/{REPO}/releases/tags/v{version}"))
    digests = {}
    for asset in release.get("assets", []):
        digest = asset.get("digest") or ""
        if digest.startswith("sha256:"):
            digests[asset["name"]] = digest.split(":", 1)[1]
        else:
            digests[asset["name"]] = hashlib.sha256(fetch(asset["browser_download_url"], "application/octet-stream")).hexdigest()
    return digests


def pkgbuild(version, source_sha):
    return f"""# Maintainer: Logesh Senthil <logesh@mediumdev.com>
pkgname=scancore
pkgver={version}
pkgrel=1
pkgdesc='{DESCRIPTION}'
arch=('x86_64')
url='https://github.com/{REPO}'
license=('MIT')
# Qt WebEngine (Arch's has the H.264 codec) is the anime player.
depends=('gcc-libs' 'glibc' 'qt6-base' 'qt6-declarative' 'qt6-imageformats' 'qt6-speech' 'qt6-svg' 'qt6-webengine')
optdepends=(
  'qt6-wayland: native Wayland platform integration'
)
# qt6-tools: lrelease, for the translations.
makedepends=('cmake' 'ninja' 'qt6-tools')
source=(
  "scancore-${{pkgver}}.tar.gz::https://github.com/{REPO}/archive/refs/tags/v${{pkgver}}.tar.gz"
  'lexbor-{LEXBOR[0]}.tar.gz::https://github.com/lexbor/lexbor/archive/refs/tags/v{LEXBOR[0]}.tar.gz'
)
sha256sums=(
  '{source_sha}'
  '{LEXBOR[1]}'
)

build() {{
  cmake -S "${{srcdir}}/scancore-${{pkgver}}" -B build -G Ninja \\
    -DCMAKE_BUILD_TYPE=Release \\
    -DCMAKE_INSTALL_PREFIX=/usr \\
    -DCMAKE_INSTALL_LIBDIR=lib \\
    -DFETCHCONTENT_SOURCE_DIR_LEXBOR="${{srcdir}}/lexbor-{LEXBOR[0]}" \\
    -DSCANCORE_BUILD_TESTS=OFF \\
    -DSCANCORE_WITH_NOVEL_PACK_INSTALLER=OFF
  cmake --build build
}}

package() {{
  DESTDIR="${{pkgdir}}" cmake --install build
  install -Dm644 "${{srcdir}}/scancore-${{pkgver}}/LICENSE" \\
    "${{pkgdir}}/usr/share/licenses/${{pkgname}}/LICENSE"
}}
"""


def srcinfo(version, source_sha):
    return f"""pkgbase = scancore
	pkgdesc = {DESCRIPTION}
	pkgver = {version}
	pkgrel = 1
	url = https://github.com/{REPO}
	arch = x86_64
	license = MIT
	makedepends = cmake
	makedepends = ninja
	makedepends = qt6-tools
	depends = gcc-libs
	depends = glibc
	depends = qt6-base
	depends = qt6-declarative
	depends = qt6-imageformats
	depends = qt6-speech
	depends = qt6-svg
	depends = qt6-webengine
	optdepends = qt6-wayland: native Wayland platform integration
	source = scancore-{version}.tar.gz::https://github.com/{REPO}/archive/refs/tags/v{version}.tar.gz
	source = lexbor-{LEXBOR[0]}.tar.gz::https://github.com/lexbor/lexbor/archive/refs/tags/v{LEXBOR[0]}.tar.gz
	sha256sums = {source_sha}
	sha256sums = {LEXBOR[1]}

pkgname = scancore
"""


def cask(version, dmg_sha):
    return f"""cask "scancore" do
  version "{version}"
  sha256 "{dmg_sha}"

  url "https://github.com/{REPO}/releases/download/v#{{version}}/SCANCORE-#{{version}}-macos-universal.dmg"
  name "SCANCORE"
  desc "Read comics and novels and watch anime in a native app"
  homepage "https://github.com/{REPO}"

  depends_on macos: :ventura

  app "SCANCORE.app"
end
"""


def winget(version, setup_sha):
    head = f"PackageIdentifier: MediumDev.Scancore\nPackageVersion: {version}\n"
    return {
        "MediumDev.Scancore.yaml": head + "DefaultLocale: en-US\nManifestType: version\nManifestVersion: 1.12.0\n",
        "MediumDev.Scancore.installer.yaml": head + f"""InstallerLocale: en-US
Platform:
  - Windows.Desktop
MinimumOSVersion: 10.0.0.0
InstallerType: inno
Scope: machine
UpgradeBehavior: install
FileExtensions:
  - sct
Installers:
  - Architecture: x64
    InstallerUrl: https://github.com/{REPO}/releases/download/v{version}/SCANCORE-{version}-windows-x64-setup.exe
    InstallerSha256: {setup_sha.upper()}
    InstallerSwitches:
      Silent: /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-
      SilentWithProgress: /SILENT /SUPPRESSMSGBOXES /NORESTART /SP-
ManifestType: installer
ManifestVersion: 1.12.0
""",
        "MediumDev.Scancore.locale.en-US.yaml": head + f"""PackageLocale: en-US
Publisher: Medium Dev
PackageName: SCANCORE
License: MIT
ShortDescription: Read comics and novels and watch anime in a native app
Description: Read manga, manhwa, webtoons and web novels, and watch anime from dozens of sites. Your library, history and downloads stay on your device.
PackageUrl: https://github.com/{REPO}
PublisherUrl: https://mediumdev.com
PublisherSupportUrl: https://github.com/{REPO}/issues
ReleaseNotesUrl: https://github.com/{REPO}/releases/tag/v{version}
Tags:
  - anime
  - comics
  - manga
  - novels
  - reader
ManifestType: defaultLocale
ManifestVersion: 1.12.0
""",
    }


def scoop(version, zip_sha):
    return json.dumps({
        "version": version,
        "description": "Read comics and novels and watch anime in a native app",
        "homepage": f"https://github.com/{REPO}",
        "license": "MIT",
        "url": f"https://github.com/{REPO}/releases/download/v{version}/SCANCORE-{version}-windows-x64-portable.zip",
        "hash": zip_sha,
        "shortcuts": [["scancore.exe", "SCANCORE"]],
        "checkver": {"github": f"https://github.com/{REPO}"},
        "autoupdate": {"url": f"https://github.com/{REPO}/releases/download/v$version/SCANCORE-$version-windows-x64-portable.zip"},
    }, indent=4) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("version")
    parser.add_argument("--out", type=pathlib.Path)
    args = parser.parse_args()
    version = args.version.removeprefix("v")
    root = pathlib.Path(__file__).resolve().parent.parent
    packaging = args.out / "packaging" if args.out else root / "packaging"
    bucket = args.out / "bucket" if args.out else root / "bucket"

    digests = asset_digests(version)
    missing = [name for name in (f"SCANCORE-{version}-macos-universal.dmg", f"SCANCORE-{version}-windows-x64-setup.exe",
                                 f"SCANCORE-{version}-windows-x64-portable.zip")
               if name not in digests]
    if missing:
        sys.exit("Release v%s has no %s yet." % (version, " or ".join(missing)))
    source_sha = hashlib.sha256(fetch(f"https://github.com/{REPO}/archive/refs/tags/v{version}.tar.gz",
                                      "application/octet-stream")).hexdigest()

    out = packaging
    (out / "aur").mkdir(parents=True, exist_ok=True)
    (out / "aur" / "PKGBUILD").write_text(pkgbuild(version, source_sha))
    (out / "aur" / ".SRCINFO").write_text(srcinfo(version, source_sha))
    (out / "homebrew" / "Casks").mkdir(parents=True, exist_ok=True)
    (out / "homebrew" / "Casks" / "scancore.rb").write_text(cask(version, digests[f"SCANCORE-{version}-macos-universal.dmg"]))
    # winget keeps one folder per version; only the newest is kept here.
    winget_root = out / "winget" / "MediumDev.Scancore"
    if winget_root.exists():
        for old in winget_root.iterdir():
            if old.is_dir() and old.name != version:
                shutil.rmtree(old)
    (winget_root / version).mkdir(parents=True, exist_ok=True)
    for name, text in winget(version, digests[f"SCANCORE-{version}-windows-x64-setup.exe"]).items():
        (winget_root / version / name).write_text(text)
    bucket.mkdir(parents=True, exist_ok=True)
    (bucket / "scancore.json").write_text(scoop(version, digests[f"SCANCORE-{version}-windows-x64-portable.zip"]))
    print(f"Manifests for {version} written to {args.out or root}")


if __name__ == "__main__":
    main()
