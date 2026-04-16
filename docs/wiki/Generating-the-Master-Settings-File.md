## Creating or Updating a Setting
The `mss_for_json.ods` file, located in the root directory of the project, defines all available settings and their related info like display name, type, tooltip, dependencies, etc. This file is used to generate the json in `resources/configs/master.conf` that ORNLSlicer parses.

To create or edit a setting:
1. Make the necessary changes to the `mss_for_json.ods` file.
2. Run the CMake by clicking "Build" > "Run CMake" in Qt Creator (or your preferred IDE)
3. Verify that `master.conf` has changed as you expect

**DO NOT edit `master.conf` directly.**



## Manual Json Generation
> **Note:** Settings should no longer be generated manually. 

1. Make the desired changes to spreadsheet document in root of repo. This should be
mms_for_json.ods
2. In Excel/Open Office, export the spreadsheet as a CSV file.
3. Goto: https://www.csvjson.com/csv2json and open up the CSV file.
4. On the output, select Hash.
5. Generate the json and save the file to the repo under resources/configs/master.conf
