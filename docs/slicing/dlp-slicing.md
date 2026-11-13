# DLP Slicing

DLP is a separate slicing mode that exports BMP exposure masks. The existing Image slicer continues to produce its original object-ID images.

## Workflow

1. Configure a rectangular build volume under **Printer > Dimensions**, including minimum and maximum X, Y, and Z.
2. Load and position the build models and any imported support models.
3. Select **Profile > Slicing > Slicing Mode > DLP** and set **Profile > Layer > Layer Height**.
4. Under **Experimental > DLP Slicer**, choose **Output Mode**, set **Projection Width**, and use **4096** for **Projection Width in Pixels**. Static output offers **1-bit** and **8-bit** BMPs; Scrolling output always uses **1-bit**.
5. Slice. The **DLP Build Preview** window opens with layer selection, lane selection, and a lane-border overlay. Reopen it through **View > DLP Preview**.
6. Use **Export DLP Build** in the preview or Project menu and select an empty destination folder. The folder selection dialog can create a new folder.

Projection width is the physical X width of one projector lane. It uses the configured distance units, including inches. Dividing this width by the 4096-pixel lane width defines the physical size of each square pixel. The default width of **204.8 mm** gives a **0.05 mm/pixel** pitch.

Static lane images are **4096 × 2160 pixels**, or **204.8 × 108 mm** at the default pitch. Static mode supports one frame in Y: reduce the configured build depth or increase the physical projection width if it would exceed 2160 rows. Shorter builds are padded with black rows below minimum Y.

Scrolling lane images remain **4096 pixels wide**. Their height is rounded up to a multiple of **2176 rows**, with black padding below minimum Y. At the default pitch, each 2176-row block is **108.8 mm** long. Selecting Scrolling mode overrides the static BMP bit-depth selection and exports 1-bit images.

The full-layer diagnostic image covers the entire configured printer XY area without lane-frame padding. DLP checks both full-layer and padded-lane dimensions against Qt's image allocation limit before slicing and reports an actionable error if they exceed it. Large printers may require a wider physical projection lane or a smaller build area for a practical test resolution.

## Geometry and images

Layers share one build-relative Z grid: `build_minimum_z + layer_height / 2`, followed by full layer-height increments. Negative machine Z limits are converted to a positive build height, and enabled W-axis travel extends that height, matching the Part View build volume. Nonnegative explicit Z bounds retain their origin. Slicing stops at the highest model/support intersection within the build height. Empty layers below or between elevated geometry are retained.

Masks include build models and imported support models, preserve holes and disconnected regions, and combine overlapping models. Clipping and settings meshes do not contribute exposure geometry. Toolpath settings, such as infill and perimeter widths, do not alter DLP masks.

Lanes span the full Y dimension, start at minimum X, and advance in +X. Their count is `ceil((maximum_x - minimum_x) / projection_width)`. Every lane has the full projector width. When the printer width is not an integer multiple of the projection width and there is more than one lane, the final footprint shifts left to end at maximum X. The overlap is black in that final lane, so the preceding lane exposes it once. For example, a 300 mm printer width and 204.8 mm projection width place lane 2 at X = 95.2 mm and mask its first 109.6 mm. A shifted final lane requires the printer width to be a whole number of pixels; adjust the X bounds or pixel pitch if necessary. A build narrower than one lane retains minimum-X placement and black padding on the right.

The full image covers the printer XY bounds, rounded up to whole pixels. Lane-frame padding and overlap masks do not change that diagnostic image.

Images use +X to the right and +Y upward, with the top row at maximum Y. Pixel centers determine filled geometry. There is no added mirroring, rotation, or antialiasing. Static BMPs support 1-bit and 8-bit output; Scrolling BMPs are 1-bit. All currently contain white geometry on black. Grayscale stitching is not generated yet.

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
layer,z_mm,lane,file,empty,x_min_mm,y_max_mm,pixel_pitch_mm,width_px,height_px
1,0.05,0,Layer 1/Layer_1_Full.bmp,0,0,108,0.05,6000,2160
1,0.05,1,Layer 1/Layer1_1.bmp,0,0,108,0.05,4096,2160
1,0.05,2,Layer 1/Layer1_2.bmp,1,95.2,108,0.05,4096,2160
```

`z_mm` is the slice-plane height in millimeters regardless of display units. Lane `0` identifies the full-layer image. `file` is relative to the export root. `empty` is `1` when the image contains no exposed pixels and `0` otherwise. `x_min_mm` and `y_max_mm` locate the image's left and top edges, including the final lane's shifted footprint. `pixel_pitch_mm` gives both physical pixel dimensions; `width_px` and `height_px` include black padding. These physical values always use millimeters. Export requires a new or empty directory to avoid mixing builds.

## Command line

Use an `.s2c` configuration with `slicing_mode` set to `3`, or a project saved in DLP mode. The DLP setting keys are `dlp_output_mode` (`0` for Static, `1` for Scrolling), `dlp_projection_width` (internal distance units: micrometers), `dlp_pixel_width` (4096), and `dlp_bit_depth` (`0` for 1-bit, `1` for 8-bit; ignored in Scrolling mode).

```sh
ornlslicer --input_stl_files model.stl --input_global_settings dlp.s2c --output_location build
```

For DLP, `--output_location` is the build directory. The GUI and CLI use the same BMP and CSV exporter.
