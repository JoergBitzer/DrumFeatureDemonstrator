# How to document your plugin (AAT2)

Good documentation is short, correct and easy to find. Four places, each with its own job:

| Where | For whom | What |
|---|---|---|
| `README.md` | people who find your repository | what the plugin does (with a screenshot), download, build, license |
| `docs/manual/` | users of the plugin | the manual (PDF), part of every release zip |
| `docs/` | you and other developers | notes on the development: decisions, measurements, problems |
| GitHub release notes | users who update | what changed in this version |

Two rules make it much easier to keep everything correct:
* **Generate what you can from the code** -- the list of controls and the screenshots (see below).
  Then they cannot become outdated.
* **Update the documentation in the same commit as the code.** Later you will not remember.

## The README

Short and in this order: one sentence what the plugin does, a screenshot, download (link to the
GitHub releases page), the features, how to build it, the license. The README of this template
explains the template -- replace it with your own.

## The manual (docs/manual)

`docs/manual/ManualDrumFeatureDemonstrator.tex` is a LaTeX template with the usual sections: introduction,
installation, how to use (numbered screenshot), controls, how it works, source code, legal stuff.

1. Replace "Joerg Bitzer", "JoergBitzer" and "Jade_Hochschule" (your `COMPANY_NAME` from CMakeLists.txt)
   and set `\pluginversion` to the version in CMakeLists.txt.
2. Write the text for your plugin: what it is for, how to use it, how it works.
3. Build it (in `docs/manual`):
   ```console
   pdflatex ManualDrumFeatureDemonstrator.tex
   pdflatex ManualDrumFeatureDemonstrator.tex
   ```
   (twice, so the section references are right; `latexmk -pdf ManualDrumFeatureDemonstrator.tex` also works).
4. Commit the PDF: the release workflow puts `docs/manual/*.pdf` into every release zip.

LaTeX writes helper files (`*.aux`, `*.log`, `*.out`, ...). Add them to your `.gitignore`, e.g.
`docs/manual/*.aux`, `docs/manual/*.log`, `docs/manual/*.out`, `docs/manual/*.fls`,
`docs/manual/*.fdb_latexmk`.

### Generated parts (with the Tester, see HowToTestYourPlugin.md)

**List of controls:** the table in the "Controls" section is the file `controls.tex`, generated from
your parameter definitions (range, default and the `help` line):
```console
DrumFeatureDemonstrator_Tester --manual tex > docs/manual/controls.tex
```
Run it again whenever you change a parameter. (`--manual md` gives the same list for the README.)

**Screenshot:** take it from the real plugin, e.g. with a preset and audio playing, so meters and
displays show something:
```console
DrumFeatureDemonstrator_Tester snapshot docs/manual/images/GUI.png --preset mypreset.xml --audio in.wav
```
For the numbered picture in "How to use", add the numbers with an image editor (e.g. GIMP) or with
ImageMagick, and save it as `images/GUI_Numbers.png`:
```console
convert GUI.png -font DejaVu-Sans-Bold -pointsize 30 -fill blue -annotate +40+30 "1" -annotate +300+30 "2" GUI_Numbers.png
```
The same screenshot (or one in the other theme) is good for the README.

## Development notes (docs/)

Write a short page (Markdown) for each bigger step or problem, e.g. `docs/2026-10-first-filter.md`:
what you did, why (the decision and the alternatives), how you tested it (with numbers:
measurements, pluginval results), and what is still open. It takes ten minutes and saves hours
when you (or someone else) come back to the code after some months. A plan file (`docs/plan.md`)
with the next steps helps as well.

## Release notes

When you make a release (see README, section AAT2), GitHub creates the release with a generic text
and the list of commits. Edit the release on GitHub and add a few lines for users: new features,
fixed bugs, and anything they have to know (e.g. that old presets do not work any more).
