# DLP Slicing

DLP is a separate slicing mode that exports BMP exposure masks. The existing Image slicer continues to produce its original object-ID images.

## Workflow

1. Configure a rectangular build volume under **Printer > Dimensions**, including minimum and maximum X, Y, and Z.
2. Load and position the build models and any imported support models.
3. Select **Profile > Slicing > Slicing Mode > DLP** and set **Profile > Layer > Layer Height**.
4. Under **Experimental > DLP Slicer**, choose **Output Mode** and set **Projection Width**. **Projection Width in Pixels** is fixed at **4096**. Static output offers **1-bit** and **8-bit** BMPs; Scrolling output always uses **1-bit**. Optionally enable **Interlaced Lane Stitching** and set **Stitching Overlap Width**.
5. Click **Slice** to review pixel pitch, image dimensions, layer/lane counts, black padding, and estimated image memory and storage. Warnings identify models outside the printer bounds. The slicer checks available space on the temporary drive before generating images. Choose **Slice** in the summary to begin; the DLP progress window supports cancellation during layers. The **DLP Build Preview** opens when slicing finishes, with layer selection, lane selection, and a lane-border overlay. Reopen it through **View > DLP Preview**.
6. Use **Export DLP Build** in the preview or Project menu, browse to the parent location, enter a **Folder name**, and click **Create Folder**. The export creates that folder and saves the build inside it.

Projection width is the physical X width of one projector lane. It uses the configured distance units, including inches. Dividing this width by the 4096-pixel lane width defines the physical size of each square pixel. The default width of **204.8 mm** gives a **0.05 mm/pixel** pitch.

If an older project contains a different pixel width, its saved value remains visible with **requires 4096**. Click the adjacent reset button (**Use required value (4096)**) to update it before slicing.

Static lane images are **4096 × 2160 pixels**, or **204.8 × 108 mm** at the default pitch. Static mode supports one frame in Y: reduce the configured build depth or increase the physical projection width if it would exceed 2160 rows. Shorter builds are padded with black rows below minimum Y.

Scrolling lane images remain **4096 pixels wide**. Their height is rounded up to a multiple of **2176 rows**, with black padding below minimum Y. At the default pitch, each 2176-row block is **108.8 mm** long. Selecting Scrolling mode overrides the static BMP bit-depth selection and exports 1-bit images.

The full-layer diagnostic image covers the entire configured printer XY area without lane-frame padding. Before slicing, DLP checks its actual grayscale working-buffer size, including scanline padding, against Qt's image allocation limit. Padded lane images retain their 32-bit memory budget under the same limit. Large printers may require a wider physical projection lane or a smaller build area if either allocation exceeds the limit.

Preview images are reduced to fit within 2048 × 2048 pixels before display. The preview retains the full-resolution coordinates, lane borders, and physical-size readouts and identifies its reduced display dimensions. Exported BMPs and CSV dimensions remain at full resolution. This allows, for example, a 16384 × 12288 full layer with a 192 MiB grayscale buffer even though a full-size 32-bit display image would require 768 MiB. The global image-loading limit remains unchanged.

To inspect fine features or seam masks, select a lane and click a point in its overview, or enable **Inspect pixels (1:1)**. Inspection displays a native-pixel region no larger than 1024 × 1024 pixels. Drag to move through the image or use the arrow keys to move one image pixel at a time; the status shows the inspected source coordinates. Turn inspection off to return to the overview. Full-layer images are labeled **diagnostic only** and are not projector frames.

## Geometry and images

Layers share one build-relative Z grid: `build_minimum_z + layer_height / 2`, followed by full layer-height increments. Negative machine Z limits are converted to a positive build height, and enabled W-axis travel extends that height, matching the Part View build volume. Nonnegative explicit Z bounds retain their origin. Slicing stops at the highest model/support intersection within the build height. Empty layers below or between elevated geometry are retained.

Masks include build models and imported support models, preserve holes and disconnected regions, and combine overlapping models. Clipping and settings meshes do not contribute exposure geometry. Toolpath settings, such as infill and perimeter widths, do not alter DLP masks.

Lanes span the full Y dimension, start at minimum X, and advance in +X. With stitching off, their count is `ceil((maximum_x - minimum_x) / projection_width)`. Every lane has the full projector width. When the printer width is not an integer multiple of the projection width and there is more than one lane, the final footprint shifts left to end at maximum X. With stitching off, the overlap is black in that final lane, so the preceding lane exposes it once. For example, a 300 mm printer width and 204.8 mm projection width place lane 2 at X = 95.2 mm and mask its first 109.6 mm. A shifted final lane requires the printer width to be a whole number of pixels; adjust the X bounds or pixel pitch if necessary. A build narrower than one lane retains minimum-X placement and black padding on the right.

The full image covers the printer XY bounds, rounded up to whole pixels. Lane-frame padding and overlap masks do not change that diagnostic image.

Images use +X to the right and +Y upward, with the top row at maximum Y. Pixel centers determine filled geometry. There is no added mirroring, rotation, or antialiasing. Static BMPs support 1-bit and 8-bit output; Scrolling BMPs are 1-bit. All currently contain white geometry on black. Grayscale stitching is not generated yet.

Preview and export use the completed slice's settings and image files. Editing settings requires slicing again to produce different images.

## Interlaced lane stitching

**Interlaced Lane Stitching** is an optional checkbox under **Experimental > DLP Slicer**, off by default. Turning it off reproduces the existing lane layout and exposure images. When enabled, **Stitching Overlap Width** controls both the minimum physical overlap between neighboring projector footprints and the X width of the interlaced transition band. Its initial **1 mm** value is a test setting that requires printer calibration.

The requested overlap rounds up to whole pixels. At the default 0.05 mm/pixel, 1 mm becomes 20 pixels. The slicer keeps each image 4096 pixels wide and preserves pixel pitch; normal lane advance becomes `projection_width - effective_overlap`. The last footprint still ends at maximum X, so its physical overlap may be larger. The transition band is centered within the actual shared footprint. Extra overlap can require an additional lane to cover the same build area. The controller must use each image's exported **x_min_mm** origin rather than assuming full-width steps.

Within each transition band, complementary black-and-white fingers assign every intended model pixel to one lane. Finger height uses the same pixel count as the effective overlap width. The pattern flips on successive layers, moving the join within the band without moving the model geometry. Ownership remains unique even where more than two footprints overlap. Padding stays black, and the full-layer diagnostic image remains unchanged. Static 8-bit and Scrolling 1-bit output use the same binary interlacing; no grayscale or exposure-time adjustment is introduced.

With stitching enabled, the full-layer preview can show **stitching bands** instead of straight exposure boundaries. Select a numbered lane and inspect its pixels at 1:1 to see the generated fingers. The slice summary reports the effective width and resulting lane count before work begins. A single-lane build has no stitching seam.

The masks maintain one commanded exposure per intended pixel under ideal positioning. This does not establish physical bond strength: calibrate the overlap and verify printed seam-crossing samples with the actual resin, exposure sequence, and positioning accuracy before production use.

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

Export runs in the background with progress and **Cancel Export**. Images and CSV are first written to a temporary folder beside the destination; the completed folder is published only after all files succeed. A failed or canceled export removes its temporary output, so the same folder name can be retried. Slicing's temporary image cache remains available for another export.

Before export, the slicer checks the destination drive for space for the additional image copy and CSV. The existing cache is already included in the drive's used space. New export folders use normal folder-creation permissions; an existing empty destination retains its access-permission bits when the completed build replaces it.

## Command line

Use an `.s2c` configuration with `slicing_mode` set to `3`, or a project saved in DLP mode. The DLP setting keys are `dlp_output_mode` (`0` for Static, `1` for Scrolling), `dlp_projection_width` (internal distance units: micrometers), `dlp_pixel_width` (4096), `dlp_bit_depth` (`0` for 1-bit, `1` for 8-bit; ignored in Scrolling mode), `dlp_interlaced_stitching` (boolean, default `false`), and `dlp_stitch_overlap` (internal distance units, default `1000` = 1 mm; used only when stitching is enabled).

```sh
ornlslicer --input_stl_files model.stl --input_global_settings dlp.s2c --output_location build
```

For DLP, `--output_location` is the build directory. The GUI and CLI use the same BMP and CSV exporter.
