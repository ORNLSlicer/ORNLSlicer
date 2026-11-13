# DLP Slicing

DLP is a separate slicing mode that exports BMP exposure masks. The existing Image slicer continues to produce its original object-ID images.

## Workflow

1. Configure a rectangular build volume under **Printer > Dimensions**, including minimum and maximum X, Y, and Z.
2. Load and position the build models and any imported support models.
3. Select **Profile > Slicing > Slicing Mode > DLP** and set **Profile > Layer > Layer Height**.
4. Under **Experimental > DLP Slicer**, set **Projection Width**, **Projection Width in Pixels**, and **BMP Bit Depth**.
5. Slice. The **DLP Build Preview** window opens with layer selection, lane selection, and a lane-border overlay. Reopen it through **View > DLP Preview**.
6. Use **Export DLP Build** in the preview or Project menu and select an empty destination folder. The folder selection dialog can create a new folder.

Projection width is the physical X width of one projector lane. It uses the configured distance units, including inches. Dividing this width by the lane's pixel count defines the physical size of each square pixel. For example, a 1-inch lane with 1,024 pixels has a pixel pitch of 1/1,024 inch in both X and Y.

The full-layer image covers the entire configured printer XY area. Large printers may require a lower pixel count or a wider projection lane for a practical test resolution. DLP checks full-layer and padded-lane dimensions against Qt's image allocation limit before slicing and reports an actionable error if they exceed it.

## Geometry and images

Layers share one build-relative Z grid: `build_minimum_z + layer_height / 2`, followed by full layer-height increments. Negative machine Z limits are converted to a positive build height, and enabled W-axis travel extends that height, matching the Part View build volume. Nonnegative explicit Z bounds retain their origin. Slicing stops at the highest model/support intersection within the build height. Empty layers below or between elevated geometry are retained.

Masks include build models and imported support models, preserve holes and disconnected regions, and combine overlapping models. Clipping and settings meshes do not contribute exposure geometry. Toolpath settings, such as infill and perimeter widths, do not alter DLP masks.

Lanes span the full Y dimension, start at minimum X, and advance in +X. Their count is `ceil((maximum_x - minimum_x) / projection_width)`. Every lane has the configured pixel width; the final lane is black-padded outside the printer bounds. The full image covers the printer XY bounds, rounded up to whole pixels.

Images use +X to the right and +Y upward, with the top row at maximum Y. Pixel centers determine filled geometry. There is no added mirroring, rotation, or antialiasing. Both selectable BMP depths, 1-bit and 8-bit, initially contain white geometry on black. Grayscale stitching is not generated yet.

Preview and export use the completed slice's settings and image files. Editing settings requires slicing again to produce different images.

## Export contract

Layer and lane numbers start at 1:

```text
build/
  build.csv
  Layer 1/
    Layer_1_Full.bmp
    Layer1_1.bmp
    Layer1_2.bmp
  Layer 2/
    Layer_2_Full.bmp
    Layer2_1.bmp
    Layer2_2.bmp
```

Empty full-layer and lane images are exported too. `build.csv` contains one row per image:

```csv
layer,z_mm,lane,file,empty
1,0.05,0,Layer 1/Layer_1_Full.bmp,0
1,0.05,1,Layer 1/Layer1_1.bmp,0
1,0.05,2,Layer 1/Layer1_2.bmp,1
```

`z_mm` is the slice-plane height in millimeters regardless of display units. Lane `0` identifies the full-layer image. `file` is relative to the export root. `empty` is `1` when the image contains no exposed pixels and `0` otherwise. Export requires a new or empty directory to avoid mixing builds.

## Command line

Use an `.s2c` configuration with `slicing_mode` set to `3`, or a project saved in DLP mode. The DLP setting keys are `dlp_projection_width` (internal distance units: micrometers), `dlp_pixel_width`, and `dlp_bit_depth` (`0` for 1-bit, `1` for 8-bit).

```sh
ornlslicer --input_stl_files model.stl --input_global_settings dlp.s2c --output_location build
```

For DLP, `--output_location` is the build directory. The GUI and CLI use the same BMP and CSV exporter.
