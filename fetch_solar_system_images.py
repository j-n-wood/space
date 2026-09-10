#!/usr/bin/env python3
"""Download moderate-resolution (HD-ish) images of solar system bodies and nearby stars.

Source: Wikipedia's MediaWiki API `pageimages` prop, which returns each
article's lead image (for planets/moons this is normally the real NASA photo
in the infobox). We request a thumbnail at a fixed width so everything comes
back at a consistent, reasonable resolution -- no scraping, stdlib only.

Usage:
    python3 fetch_solar_system_images.py [--width 1200] [--out solar_system_images]
"""
import argparse
import json
import os
import sys
import time
import urllib.parse
import urllib.request

API = "https://en.wikipedia.org/w/api.php"
COMMONS_API = "https://commons.wikimedia.org/w/api.php"
UA = "SolarSystemImageFetcher/1.0 (educational use; joshw@wearebasis.com)"

# Display name -> Wikipedia article title. Redirects are followed by the API,
# so approximate titles are fine, but these are the canonical ones.
PLANETS = {
    "Mercury": "Mercury (planet)",
    "Venus": "Venus",
    "Earth": "Earth",
    "Mars": "Mars",
    "Jupiter": "Jupiter",
    "Saturn": "Saturn",
    "Uranus": "Uranus",
    "Neptune": "Neptune",
    "Pluto": "Pluto",  # dwarf planet, included for completeness
}

ASTEROIDS = {
    # Dwarf planet in the asteroid belt
    "Ceres": "Ceres (dwarf planet)",
    # Large / well-imaged main-belt asteroids
    "Vesta": "4 Vesta",
    "Pallas": "2 Pallas",
    "Hygiea": "10 Hygiea",
    "Juno": "3 Juno",
    "Eros": "433 Eros",
    "Ida": "243 Ida",
    "Gaspra": "951 Gaspra",
    "Mathilde": "253 Mathilde",
    "Lutetia": "21 Lutetia",
    "Bennu": "101955 Bennu",
    "Ryugu": "162173 Ryugu",
    "Itokawa": "25143 Itokawa",
    "Psyche": "16 Psyche",
    "Dinkinesh": "152830 Dinkinesh",
    "Steins": "2867 Steins",
    "Annefrank": "5535 Annefrank",
}

DWARF_TNOS = {
    # Trans-Neptunian dwarf planets and candidates
    "Eris": "Eris (dwarf planet)",
    "Haumea": "Haumea",
    "Makemake": "Makemake",
    "Sedna": "Sedna (dwarf planet)",
    "Gonggong": "Gonggong (dwarf planet)",
    "Quaoar": "50000 Quaoar",
    "Orcus": "90482 Orcus",
    "Salacia": "120347 Salacia",
}

MOONS = {
    # Earth
    "Moon": "Moon",
    # Mars
    "Phobos": "Phobos (moon)",
    "Deimos": "Deimos (moon)",
    # Jupiter (Galileans + a few inner/irregular with real imagery)
    "Io": "Io (moon)",
    "Europa": "Europa (moon)",
    "Ganymede": "Ganymede (moon)",
    "Callisto": "Callisto (moon)",
    "Amalthea": "Amalthea (moon)",
    "Thebe": "Thebe (moon)",
    # Saturn
    "Titan": "Titan (moon)",
    "Rhea": "Rhea (moon)",
    "Iapetus": "Iapetus (moon)",
    "Dione": "Dione (moon)",
    "Tethys": "Tethys (moon)",
    "Enceladus": "Enceladus",
    "Mimas": "Mimas",
    "Hyperion": "Hyperion (moon)",
    "Phoebe": "Phoebe (moon)",
    "Janus": "Janus (moon)",
    "Epimetheus": "Epimetheus (moon)",
    "Prometheus": "Prometheus (moon)",
    "Pandora": "Pandora (moon)",
    "Pan": "Pan (moon)",
    "Atlas": "Atlas (moon)",
    # Uranus
    "Titania": "Titania (moon)",
    "Oberon": "Oberon (moon)",
    "Umbriel": "Umbriel",
    "Ariel": "Ariel (moon)",
    "Miranda": "Miranda (moon)",
    "Puck": "Puck (moon)",
    # Neptune
    "Triton": "Triton (moon)",
    "Proteus": "Proteus (moon)",
    "Nereid": "Nereid (moon)",
    # Pluto
    "Charon": "Charon (moon)",
    "Nix": "Nix (moon)",
    "Hydra": "Hydra (moon)",
}

# The ten nearest star systems, Sol first, then by distance. One entry per
# system rather than per star, because Wikipedia has a single article (and so
# a single lead image) for each multiple: "Alpha Centauri" covers A and B,
# "Sirius" covers A and B, "Luyten 726-8" covers BL/UV Ceti.
#
# Only true stars are listed. The brown dwarfs Luhman 16 (6.5 ly) and
# WISE 0855-0714 (7.4 ly) are nearer than Ross 154 -- add them here if you
# want substellar objects too.
#
# Beyond Sol these are point sources: expect Hubble/survey frames for the
# brighter ones and artist renderings for the red dwarfs.
STARS = {
    "Sol": "Sun",                              # 0 ly
    "Proxima Centauri": "Proxima Centauri",    # 4.25 ly
    "Alpha Centauri": "Alpha Centauri",        # 4.37 ly (A + B)
    "Barnard's Star": "Barnard's Star",        # 5.96 ly
    "Wolf 359": "Wolf 359",                    # 7.86 ly
    "Lalande 21185": "Lalande 21185",          # 8.31 ly
    "Sirius": "Sirius",                        # 8.66 ly (A + B)
    "Luyten 726-8": "Luyten 726-8",            # 8.73 ly (BL + UV Ceti)
    "Ross 154": "Ross 154",                    # 9.71 ly
    "Ross 248": "Ross 248",                    # 10.30 ly
}

# Display name -> Commons file, used instead of the article's lead image.
# Needed because pageimages is unreliable for stars: Wolf 359 / Ross 154 /
# Ross 248 have no lead image at all, and Sirius and Lalande 21185 lead with a
# constellation chart. Real telescope frames where one is legible, artist
# renderings otherwise -- a nearby red dwarf is a faint point source in every
# real image of it, so a rendering is the only usable portrait.
COMMONS_OVERRIDES = {
    # Hubble, resolves A and B as a pair (article leads with a wide sky field)
    "Alpha Centauri": "File:Best image of Alpha Centauri A and B.jpg",
    # ESO rendering; the real image is a grainy annotated finder chart
    "Barnard's Star": "File:Artist’s impression of a sub-Earth-mass planet orbiting Barnard’s star (eso2414a).jpg",
    "Wolf 359": "File:Flaring red dwarf star (artist's impression) (opo1102a).jpg",
    "Lalande 21185": "File:Star Laland21185 2.jpg",
    # The classic Hubble frame: Sirius A with the white dwarf B lower left
    "Sirius": "File:Sirius A and B Hubble photo.jpg",
    # Rendering of the pair; beats the 928px DSS2 point source
    "Luyten 726-8": "File:Luyten 726-8.png",
    "Ross 154": "File:Red Dwarf Flare Star (Artist's Illustration) (2018-46-4241).jpg",
    "Ross 248": "File:RedDwarfNASA.jpg",
}


def api_get(params, api=API):
    params = {**params, "format": "json"}
    url = api + "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r)


def thumb_url(title, width):
    """Return (thumbnail_url, original_width_of_source) or (None, None)."""
    data = api_get({
        "action": "query",
        "titles": title,
        "prop": "pageimages",
        "piprop": "thumbnail|original",
        "pithumbsize": width,
        "redirects": 1,
    })
    pages = data.get("query", {}).get("pages", {})
    for _, page in pages.items():
        thumb = page.get("thumbnail")
        if thumb and thumb.get("source"):
            orig_w = page.get("original", {}).get("width")
            return thumb["source"], orig_w
    return None, None


def commons_thumb_url(file_title, width):
    """Return (url, native_width) for an explicitly named Commons file."""
    data = api_get({
        "action": "query",
        "titles": file_title,
        "prop": "imageinfo",
        "iiprop": "url|size",
        "iiurlwidth": width,
    }, api=COMMONS_API)
    pages = data.get("query", {}).get("pages", {})
    for _, page in pages.items():
        info = (page.get("imageinfo") or [{}])[0]
        native = info.get("width")
        # Don't ask for an upscale -- take the original when it's smaller.
        if native and native <= width and info.get("url"):
            return info["url"], native
        if info.get("thumburl"):
            return info["thumburl"], native
    return None, None


def download(url, dest):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=60) as r:
        data = r.read()
    with open(dest, "wb") as f:
        f.write(data)
    return len(data)


def ext_from_url(url):
    path = urllib.parse.urlparse(url).path
    ext = os.path.splitext(path)[1].lower()
    return ext if ext in (".jpg", ".jpeg", ".png", ".gif", ".webp", ".tif", ".tiff") else ".jpg"


def fetch_group(group_name, mapping, out_root, width, overrides=None):
    out_dir = os.path.join(out_root, group_name)
    os.makedirs(out_dir, exist_ok=True)
    overrides = overrides or {}
    results = []
    for display, title in mapping.items():
        try:
            if display in overrides:
                url, orig_w = commons_thumb_url(overrides[display], width)
            else:
                url, orig_w = thumb_url(title, width)
            if not url:
                print(f"  [MISS] {display:16s} -> no image for '{overrides.get(display, title)}'")
                results.append((display, title, None, None))
                continue
            fname = display.replace(" ", "_").replace("/", "-") + ext_from_url(url)
            dest = os.path.join(out_dir, fname)
            size = download(url, dest)
            note = f"(source up to {orig_w}px)" if orig_w else ""
            print(f"  [OK]   {display:16s} -> {fname} {size//1024} KB {note}")
            results.append((display, title, dest, url))
        except Exception as e:
            print(f"  [ERR]  {display:16s} -> {e}")
            results.append((display, title, None, None))
        time.sleep(0.2)  # be polite to the API
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=int, default=1200, help="target thumbnail width in px")
    ap.add_argument("--out", default="solar_system_images", help="output directory")
    args = ap.parse_args()

    print(f"Fetching planets ({len(PLANETS)})...")
    p = fetch_group("planets", PLANETS, args.out, args.width)
    print(f"\nFetching moons ({len(MOONS)})...")
    m = fetch_group("moons", MOONS, args.out, args.width)

    print(f"\nFetching asteroids ({len(ASTEROIDS)})...")
    a = fetch_group("asteroids", ASTEROIDS, args.out, args.width)

    print(f"\nFetching trans-Neptunian dwarf planets ({len(DWARF_TNOS)})...")
    d = fetch_group("dwarf_tnos", DWARF_TNOS, args.out, args.width)

    print(f"\nFetching nearest stars ({len(STARS)})...")
    s = fetch_group("stars", STARS, args.out, args.width, COMMONS_OVERRIDES)

    got = sum(1 for r in p + m + a + d + s if r[2])
    total = len(p) + len(m) + len(a) + len(d) + len(s)
    print(f"\nDone: {got}/{total} images -> {os.path.abspath(args.out)}/")

    # Write a manifest for provenance.
    manifest = os.path.join(args.out, "MANIFEST.txt")
    os.makedirs(args.out, exist_ok=True)
    with open(manifest, "w") as f:
        f.write("body\twikipedia_title\tfile\tsource_url\n")
        for display, title, dest, url in p + m + a + d + s:
            f.write(f"{display}\t{title}\t{dest or '-'}\t{url or '-'}\n")
    print(f"Manifest: {manifest}")


if __name__ == "__main__":
    main()
