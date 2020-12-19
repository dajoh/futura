# Converting BDF to PSF2 (PC Screen Font version 2)

1. Convert the BDF font to PSF1 using `bdf2psf`

```
bdf2psf --fb \
  FONTNAME.bdf \
  /usr/share/bdf2psf/standard.equivalents \
  /usr/share/bdf2psf/ascii.set+/usr/share/bdf2psf/linux.set+/usr/share/bdf2psf/useful.set \
  512 \
  FONTNAME.psf1
```

2. Decompile the PSF1 font using `psfd`
```
psfd FONTNAME.psf1 > FONTNAME.psf1_txt
```

3. Change the PSF version in the header of `FONTNAME.psf1_txt` to version 2

4. Compile the font using `psfc`
```sh
psfc FONTNAME.psf1_txt FONTNAME.psfu
```
