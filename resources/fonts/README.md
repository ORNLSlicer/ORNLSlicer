# Bundled Noto Sans fonts

The GUI uses the unmodified hinted Noto Sans Regular and SemiBold TrueType faces.
They are embedded in `styles.qrc` and registered at GUI startup, so installation
does not depend on the fonts available on the host system.

Source: [notofonts/noto-fonts](https://github.com/notofonts/noto-fonts/tree/ffebf8c1ee449e544955a7e813c54f9b73848eac/hinted/ttf/NotoSans),
revision `ffebf8c1ee449e544955a7e813c54f9b73848eac`.
The source repository's license is included verbatim in [OFL.txt](OFL.txt).
CMake also installs it under the application documentation's `licenses/NotoSans-OFL.txt`.

| File | SHA-256 |
|--|--|
| `NotoSans-Regular.ttf` | `b85c38ecea8a7cfb39c24e395a4007474fa5a4fc864f6ee33309eb4948d232d5` |
| `NotoSans-SemiBold.ttf` | `87a8b90ece1e89746b544e4e086f85a3710e41485a8078f9be874837dfad45d5` |
